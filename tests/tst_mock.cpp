#include "mock/FixtureLoader.h"
#include "mock/MockAccountSource.h"
#include "mock/TreeGenerator.h"

#include <QSignalSpy>
#include <QTest>

#include <functional>

namespace
{

const SizeNode* child(const SizeNode* parent, const QString& name)
{
    for (const auto& c : parent->children)
    {
        if (c->name == name)
        {
            return c.get();
        }
    }
    return nullptr;
}

qint64 countFiles(const SizeNode& node)
{
    if (!node.isFolder())
    {
        return 1;
    }
    qint64 n = 0;
    for (const auto& c : node.children)
    {
        n += countFiles(*c);
    }
    return n;
}

QString fingerprint(const SizeNode& node)
{
    QString out = node.name + QLatin1Char(':') + QString::number(node.size) + QLatin1Char('(');
    for (const auto& c : node.children)
    {
        out += fingerprint(*c);
    }
    return out + QLatin1Char(')');
}

} // namespace

class TestMock : public QObject
{
    Q_OBJECT

private slots:
    void loadsSampleFixture()
    {
        QString error;
        const auto snapshot =
            loadFixtureFile(QStringLiteral(MEGADIRSTAT_FIXTURE_DIR "/sample.json"), &error);
        QVERIFY2(snapshot, qPrintable(error));

        const SizeNode* root = snapshot->root.get();
        QCOMPARE(root->children.size(), std::size_t(2));
        const SizeNode* drive = child(root, QStringLiteral("Cloud Drive"));
        const SizeNode* rubbish = child(root, QStringLiteral("Rubbish Bin"));
        QVERIFY(drive && rubbish);
        QCOMPARE(rubbish->size, 65150000);
        QCOMPARE(root->size, drive->size + rubbish->size);
        QCOMPARE(root->fileCount, countFiles(*root));

        const SizeNode* photos = child(drive, QStringLiteral("Photos"));
        QVERIFY(photos && photos->isFolder());
        QCOMPARE(photos->size, 4200000 + 3900000 + 2100000 + 180000000 + 5100000 + 29000000);

        const SizeNode* empty = child(child(drive, QStringLiteral("Documents")),
                                      QStringLiteral("Empty folder"));
        QVERIFY(empty && empty->isFolder());
        QCOMPARE(empty->size, 0);

        QCOMPARE(snapshot->usage.used, 3500000000);
        QCOMPARE(snapshot->usage.total, 21474836480);
    }

    void rejectsMalformedFixtures_data()
    {
        QTest::addColumn<QByteArray>("json");
        QTest::addColumn<QString>("errorFragment");
        QTest::newRow("not json") << QByteArray("{") << "invalid JSON";
        QTest::newRow("no roots") << QByteArray("{}") << "\"roots\"";
        QTest::newRow("root is a file")
            << QByteArray(R"({"roots":[{"name":"x","size":1}]})") << "folder";
        QTest::newRow("nameless")
            << QByteArray(R"({"roots":[{"name":"d","children":[{"size":1}]}]})") << "\"name\"";
        QTest::newRow("negative size")
            << QByteArray(R"({"roots":[{"name":"d","children":[{"name":"f","size":-1}]}]})")
            << "roots/d/f";
        QTest::newRow("bad date")
            << QByteArray(
                   R"({"roots":[{"name":"d","children":[{"name":"f","size":1,"modified":"x"}]}]})")
            << "ISO 8601";
    }

    void rejectsMalformedFixtures()
    {
        QFETCH(QByteArray, json);
        QFETCH(QString, errorFragment);
        QString error;
        QVERIFY(!parseFixture(json, &error));
        QVERIFY2(error.contains(errorFragment), qPrintable(error));
    }

    void usageDefaultsToTreeSize()
    {
        const auto snapshot =
            parseFixture(R"({"roots":[{"name":"d","children":[{"name":"f","size":7}]}]})", nullptr);
        QVERIFY(snapshot);
        QCOMPARE(snapshot->usage.used, 7);
        QCOMPARE(snapshot->usage.total, -1);
    }

    void generatorIsDeterministic()
    {
        const auto a = generateSnapshot({3000, 42});
        const auto b = generateSnapshot({3000, 42});
        const auto c = generateSnapshot({3000, 43});
        QCOMPARE(fingerprint(*a->root), fingerprint(*b->root));
        QVERIFY(fingerprint(*a->root) != fingerprint(*c->root));
    }

