#include "mega/MegaAccountSource.h"

#include "core/RunCacheDir.h"
#include "mega/SdkListeners.h"

#include <QDir>
#include <QElapsedTimer>
#include <QLocale>
#include <QLoggingCategory>
#include <QMetaObject>
#include <QTimer>
#include <QtConcurrent/QtConcurrentRun>

#include <megaapi.h>

#include <algorithm>
#include <functional>

Q_LOGGING_CATEGORY(lcMega, "megadirstat.mega")

namespace
{

// How long the fetchNodes byte progress must stay quiet before the download is
// called done and the page switches to "Decrypting". MegaExplorer measured ~1.15 s
// between updates, and the SDK sends none after the response is complete, so a
// long quiet spell means the download finished even when the last update fell
// short of the total.
constexpr int kStallTimeoutMs = 8000;

// Logging out tells the server to end the session. Bounded so that closing the
// app while offline does not hang; the session then simply expires server-side.
constexpr int kLogoutTimeoutMs = 5000;

constexpr qint64 kWalkReportIntervalMs = 100;

QString formatBytes(qint64 bytes)
{
    return QLocale().formattedDataSize(bytes);
}

QString loginErrorText(int code, bool withCode, const QString& sdkMessage)
{
    using E = mega::MegaError;
    switch (code)
    {
        case E::API_ENOENT:
            return withCode ? MegaAccountSource::tr("The code you entered is incorrect.")
                            : MegaAccountSource::tr("Incorrect email or password.");
        // megaapi.h names no code for a wrong PIN; these are the ones seen for it.
        case E::API_EFAILED:
        case E::API_EEXPIRED:
            if (withCode)
            {
                return MegaAccountSource::tr("The code you entered is incorrect.");
            }
            break;
        case E::API_EBLOCKED:
            return MegaAccountSource::tr("This account has been suspended.");
        case E::API_ETOOMANY:
            return MegaAccountSource::tr("Too many attempts. Please try again later.");
        case E::API_EINCOMPLETE:
            return MegaAccountSource::tr(
                "This account has not been confirmed yet. Check your email for the "
                "confirmation link.");
        case E::API_EAGAIN:
        case E::API_ERATELIMIT:
            return MegaAccountSource::tr("Couldn't connect. Please check your connection.");
        default:
            break;
    }
    return MegaAccountSource::tr("Couldn't sign in (%1).").arg(sdkMessage);
}

struct Walk
{
    mega::MegaApi& api;
    const std::atomic<bool>& cancel;
    std::function<void(qint64 done)> report;
    qint64 done = 0;
    QElapsedTimer sinceReport;
};

// Recursion depth is the folder nesting depth, which stays far below what the
// stack can take.
void walkChildren(Walk& walk, mega::MegaNode& parent, SizeNode& out)
{
    // ORDER_NONE: finalizeTree sorts by size anyway.
    const std::unique_ptr<mega::MegaNodeList> children(
        walk.api.getChildren(&parent, mega::MegaApi::ORDER_NONE));
    if (!children)
    {
        return;
    }
    for (int i = 0; i < children->size() && !walk.cancel; ++i)
    {
        mega::MegaNode* child = children->get(i); // owned by the list
        const char* rawName = child->getName();
        const QString name = QString::fromUtf8(rawName ? rawName : "");
        if (child->isFile())
        {
            out.addFile(name, child->getSize(), child->getModificationTime());
        }
        else if (child->isFolder())
        {
            walkChildren(walk, *child, *out.addFolder(name));
        }
        ++walk.done;
        if (walk.sinceReport.hasExpired(kWalkReportIntervalMs))
        {
            walk.report(walk.done);
            walk.sinceReport.restart();
        }
    }
}

} // namespace

template <typename F>
void MegaAccountSource::post(F&& f)
{
    QMetaObject::invokeMethod(this, std::forward<F>(f), Qt::QueuedConnection);
}

MegaAccountSource::MegaAccountSource(std::unique_ptr<RunCacheDir> cacheDir,
                                     const QString& userAgent,
                                     QObject* parent)
    : IAccountSource(parent), mCacheDir(std::move(cacheDir)), mStallTimer(new QTimer(this))
{
    // Native separators: the SDK appends its own file names with backslashes on
    // Windows, and a mixed path breaks once something prefixes it with \\?\.
    const QByteArray basePath = QDir::toNativeSeparators(mCacheDir->path()).toUtf8();
    mApi = std::make_unique<mega::MegaApi>(nullptr, basePath.constData(), userAgent.toUtf8().constData());

    mStallTimer->setSingleShot(true);
    mStallTimer->setInterval(kStallTimeoutMs);
    connect(mStallTimer, &QTimer::timeout, this, [this] {
        if (mPhase == Phase::Fetching)
        {
            enterDecrypting();
        }
    });
}

