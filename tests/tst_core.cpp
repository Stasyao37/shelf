// Модель папки и операции с файлами — без QML, на временных папках; подкраска иконок.
// Корзина и настройки — тоже во временной папке: HOME и XDG_* подменяются до первого обращения
#include "DirModel.h"
#include "FileOps.h"
#include "GlassIcons.h"
#include "Language.h"

#include <QAbstractItemModelTester>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QRandomGenerator>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

namespace {
void write(const QString &path, const QByteArray &data = "x")
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(data);
}
QByteArray read(const QString &path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}
QStringList names(const QString &dir)
{
    return QDir(dir).entryList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden, QDir::Name);
}
}

class TestCore : public QObject
{
    Q_OBJECT

    QTemporaryDir m_home;
    QString m_dir; // своя папка на каждый тест

    // Фоновая операция FileOps: дождаться finished
    static bool waitJob(FileOps &ops)
    {
        QSignalSpy done(&ops, &FileOps::finished);
        return ops.busy() ? done.wait(10000) : true;
    }

private slots:
    void initTestCase()
    {
        QVERIFY(m_home.isValid());
        qputenv("HOME", m_home.path().toUtf8());
        qputenv("XDG_DATA_HOME", (m_home.path() + "/.local/share").toUtf8());
        qputenv("XDG_CONFIG_HOME", (m_home.path() + "/.config").toUtf8());
        // Qt создаёт корзину без mkpath: ~/.local/share должна уже быть
        QVERIFY(QDir().mkpath(m_home.path() + "/.local/share"));
        // Язык системы на сборочной машине может быть любым — сообщения проверяем по-русски
        Language::apply(QStringLiteral("ru"));
    }
    void init()
    {
        m_dir = m_home.path() + "/w/" + QTest::currentTestFunction();
        QVERIFY(QDir().mkpath(m_dir));
    }

    // ── DirModel ───────────────────────────────────────────────────
    // Смена фильтра — сброс модели, а не построчные удаления: иначе GridView
    // обрывает анимации и оставляет «призраков» (окно выбора файлов, 0.3.2)
    void filterChangeResetsModel()
    {
        write(m_dir + "/a.csv");
        write(m_dir + "/b.txt");
        write(m_dir + "/c.pdf");
        QDir().mkpath(m_dir + "/sub");
        DirModel model;
        QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
        model.setPath(m_dir);
        QCOMPARE(model.count(), 4);

        QSignalSpy reset(&model, &QAbstractItemModel::modelReset);
        QSignalSpy removed(&model, &QAbstractItemModel::rowsRemoved);
        QSignalSpy inserted(&model, &QAbstractItemModel::rowsInserted);
        model.setNameFilters({"*.csv"});
        model.setMimeFilters({"application/pdf"});
        QCOMPARE(model.count(), 3); // папка всегда видна
        model.setDirsOnly(true);
        QCOMPARE(model.count(), 1);
        QCOMPARE(reset.count(), 3);
        QCOMPARE(removed.count(), 0);
        QCOMPARE(inserted.count(), 0);
    }

    // Новый файл в той же папке — одна вставленная строка, без сброса
    void newFileInsertsRow()
    {
        write(m_dir + "/a.txt");
        write(m_dir + "/c.txt");
        DirModel model;
        QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
        model.setPath(m_dir);

        QSignalSpy reset(&model, &QAbstractItemModel::modelReset);
        QSignalSpy inserted(&model, &QAbstractItemModel::rowsInserted);
        write(m_dir + "/b.txt");
        model.reload();
        QCOMPARE(reset.count(), 0);
        QCOMPARE(inserted.count(), 1);
        QCOMPARE(inserted.first().at(1).toInt(), 1);
        QCOMPARE(model.pathAt(1), m_dir + "/b.txt");
    }

    // Другая папка — сброс, даже если строк мало
    void pathChangeResetsModel()
    {
        write(m_dir + "/one/a.txt");
        write(m_dir + "/two/b.txt");
        DirModel model;
        QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
        model.setPath(m_dir + "/one");
        QSignalSpy reset(&model, &QAbstractItemModel::modelReset);
        QSignalSpy removed(&model, &QAbstractItemModel::rowsRemoved);
        model.setPath(m_dir + "/two");
        QCOMPARE(reset.count(), 1);
        QCOMPARE(removed.count(), 0);
        QCOMPARE(model.pathAt(0), m_dir + "/two/b.txt");
    }

