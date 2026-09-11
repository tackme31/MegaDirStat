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

    IAccountSource& mSource;
    bool mHasSnapshot = false;
    bool mCodeAttempt = false; // the login in flight carries a two-factor code

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
};