MegaAccountSource::~MegaAccountSource()
{
    logout();
}

bool MegaAccountSource::requiresLogin() const
{
    return !mLoggedIn;
}

void MegaAccountSource::login(const QString& email,
                              const QString& password,
                              const QString& twoFactorCode)
{
    if (!mApi)
    {
        return;
    }
    const bool withCode = !twoFactorCode.isEmpty();
    auto* listener = new megasdk::RequestListener([this, withCode](mega::MegaRequest&,
                                                                   mega::MegaError& error) {
        const int code = error.getErrorCode();
        const QString message = QString::fromUtf8(error.getErrorString());
        post([this, code, withCode, message] {
            if (code == mega::MegaError::API_OK)
            {
                mLoggedIn = true;
                mNodesFetched = false;
                emit loginFinished(LoginResult::Ok, {});
            }
            else if (code == mega::MegaError::API_EMFAREQUIRED)
            {
                emit loginFinished(LoginResult::NeedsTwoFactor, {});
            }
            else
            {
                // Never the email: nothing identifying goes to the log.
                qCWarning(lcMega) << "login failed:" << code << message;
                emit loginFinished(LoginResult::Failed, loginErrorText(code, withCode, message));
            }
        });
    });

    const QByteArray emailUtf8 = email.trimmed().toUtf8();
    const QByteArray passwordUtf8 = password.toUtf8();
    if (withCode)
    {
        mApi->multiFactorAuthLogin(emailUtf8.constData(),
                                   passwordUtf8.constData(),
                                   twoFactorCode.toUtf8().constData(),
                                   listener);
    }
    else
    {
        mApi->login(emailUtf8.constData(), passwordUtf8.constData(), listener);
    }
}

void MegaAccountSource::load()
{
    if (!mApi || !mLoggedIn)
    {
        emit failed(tr("Not signed in."));
        return;
    }
    if (mPhase != Phase::Idle)
    {
        return; // the load in flight will answer
    }

    const quint64 generation = ++mGeneration;
    mPhase = Phase::Fetching;
    mDecrypting = false;
    auto onFinish = [this, generation](mega::MegaRequest&, mega::MegaError& error) {
        const int code = error.getErrorCode();
        const QString message = QString::fromUtf8(error.getErrorString());
        post([this, generation, code, message] { onFetchFinished(generation, code, message); });
    };

    if (mNodesFetched)
    {
        // The SDK keeps its tree current from action packets after fetchNodes;
        // catchup waits until everything the server had sent so far is applied.
        emit progress(tr("Checking for changes…"), {}, 0, -1);
        mApi->catchup(new megasdk::RequestListener(onFinish));
        return;
    }

    emit progress(tr("Requesting your file list…"), {}, 0, -1);
    auto onUpdate = [this, generation](mega::MegaRequest& request) {
        // Both can still be at their -1 default before the response length is known.
        const qint64 transferred = request.getTransferredBytes();
        const qint64 total = request.getTotalBytes();
        post([this, generation, transferred, total] {
            onFetchProgress(generation, transferred, total);
        });
    };
    mApi->fetchNodes(new megasdk::RequestListener(onFinish, onUpdate));
}

void MegaAccountSource::onFetchProgress(quint64 generation, qint64 transferred, qint64 total)
{
    if (generation != mGeneration || mPhase != Phase::Fetching || total <= 0)
    {
        return;
    }
    if (transferred >= total)
    {
        enterDecrypting();
        return;
    }
    // Not monotonic: the request can be retried, which starts the bytes over.
    mDecrypting = false;
    emit progress(tr("Downloading your file list…"),
                  tr("%1 of %2").arg(formatBytes(std::max<qint64>(transferred, 0)), formatBytes(total)),
                  std::max<qint64>(transferred, 0),
                  total);
    mStallTimer->start();
}

void MegaAccountSource::enterDecrypting()
{
    mStallTimer->stop();
    if (mDecrypting)
    {
        return;
    }
    mDecrypting = true;
    // The SDK reports nothing while it decrypts and builds its tree, so no bar.
    emit progress(tr("Decrypting your file list…"),
                  tr("This can take a few minutes on a large account."),
                  0,
                  -1);
}

void MegaAccountSource::onFetchFinished(quint64 generation, int errorCode, const QString& sdkMessage)
{
    if (generation != mGeneration)
    {
        return;
    }
    mStallTimer->stop();
    if (errorCode != mega::MegaError::API_OK)
    {
        mPhase = Phase::Idle;
        qCWarning(lcMega) << "fetching the file list failed:" << errorCode << sdkMessage;
        const QString error = tr("Couldn't load your file list (%1).").arg(sdkMessage);
        if (mNodesFetched)
        {
            emit failed(error);
        }
        else
        {
            failFirstLoad(error);
        }
        return;
    }
    mNodesFetched = true;
    startWalk(generation);
}