    // Файл без расширения: листинг его не открывает, тип по содержимому приходит
    // из фона одной правкой строки; перечитывание берёт его из памяти, без повторного захода
    void contentTypeInBackground()
    {
        write(m_dir + "/doc", "%PDF-1.4\n%\xe2\xe3\xcf\xd3\n");
        write(m_dir + "/a.txt");
        DirModel model;
        QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
        QSignalSpy changed(&model, &QAbstractItemModel::dataChanged);
        model.setPath(m_dir);
        const int row = model.indexOfPath(m_dir + "/doc");
        QCOMPARE(model.mimeTypeAt(row), QStringLiteral("application/octet-stream"));
        QTRY_COMPARE(model.mimeTypeAt(row), QStringLiteral("application/pdf"));
        QCOMPARE(changed.count(), 1);

        QSignalSpy reset(&model, &QAbstractItemModel::modelReset);
        model.reload();
        QCOMPARE(model.mimeTypeAt(row), QStringLiteral("application/pdf"));
        QCOMPARE(reset.count(), 0);
    }

    // ── Конфликты при переносе ─────────────────────────────────────
    void conflictsListsOnlyClashes()
    {
        write(m_dir + "/src/a.txt");
        write(m_dir + "/src/b.txt");
        QDir().mkpath(m_dir + "/src/d");
        write(m_dir + "/dst/a.txt");
        QDir().mkpath(m_dir + "/dst/d");
        FileOps ops;
        const QVariantList cs = ops.conflicts({m_dir + "/src/a.txt", m_dir + "/src/b.txt", m_dir + "/src/d"}, m_dir + "/dst");
        QCOMPARE(cs.size(), 2);
        QCOMPARE(cs.at(0).toMap().value("name").toString(), "a.txt");
        QCOMPARE(cs.at(0).toMap().value("merge").toBool(), false);
        QCOMPARE(cs.at(1).toMap().value("merge").toBool(), true);
        // Копия в ту же папку — не конфликт, всегда « (2)»
        QVERIFY(ops.conflicts({m_dir + "/src/a.txt"}, m_dir + "/src").isEmpty());
    }

    void keepBothByDefault()
    {
        write(m_dir + "/src/a.txt", "new");
        write(m_dir + "/dst/a.txt", "old");
        FileOps ops;
        ops.transfer({m_dir + "/src/a.txt"}, m_dir + "/dst", false);
        QVERIFY(waitJob(ops));
        QCOMPARE(names(m_dir + "/dst"), QStringList({"a (2).txt", "a.txt"}));
        QCOMPARE(read(m_dir + "/dst/a.txt"), "old");
    }

    void skipLeavesTarget()
    {
        write(m_dir + "/src/a.txt", "new");
        write(m_dir + "/dst/a.txt", "old");
        FileOps ops;
        ops.transfer({m_dir + "/src/a.txt"}, m_dir + "/dst", true, {{"a.txt", "skip"}});
        QVERIFY(waitJob(ops));
        QCOMPARE(read(m_dir + "/dst/a.txt"), "old");
        QVERIFY(QFile::exists(m_dir + "/src/a.txt"));
        QVERIFY(ops.undoText().isEmpty()); // ничего не сделано — отменять нечего
    }

    void replaceAndUndo()
    {
        write(m_dir + "/src/a.txt", "new");
        write(m_dir + "/dst/a.txt", "old");
        FileOps ops;
        ops.transfer({m_dir + "/src/a.txt"}, m_dir + "/dst", false, {{"a.txt", "replace"}});
        QVERIFY(waitJob(ops));
        QCOMPARE(names(m_dir + "/dst"), QStringList({"a.txt"}));
        QCOMPARE(read(m_dir + "/dst/a.txt"), "new");
        QCOMPARE(ops.undoText(), "копирование «a.txt»");

        const QString trashFiles = m_home.path() + "/.local/share/Trash/files";
        const qsizetype inTrash = names(trashFiles).size(); // там старый a.txt
        QCOMPARE(ops.undoTargets(), QStringList({m_dir + "/dst/a.txt"}));
        QSignalSpy undone(&ops, &FileOps::undone);
        ops.undo();
        QVERIFY(waitJob(ops));
        QCOMPARE(undone.count(), 1);
        QCOMPARE(read(m_dir + "/dst/a.txt"), "old");
        QCOMPARE(names(m_dir + "/dst"), QStringList({"a.txt"}));
        QCOMPARE(read(m_dir + "/src/a.txt"), "new");
        // Старый вернулся из корзины, а копия удалена совсем, не легла туда вместо него
        QCOMPARE(names(trashFiles).size(), inTrash - 1);
    }

