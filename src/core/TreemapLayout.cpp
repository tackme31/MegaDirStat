#include "core/TreemapLayout.h"

#include <algorithm>
#include <limits>
#include <numeric>

namespace
{

double worstAspect(double largestArea, double smallestArea, double rowArea, double side)
{
    const double side2 = side * side;
    const double row2 = rowArea * rowArea;
    return std::max(side2 * largestArea / row2, row2 / (side2 * smallestArea));
}

void layoutNode(const SizeNode& node,
                const QRectF& rect,
                int depth,
                const TreemapOptions& options,
                std::vector<TreemapCell>& out)
{
    const bool subdivide = node.isFolder() && node.size > 0 && !node.children.empty() &&
                           rect.width() >= options.minSide && rect.height() >= options.minSide;
    out.push_back({&node, rect, depth, !subdivide});
    if (!subdivide)
    {
        return;
    }

    // The invisible root is not framed; below it, pad only while the inset
    // still leaves room for content.
    QRectF inner = rect;
    const double p = options.padding;
    if (depth > 0 && p > 0 && rect.width() > 4 * p + options.minSide &&
        rect.height() > 4 * p + options.minSide)
    {
        inner = rect.adjusted(p, p, -p, -p);
    }

    std::vector<qint64> sizes;
    sizes.reserve(node.children.size());
    for (const auto& child : node.children)
    {
        // Children are sorted descending, so the zero-size ones are all at the end.
        if (child->size <= 0)
        {
            break;
        }
        sizes.push_back(child->size);
    }

    const std::vector<QRectF> rects = squarify(sizes, inner);
    for (std::size_t i = 0; i < rects.size(); ++i)
    {
        layoutNode(*node.children[i], rects[i], depth + 1, options, out);
    }
}

} // namespace

std::vector<QRectF> squarify(const std::vector<qint64>& sizes, const QRectF& bounds)
{
    const std::size_t n = sizes.size();
    std::vector<QRectF> out(n);
    const double total = std::accumulate(sizes.begin(), sizes.end(), 0.0, [](double acc, qint64 v) {
        return acc + static_cast<double>(v);
    });
    if (n == 0 || total <= 0 || bounds.width() <= 0 || bounds.height() <= 0)
    {
        return out;
    }

    const double scale = bounds.width() * bounds.height() / total;
    QRectF rest = bounds;
    std::size_t start = 0;

    while (start < n)
    {
        // The row runs along the shorter side of what is left.
        const bool rowAlongWidth = rest.width() < rest.height();
        const double side = rowAlongWidth ? rest.width() : rest.height();

        std::size_t end = start;
        double rowSum = 0;
        double worst = std::numeric_limits<double>::infinity();
        while (end < n)
        {
            const double candidateSum = rowSum + static_cast<double>(sizes[end]);
            const double candidate = worstAspect(static_cast<double>(sizes[start]) * scale,
                                                 static_cast<double>(sizes[end]) * scale,
                                                 candidateSum * scale,
                                                 side);
            if (end > start && candidate > worst)
            {
                break;
            }
            worst = candidate;
            rowSum = candidateSum;
            ++end;
        }

        // The last row takes whatever is left, so float drift never opens a gap.
        const double thickness = end == n ? (rowAlongWidth ? rest.height() : rest.width())
                                          : rowSum * scale / side;
        const double rowStart = rowAlongWidth ? rest.left() : rest.top();
        const double rowEnd = rowAlongWidth ? rest.right() : rest.bottom();
        double pos = rowStart;
        for (std::size_t i = start; i < end; ++i)
        {
            const double next =
                i + 1 == end ? rowEnd : pos + side * static_cast<double>(sizes[i]) / rowSum;
            out[i] = rowAlongWidth ? QRectF(pos, rest.top(), next - pos, thickness)
                                   : QRectF(rest.left(), pos, thickness, next - pos);
            pos = next;
        }

        if (rowAlongWidth)
        {
            rest.setTop(rest.top() + thickness);
        }
        else
        {
            rest.setLeft(rest.left() + thickness);
        }
        start = end;
    }
    return out;
}

std::vector<TreemapCell> layoutTreemap(const SizeNode& root,
                                       const QRectF& bounds,
                                       const TreemapOptions& options)
{
    std::vector<TreemapCell> cells;
    if (root.size <= 0 || bounds.width() <= 0 || bounds.height() <= 0)
    {
        return cells;
    }
    cells.reserve(static_cast<std::size_t>(std::min<qint64>(root.fileCount, 1'000'000)) + 64);
    layoutNode(root, bounds, 0, options, cells);
    return cells;
}
