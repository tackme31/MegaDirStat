#pragma once

#include "core/AccountSnapshot.h"

#include <QAbstractItemModel>
#include <QIcon>

// Read-only tree model straight over an AccountSnapshot. The snapshot is
// immutable, so SizeNode pointers double as stable internal ids.
class SizeTreeModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    enum Column
    {
        NameColumn,
        PercentColumn,
        SizeColumn,
        FilesColumn,
        ModifiedColumn,
        ColumnCount,
    };

    enum Role
    {
        FractionRole = Qt::UserRole + 1, // double, share of the parent's size
        DepthRole,                       // int, 0 for the top-level roots
    };

    explicit SizeTreeModel(QObject* parent = nullptr);

    void setSnapshot(SnapshotPtr snapshot);
    SnapshotPtr snapshot() const;

    const SizeNode* nodeAt(const QModelIndex& index) const;
    QModelIndex indexFor(const SizeNode* node, int column = 0) const;

    QModelIndex index(int row, int column, const QModelIndex& parent = {}) const override;
    QModelIndex parent(const QModelIndex& child) const override;
    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    bool hasChildren(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section,
                        Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;

private:
    const SizeNode* nodeOrRoot(const QModelIndex& index) const;

    SnapshotPtr mSnapshot;
    QIcon mFolderIcon;
    QIcon mFileIcon;
};