    // Перемещение папки в папку с тем же именем: объединение и полный откат
    void mergeMoveAndUndo()
    {
        write(m_dir + "/src/d/x.txt", "x");
        write(m_dir + "/src/d/y.txt", "new y");
        write(m_dir + "/src/d/in/deep.txt", "deep");
        write(m_dir + "/dst/d/y.txt", "old y");
        write(m_dir + "/dst/d/z.txt", "z");
        FileOps ops;
        ops.transfer({m_dir + "/src/d"}, m_dir + "/dst", true, {{"d", "replace"}});
        QVERIFY(waitJob(ops));
        QCOMPARE(names(m_dir + "/dst/d"), QStringList({"in", "x.txt", "y.txt", "z.txt"}));
        QCOMPARE(read(m_dir + "/dst/d/y.txt"), "new y");
        QVERIFY(!QFileInfo::exists(m_dir + "/src/d"));

        ops.undo();
        QVERIFY(waitJob(ops));
        QCOMPARE(names(m_dir + "/src/d"), QStringList({"in", "x.txt", "y.txt"}));
        QCOMPARE(read(m_dir + "/src/d/y.txt"), "new y");
        QCOMPARE(read(m_dir + "/src/d/in/deep.txt"), "deep");
        QCOMPARE(names(m_dir + "/dst/d"), QStringList({"y.txt", "z.txt"}));
        QCOMPARE(read(m_dir + "/dst/d/y.txt"), "old y");
    }

    // Папку, внутри которой лежит переносимое, не заменяем, даже если попросили
    void replaceAncestorKeepsBoth()
    {
        write(m_dir + "/d/d/a.txt");
        FileOps ops;
        const QVariantList cs = ops.conflicts({m_dir + "/d/d"}, m_dir);
        QCOMPARE(cs.size(), 1);
        QCOMPARE(cs.first().toMap().value("canReplace").toBool(), false);
        ops.transfer({m_dir + "/d/d"}, m_dir, true, {{"d", "replace"}});
        QVERIFY(waitJob(ops));
        QVERIFY(QFile::exists(m_dir + "/d (2)/a.txt"));
        QVERIFY(QFileInfo(m_dir + "/d").isDir());
    }

    // ── Отмена простых действий ────────────────────────────────────
    void trashAndUndo()
    {
        write(m_dir + "/a.txt", "a");
        FileOps ops;
        QCOMPARE(ops.trash({m_dir + "/a.txt"}), QString());
        QVERIFY(!QFile::exists(m_dir + "/a.txt"));
        QCOMPARE(ops.undoText(), "удаление в корзину «a.txt»");
        ops.undo();
        QVERIFY(waitJob(ops));
        QCOMPARE(read(m_dir + "/a.txt"), "a");
    }

    void renameAndMakeDirUndo()
    {
        write(m_dir + "/a.txt", "a");
        FileOps ops;
        QCOMPARE(ops.rename(m_dir + "/a.txt", "b.txt"), QString());
        const QString made = ops.makeDir(m_dir, "Новая папка");
        QVERIFY(QFileInfo(made).isDir());

        ops.undo(); // папка
        QVERIFY(waitJob(ops));
        QVERIFY(!QFileInfo::exists(made));
        ops.undo(); // имя
        QVERIFY(waitJob(ops));
        QCOMPARE(names(m_dir), QStringList({"a.txt"}));
        QVERIFY(ops.undoText().isEmpty());
    }

