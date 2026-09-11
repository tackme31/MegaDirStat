#pragma once

#include "core/AccountSnapshot.h"

#include <memory>

struct GeneratorOptions
{
    qint64 fileCount = 10000;
    quint64 seed = 1;
};

// A plausible-looking account (photos, videos, music, documents, source trees,
// backups, a Rubbish Bin) with about fileCount files. Deterministic for a given
// seed. The returned snapshot is already finalized.
std::unique_ptr<AccountSnapshot> generateSnapshot(const GeneratorOptions& options);
