#pragma once

#include "core/AccountSnapshot.h"

#include <QByteArray>
#include <QString>

#include <memory>

// Reads the JSON fixture format described in docs/DESIGN.md §4. The returned
// snapshot is already finalized. On failure returns nullptr and sets *error.
std::unique_ptr<AccountSnapshot> parseFixture(const QByteArray& json, QString* error);
std::unique_ptr<AccountSnapshot> loadFixtureFile(const QString& path, QString* error);