    // Отмена, когда файл уже унесли: ошибка, остальное откатывается
    void undoMissingReportsError()
    {
        write(m_dir + "/a.txt");
        write(m_dir + "/b.txt");
        QDir().mkpath(m_dir + "/dst");
        FileOps ops;
        ops.transfer({m_dir + "/a.txt", m_dir + "/b.txt"}, m_dir + "/dst", true);
        QVERIFY(waitJob(ops));
        QFile::remove(m_dir + "/dst/a.txt");
        QSignalSpy failed(&ops, &FileOps::failed);
        ops.undo();
        QVERIFY(waitJob(ops));
        QCOMPARE(failed.count(), 1);
        QVERIFY(QFile::exists(m_dir + "/b.txt"));
    }

    // ── Повтор отменённого: undo → redo → undo ────────────────────
    void redoMove()
    {
        write(m_dir + "/a.txt", "a");
        QDir().mkpath(m_dir + "/dst");
        FileOps ops;
        ops.transfer({m_dir + "/a.txt"}, m_dir + "/dst", true);
        QVERIFY(waitJob(ops));
        QVERIFY(ops.redoText().isEmpty());
        ops.undo();
        QVERIFY(waitJob(ops));
        QCOMPARE(read(m_dir + "/a.txt"), "a");
        QCOMPARE(ops.redoText(), "перемещение «a.txt»");
        QVERIFY(ops.undoText().isEmpty());

        QSignalSpy redone(&ops, &FileOps::redone);
        ops.redo();
        QVERIFY(waitJob(ops));
        QCOMPARE(redone.count(), 1);
        QVERIFY(!QFile::exists(m_dir + "/a.txt"));
        QCOMPARE(read(m_dir + "/dst/a.txt"), "a");
        QCOMPARE(ops.undoText(), "перемещение «a.txt»");
        QCOMPARE(ops.undoKind(), "move");
        QVERIFY(ops.redoText().isEmpty());

        ops.undo();
        QVERIFY(waitJob(ops));
        QCOMPARE(read(m_dir + "/a.txt"), "a");
        QCOMPARE(names(m_dir + "/dst"), QStringList());
    }

    void redoCopy()
    {
        write(m_dir + "/a.txt", "a");
        QDir().mkpath(m_dir + "/dst");
        FileOps ops;
        ops.transfer({m_dir + "/a.txt"}, m_dir + "/dst", false);
        QVERIFY(waitJob(ops));
        ops.undo();
        QVERIFY(waitJob(ops));
        QCOMPARE(names(m_dir + "/dst"), QStringList());
        ops.redo();
        QVERIFY(waitJob(ops));
        QCOMPARE(read(m_dir + "/dst/a.txt"), "a");
        QCOMPARE(read(m_dir + "/a.txt"), "a");
        QCOMPARE(ops.undoTargets(), QStringList({m_dir + "/dst/a.txt"}));
        ops.undo();
        QVERIFY(waitJob(ops));
        QCOMPARE(names(m_dir + "/dst"), QStringList());
        QCOMPARE(read(m_dir + "/a.txt"), "a");
    }

    void redoTrash()
    {
        write(m_dir + "/a.txt", "a");
        FileOps ops;
        QCOMPARE(ops.trash({m_dir + "/a.txt"}), QString());
        ops.undo();
        QVERIFY(waitJob(ops));
        QCOMPARE(read(m_dir + "/a.txt"), "a");
        ops.redo();
        QVERIFY(waitJob(ops));
        QVERIFY(!QFile::exists(m_dir + "/a.txt"));
        QCOMPARE(ops.undoText(), "удаление в корзину «a.txt»");
        ops.undo();
        QVERIFY(waitJob(ops));
        QCOMPARE(read(m_dir + "/a.txt"), "a");
    }

    void redoRenameAndMakeDir()
    {
        write(m_dir + "/a.txt", "a");
        FileOps ops;
        QCOMPARE(ops.rename(m_dir + "/a.txt", "b.txt"), QString());
        const QString made = ops.makeDir(m_dir, "Новая папка");
        ops.undo(); // папка
        QVERIFY(waitJob(ops));
        ops.undo(); // имя
        QVERIFY(waitJob(ops));
        QCOMPARE(names(m_dir), QStringList({"a.txt"}));

        // Повтор — в обратном порядке отмены: сначала имя, потом папка
        QCOMPARE(ops.redoText(), "переименование «a.txt»");
        ops.redo();
        QVERIFY(waitJob(ops));
        QCOMPARE(names(m_dir), QStringList({"b.txt"}));
        ops.redo();
        QVERIFY(waitJob(ops));
        QVERIFY(QFileInfo(made).isDir());
        QVERIFY(ops.redoText().isEmpty());

        ops.undo();
        QVERIFY(waitJob(ops));
        QVERIFY(!QFileInfo::exists(made));
        ops.undo();
        QVERIFY(waitJob(ops));
        QCOMPARE(names(m_dir), QStringList({"a.txt"}));
    }

