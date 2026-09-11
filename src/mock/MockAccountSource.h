#pragma once

#include "core/IAccountSource.h"

#include <QElapsedTimer>
#include <QString>

class QTimer;

struct MockOptions
{
    QString fixturePath;       // used when non-empty
    qint64 generateCount = 0;  // otherwise, a generated tree of this many files
    quint64 seed = 1;
    int delayMs = 0;           // simulated load time, with progress ticks
    bool fail = false;         // end the load with failed()
    bool requireLogin = false; // show the sign-in page first (any credentials)
    bool twoFactor = false;    // ...then ask for a code (implies requireLogin)
};

// Serves a fixture or a generated tree without any network or login.
//
// With requireLogin, any credentials sign in except the password "wrong";
// with twoFactor, the code kMockTwoFactorCode is the only one accepted.
class MockAccountSource : public IAccountSource
{
    Q_OBJECT

public:
    static constexpr const char* kMockWrongPassword = "wrong";
    static constexpr const char* kMockTwoFactorCode = "123456";

    explicit MockAccountSource(MockOptions options, QObject* parent = nullptr);

    bool requiresLogin() const override;
    void login(const QString& email,
               const QString& password,
               const QString& twoFactorCode) override;
    void load() override;
    void logout() override;

private:
    void finishLogin(const QString& password, const QString& twoFactorCode);
    void tick();
    void build();

    MockOptions mOptions;
    QTimer* mTimer = nullptr;
    QElapsedTimer mElapsed;
    bool mLoggedIn = false;
};
