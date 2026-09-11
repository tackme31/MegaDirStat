#include "ui/MainWindow.h"

#include "ui/LoginView.h"
#include "ui/PercentBarDelegate.h"
#include "ui/SizeTreeModel.h"
#include "ui/TreemapWidget.h"

#include <QAction>
#include <QEvent>
#include <QHeaderView>
#include <QLabel>
#include <QLocale>
#include <QMenu>
#include <QMessageBox>
#include <QProgressBar>
#include <QSplitter>
#include <QStackedWidget>
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
    mTree->setContextMenuPolicy(Qt::CustomContextMenu);
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

    mSplitter = new QSplitter(Qt::Vertical);
    mSplitter->addWidget(mTree);
    mSplitter->addWidget(mTreemap);
    mSplitter->setChildrenCollapsible(false);
    mSplitter->setStretchFactor(0, 2);
    mSplitter->setStretchFactor(1, 3);

    mLoginView = new LoginView;
    mPages = new QStackedWidget;
    mPages->addWidget(mLoginView);
    mPages->addWidget(mSplitter);
    setCentralWidget(mPages);

    mToolBar = addToolBar(tr("Main"));
    mToolBar->setMovable(false);
    mReloadAction = mToolBar->addAction(tr("Reload"), this, &MainWindow::reload);
    mReloadAction->setShortcut(QKeySequence::Refresh);
    mToolBar->addSeparator();
    mUpAction = mToolBar->addAction(tr("Up"), this, [this] { scopeUp(1); });
    mUpAction->setShortcuts({QKeySequence(Qt::ALT | Qt::Key_Up), QKeySequence(Qt::Key_Backspace)});
    mUpAction->setToolTip(tr("Show the parent folder (Alt+Up)"));
    mBreadcrumb = new QLabel;
    mBreadcrumb->setTextFormat(Qt::RichText);
    mBreadcrumb->setContentsMargins(8, 0, 8, 0);
    mToolBar->addWidget(mBreadcrumb);

    mStatusLabel = new QLabel;
    mProgressBar = new QProgressBar;
    mProgressBar->setMaximumWidth(200);
    mProgressBar->setVisible(false);
    statusBar()->addWidget(mStatusLabel, 1);
    statusBar()->addPermanentWidget(mProgressBar);

    connect(mLoginView, &LoginView::signInRequested, this, &MainWindow::onSignInRequested);
    connect(mLoginView, &LoginView::twoFactorSubmitted, this, &MainWindow::onTwoFactorSubmitted);
    connect(mLoginView, &LoginView::twoFactorCancelled, this, [this] { mLoginView->showSignIn(); });
    connect(mLoginView, &LoginView::retryRequested, this, &MainWindow::reload);
    connect(&mSource, &IAccountSource::loginFinished, this, &MainWindow::onLoginFinished);
    connect(&mSource, &IAccountSource::progress, this, &MainWindow::onProgress);
    connect(&mSource, &IAccountSource::loaded, this, &MainWindow::onLoaded);
    connect(&mSource, &IAccountSource::failed, this, &MainWindow::onFailed);
    connect(mTree->selectionModel(),
            &QItemSelectionModel::currentChanged,
            this,
            &MainWindow::onCurrentChanged);
    connect(mTreemap, &TreemapWidget::nodeClicked, this, &MainWindow::onTreemapClicked);
    connect(mTreemap, &TreemapWidget::contextMenuRequested, this, &MainWindow::showContextMenu);
    connect(mTree, &QWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        showContextMenu(mModel->nodeAt(mTree->indexAt(pos)), mTree->viewport()->mapToGlobal(pos));
    });
    connect(mBreadcrumb, &QLabel::linkActivated, this, [this](const QString& link) {
        scopeUp(link.toInt());
    });
    updateScopeBar();

    resize(1100, 800);
}

void MainWindow::start()
{
    showLoginView();
    if (mSource.requiresLogin())
    {
        mLoginView->showSignIn();
    }
    else
    {
        reload();
    }
}

void MainWindow::changeEvent(QEvent* event)
{
    // The breadcrumb's links carry the accent colour inline; redo them for a
    // light/dark switch.
    if (event->type() == QEvent::PaletteChange && mBreadcrumb)
    {
        updateScopeBar();
    }
    QMainWindow::changeEvent(event);
}

void MainWindow::showLoginView()
{
    mPages->setCurrentWidget(mLoginView);
    mToolBar->setVisible(false);
    statusBar()->setVisible(false);
}

void MainWindow::showAccountView()
{
    mLoginView->stopProgress();
    mPages->setCurrentWidget(mSplitter);
    mToolBar->setVisible(true);
    statusBar()->setVisible(true);
}

void MainWindow::reload()
{
    mReloadAction->setEnabled(false);
    if (!mHasSnapshot)
    {
        mLoginView->showProgress(tr("Loading…"), {}, 0, -1);
    }
    mSource.load();
}