    void redoReplace()
    {
        write(m_dir + "/src/a.txt", "new");
        write(m_dir + "/dst/a.txt", "old");
        FileOps ops;
        ops.transfer({m_dir + "/src/a.txt"}, m_dir + "/dst", true, {{"a.txt", "replace"}});
        QVERIFY(waitJob(ops));
        ops.undo();
        QVERIFY(waitJob(ops));
        QCOMPARE(read(m_dir + "/dst/a.txt"), "old");
        QCOMPARE(read(m_dir + "/src/a.txt"), "new");

        ops.redo();
        QVERIFY(waitJob(ops));
        QCOMPARE(names(m_dir + "/dst"), QStringList({"a.txt"}));
        QCOMPARE(read(m_dir + "/dst/a.txt"), "new");
        QVERIFY(!QFile::exists(m_dir + "/src/a.txt"));

        ops.undo();
        QVERIFY(waitJob(ops));
        QCOMPARE(names(m_dir + "/dst"), QStringList({"a.txt"}));
        QCOMPARE(read(m_dir + "/dst/a.txt"), "old");
        QCOMPARE(read(m_dir + "/src/a.txt"), "new");
    }

    void redoMergeMove()
    {
        write(m_dir + "/src/d/x.txt", "x");
        write(m_dir + "/src/d/y.txt", "new y");
        write(m_dir + "/dst/d/y.txt", "old y");
        FileOps ops;
        ops.transfer({m_dir + "/src/d"}, m_dir + "/dst", true, {{"d", "replace"}});
        QVERIFY(waitJob(ops));
        ops.undo();
        QVERIFY(waitJob(ops));
        ops.redo();
        QVERIFY(waitJob(ops));
        QCOMPARE(names(m_dir + "/dst/d"), QStringList({"x.txt", "y.txt"}));
        QCOMPARE(read(m_dir + "/dst/d/y.txt"), "new y");
        QVERIFY(!QFileInfo::exists(m_dir + "/src/d"));
        ops.undo();
        QVERIFY(waitJob(ops));
        QCOMPARE(names(m_dir + "/src/d"), QStringList({"x.txt", "y.txt"}));
        QCOMPARE(read(m_dir + "/dst/d/y.txt"), "old y");
    }

    // Новая операция после отмены — повторять больше нечего
    void newOperationDropsRedo()
    {
        write(m_dir + "/a.txt");
        FileOps ops;
        QCOMPARE(ops.rename(m_dir + "/a.txt", "b.txt"), QString());
        ops.undo();
        QVERIFY(waitJob(ops));
        QVERIFY(!ops.redoText().isEmpty());
        QVERIFY(!ops.makeDir(m_dir, "x").isEmpty());
        QVERIFY(ops.redoText().isEmpty());
    }

