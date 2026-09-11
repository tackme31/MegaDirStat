#pragma once

#include <QString>

#include <memory>
#include <vector>

enum class NodeKind : quint8
{
    Folder,
    File,
};

// One entry of an account snapshot. Built once by an IAccountSource, finalized
// with finalizeTree(), then treated as immutable by everything downstream.
struct SizeNode
{
    QString name;
    qint64 size = 0;      // folders: sum over the subtree (finalizeTree)
    qint64 fileCount = 0; // files in the subtree; 1 for a file
    qint64 mtime = 0;     // seconds since the epoch, 0 = unknown; folders: newest in subtree
    SizeNode* parent = nullptr;
    std::vector<std::unique_ptr<SizeNode>> children;
    int row = 0; // index in parent->children, valid after finalizeTree
    NodeKind kind = NodeKind::File;

    bool isFolder() const
    {
        return kind == NodeKind::Folder;
    }

    SizeNode* addFolder(QString folderName);
    SizeNode* addFile(QString fileName, qint64 fileSize, qint64 fileMtime = 0);

    // Slash-separated path from the first visible level, e.g. "Cloud Drive/Photos/a.jpg".
    QString path() const;

    // Lower-case extension without the dot; empty for folders and extension-less files.
    QString extension() const;
};

// Aggregates size, fileCount and mtime into every folder, sorts each children
// list by size descending (then name), and assigns row.
void finalizeTree(SizeNode& root);
