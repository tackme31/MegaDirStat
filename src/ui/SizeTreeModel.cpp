#include "ui/SizeTreeModel.h"

#include <QApplication>
#include <QDateTime>
#include <QLocale>
#include <QStyle>

SizeTreeModel::SizeTreeModel(QObject* parent)
    : QAbstractItemModel(parent),
      mFolderIcon(QApplication::style()->standardIcon(QStyle::SP_DirIcon)),
      mFileIcon(QApplication::style()->standardIcon(QStyle::SP_FileIcon))
{
}

void SizeTreeModel::setSnapshot(SnapshotPtr snapshot)
{
    beginResetModel();
    mSnapshot = std::move(snapshot);
    endResetModel();
}

SnapshotPtr SizeTreeModel::snapshot() const
{
    return mSnapshot;
}

const SizeNode* SizeTreeModel::nodeAt(const QModelIndex& index) const
{
    return index.isValid() ? static_cast<const SizeNode*>(index.internalPointer()) : nullptr;
}

const SizeNode* SizeTreeModel::nodeOrRoot(const QModelIndex& index) const
{
    if (index.isValid())
    {
        return nodeAt(index);
    }
    return mSnapshot ? mSnapshot->root.get() : nullptr;
}

QModelIndex SizeTreeModel::indexFor(const SizeNode* node, int column) const
{
    if (!node || !node->parent || !mSnapshot)
    {
        return {};
    }
    return createIndex(node->row, column, const_cast<SizeNode*>(node));
}

QModelIndex SizeTreeModel::index(int row, int column, const QModelIndex& parent) const
{
    const SizeNode* p = nodeOrRoot(parent);
    if (!p || row < 0 || column < 0 || column >= ColumnCount ||
        row >= static_cast<int>(p->children.size()))
    {
        return {};
    }
    return createIndex(row, column, p->children[static_cast<std::size_t>(row)].get());
}

QModelIndex SizeTreeModel::parent(const QModelIndex& child) const
{
    const SizeNode* node = nodeAt(child);
    if (!node || !node->parent || !node->parent->parent)
    {
        return {};
    }
    return createIndex(node->parent->row, 0, node->parent);
}

int SizeTreeModel::rowCount(const QModelIndex& parent) const
{
    if (parent.column() > 0)
    {
        return 0;
    }
    const SizeNode* p = nodeOrRoot(parent);
    return p ? static_cast<int>(p->children.size()) : 0;
}

int SizeTreeModel::columnCount(const QModelIndex&) const
{
    return ColumnCount;
}

bool SizeTreeModel::hasChildren(const QModelIndex& parent) const
{
    return rowCount(parent) > 0;
}

QVariant SizeTreeModel::data(const QModelIndex& index, int role) const
{
    const SizeNode* node = nodeAt(index);
    if (!node)
    {
        return {};
    }

    const auto fraction = [node] {
        const qint64 parentSize = node->parent ? node->parent->size : 0;
        return parentSize > 0 ? static_cast<double>(node->size) / static_cast<double>(parentSize)
                              : 0.0;
    };

    switch (role)
    {
        case Qt::DisplayRole:
            switch (index.column())
            {
                case NameColumn:
                    return node->name;
                case PercentColumn:
                    return QStringLiteral("%1%").arg(fraction() * 100.0, 0, 'f', 1);
                case SizeColumn:
                    return QLocale().formattedDataSize(node->size);
                case FilesColumn:
                    return node->isFolder() ? QLocale().toString(node->fileCount) : QString();
                case ModifiedColumn:
                    if (node->mtime <= 0)
                    {
                        return {};
                    }
                    return QLocale().toString(QDateTime::fromSecsSinceEpoch(node->mtime),
                                              QLocale::ShortFormat);
                default:
                    return {};
            }
        case Qt::DecorationRole:
            if (index.column() == NameColumn)
            {
                return node->isFolder() ? mFolderIcon : mFileIcon;
            }
            return {};
        case Qt::TextAlignmentRole:
            if (index.column() == NameColumn)
            {
                return QVariant::fromValue(Qt::AlignLeft | Qt::AlignVCenter);
            }
            return QVariant::fromValue(Qt::AlignRight | Qt::AlignVCenter);
        case Qt::ToolTipRole:
            return node->path();
        case FractionRole:
            return fraction();
        case DepthRole:
        {
            int depth = 0;
            for (const SizeNode* p = node->parent; p && p->parent; p = p->parent)
            {
                ++depth;
            }
            return depth;
        }
        default:
            return {};
    }
}

QVariant SizeTreeModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal)
    {
        return {};
    }
    if (role == Qt::TextAlignmentRole)
    {
        return QVariant::fromValue(section == NameColumn ? Qt::AlignLeft | Qt::AlignVCenter
                                                         : Qt::AlignRight | Qt::AlignVCenter);
    }
    if (role != Qt::DisplayRole)
    {
        return {};
    }
    switch (section)
    {
        case NameColumn:
            return tr("Name");
        case PercentColumn:
            return tr("% of Parent");
        case SizeColumn:
            return tr("Size");
        case FilesColumn:
            return tr("Files");
        case ModifiedColumn:
            return tr("Last Modified");
        default:
            return {};
    }
}