    // ── Архивы (bsdtar или 7z — что найдётся) ─────────────────────
    void archiveRoundTrip()
    {
        if (FileOps::archiver().isEmpty())
            QSKIP("нет ни bsdtar, ни 7z");
        write(m_dir + "/src/docs/a.txt", "a");
        write(m_dir + "/src/docs/sub/b.txt", "b");
        FileOps ops;
        QVERIFY(ops.canCompress("zip"));
        QVERIFY(ops.canExtract("application/zip"));
        QVERIFY(!ops.canExtract("text/plain"));
        QCOMPARE(FileOps::archiveBaseName("docs.tar.gz"), QStringLiteral("docs"));
        QCOMPARE(FileOps::archiveBaseName("docs.zip"), QStringLiteral("docs"));

        // Сжать — архив рядом, отмена убирает его
        ops.compress({m_dir + "/src/docs"}, "zip");
        QVERIFY(waitJob(ops));
        const QString zip = m_dir + "/src/docs.zip";
        QVERIFY(QFileInfo(zip).size() > 0);
        QCOMPARE(names(m_dir + "/src"), QStringList({"docs", "docs.zip"})); // временного файла не осталось
        QCOMPARE(ops.undoText(), QStringLiteral("создание архива «docs.zip»"));

        // В папку с именем архива
        QDir().mkpath(m_dir + "/dst");
        ops.extract(zip, m_dir + "/dst", true);
        QVERIFY(waitJob(ops));
        QCOMPARE(read(m_dir + "/dst/docs/docs/sub/b.txt"), "b");
        ops.undo();
        QVERIFY(waitJob(ops));
        QCOMPARE(names(m_dir + "/dst"), QStringList());
        QVERIFY(ops.redoText().isEmpty()); // распакованное не вернуть повтором

        // Сюда, с совпадением: сначала вопрос, потом замена; отмена возвращает старое
        write(m_dir + "/dst/docs/a.txt", "old");
        QSignalSpy got(&ops, &FileOps::extracted);
        ops.extract(zip, m_dir + "/dst", false);
        QVERIFY(waitJob(ops));
        QCOMPARE(got.count(), 1);
        const QString tmp = got.first().at(0).toString();
        const QVariantList cs = got.first().at(2).toList();
        QCOMPARE(cs.size(), 1);
        QCOMPARE(cs.first().toMap().value("name").toString(), QStringLiteral("docs"));
        ops.placeExtracted(tmp, m_dir + "/dst", {{"docs", "replace"}});
        QVERIFY(waitJob(ops));
        QVERIFY(!QFileInfo::exists(tmp));
        QCOMPARE(read(m_dir + "/dst/docs/a.txt"), "a");
        QCOMPARE(read(m_dir + "/dst/docs/sub/b.txt"), "b");
        QCOMPARE(ops.undoText(), QStringLiteral("распаковывание «docs.zip»"));
        ops.undo();
        QVERIFY(waitJob(ops));
        QCOMPARE(read(m_dir + "/dst/docs/a.txt"), "old");
        QVERIFY(!QFileInfo::exists(m_dir + "/dst/docs/sub"));

        // Передумали на вопросе — временной папки нет
        got.clear();
        ops.extract(zip, m_dir + "/dst", false);
        QVERIFY(waitJob(ops));
        QCOMPARE(got.count(), 1);
        ops.discardExtracted(got.first().at(0).toString());
        QCOMPARE(names(m_dir + "/dst"), QStringList({"docs"}));

        // Битый архив — ошибка, без мусора
        write(m_dir + "/bad.zip", "not a zip");
        QSignalSpy failed(&ops, &FileOps::failed);
        ops.extract(m_dir + "/bad.zip", m_dir + "/dst", true);
        QVERIFY(waitJob(ops));
        QCOMPARE(failed.count(), 1);
        QCOMPARE(names(m_dir + "/dst"), QStringList({"docs"}));
    }

    // Отмена посреди сжатия: ни архива, ни временного файла
    void archiveCancel()
    {
        if (FileOps::archiver().isEmpty())
            QSKIP("нет ни bsdtar, ни 7z");
        QByteArray big(64 * 1024 * 1024, Qt::Uninitialized);
        QRandomGenerator gen(42);
        gen.fillRange(reinterpret_cast<quint32 *>(big.data()), big.size() / 4);
        write(m_dir + "/big.bin", big);
        FileOps ops;
        ops.compress({m_dir + "/big.bin"}, "zip");
        QTest::qWait(150);
        QVERIFY(ops.busy());
        ops.cancel();
        QVERIFY(waitJob(ops));
        QCOMPARE(names(m_dir), QStringList({"big.bin"}));
        QVERIFY(ops.undoText().isEmpty());
    }

