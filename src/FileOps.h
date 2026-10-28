#pragma once

#include <QFuture>
#include <QObject>
#include <QStringList>
#include <QUrl>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

#include <atomic>
#include <memory>

// Действия над файлами. Копирование/перемещение — в фоновом потоке
// с прогрессом; одновременно идёт одна операция.
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

public:
    explicit FileOps(QObject *parent = nullptr);
    ~FileOps() override;

    bool busy() const { return m_busy; }
    double progress() const { return m_progress; }
    QString jobText() const { return m_jobText; }
    bool clipboardHasFiles() const { return !m_clipFiles.isEmpty(); }
    bool clipboardIsCut() const { return m_clipCut; }

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
    Q_INVOKABLE void paste(const QString &destDir);
    Q_INVOKABLE void transfer(const QStringList &sources, const QString &destDir, bool move);
    // Перетаскивание: локальные пути из URL; перемещать или копировать —
    // Ctrl копирует, Shift перемещает, без клавиш — перемещение в пределах диска
    Q_INVOKABLE QString uriList(const QStringList &paths) const;
    Q_INVOKABLE QStringList localPaths(const QList<QUrl> &urls) const;
    Q_INVOKABLE bool dropMoves(const QStringList &sources, const QString &destDir) const;
    // Тип брошенного файла — для его листка в анимации броска (файл ещё на старом месте)
    Q_INVOKABLE QString mimeTypeOf(const QString &path) const;
    Q_INVOKABLE void cancel();

    Q_INVOKABLE QVariantMap info(const QString &path);

signals:
    void busyChanged();
    void progressChanged();
    void clipboardChanged();
    void failed(const QString &message);
    // Операция закончилась; newPaths — что появилось в папке назначения
    void finished(const QStringList &newPaths);

private:
    void readClipboard();
    void setProgress(double p, const QString &text);
    void removeInBackground(const QStringList &paths, const QString &text);
    void finishJob(const QStringList &created, const QString &error, bool cancelled);

    bool m_busy = false;
    double m_progress = 0;
    QString m_jobText;
    QStringList m_clipFiles;
    bool m_clipCut = false;
    std::shared_ptr<std::atomic_bool> m_cancel;
    QFuture<void> m_job;
};
