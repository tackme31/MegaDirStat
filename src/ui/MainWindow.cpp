#include "ui/MainWindow.h"

#include "ui/PercentBarDelegate.h"
#include "ui/SizeTreeModel.h"
#include "ui/TreemapWidget.h"

#include <QAction>
#include <QCloseEvent>
#include <QHeaderView>
#include <QLabel>
#include <QLocale>
#include <QMessageBox>
#include <QProgressBar>
#include <QSplitter>
#include <QStatusBar>
#include <QToolBar>
#include <QTreeView>

MainWindow::MainWindow(IAccountSource& source, QWidget* parent)
    : QMainWindow(parent), mSource(source)
{
    setWindowTitle(QStringLiteral("MegaDirStat"));

    mModel = new SizeTreeModel(this);

    mTree = new QTreeView;
    mTree->setModel(mModel);
    mTree->setUniformRowHeights(true);
    mTree->setAlternatingRowColors(true);
    mTree->setSelectionMode(QAbstractItemView::SingleSelection);
    mTree->setItemDelegateForColumn(SizeTreeModel::PercentColumn, new PercentBarDelegate(mTree));
    mTree->header()->setStretchLastSection(false);
    mTree->header()->setSectionResizeMode(SizeTreeModel::NameColumn, QHeaderView::Stretch);
    for (int column = SizeTreeModel::PercentColumn; column < SizeTreeModel::ColumnCount; ++column)
    {
        mTree->header()->setSectionResizeMode(column, QHeaderView::Interactive);
    }
    mTree->header()->resizeSection(SizeTreeModel::PercentColumn, 140);
    mTree->header()->resizeSection(SizeTreeModel::SizeColumn, 100);
    mTree->header()->resizeSection(SizeTreeModel::FilesColumn, 90);
    mTree->header()->resizeSection(SizeTreeModel::ModifiedColumn, 140);

    mTreemap = new TreemapWidget;

    auto* splitter = new QSplitter(Qt::Vertical);
    splitter->addWidget(mTree);
    splitter->addWidget(mTreemap);
    splitter->setChildrenCollapsible(false);
    splitter->setStretchFactor(0, 2);
    splitter->setStretchFactor(1, 3);
    setCentralWidget(splitter);

    auto* toolbar = addToolBar(tr("Main"));
    toolbar->setMovable(false);
    mReloadAction = toolbar->addAction(tr("Reload"), this, &MainWindow::reload);
    mReloadAction->setShortcut(QKeySequence::Refresh);

    mStatusLabel = new QLabel;
    mProgressBar = new QProgressBar;
    mProgressBar->setMaximumWidth(200);
    mProgressBar->setVisible(false);
    statusBar()->addWidget(mStatusLabel, 1);
    statusBar()->addPermanentWidget(mProgressBar);

    connect(&mSource, &IAccountSource::progress, this, &MainWindow::onProgress);
    connect(&mSource, &IAccountSource::loaded, this, &MainWindow::onLoaded);
    connect(&mSource, &IAccountSource::failed, this, &MainWindow::onFailed);
    connect(mTree->selectionModel(),
            &QItemSelectionModel::currentChanged,
            this,
            &MainWindow::onCurrentChanged);
    connect(mTreemap, &TreemapWidget::nodeClicked, this, &MainWindow::onTreemapClicked);

    resize(1100, 800);
}

void MainWindow::start()
{
    reload();
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    mSource.logout();
    event->accept();
}

void MainWindow::reload()
{
    mReloadAction->setEnabled(false);
    mSource.load();
}

void MainWindow::onProgress(const QString& stage, qint64 done, qint64 total)
{
    mStatusLabel->setText(stage);
    mProgressBar->setVisible(true);
    if (total > 0)
    {
        // QProgressBar is int-based; scale so byte counts cannot overflow it.
        mProgressBar->setRange(0, 1000);
        mProgressBar->setValue(static_cast<int>(done * 1000 / total));
    }
    else
    {
        mProgressBar->setRange(0, 0);
    }
}

void MainWindow::onLoaded(SnapshotPtr snapshot)
{
    mProgressBar->setVisible(false);
    mReloadAction->setEnabled(true);

    mModel->setSnapshot(snapshot);
    mTreemap->setSnapshot(snapshot);

    const QLocale locale;
    QString status = tr("%1 in %2 files")
                         .arg(locale.formattedDataSize(snapshot->root->size),
                              locale.toString(snapshot->root->fileCount));
    if (snapshot->usage.used >= 0 && snapshot->usage.total > 0)
    {
        status += tr("  |  Storage used: %1 of %2")
                      .arg(locale.formattedDataSize(snapshot->usage.used),
                           locale.formattedDataSize(snapshot->usage.total));
    }
    mStatusLabel->setText(status);
}

void MainWindow::onFailed(const QString& error)
{
    mProgressBar->setVisible(false);
    mReloadAction->setEnabled(true);
    mStatusLabel->setText(tr("Loading failed"));
    QMessageBox::critical(this, tr("MegaDirStat"), tr("Could not load the account:\n%1").arg(error));
}

void MainWindow::onCurrentChanged(const QModelIndex& current)
{
    mTreemap->setSelectedNode(mModel->nodeAt(current));
}

void MainWindow::onTreemapClicked(const SizeNode* node)
{
    const QModelIndex index = mModel->indexFor(node);
    if (!index.isValid())
    {
        return;
    }
    // scrollTo expands the collapsed ancestors on the way.
    mTree->setCurrentIndex(index);
    mTree->scrollTo(index, QAbstractItemView::PositionAtCenter);
}