void MainWindow::onSignInRequested(const QString& email, const QString& password)
{
    mCodeAttempt = false;
    mLoginView->showProgress(tr("Signing you in…"), {}, 0, -1);
    mSource.login(email, password, {});
}

void MainWindow::onTwoFactorSubmitted(const QString& code)
{
    mCodeAttempt = true;
    mLoginView->showProgress(tr("Signing you in…"), {}, 0, -1);
    mSource.login(mLoginView->email(), mLoginView->password(), code);
}

void MainWindow::onLoginFinished(IAccountSource::LoginResult result, const QString& error)
{
    switch (result)
    {
        case IAccountSource::LoginResult::Ok:
            mLoginView->clearPassword();
            reload();
            break;
        case IAccountSource::LoginResult::NeedsTwoFactor:
            mLoginView->showTwoFactor();
            break;
        case IAccountSource::LoginResult::Failed:
            if (mCodeAttempt)
            {
                mLoginView->showTwoFactor(error);
            }
            else
            {
                mLoginView->showSignIn(error);
            }
            break;
    }
}

void MainWindow::onProgress(const QString& stage, const QString& detail, qint64 done, qint64 total)
{
    if (!mHasSnapshot)
    {
        mLoginView->showProgress(stage, detail, done, total);
        return;
    }
    mStatusLabel->setText(detail.isEmpty() ? stage : tr("%1  %2").arg(stage, detail));
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
    mHasSnapshot = true;
    showAccountView();

    // Resolved before the old snapshot, which mScope points into, is let go.
    const SizeNode* scope = mScope ? findSamePath(*snapshot->root, *mScope) : nullptr;
    mModel->setSnapshot(snapshot);
    mTreemap->setSnapshot(snapshot);
    setScope(scope);

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
    if (!mHasSnapshot)
    {
        // A source that needs a login has signed itself out again (IAccountSource).
        if (mSource.requiresLogin())
        {
            mLoginView->showSignIn(error);
        }
        else
        {
            mLoginView->showLoadError(error);
        }
        return;
    }
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

void MainWindow::showContextMenu(const SizeNode* node, const QPoint& globalPos)
{
    QMenu menu(this);
    // A file (or a merged group, which stands for its folder) focuses on the
    // folder it is in.
    const SizeNode* target = node && !node->isFolder() ? node->parent : node;
    if (target && target->parent && target != mScope && target->size > 0)
    {
        QString name = target->name;
        if (name.size() > 48)
        {
            name = name.left(47) + QChar(0x2026);
        }
        name.replace(QLatin1Char('&'), QLatin1String("&&")); // not a mnemonic
        menu.addAction(tr("Focus on “%1”").arg(name), this, [this, target] { setScope(target); });
    }
    if (mScope)
    {
        if (!menu.isEmpty())
        {
            menu.addSeparator();
        }
        menu.addAction(mUpAction);
        menu.addAction(tr("Show Whole Account"), this, [this] { setScope(nullptr, mScope); });
    }
    if (!menu.isEmpty())
    {
        menu.exec(globalPos);
    }
}

void MainWindow::setScope(const SizeNode* scope, const SizeNode* select)
{
    if (scope && !scope->parent)
    {
        scope = nullptr;
    }
    mScope = scope;
    mModel->setScope(scope);
    mTreemap->setScope(scope);
    if (scope)
    {
        mTree->expand(mModel->index(0, 0));
    }
    updateScopeBar();
    if (select)
    {
        onTreemapClicked(select);
    }
    else
    {
        // A model reset keeps the old scroll offset, which would hide the new top row.
        mTree->scrollToTop();
    }
}

void MainWindow::scopeUp(int levels)
{
    if (!mScope)
    {
        return;
    }
    const SizeNode* scope = mScope;
    for (int i = 0; i < levels && scope->parent; ++i)
    {
        scope = scope->parent;
    }
    setScope(scope, mScope);
}

void MainWindow::updateScopeBar()
{
    mUpAction->setEnabled(mScope != nullptr);
    if (!mScope)
    {
        mBreadcrumb->clear();
        return;
    }
    // Each link's href is how many levels up it goes; "All" goes to the root.
    // QPalette::Link is near the text colour in the Windows 11 style, so the
    // accent colour marks what is clickable.
    const QString link =
        QStringLiteral("<a href=\"%1\" style=\"text-decoration:none; color:%3\">%2</a>");
    const QString accent = palette().color(QPalette::Highlight).name();
    QStringList parts;
    int levels = 0;
    for (const SizeNode* n = mScope; n->parent; n = n->parent, ++levels)
    {
        const QString name = n->name.toHtmlEscaped();
        parts.prepend(levels == 0 ? QStringLiteral("<b>%1</b>").arg(name)
                                  : link.arg(QString::number(levels), name, accent));
    }
    parts.prepend(link.arg(QString::number(levels), tr("All").toHtmlEscaped(), accent));
    mBreadcrumb->setText(parts.join(QStringLiteral(" &rsaquo; ")));
}
