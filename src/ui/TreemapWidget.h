#pragma once

#include "core/AccountSnapshot.h"
#include "core/TreemapLayout.h"

#include <QHash>
#include <QImage>
#include <QWidget>

#include <vector>

// Flat treemap of a whole snapshot: one colour per file type, a uniform gap
// between cells, and anything below kMinCellPx grouped into one muted cell. The
// rendered image is cached and only rebuilt on resize, a palette change or a
// new snapshot; selection is drawn on top of it.
class TreemapWidget : public QWidget
{
    Q_OBJECT

public:
    explicit TreemapWidget(QWidget* parent = nullptr);

    void setSnapshot(SnapshotPtr snapshot);
    void setSelectedNode(const SizeNode* node);

    // The painted cell under pos (widget coordinates), or nullptr. For a merged
    // group of small items that is the folder holding them.
    const SizeNode* nodeAt(const QPoint& pos) const;

signals:
    void nodeClicked(const SizeNode* node);

protected:
    bool event(QEvent* event) override;
    void changeEvent(QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    const TreemapCell* cellAt(const QPoint& pos) const;
    void rebuild();
    void assignColors();
    QRgb baseColor(const SizeNode& node) const;

    SnapshotPtr mSnapshot;
    std::vector<TreemapCell> mCells;
    QHash<const SizeNode*, qsizetype> mCellIndex;
    QHash<QString, QRgb> mExtensionColors;
    QHash<const SizeNode*, QRgb> mFolderColors;
    QImage mImage;
    const SizeNode* mSelected = nullptr;
    bool mDirty = true;
};
