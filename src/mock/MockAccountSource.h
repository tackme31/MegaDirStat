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
};

// Serves a fixture or a generated tree without any network or login.
class MockAccountSource : public IAccountSource
{
    Q_OBJECT

public:
    explicit MockAccountSource(MockOptions options, QObject* parent = nullptr);

    bool requiresLogin() const override;
    void login(const QString& email, const QString& password) override;
    void load() override;
    void logout() override;

private:
    void tick();
    void build();

    MockOptions mOptions;
    QTimer* mTimer = nullptr;
    QElapsedTimer mElapsed;
};
