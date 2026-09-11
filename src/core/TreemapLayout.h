#pragma once

#include "core/SizeNode.h"

#include <QRectF>

#include <vector>

struct TreemapCell
{
    const SizeNode* node = nullptr;
    QRectF rect;
    int depth = 0;
    // Painted cells: files, and folders too small to subdivide. The other
    // cells are folders whose children are laid out inside them.
    bool leaf = false;
};

struct TreemapOptions
{
    // Inset between a folder's rectangle and its children, so nesting shows as
    // a frame. Skipped for folders too small to afford it.
    double padding = 0.0;
    // A folder whose rectangle is thinner than this is painted as one cell.
    double minSide = 1.0;
};

// Squarified layout (Bruls, Huizing, van Wijk 2000). sizes must be positive and
// sorted descending; the returned rectangles are in the same order and tile bounds.
std::vector<QRectF> squarify(const std::vector<qint64>& sizes, const QRectF& bounds);

// Lays out the whole subtree of root in pre-order (a folder before its
// descendants). Zero-size nodes get no cell. With padding 0 the leaves tile
// bounds exactly.
std::vector<TreemapCell> layoutTreemap(const SizeNode& root,
                                       const QRectF& bounds,
                                       const TreemapOptions& options = {});
