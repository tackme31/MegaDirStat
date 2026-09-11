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
    using QObject::QObject;

    virtual bool requiresLogin() const = 0;

    // Only called when requiresLogin(). Answered by exactly one loginFinished().
    virtual void login(const QString& email, const QString& password) = 0;

    // Answered by any number of progress() and then exactly one loaded() or failed().
    virtual void load() = 0;

    virtual void logout() = 0;

signals:
    void loginFinished(bool ok, const QString& error);
    void progress(const QString& stage, qint64 done, qint64 total); // total -1 = unknown
    void loaded(SnapshotPtr snapshot);
    void failed(const QString& error);
};
