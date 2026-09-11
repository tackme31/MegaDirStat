#include "core/RunCacheDir.h"

#include <QDir>
#include <QLockFile>
#include <QRegularExpression>
#include <QSet>
#include <QUuid>

namespace
{

const QString kLockSuffix = QStringLiteral(".lock");

bool isRunId(const QString& name)
{
    static const QRegularExpression pattern(QStringLiteral("^[0-9a-f]{32}$"));
    return pattern.match(name).hasMatch();
}

std::unique_ptr<QLockFile> makeLock(const QString& baseDir, const QString& id)
{
    auto lock = std::make_unique<QLockFile>(QDir(baseDir).filePath(id + kLockSuffix));
    // 0 = judge staleness only by whether the owning process is still running.
    // The 30 s default would let a second instance take over the lock of a run
    // that has simply been open for a while (on Unix, where it can delete it).
    lock->setStaleLockTime(0);
    return lock;
}

} // namespace

RunCacheDir::RunCacheDir(QString path, std::unique_ptr<QLockFile> lock)
    : mPath(std::move(path)), mLock(std::move(lock))
{}

RunCacheDir::~RunCacheDir()
{
    // Directory first, lock last: an orphaned directory without its lock would
    // look abandoned to other instances, which is fine, but never the reverse.
    QDir(mPath).removeRecursively();
    mLock->unlock();
}

int RunCacheDir::removeStale(const QString& baseDir)
{
    const QDir base(baseDir);
    QSet<QString> ids;
    for (const QString& name : base.entryList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot))
    {
        const QString id = name.endsWith(kLockSuffix) ? name.chopped(kLockSuffix.size()) : name;
        if (isRunId(id))
        {
            ids.insert(id);
        }
    }

    int removed = 0;
    for (const QString& id : std::as_const(ids))
    {
        const auto lock = makeLock(baseDir, id);
        if (!lock->tryLock(0))
        {
            continue; // owned by a running instance
        }
        QDir(base.filePath(id)).removeRecursively();
        lock->unlock();
        ++removed;
    }
    return removed;
}

std::unique_ptr<RunCacheDir> RunCacheDir::create(const QString& baseDir, QString* error)
{
    auto fail = [error](const QString& message) {
        if (error)
        {
            *error = message;
        }
        return nullptr;
    };

    if (!QDir().mkpath(baseDir))
    {
        return fail(QStringLiteral("cannot create %1").arg(QDir::toNativeSeparators(baseDir)));
    }
    removeStale(baseDir);

    const QString id = QUuid::createUuid().toString(QUuid::Id128);
    auto lock = makeLock(baseDir, id);
    // Lock before the directory exists, so no other instance can ever see the
    // directory without its lock held.
    if (!lock->tryLock(0))
    {
        return fail(QStringLiteral("cannot lock %1").arg(QDir::toNativeSeparators(lock->fileName())));
    }
    const QString path = QDir(baseDir).filePath(id);
    if (!QDir().mkdir(path, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner))
    {
        lock->unlock();
        return fail(QStringLiteral("cannot create %1").arg(QDir::toNativeSeparators(path)));
    }
    return std::unique_ptr<RunCacheDir>(new RunCacheDir(path, std::move(lock)));
}
