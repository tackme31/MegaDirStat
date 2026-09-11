#include "ui/TreemapWidget.h"

#include <QHelpEvent>
#include <QLocale>
#include <QMouseEvent>
#include <QPainter>
#include <QToolTip>

#include <algorithm>
#include <array>
#include <cmath>

namespace
{

// Flat fills, mid-tone enough to read on both light and dark backgrounds.
constexpr std::array<QRgb, 12> kPalette = {
    qRgb(0x3B, 0x82, 0xF6), // blue
    qRgb(0x10, 0xB9, 0x81), // emerald
    qRgb(0xF5, 0x9E, 0x0B), // amber
    qRgb(0xF4, 0x3F, 0x5E), // rose
    qRgb(0x8B, 0x5C, 0xF6), // violet
    qRgb(0x06, 0xB6, 0xD4), // cyan
    qRgb(0xF9, 0x73, 0x16), // orange
    qRgb(0x84, 0xCC, 0x16), // lime
    qRgb(0xEC, 0x48, 0x99), // pink
    qRgb(0x14, 0xB8, 0xA6), // teal
    qRgb(0x63, 0x66, 0xF1), // indigo
    qRgb(0xEA, 0xB3, 0x08), // yellow
};
constexpr QRgb kOtherFiles = qRgb(0x94, 0xA3, 0xB8);
constexpr QRgb kCollapsedFolder = qRgb(0x64, 0x74, 0x8B);

constexpr double kPaddingPx = 2.0; // logical px between a folder's frame and its content

QColor blend(const QColor& a, const QColor& b, double t)
{
    return QColor::fromRgbF(static_cast<float>(a.redF() + (b.redF() - a.redF()) * t),
                            static_cast<float>(a.greenF() + (b.greenF() - a.greenF()) * t),
                            static_cast<float>(a.blueF() + (b.blueF() - a.blueF()) * t));
}

// Rounds both edges independently, so neighbours that share a float edge also
// share the pixel edge and nothing overlaps or leaves a hole.
QRect snapped(const QRectF& r)
{
    const int x0 = static_cast<int>(std::lround(r.left()));
    const int y0 = static_cast<int>(std::lround(r.top()));
    const int x1 = static_cast<int>(std::lround(r.right()));
    const int y1 = static_cast<int>(std::lround(r.bottom()));
    return QRect(x0, y0, x1 - x0, y1 - y0);
}

} // namespace

TreemapWidget::TreemapWidget(QWidget* parent) : QWidget(parent)
{
    setAttribute(Qt::WA_OpaquePaintEvent);
    setMinimumSize(80, 60);
}

void TreemapWidget::setSnapshot(SnapshotPtr snapshot)
{
    mSnapshot = std::move(snapshot);
    mSelected = nullptr;
    assignExtensionColors();
    mDirty = true;
    update();
}

void TreemapWidget::setSelectedNode(const SizeNode* node)
{
    if (node == mSelected)
    {
        return;
    }
    mSelected = node;
    update();
}

const SizeNode* TreemapWidget::nodeAt(const QPoint& pos) const
{
    const QPointF p = QPointF(pos) * devicePixelRatioF();
    // Pre-order, so the last hit is the deepest; a hit in a folder's padding
    // then resolves to the folder itself.
    const SizeNode* hit = nullptr;
    for (const TreemapCell& cell : mCells)
    {
        if (cell.depth > 0 && cell.rect.contains(p))
        {
            hit = cell.node;
        }
    }
    return hit;
}

bool TreemapWidget::event(QEvent* event)
{
    if (event->type() == QEvent::ToolTip)
    {
        const auto* help = static_cast<QHelpEvent*>(event);
        if (const SizeNode* node = nodeAt(help->pos()))
        {
            QToolTip::showText(help->globalPos(),
                               QStringLiteral("%1\n%2")
                                   .arg(node->path(), QLocale().formattedDataSize(node->size)),
                               this);
        }
        else
        {
            QToolTip::hideText();
        }
        return true;
    }
    return QWidget::event(event);
}

void TreemapWidget::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::PaletteChange)
    {
        mDirty = true;
        update();
    }
    QWidget::changeEvent(event);
}

