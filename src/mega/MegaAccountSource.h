#pragma once

#include "core/IAccountSource.h"

#include <QFuture>
#include <QString>

#include <atomic>
#include <memory>
#include <optional>

class QTimer;
class RunCacheDir;

namespace mega
{
class MegaApi;
}

// IAccountSource over the MEGA SDK: login (with two-factor), fetchNodes, then a
// walk of Cloud Drive and Rubbish Bin into a SizeNode snapshot on a worker
// thread. The only class that talks to MegaApi; the header keeps <megaapi.h>
// out of everything that includes it.
class MegaAccountSource : public IAccountSource
{
    Q_OBJECT

public:
    // The SDK keeps its state cache in cacheDir, which outlives the MegaApi.
    MegaAccountSource(std::unique_ptr<RunCacheDir> cacheDir,
                      const QString& userAgent,
                      QObject* parent = nullptr);
    ~MegaAccountSource() override;

    bool requiresLogin() const override;
    void login(const QString& email,
               const QString& password,
               const QString& twoFactorCode) override;
    void load() override;
    void logout() override;

private:
    enum class Phase
    {
        Idle,
        Fetching, // fetchNodes or catchup in flight
        Walking,  // building the snapshot
    };

    // Runs f on the GUI thread. Safe from SDK callbacks: ~MegaAccountSource joins
    // the SDK and the walk before the QObject goes away, and events still queued
    // for it are then dropped.
    template <typename F>
    void post(F&& f);

    void onFetchProgress(quint64 generation, qint64 transferred, qint64 total);
    void enterDecrypting();
    void onFetchFinished(quint64 generation, int errorCode, const QString& sdkMessage);
    void startWalk(quint64 generation);
    void onWalkProgress(quint64 generation, qint64 done, qint64 total);
    void finishIfComplete();
    void failFirstLoad(const QString& error);
    void stopWalk();

    // Declared before mApi so the MegaApi (and its open database) goes first.
    std::unique_ptr<RunCacheDir> mCacheDir;
    std::unique_ptr<mega::MegaApi> mApi;

    QTimer* mStallTimer = nullptr;
    Phase mPhase = Phase::Idle;
    bool mLoggedIn = false;
    bool mNodesFetched = false;
    bool mDecrypting = false;
    quint64 mGeneration = 0; // bumped by every load(); stale callbacks compare against it

    QFuture<void> mWalk;
    std::atomic<bool> mCancelWalk{false};
    std::unique_ptr<SizeNode> mWalkResult; // written by the walk job, see startWalk()
    bool mWalkDone = false;
    std::optional<AccountUsage> mPendingUsage; // the account-details request, run alongside the walk
};
