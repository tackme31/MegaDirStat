#include "mock/TreeGenerator.h"

#include <QString>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <random>
#include <vector>

namespace
{

// std::*_distribution output differs between standard libraries; only the
// engine is specified exactly. Doing the transforms by hand keeps a seed
// producing the same tree on MSVC, libstdc++ and libc++.
class Rng
{
public:
    explicit Rng(quint64 seed) : mEngine(seed)
    {
    }

    double uniform()
    {
        return static_cast<double>(mEngine() >> 11) * 0x1.0p-53;
    }

    std::size_t index(std::size_t count)
    {
        return std::min(count - 1, static_cast<std::size_t>(uniform() * static_cast<double>(count)));
    }

    double normal()
    {
        const double u1 = std::max(uniform(), 1e-300);
        const double u2 = uniform();
        return std::sqrt(-2.0 * std::log(u1)) * std::cos(2.0 * std::numbers::pi * u2);
    }

private:
    std::mt19937_64 mEngine;
};

struct ExtSpec
{
    const char* ext;
    double weight;
    double medianBytes;
    double sigma;
};

struct Profile
{
    const char* folder;
    double weight;
    const char* filePrefix;
    std::vector<ExtSpec> exts;
    std::vector<const char*> subfolders;
    // Share of a folder's files that take the folder's own dominant extension;
    // the rest are drawn from exts at large.
    double homogeneity;
};

constexpr double KB = 1024.0;
constexpr double MB = 1024.0 * KB;
constexpr double GB = 1024.0 * MB;

const std::vector<Profile>& profiles()
{
    static const std::vector<Profile> all = {
        {"Camera Uploads",
         30,
         "IMG_",
         {{"jpg", 0.6, 3.5 * MB, 0.5},
          {"heic", 0.25, 2.2 * MB, 0.4},
          {"mp4", 0.1, 80 * MB, 1.0},
          {"cr3", 0.05, 28 * MB, 0.2}},
         {"2019", "2020", "2021", "2022", "2023", "2024", "2025", "Screenshots"},
         0.7},
        {"Videos",
         5,
         "video_",
         {{"mp4", 0.6, 400 * MB, 1.1}, {"mkv", 0.3, 1.5 * GB, 0.8}, {"mov", 0.1, 900 * MB, 1.0}},
         {"Movies", "Family", "Travel", "Recordings", "Lectures"},
         0.8},
        {"Music",
         15,
         "Track ",
         {{"mp3", 0.6, 6 * MB, 0.4}, {"flac", 0.3, 28 * MB, 0.4}, {"m4a", 0.1, 7 * MB, 0.4}},
         {"Artist", "Album", "Live", "Soundtracks", "Podcasts"},
         0.95},
        {"Documents",
         20,
         "Document ",
         {{"pdf", 0.35, 800 * KB, 1.3},
          {"docx", 0.2, 150 * KB, 1.0},
          {"xlsx", 0.15, 90 * KB, 1.0},
          {"pptx", 0.08, 3 * MB, 1.0},
          {"txt", 0.12, 6 * KB, 1.2},
          {"md", 0.1, 4 * KB, 1.0}},
         {"Work", "Taxes", "Receipts", "Manuals", "Notes", "Scans"},
         0.5},
        {"Projects",
         25,
         "file_",
         {{"cpp", 0.2, 12 * KB, 1.0},
          {"h", 0.15, 4 * KB, 1.0},
          {"js", 0.15, 8 * KB, 1.2},
          {"json", 0.1, 3 * KB, 1.4},
          {"png", 0.15, 60 * KB, 1.2},
          {"svg", 0.05, 8 * KB, 1.0},
          {"zip", 0.02, 40 * MB, 1.2},
          {"py", 0.18, 6 * KB, 1.0}},
         {"src", "include", "assets", "tests", "docs", "tools", "node_modules"},
         0.5},
        {"Backups",
         2,
         "backup_",
         {{"zip", 0.4, 1.2 * GB, 1.2},
          {"7z", 0.3, 2 * GB, 1.0},
          {"iso", 0.1, 4 * GB, 0.3},
          {"bak", 0.2, 500 * MB, 1.5}},
         {"PC", "Phone", "NAS", "Old laptop"},
         0.8},
        {"Downloads",
         3,
         "download_",
         {{"zip", 0.3, 50 * MB, 1.5},
          {"exe", 0.2, 60 * MB, 1.2},
          {"pdf", 0.2, 2 * MB, 1.2},
          {"dmg", 0.1, 150 * MB, 1.0},
          {"iso", 0.05, 3 * GB, 0.5},
          {"jpg", 0.15, 1 * MB, 1.0}},
         {"Installers", "Papers"},
         0.3},
    };
    return all;
}

template <typename T, typename WeightOf>
std::size_t pickWeighted(const std::vector<T>& items, WeightOf weightOf, Rng& rng)
{
    double total = 0;
    for (const T& item : items)
    {
        total += weightOf(item);
    }
    double r = rng.uniform() * total;
    for (std::size_t i = 0; i < items.size(); ++i)
    {
        r -= weightOf(items[i]);
        if (r < 0)
        {
            return i;
        }
    }
    return items.size() - 1;
}

int depthBelow(const SizeNode* folder, const SizeNode* top)
{
    int depth = 0;
    for (const SizeNode* n = folder; n != top; n = n->parent)
    {
        ++depth;
    }
    return depth;
}

struct Area
{
    const Profile* profile;
    SizeNode* top;
    std::vector<SizeNode*> folders;
    // Parallel to folders: index into profile->exts of each folder's dominant extension.
    std::vector<std::size_t> folderExts;
    int nextFolder = 1;
    qint64 nextFile = 1;
};

std::size_t pickExt(const Profile& profile, Rng& rng)
{
    return pickWeighted(profile.exts, [](const ExtSpec& e) { return e.weight; }, rng);
}

void addOneFile(Area& area, Rng& rng, double newFolderChance, int maxDepth)
{
    if (rng.uniform() < newFolderChance)
    {
        const std::size_t underIndex = rng.index(area.folders.size());
        SizeNode* under = area.folders[underIndex];
        if (depthBelow(under, area.top) < maxDepth)
        {
            const auto& names = area.profile->subfolders;
            const QString name = QStringLiteral("%1 %2")
                                     .arg(QLatin1String(names[rng.index(names.size())]))
                                     .arg(area.nextFolder++);
            const std::size_t parentExt = area.folderExts[underIndex];
            area.folders.push_back(under->addFolder(name));
            // Half the subfolders carry on their parent's kind of content, so
            // same-coloured regions span several levels as in real accounts.
            area.folderExts.push_back(rng.uniform() < 0.5 ? parentExt : pickExt(*area.profile, rng));
        }
    }

    // Bias towards recently created folders so the tree grows clusters rather
    // than a flat spread.
    const std::size_t n = area.folders.size();
    const std::size_t recent = std::min<std::size_t>(n, 8);
    const std::size_t folderIndex =
        rng.uniform() < 0.7 ? n - 1 - rng.index(recent) : rng.index(n);
    SizeNode* folder = area.folders[folderIndex];

    // Mixing the folder's own extension with a draw from the profile's weights
    // leaves the account-wide mix of extensions unchanged.
    const std::size_t extIndex = rng.uniform() < area.profile->homogeneity
                                     ? area.folderExts[folderIndex]
                                     : pickExt(*area.profile, rng);
    const ExtSpec& ext = area.profile->exts[extIndex];
    const double bytes = std::clamp(
        std::exp(std::log(ext.medianBytes) + ext.sigma * rng.normal()), 0.0, 50.0 * GB);

    // A fixed "now" keeps the output independent of when it runs.
    constexpr qint64 now = 1767225600; // 2026-01-01T00:00:00Z
    constexpr qint64 span = 8LL * 365 * 24 * 3600;
    const qint64 mtime = now - static_cast<qint64>(rng.uniform() * static_cast<double>(span));

    const QString name = QStringLiteral("%1%2.%3")
                             .arg(QLatin1String(area.profile->filePrefix))
                             .arg(area.nextFile++, 4, 10, QLatin1Char('0'))
                             .arg(QLatin1String(ext.ext));
    folder->addFile(name, static_cast<qint64>(bytes), mtime);
}

qint64 quotaFor(qint64 used)
{
    for (const double tier : {20 * GB, 400 * GB, 2048 * GB, 8192 * GB, 16384 * GB})
    {
        if (static_cast<double>(used) * 1.2 <= tier)
        {
            return static_cast<qint64>(tier);
        }
    }
    return static_cast<qint64>(static_cast<double>(used) * 1.5);
}

} // namespace