void TreemapWidget::paintEvent(QPaintEvent*)
{
    if (mDirty)
    {
        rebuild();
    }

    QPainter painter(this);
    if (mImage.isNull())
    {
        painter.fillRect(rect(), palette().color(QPalette::Window));
        return;
    }
    painter.drawImage(QPointF(0, 0), mImage);

    if (mSelected)
    {
        const auto it = mCellIndex.constFind(mSelected);
        if (it != mCellIndex.cend())
        {
            const qreal dpr = devicePixelRatioF();
            const QRectF r = mCells[static_cast<std::size_t>(*it)].rect;
            const QRectF logical(r.topLeft() / dpr, r.size() / dpr);
            painter.setRenderHint(QPainter::Antialiasing);
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen(palette().color(QPalette::Base), 4));
            painter.drawRect(logical.adjusted(2, 2, -2, -2));
            painter.setPen(QPen(palette().color(QPalette::Highlight), 2));
            painter.drawRect(logical.adjusted(2, 2, -2, -2));
        }
    }
}

void TreemapWidget::resizeEvent(QResizeEvent* event)
{
    mDirty = true;
    QWidget::resizeEvent(event);
}

void TreemapWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton)
    {
        if (const SizeNode* node = nodeAt(event->position().toPoint()))
        {
            emit nodeClicked(node);
        }
    }
    QWidget::mousePressEvent(event);
}

void TreemapWidget::rebuild()
{
    mDirty = false;
    mCells.clear();
    mCellIndex.clear();
    mImage = QImage();
    if (!mSnapshot || !mSnapshot->root)
    {
        return;
    }

    // Laid out and rendered in device pixels, so HiDPI screens get full detail.
    const qreal dpr = devicePixelRatioF();
    const QSize pixels = (QSizeF(size()) * dpr).toSize();
    if (pixels.isEmpty())
    {
        return;
    }

    TreemapOptions options;
    options.padding = kPaddingPx * dpr;
    mCells = layoutTreemap(*mSnapshot->root, QRectF(QPointF(0, 0), QSizeF(pixels)), options);
    mCellIndex.reserve(static_cast<qsizetype>(mCells.size()));
    for (std::size_t i = 0; i < mCells.size(); ++i)
    {
        mCellIndex.insert(mCells[i].node, static_cast<qsizetype>(i));
    }

    const QColor background = palette().color(QPalette::Window);
    const QColor folderFill = blend(background, palette().color(QPalette::WindowText), 0.10);
    const int gap = std::max(1, static_cast<int>(std::lround(dpr)));

    mImage = QImage(pixels, QImage::Format_RGB32);
    mImage.fill(background);
    QPainter painter(&mImage);
    painter.setPen(Qt::NoPen);
    for (const TreemapCell& cell : mCells)
    {
        if (cell.depth == 0)
        {
            continue;
        }
        QRect r = snapped(cell.rect);
        // The gap goes on the right and bottom only, so each boundary between
        // two neighbours gets exactly one. Cells too thin to spare it keep it.
        if (r.width() > 2 * gap)
        {
            r.setWidth(r.width() - gap);
        }
        if (r.height() > 2 * gap)
        {
            r.setHeight(r.height() - gap);
        }
        if (r.isEmpty())
        {
            continue;
        }
        painter.fillRect(r, cell.leaf ? QColor(baseColor(*cell.node)) : folderFill);
    }
    painter.end();
    mImage.setDevicePixelRatio(dpr);
}

void TreemapWidget::assignExtensionColors()
{
    mExtensionColors.clear();
    if (!mSnapshot || !mSnapshot->root)
    {
        return;
    }

    // The extensions taking the most bytes get the distinct colours.
    QHash<QString, qint64> bytes;
    std::vector<const SizeNode*> stack{mSnapshot->root.get()};
    while (!stack.empty())
    {
        const SizeNode* node = stack.back();
        stack.pop_back();
        if (node->isFolder())
        {
            for (const auto& child : node->children)
            {
                stack.push_back(child.get());
            }
        }
        else
        {
            bytes[node->extension()] += node->size;
        }
    }

    std::vector<std::pair<qint64, QString>> ranked;
    ranked.reserve(static_cast<std::size_t>(bytes.size()));
    for (auto it = bytes.cbegin(); it != bytes.cend(); ++it)
    {
        if (!it.key().isEmpty())
        {
            ranked.emplace_back(it.value(), it.key());
        }
    }
    std::sort(ranked.begin(), ranked.end(), [](const auto& a, const auto& b) {
        return a.first != b.first ? a.first > b.first : a.second < b.second;
    });
    for (std::size_t i = 0; i < ranked.size() && i < kPalette.size(); ++i)
    {
        mExtensionColors.insert(ranked[i].second, kPalette[i]);
    }
}

QRgb TreemapWidget::baseColor(const SizeNode& node) const
{
    if (node.isFolder())
    {
        return kCollapsedFolder;
    }
    return mExtensionColors.value(node.extension(), kOtherFiles);
}
