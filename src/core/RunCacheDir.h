#pragma once

#include <QString>

#include <memory>

class QLockFile;

// A scratch directory for one run of the app, <base>/<id>/, guarded by the lock
// file <base>/<id>.lock and deleted again by the destructor. Several instances
// can share <base>; each one removes what crashed runs left behind, but never a
// directory whose owner is still alive (docs/DESIGN.md §5).
class RunCacheDir
{
public:
    // Removes stale leftovers under baseDir, then creates a fresh directory.
    // Returns nullptr (and sets error) if it cannot be created.
    static std::unique_ptr<RunCacheDir> create(const QString& baseDir, QString* error = nullptr);

    // Deletes every <id>/ and <id>.lock under baseDir whose lock can be taken.
    // Only names shaped like our ids are touched. Returns how many ids were removed.
    static int removeStale(const QString& baseDir);

    ~RunCacheDir();
    RunCacheDir(const RunCacheDir&) = delete;
    RunCacheDir& operator=(const RunCacheDir&) = delete;

    QString path() const
    {
        return mPath;
    }

private:
    RunCacheDir(QString path, std::unique_ptr<QLockFile> lock);

    QString mPath;
    std::unique_ptr<QLockFile> mLock;
};
