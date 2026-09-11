#pragma once

#include "core/AccountSnapshot.h"

#include <QObject>
#include <QString>

// Where the account's data comes from: the MEGA SDK, or mock data. The UI
// only ever talks to this port. Signals are emitted on the GUI thread.
class IAccountSource : public QObject
{
    Q_OBJECT

public:
    enum class LoginResult
    {
        Ok,
        NeedsTwoFactor, // call login() again with the same credentials and a code
        Failed,
    };
    Q_ENUM(LoginResult)

    using QObject::QObject;

    // True while load() cannot run: before a successful login, and again after a
    // first load that failed (the source signs itself out then).
    virtual bool requiresLogin() const = 0;

    // Answered by exactly one loginFinished(). twoFactorCode is empty on the
    // first attempt. The source keeps no copy of the credentials, so the second
    // attempt after NeedsTwoFactor passes them again.
    virtual void login(const QString& email,
                       const QString& password,
                       const QString& twoFactorCode) = 0;

    // Answered by any number of progress() and then exactly one loaded() or failed().
    virtual void load() = 0;

    // Ends the session before the application exits. May block briefly.
    virtual void logout() = 0;

signals:
    // error is user-facing text, empty unless result is Failed.
    void loginFinished(IAccountSource::LoginResult result, const QString& error);

    // stage is the headline ("Downloading your file list…"), detail an optional
    // second line ("12 MB of 40 MB"). total -1 = unknown (busy indicator).
    void progress(const QString& stage, const QString& detail, qint64 done, qint64 total);

    void loaded(SnapshotPtr snapshot);
    void failed(const QString& error);
};
