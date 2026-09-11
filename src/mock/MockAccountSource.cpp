#include "mock/MockAccountSource.h"

#include "mock/FixtureLoader.h"
#include "mock/TreeGenerator.h"

#include <QFutureWatcher>
#include <QTimer>
#include <QtConcurrent/QtConcurrentRun>

namespace
{

struct BuildResult
{
    SnapshotPtr snapshot;
    QString error;
};

BuildResult buildSnapshot(const MockOptions& options)
{
    BuildResult result;
    if (!options.fixturePath.isEmpty())
    {
        result.snapshot = loadFixtureFile(options.fixturePath, &result.error);
    }
    else
    {
        result.snapshot = generateSnapshot({options.generateCount, options.seed});
    }
    return result;
}

} // namespace

MockAccountSource::MockAccountSource(MockOptions options, QObject* parent)
    : IAccountSource(parent), mOptions(std::move(options)), mTimer(new QTimer(this))
{
    mTimer->setInterval(100);
    connect(mTimer, &QTimer::timeout, this, &MockAccountSource::tick);
}

bool MockAccountSource::requiresLogin() const
{
    return false;
}

void MockAccountSource::login(const QString&, const QString&)
{
    emit loginFinished(true, {});
}

void MockAccountSource::load()
{
    mElapsed.start();
    emit progress(tr("Loading mock data"), 0, mOptions.delayMs > 0 ? mOptions.delayMs : -1);
    if (mOptions.delayMs > 0)
    {
        mTimer->start();
    }
    else
    {
        build();
    }
}

void MockAccountSource::logout()
{
    mTimer->stop();
}

void MockAccountSource::tick()
{
    const qint64 elapsed = mElapsed.elapsed();
    if (elapsed < mOptions.delayMs)
    {
        emit progress(tr("Loading mock data"), elapsed, mOptions.delayMs);
        return;
    }
    mTimer->stop();
    build();
}

void MockAccountSource::build()
{
    if (mOptions.fail)
    {
        emit failed(tr("Simulated failure (--mock-fail)"));
        return;
    }

    auto* watcher = new QFutureWatcher<BuildResult>(this);
    connect(watcher, &QFutureWatcher<BuildResult>::finished, this, [this, watcher] {
        const BuildResult result = watcher->result();
        watcher->deleteLater();
        if (result.snapshot)
        {
            emit loaded(result.snapshot);
        }
        else
        {
            emit failed(result.error);
        }
    });
    watcher->setFuture(QtConcurrent::run(buildSnapshot, mOptions));
}
