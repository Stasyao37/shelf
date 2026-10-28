// Модель папки и операции с файлами — без QML, на временных папках.
// Корзина и настройки — тоже во временной папке: HOME и XDG_* подменяются до первого обращения
#include "DirModel.h"
#include "FileOps.h"

#include <QAbstractItemModelTester>
#include <QDir>
#include <QFile>
#include <QSignalSpy>
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
};

QTEST_MAIN(TestCore)
#include "tst_core.moc"