    void generatorProducesRequestedShape()
    {
        const auto snapshot = generateSnapshot({5000, 7});
        const SizeNode* root = snapshot->root.get();
        QCOMPARE(root->fileCount, 5000);
        QCOMPARE(countFiles(*root) - 0, 5000);
        QVERIFY(child(root, QStringLiteral("Cloud Drive")));
        QVERIFY(child(root, QStringLiteral("Rubbish Bin")));
        QVERIFY(snapshot->usage.total >= snapshot->usage.used);
        QCOMPARE(snapshot->usage.used, root->size);
    }

    void sourceEmitsLoaded()
    {
        MockOptions options;
        options.generateCount = 500;
        MockAccountSource source(options);
        QVERIFY(!source.requiresLogin());
        QSignalSpy loaded(&source, &IAccountSource::loaded);
        QSignalSpy failed(&source, &IAccountSource::failed);
        source.load();
        QVERIFY(loaded.wait(5000));
        QCOMPARE(failed.count(), 0);
        const auto snapshot = loaded.takeFirst().at(0).value<SnapshotPtr>();
        QVERIFY(snapshot);
        QCOMPARE(snapshot->root->fileCount, 500);
    }

    void sourceSimulatesDelayAndFailure()
    {
        MockOptions options;
        options.generateCount = 10;
        options.delayMs = 300;
        options.fail = true;
        MockAccountSource source(options);
        QSignalSpy progress(&source, &IAccountSource::progress);
        QSignalSpy failed(&source, &IAccountSource::failed);
        source.load();
        QVERIFY(failed.wait(5000));
        QVERIFY(progress.count() >= 2);
    }

    void sourceSimulatesLogin_data()
    {
        QTest::addColumn<bool>("twoFactor");
        QTest::addColumn<QString>("password");
        QTest::addColumn<QString>("code");
        QTest::addColumn<IAccountSource::LoginResult>("expected");
        using R = IAccountSource::LoginResult;
        const QString good = QString::fromLatin1(MockAccountSource::kMockTwoFactorCode);
        QTest::newRow("plain ok") << false << "pw" << QString() << R::Ok;
        QTest::newRow("wrong password") << false << "wrong" << QString() << R::Failed;
        QTest::newRow("asks for code") << true << "pw" << QString() << R::NeedsTwoFactor;
        QTest::newRow("wrong code") << true << "pw" << "000000" << R::Failed;
        QTest::newRow("right code") << true << "pw" << good << R::Ok;
        QTest::newRow("wrong password beats code") << true << "wrong" << good << R::Failed;
    }

    void sourceSimulatesLogin()
    {
        QFETCH(bool, twoFactor);
        QFETCH(QString, password);
        QFETCH(QString, code);
        QFETCH(IAccountSource::LoginResult, expected);

        MockOptions options;
        options.generateCount = 10;
        options.requireLogin = true;
        options.twoFactor = twoFactor;
        MockAccountSource source(options);
        QVERIFY(source.requiresLogin());

        QSignalSpy finished(&source, &IAccountSource::loginFinished);
        source.login(QStringLiteral("a@example.com"), password, code);
        QVERIFY(finished.wait(5000));
        const auto result = finished.first().at(0).value<IAccountSource::LoginResult>();
        QCOMPARE(result, expected);
        QCOMPARE(finished.first().at(1).toString().isEmpty(),
                 result != IAccountSource::LoginResult::Failed);
        QCOMPARE(source.requiresLogin(), expected != IAccountSource::LoginResult::Ok);
    }

    void failedFirstLoadSignsOut()
    {
        MockOptions options;
        options.generateCount = 10;
        options.requireLogin = true;
        options.fail = true;
        MockAccountSource source(options);
        QSignalSpy finished(&source, &IAccountSource::loginFinished);
        source.login(QStringLiteral("a@example.com"), QStringLiteral("pw"), {});
        QVERIFY(finished.wait(5000));
        QVERIFY(!source.requiresLogin());

        QSignalSpy failed(&source, &IAccountSource::failed);
        source.load(); // without --mock-delay the failure is emitted from inside load()
        QTRY_COMPARE(failed.count(), 1);
        QVERIFY(source.requiresLogin());
    }

    void sourceReportsMissingFixture()
    {
        MockOptions options;
        options.fixturePath = QStringLiteral("does/not/exist.json");
        MockAccountSource source(options);
        QSignalSpy failed(&source, &IAccountSource::failed);
        source.load();
        QVERIFY(failed.wait(5000));
        QVERIFY(failed.takeFirst().at(0).toString().contains(QStringLiteral("cannot open")));
    }
};

QTEST_GUILESS_MAIN(TestMock)
#include "tst_mock.moc"
