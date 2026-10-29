#pragma once

#include <QFuture>
#include <QHash>
#include <QObject>
#include <QStringList>
#include <QUrl>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

#include <atomic>
#include <memory>

// Шаг операции, который можно откатить (Ctrl+Z)
struct UndoStep {
    enum Kind {
        Created,    // from — появился (копия с to): откат — удалить
        Moved,      // from → to (перемещение, переименование): откат — обратно
        Trashed,    // from убран в корзину, лежит в to: откат — вернуть
        MadeDir,    // from — новая пустая папка: откат — удалить, если пуста
        RemovedDir, // from — опустевшая папка после перемещения с объединением: откат — создать
        // Только для повтора (Ctrl+Shift+Z) — обратные шаги отката:
        Uncopied,   // копию from удалили: откат — снова скопировать to в from
        Restored,   // from вернули из корзины: откат — снова в корзину
    };
    Kind kind;
    QString from;
    QString to;
};

// Действия над файлами. Копирование/перемещение — в фоновом потоке
// с прогрессом; одновременно идёт одна операция. Сделанное записывается
// в журнал, последнее можно отменить
class FileOps : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(double progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(QString jobText READ jobText NOTIFY progressChanged)
    Q_PROPERTY(bool clipboardHasFiles READ clipboardHasFiles NOTIFY clipboardChanged)
    Q_PROPERTY(bool clipboardIsCut READ clipboardIsCut NOTIFY clipboardChanged)
    // Что отменит undo(): «перемещение «отчёт.pdf»»; пусто — отменять нечего
    Q_PROPERTY(QString undoText READ undoText NOTIFY undoChanged)
    // Что это за операция — для логики, текст undoText переведён: move, copy, trash, rename, mkdir
    Q_PROPERTY(QString undoKind READ undoKind NOTIFY undoChanged)
    // Что вернёт redo() — отменённое последним; пусто — повторять нечего
    Q_PROPERTY(QString redoText READ redoText NOTIFY undoChanged)
    // Чем работать с архивами: bsdtar (libarchive) или 7z; пусто — нечем
    Q_PROPERTY(QString archiver READ archiver CONSTANT)

public:
    explicit FileOps(QObject *parent = nullptr);
    ~FileOps() override;

    bool busy() const { return m_busy; }
    double progress() const { return m_progress; }
    QString jobText() const { return m_jobText; }
    bool clipboardHasFiles() const { return !m_clipFiles.isEmpty(); }
    bool clipboardIsCut() const { return m_clipCut; }
    QString undoText() const { return m_undo.isEmpty() ? QString() : m_undo.last().label; }
    QString undoKind() const { return m_undo.isEmpty() ? QString() : m_undo.last().kind; }
    QString redoText() const { return m_redo.isEmpty() ? QString() : m_redo.last().label; }
    static QString archiver();

    Q_INVOKABLE void openTerminal(const QString &dir);
    // Возвращают пустую строку при успехе, иначе текст ошибки
    Q_INVOKABLE QString trash(const QStringList &paths);
    // Возвращает { error, paths } — куда вернулись объекты (имя могло получить « (2)»)
    Q_INVOKABLE QVariantMap restore(const QStringList &paths);
    // Безвозвратно, в фоне; только для того, что лежит в корзине
    Q_INVOKABLE void deleteForever(const QStringList &paths);
    Q_INVOKABLE void emptyTrash();
    Q_INVOKABLE QVariantMap trashInfo(const QString &path);
    Q_INVOKABLE QString rename(const QString &path, const QString &newName);
    // Новая папка с уникальным именем; возвращает её путь или пустую строку
    Q_INVOKABLE QString makeDir(const QString &dir, const QString &baseName);

    Q_INVOKABLE void copyToClipboard(const QStringList &paths, bool cut);
    Q_INVOKABLE void copyText(const QString &text);
    Q_INVOKABLE QStringList clipboardPaths();
    // Что из sources уже есть в destDir — для вопроса перед переносом:
    // [{ name, isDir, merge, canReplace, srcInfo, dstInfo, newer: "src" | "dst" | "" }]
    Q_INVOKABLE QVariantList conflicts(const QStringList &sources, const QString &destDir) const;
    // resolutions: имя → "replace" | "skip" | "keep". Без ответа — «keep»: рядом, с « (2)».
    // replace: файл — заменить (старый уходит в корзину), папку — объединить с папкой
    Q_INVOKABLE void paste(const QString &destDir, const QVariantMap &resolutions = {});
    Q_INVOKABLE void transfer(const QStringList &sources, const QString &destDir, bool move,
                              const QVariantMap &resolutions = {});
    // Перетаскивание: локальные пути из URL; перемещать или копировать —
    // Ctrl копирует, Shift перемещает, без клавиш — перемещение в пределах диска
    Q_INVOKABLE QString uriList(const QStringList &paths) const;
    Q_INVOKABLE QStringList localPaths(const QList<QUrl> &urls) const;
    Q_INVOKABLE bool dropMoves(const QStringList &sources, const QString &destDir) const;
    // Тип брошенного файла — для его листка в анимации броска (файл ещё на старом месте)
    Q_INVOKABLE QString mimeTypeOf(const QString &path) const;
    Q_INVOKABLE void cancel();
    // Откатить последнюю операцию из журнала — в фоне, как перенос
    Q_INVOKABLE void undo();
    // Повторить последнее отменённое; новая операция забывает отменённое
    Q_INVOKABLE void redo();
    // Что исчезнет при undo(): копии и перенесённое (для анимации «вылетают из папки»)
    Q_INVOKABLE QStringList undoTargets() const;

    Q_INVOKABLE QVariantMap info(const QString &path);

    // ── Архивы ──
    // Архив ли это (по MIME-типу) и есть ли чем его распаковать
    Q_INVOKABLE bool canExtract(const QString &mimeType) const;
    // Умеем ли создать архив: "zip" или "tar.zst"
    Q_INVOKABLE bool canCompress(const QString &format) const;
    // В фоне. intoFolder — в новую папку с именем архива; иначе сюда: сначала во временную
    // папку рядом, потом сигнал extracted — с вопросом о совпадениях имён, как при переносе
    Q_INVOKABLE void extract(const QString &archive, const QString &destDir, bool intoFolder);
    // Второй шаг «распаковать сюда»: из временной папки в destDir (resolutions — как у transfer)
    Q_INVOKABLE void placeExtracted(const QString &tmpDir, const QString &destDir, const QVariantMap &resolutions = {});
    // Передумали на вопросе о совпадениях — убрать временную папку
    Q_INVOKABLE void discardExtracted(const QString &tmpDir);
    // Архив format из paths (одна папка) рядом с ними, в фоне
    Q_INVOKABLE void compress(const QStringList &paths, const QString &format);
    // «отчёт.tar.gz» → «отчёт»
    Q_INVOKABLE static QString archiveBaseName(const QString &fileName);

    // Быстрый просмотр (Space): как показать файл — "image", "text", "dir" или "other"
    Q_INVOKABLE QString previewKind(const QString &path, const QString &mimeType) const;
    // Начало текстового файла — в фоне, ответ сигналом textPreview
    Q_INVOKABLE void requestTextPreview(const QString &path);
    // Первые limit байт как текст: { text, truncated, binary }
    static QVariantMap readTextPreview(const QString &path, qint64 limit = 64 * 1024);

signals:
    void busyChanged();
    void progressChanged();
    void clipboardChanged();
    void failed(const QString &message);
    // Операция закончилась; newPaths — что появилось в папке назначения
    void finished(const QStringList &newPaths);
    void undoChanged();
    // Отмена закончилась без ошибок; label — что отменили
    void undone(const QString &label);
    // Повтор закончился без ошибок
    void redone(const QString &label);
    // Распаковано во временную папку tmpDir; conflicts — как у conflicts(), пусто — можно класть
    void extracted(const QString &tmpDir, const QString &destDir, const QVariantList &conflicts);
    // Ответ на requestTextPreview; binary — внутри нули, это не текст
    void textPreview(const QString &path, const QString &text, bool truncated, bool binary);

private:
    void readClipboard();
    void setProgress(double p, const QString &text);
    void removeInBackground(const QStringList &paths, const QString &text);
    void finishJob(const QStringList &created, const QString &error, bool cancelled);
    bool startJob(const QString &text);
    void record(const QString &kind, const QString &label, const QList<UndoStep> &steps);

    struct UndoEntry {
        QString kind;
        QString label;
        QList<UndoStep> steps;
    };
    // Откатить запись в фоне; её обратные шаги уходят в другой журнал:
    // из отмены — в повтор, из повтора — снова в отмену
    void rollBack(bool redo);
    QList<UndoEntry> m_undo;
    QList<UndoEntry> m_redo;
    // Временная папка распаковки → имя архива, пока ждём ответа о совпадениях
    QHash<QString, QString> m_extracting;

    bool m_busy = false;
    double m_progress = 0;
    QString m_jobText;
    QStringList m_clipFiles;
    bool m_clipCut = false;
    std::shared_ptr<std::atomic_bool> m_cancel;
    QFuture<void> m_job;
};