    void archiveTarFormats()
    {
        if (FileOps::archiver().isEmpty())
            QSKIP("нет ни bsdtar, ни 7z");
        write(m_dir + "/src/d/a.txt", "a");
        FileOps ops;
        // tar.gz — системным tar, как у большинства скачанных архивов
        if (!QStandardPaths::findExecutable("tar").isEmpty()) {
            QCOMPARE(QProcess::execute("tar", {"-czf", m_dir + "/d.tar.gz", "-C", m_dir + "/src", "d"}), 0);
            QVERIFY(QDir().mkpath(m_dir + "/out"));
            ops.extract(m_dir + "/d.tar.gz", m_dir + "/out", true);
            QVERIFY(waitJob(ops));
            QCOMPARE(read(m_dir + "/out/d/d/a.txt"), "a");
        }
        // .7z — создаём самим 7z, распаковываем тем, что выбрал Shelf
        const QString sevenZip = QStandardPaths::findExecutable("7z");
        if (!sevenZip.isEmpty()) {
            QProcess p;
            p.setWorkingDirectory(m_dir + "/src");
            p.start(sevenZip, {"a", "-bd", m_dir + "/d.7z", "d"});
            QVERIFY(p.waitForFinished(10000));
            QCOMPARE(p.exitCode(), 0);
            QVERIFY(QDir().mkpath(m_dir + "/seven"));
            ops.extract(m_dir + "/d.7z", m_dir + "/seven", true);
            QVERIFY(waitJob(ops));
            QCOMPARE(read(m_dir + "/seven/d/d/a.txt"), "a");
        }
        if (ops.canCompress("tar.zst")) {
            ops.compress({m_dir + "/src/d"}, "tar.zst");
            QVERIFY(waitJob(ops));
            QVERIFY(QFileInfo::exists(m_dir + "/src/d.tar.zst"));
            QVERIFY(QDir().mkpath(m_dir + "/zst"));
            ops.extract(m_dir + "/src/d.tar.zst", m_dir + "/zst", true);
            QVERIFY(waitJob(ops));
            QCOMPARE(read(m_dir + "/zst/d/d/a.txt"), "a");
        }
    }

    // ── Быстрый просмотр ──────────────────────────────────────────
    void previewText()
    {
        write(m_dir + "/a.txt", "привет");
        QVariantMap r = FileOps::readTextPreview(m_dir + "/a.txt");
        QCOMPARE(r.value("text").toString(), QStringLiteral("привет"));
        QCOMPARE(r.value("truncated").toBool(), false);
        QCOMPARE(r.value("binary").toBool(), false);

        // Обрезано посреди «и» (2 байта в UTF-8): половинка не превращается в «�»
        write(m_dir + "/long.txt", QByteArray("ab") + QString("и").toUtf8() + "cd");
        r = FileOps::readTextPreview(m_dir + "/long.txt", 3);
        QCOMPARE(r.value("text").toString(), QStringLiteral("ab"));
        QCOMPARE(r.value("truncated").toBool(), true);

        write(m_dir + "/bin", QByteArray("ELF\0\1\2", 6));
        r = FileOps::readTextPreview(m_dir + "/bin");
        QCOMPARE(r.value("binary").toBool(), true);
        QVERIFY(r.value("text").toString().isEmpty());
    }

    void previewKindAndRowInfo()
    {
        FileOps ops;
        QDir().mkpath(m_dir + "/sub");
        write(m_dir + "/a.py", "print()");
        QCOMPARE(ops.previewKind(m_dir + "/sub", "inode/directory"), "dir");
        QCOMPARE(ops.previewKind(m_dir + "/a.png", "image/png"), "image");
        QCOMPARE(ops.previewKind(m_dir + "/a.py", "text/x-python"), "text");
        QCOMPARE(ops.previewKind(m_dir + "/a.json", "application/json"), "text");
        QCOMPARE(ops.previewKind(m_dir + "/a.pdf", "application/pdf"), "other");

        QSignalSpy got(&ops, &FileOps::textPreview);
        ops.requestTextPreview(m_dir + "/a.py");
        QVERIFY(got.wait(5000));
        QCOMPARE(got.first().at(0).toString(), m_dir + "/a.py");
        QCOMPARE(got.first().at(1).toString(), QStringLiteral("print()"));

        DirModel model;
        model.setPath(m_dir);
        const QVariantMap info = model.rowInfo(model.indexOfPath(m_dir + "/a.py"));
        QCOMPARE(info.value("name").toString(), QStringLiteral("a.py"));
        QCOMPARE(info.value("isDir").toBool(), false);
        QVERIFY(!info.value("sizeText").toString().isEmpty());
        QVERIFY(model.rowInfo(-1).isEmpty());
    }

