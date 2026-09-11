#pragma once

#include "core/IAccountSource.h"

#include <QMainWindow>

class LoginView;
class QLabel;
class QProgressBar;
class QSplitter;
class QStackedWidget;
class QToolBar;
class QTreeView;
class SizeTreeModel;
class TreemapWidget;

// Tree on top, treemap below, split by a QSplitter (docs/DESIGN.md §2). Until
// the first snapshot arrives, LoginView takes the whole window instead.
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(IAccountSource& source, QWidget* parent = nullptr);

    // Shows the sign-in page if the source needs it, otherwise starts loading.
    void start();

protected:
    void changeEvent(QEvent* event) override;

private:
    void showLoginView();
    void showAccountView();
    void reload();
    void onSignInRequested(const QString& email, const QString& password);
    void onTwoFactorSubmitted(const QString& code);
    void onLoginFinished(IAccountSource::LoginResult result, const QString& error);
    void onProgress(const QString& stage, const QString& detail, qint64 done, qint64 total);
    void onLoaded(SnapshotPtr snapshot);
    void onFailed(const QString& error);
    void onCurrentChanged(const QModelIndex& current);
    void onTreemapClicked(const SizeNode* node);
    void showContextMenu(const SizeNode* node, const QPoint& globalPos);
    // scope: nullptr for the whole account. select (optional) is selected
    // afterwards, e.g. the folder just left when going up.
    void setScope(const SizeNode* scope, const SizeNode* select = nullptr);
    void scopeUp(int levels);
    void updateScopeBar();

    IAccountSource& mSource;
    bool mHasSnapshot = false;
    bool mCodeAttempt = false; // the login in flight carries a two-factor code
    // Points into the snapshot the model and treemap hold.
    const SizeNode* mScope = nullptr;

    QStackedWidget* mPages = nullptr;
    LoginView* mLoginView = nullptr;
    QSplitter* mSplitter = nullptr;
    SizeTreeModel* mModel = nullptr;
    QTreeView* mTree = nullptr;
    TreemapWidget* mTreemap = nullptr;
    QToolBar* mToolBar = nullptr;
    QLabel* mStatusLabel = nullptr;
    QProgressBar* mProgressBar = nullptr;
    QAction* mReloadAction = nullptr;
    QAction* mUpAction = nullptr;
    QLabel* mBreadcrumb = nullptr;
};
