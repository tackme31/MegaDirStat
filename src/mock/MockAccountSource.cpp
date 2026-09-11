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
    return (mOptions.requireLogin || mOptions.twoFactor) && !mLoggedIn;
}

void MockAccountSource::login(const QString& /*email*/,
                              const QString& password,
                              const QString& twoFactorCode)
{
    // Long enough for the "Signing you in…" page to be seen and screenshotted.
    QTimer::singleShot(400, this, [this, password, twoFactorCode] {
        finishLogin(password, twoFactorCode);
    });
}

void MockAccountSource::finishLogin(const QString& password, const QString& twoFactorCode)
{
    if (password == QLatin1String(kMockWrongPassword))
    {
        emit loginFinished(LoginResult::Failed, tr("Incorrect email or password."));
        return;
    }
    if (mOptions.twoFactor && twoFactorCode.isEmpty())
    {
        emit loginFinished(LoginResult::NeedsTwoFactor, {});
        return;
    }
    if (mOptions.twoFactor && twoFactorCode != QLatin1String(kMockTwoFactorCode))
    {
        emit loginFinished(LoginResult::Failed, tr("The code you entered is incorrect."));
        return;
    }
    mLoggedIn = true;
    emit loginFinished(LoginResult::Ok, {});
}

void MockAccountSource::load()
{
    mElapsed.start();
    emit progress(tr("Loading mock data…"), {}, 0, mOptions.delayMs > 0 ? mOptions.delayMs : -1);
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
    mLoggedIn = false;
}

void MockAccountSource::tick()
{
    const qint64 elapsed = mElapsed.elapsed();
    if (elapsed < mOptions.delayMs)
    {
        const QString detail = tr("%1 of %2 s")
                                   .arg(elapsed / 1000.0, 0, 'f', 1)
                                   .arg(mOptions.delayMs / 1000.0, 0, 'f', 1);
        emit progress(tr("Loading mock data…"), detail, elapsed, mOptions.delayMs);
        return;
    }
    mTimer->stop();
    build();
}

void MockAccountSource::build()
{
    if (mOptions.fail)
    {
        // Mirrors MegaAccountSource, which signs out when a first load fails.
        mLoggedIn = false;
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