std::unique_ptr<AccountSnapshot> generateSnapshot(const GeneratorOptions& options)
{
    Rng rng(options.seed);

    auto snapshot = std::make_unique<AccountSnapshot>();
    snapshot->root = std::make_unique<SizeNode>();
    snapshot->root->kind = NodeKind::Folder;
    SizeNode* cloudDrive = snapshot->root->addFolder(QStringLiteral("Cloud Drive"));
    SizeNode* rubbish = snapshot->root->addFolder(QStringLiteral("Rubbish Bin"));

    std::vector<Area> areas;
    std::vector<Area> rubbishAreas;
    for (const Profile& p : profiles())
    {
        SizeNode* top = cloudDrive->addFolder(QLatin1String(p.folder));
        areas.push_back({&p, top, {top}, {pickExt(p, rng)}});
        rubbishAreas.push_back({&p, rubbish, {rubbish}, {pickExt(p, rng)}});
    }

    for (qint64 i = 0; i < options.fileCount; ++i)
    {
        const bool toRubbish = rng.uniform() < 0.03;
        std::vector<Area>& pool = toRubbish ? rubbishAreas : areas;
        Area& area = pool[pickWeighted(pool, [](const Area& a) { return a.profile->weight; }, rng)];
        addOneFile(area, rng, toRubbish ? 0.02 : 0.06, toRubbish ? 1 : 5);
    }

    finalizeTree(*snapshot->root);
    snapshot->usage.used = snapshot->root->size;
    snapshot->usage.total = quotaFor(snapshot->root->size);
    return snapshot;
}