void MegaAccountSource::failFirstLoad(const QString& error)
{
    // Without a file list the session is of no use, and IAccountSource promises
    // the UI a signed-out source to retry the login on.
    mApi->logout(false, new megasdk::RequestListener([this, error](mega::MegaRequest&, mega::MegaError&) {
        post([this, error] {
            mLoggedIn = false;
            emit failed(error);
        });
    }));
}

void MegaAccountSource::startWalk(quint64 generation)
{
    mPhase = Phase::Walking;
    mCancelWalk = false;
    mWalkResult.reset();
    mWalkDone = false;
    mPendingUsage.reset();
    emit progress(tr("Measuring folders…"), {}, 0, -1);

    mApi->getAccountDetails(new megasdk::RequestListener([this, generation](mega::MegaRequest& request,
                                                                            mega::MegaError& error) {
        AccountUsage usage;
        if (error.getErrorCode() == mega::MegaError::API_OK)
        {
            // getMegaAccountDetails() hands over ownership (megaapi.h).
            const std::unique_ptr<mega::MegaAccountDetails> details(request.getMegaAccountDetails());
            if (details)
            {
                usage.used = details->getStorageUsed();
                usage.total = details->getStorageMax();
            }
        }
        post([this, generation, usage] {
            if (generation == mGeneration)
            {
                mPendingUsage = usage;
                finishIfComplete();
            }
        });
    }));

    mega::MegaApi* api = mApi.get();
    // Counts versions, the Vault and shares as well, so the walk ends short of it.
    const qint64 total = static_cast<qint64>(api->getNumNodes());
    const QString driveName = tr("Cloud Drive");
    const QString rubbishName = tr("Rubbish Bin");
    mWalk = QtConcurrent::run([this, api, generation, total, driveName, rubbishName] {
        Walk walk{*api, mCancelWalk, [this, generation, total](qint64 done) {
                      post([this, generation, done, total] {
                          onWalkProgress(generation, done, total);
                      });
                  }};
        walk.sinceReport.start();

        auto root = std::make_unique<SizeNode>();
        root->kind = NodeKind::Folder;
        const std::unique_ptr<mega::MegaNode> drive(api->getRootNode());
        if (drive)
        {
            walkChildren(walk, *drive, *root->addFolder(driveName));
        }
        const std::unique_ptr<mega::MegaNode> rubbish(api->getRubbishNode());
        if (rubbish)
        {
            walkChildren(walk, *rubbish, *root->addFolder(rubbishName));
        }
        if (mCancelWalk)
        {
            return;
        }
        finalizeTree(*root);
        // Handed over through the member: the GUI thread reads it only after the
        // event posted below, and stopWalk() waits for this job before anything
        // else could touch it.
        mWalkResult = std::move(root);
        post([this, generation] {
            if (generation == mGeneration)
            {
                mWalkDone = true;
                finishIfComplete();
            }
        });
    });
}

void MegaAccountSource::onWalkProgress(quint64 generation, qint64 done, qint64 total)
{
    if (generation != mGeneration || mPhase != Phase::Walking)
    {
        return;
    }
    emit progress(tr("Measuring folders…"),
                  tr("%L1 items").arg(done),
                  std::min(done, total),
                  total > 0 ? total : -1);
}

void MegaAccountSource::finishIfComplete()
{
    if (mPhase != Phase::Walking || !mWalkDone || !mPendingUsage)
    {
        return;
    }
    auto snapshot = std::make_shared<AccountSnapshot>();
    snapshot->root = std::move(mWalkResult);
    snapshot->usage = *mPendingUsage;
    mPendingUsage.reset();
    mWalkDone = false;
    mPhase = Phase::Idle;
    emit loaded(snapshot);
}

void MegaAccountSource::stopWalk()
{
    mCancelWalk = true;
    mWalk.waitForFinished();
}

void MegaAccountSource::logout()
{
    // The walk reads through mApi from a pool thread.
    stopWalk();
    if (!mApi)
    {
        return;
    }
    mLoggedIn = false;
    // Declared before the reset below: ~MegaApi still delivers the finish of a
    // logout that timed out, so the listener has to outlive the MegaApi.
    const auto listener = std::make_unique<mega::SynchronousRequestListener>();
    if (mApi->isLoggedIn())
    {
        mApi->logout(false, listener.get());
        if (listener->trywait(kLogoutTimeoutMs) != 0)
        {
            qCWarning(lcMega) << "logout did not finish within" << kLogoutTimeoutMs << "ms";
        }
    }
    // Joins the SDK's threads and closes its database, so the cache directory
    // can be deleted afterwards (mCacheDir, destroyed after this).
    mApi.reset();
}
