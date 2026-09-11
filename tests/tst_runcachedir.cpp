#include "core/RunCacheDir.h"

#include <QDir>
#include <QFile>
#include <QLockFile>
#include <QProcess>
#include <QTemporaryDir>
#include <QTest>

#include <cstdlib>

namespace
{

void touch(const QString& path)
{
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("x");
}

} // namespace

const char kCrashEnv[] = "TST_RUNCACHEDIR_CRASH_HOLDING";

class TestRunCacheDir : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        // Child mode for locksOfCrashedRunsAreRemoved: take the lock and die
        // without running QLockFile's destructor, exactly like a crashed run.
        const QByteArray lockPath = qgetenv(kCrashEnv);
        if (!lockPath.isEmpty())
        {
            QLockFile lock(QString::fromLocal8Bit(lockPath));
            std::_Exit(lock.tryLock(0) ? 0 : 2);
        }
    }

    void createsAndRemovesItsDirectory()
    {
        QTemporaryDir base;
        QString path;
        {
            QString error;
            const auto dir = RunCacheDir::create(base.filePath(QStringLiteral("sdk-cache")), &error);
            QVERIFY2(dir, qPrintable(error));
            path = dir->path();
            QVERIFY(QFileInfo(path).isDir());
            QVERIFY(QFile::exists(path + QStringLiteral(".lock")));
            touch(QDir(path).filePath(QStringLiteral("megaclient_statecache.db")));
        }
        QVERIFY(!QFileInfo::exists(path));
        QVERIFY(!QFile::exists(path + QStringLiteral(".lock")));
    }

    void locksOfCrashedRunsAreRemoved()
    {
        QTemporaryDir base;
        const QDir dir(base.path());
        const QString id = QStringLiteral("0123456789abcdef0123456789abcdef");
        QVERIFY(dir.mkdir(id));
        touch(dir.filePath(id + QStringLiteral("/megaclient_statecache.db")));
        const QString lockPath = dir.filePath(id + QStringLiteral(".lock"));

        QProcess child;
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert(QString::fromLatin1(kCrashEnv), lockPath);
        child.setProcessEnvironment(env);
        // Any single test function: initTestCase runs first and exits.
        child.start(QCoreApplication::applicationFilePath(),
                    {QStringLiteral("ignoresNamesThatAreNotRunIds")});
        QVERIFY(child.waitForFinished(30000));
        QCOMPARE(child.exitCode(), 0);
        QVERIFY(QFile::exists(lockPath));

        QCOMPARE(RunCacheDir::removeStale(base.path()), 1);
        QVERIFY(dir.entryList(QDir::AllEntries | QDir::NoDotAndDotDot).isEmpty());
    }

    void directoriesWithoutALockAreRemoved()
    {
        QTemporaryDir base;
        const QDir dir(base.path());
        const QString id = QStringLiteral("0123456789abcdef0123456789abcdef");
        QVERIFY(dir.mkdir(id));
        touch(dir.filePath(id + QStringLiteral("/megaclient_statecache.db")));

        QCOMPARE(RunCacheDir::removeStale(base.path()), 1);
        QVERIFY(dir.entryList(QDir::AllEntries | QDir::NoDotAndDotDot).isEmpty());
    }

    void keepsDirectoriesOfLiveRuns()
    {
        QTemporaryDir base;
        const auto live = RunCacheDir::create(base.path());
        QVERIFY(live);
        const auto other = RunCacheDir::create(base.path()); // runs removeStale too
        QVERIFY(other);
        QVERIFY(live->path() != other->path());
        QVERIFY(QFileInfo(live->path()).isDir());
        QCOMPARE(RunCacheDir::removeStale(base.path()), 0);
        QVERIFY(QFileInfo(live->path()).isDir());
    }

    void ignoresNamesThatAreNotRunIds()
    {
        QTemporaryDir base;
        const QDir dir(base.path());
        QVERIFY(dir.mkdir(QStringLiteral("keep-me")));
        touch(dir.filePath(QStringLiteral("notes.lock")));
        QCOMPARE(RunCacheDir::removeStale(base.path()), 0);
        QVERIFY(dir.exists(QStringLiteral("keep-me")));
        QVERIFY(dir.exists(QStringLiteral("notes.lock")));
    }
};

QTEST_GUILESS_MAIN(TestRunCacheDir)
#include "tst_runcachedir.moc"