    // ── Язык ───────────────────────────────────────────────────────
    // Русские формы слов при числах — из ru.json, английский — из en.json,
    // и модель перечитывает свои строки сама, когда язык меняется
    void languageSwitch()
    {
        QDir().mkpath(m_dir + "/sub");
        write(m_dir + "/sub/a.txt");
        write(m_dir + "/sub/b.txt");
        write(m_dir + "/sub/c.txt");
        FileOps ops;
        DirModel model;
        model.setPath(m_dir);
        const QModelIndex sub = model.index(model.indexOfPath(m_dir + "/sub"));
        QCOMPARE(model.data(sub, DirModel::SizeTextRole).toString(), QStringLiteral("3 эл."));
        QCOMPARE(ops.rename(m_dir + "/sub", "a/b"), QStringLiteral("Недопустимое имя"));
        QCOMPARE(QCoreApplication::translate("FileOps", "%n объект(ов)", nullptr, 3), QStringLiteral("3 объекта"));
        QCOMPARE(QCoreApplication::translate("FileOps", "%n объект(ов)", nullptr, 11), QStringLiteral("11 объектов"));
        QCOMPARE(formatSize(10), QStringLiteral("10 Б"));

        QSignalSpy reset(&model, &QAbstractItemModel::modelReset);
        Language::apply(QStringLiteral("en"));
        QCOMPARE(Language::current(), QStringLiteral("en"));
        QCOMPARE(ops.rename(m_dir + "/sub", "a/b"), QStringLiteral("Invalid name"));
        QCOMPARE(QCoreApplication::translate("FileOps", "%n объект(ов)", nullptr, 1), QStringLiteral("1 item"));
        QCOMPARE(QCoreApplication::translate("FileOps", "%n объект(ов)", nullptr, 3), QStringLiteral("3 items"));
        QCOMPARE(formatSize(2560), QStringLiteral("2.5 KB"));
        QCOMPARE(model.data(sub, DirModel::SizeTextRole).toString(), QStringLiteral("3 items"));
        QCOMPARE(model.mimeCommentAt(model.indexOfPath(m_dir + "/sub")), QStringLiteral("Folder"));

        Language::apply(QStringLiteral("ru"));
        QCOMPARE(model.mimeCommentAt(model.indexOfPath(m_dir + "/sub")), QStringLiteral("Папка"));
    }

    // Подкраска иконок в C++ — формулы шейдера MultiEffect: premultiplied,
    // сначала яркость, потом смешение с серым, умноженным на цвет
    void glassTint()
    {
        QImage img(3, 1, QImage::Format_ARGB32_Premultiplied);
        img.setPixel(0, 0, qRgba(255, 255, 255, 255));
        img.setPixel(1, 0, qRgba(64, 64, 64, 128));
        img.setPixel(2, 0, qRgba(0, 0, 0, 0));
        QImage same = img;
        GlassIcons::tint(same, QColor(200, 100, 0), 0, 0);
        QCOMPARE(same, img);

        GlassIcons::tint(img, QColor(200, 100, 0), 0.5, 0.1);
        QCOMPARE(img.pixel(0, 0), qRgba(250, 195, 140, 255));
        QCOMPARE(img.pixel(1, 0), qRgba(69, 53, 38, 128));
        QCOMPARE(img.pixel(2, 0), qRgba(0, 0, 0, 0));
    }

    // Стеклянная эмблема: поля под тень, заливка плотнее сверху, тень снизу
    void glassEmblem()
    {
        QImage shape(10, 10, QImage::Format_ARGB32_Premultiplied);
        shape.fill(Qt::white);
        const QImage g = GlassIcons::glass(shape, 1.0, 2.0);
        QCOMPARE(g.size(), QSize(10 + 2 * GlassIcons::glassPad, 10 + 2 * GlassIcons::glassPad));
        const int p = GlassIcons::glassPad;
        QVERIFY(qAlpha(g.pixel(p + 5, p)) > qAlpha(g.pixel(p + 5, p + 9)));
        QCOMPARE(qRed(g.pixel(p + 5, p)), qAlpha(g.pixel(p + 5, p))); // белое стекло
        QCOMPARE(qAlpha(g.pixel(0, 0)), 0);
        QVERIFY(qAlpha(g.pixel(p + 5, p + 11)) > 0); // тень под эмблемой
        QCOMPARE(qRed(g.pixel(p + 5, p + 11)), 0);
    }
};

QTEST_MAIN(TestCore)
#include "tst_core.moc"
