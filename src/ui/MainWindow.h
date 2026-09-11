#pragma once

#include "core/IAccountSource.h"

#include <QMainWindow>

class QLabel;
class QProgressBar;
class QTreeView;
class SizeTreeModel;
class TreemapWidget;

// Tree on top, treemap below, split by a QSplitter (docs/DESIGN.md §2).
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(IAccountSource& source, QWidget* parent = nullptr);

    // Logs in if the source needs it, then loads the account.
    void start();

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void reload();
    void onProgress(const QString& stage, qint64 done, qint64 total);
    void onLoaded(SnapshotPtr snapshot);
    void onFailed(const QString& error);
    void onCurrentChanged(const QModelIndex& current);
    void onTreemapClicked(const SizeNode* node);

    IAccountSource& mSource;
    SizeTreeModel* mModel = nullptr;
    QTreeView* mTree = nullptr;
    TreemapWidget* mTreemap = nullptr;
    QLabel* mStatusLabel = nullptr;
    QProgressBar* mProgressBar = nullptr;
    QAction* mReloadAction = nullptr;
};
