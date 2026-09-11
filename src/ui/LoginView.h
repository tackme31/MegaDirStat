#pragma once

#include <QList>
#include <QWidget>

class QLabel;
class QLineEdit;
class QProgressBar;
class QPushButton;
class QStackedWidget;

// Everything the window shows before the first snapshot: the sign-in form, the
// two-factor code form, the loading progress, and a load error. One centred
// column whose pages share a height, so switching between them does not make
// the form jump. Holds no state beyond its fields; MainWindow drives it.
class LoginView : public QWidget
{
    Q_OBJECT

public:
    explicit LoginView(QWidget* parent = nullptr);

    void showSignIn(const QString& error = {});
    void showTwoFactor(const QString& error = {});
    // total -1 = unknown (busy bar); detail may be empty.
    void showProgress(const QString& stage, const QString& detail, qint64 done, qint64 total);
    // For sources without a login: the load failed and can only be retried.
    void showLoadError(const QString& error);

    QString email() const;
    QString password() const;
    void clearPassword();

    // Leaves the progress page's busy animation stopped once the view is hidden.
    void stopProgress();

signals:
    void signInRequested(const QString& email, const QString& password);
    void twoFactorSubmitted(const QString& code);
    void twoFactorCancelled();
    void retryRequested();

protected:
    void changeEvent(QEvent* event) override;

private:
    QWidget* buildSignInPage();
    QWidget* buildTwoFactorPage();
    QWidget* buildProgressPage();
    QWidget* buildErrorPage();
    void updateButtons();
    void updateColors();
    void submitSignIn();
    void submitCode();

    QStackedWidget* mPages = nullptr;

    QWidget* mSignInPage = nullptr;
    QLineEdit* mEmail = nullptr;
    QLineEdit* mPassword = nullptr;
    QLabel* mSignInError = nullptr;
    QPushButton* mSignInButton = nullptr;

    QWidget* mTwoFactorPage = nullptr;
    QLineEdit* mCode = nullptr;
    QLabel* mCodeError = nullptr;
    QPushButton* mConfirmButton = nullptr;

    QWidget* mProgressPage = nullptr;
    QLabel* mStage = nullptr;
    QProgressBar* mBar = nullptr;
    QLabel* mDetail = nullptr;

    QWidget* mErrorPage = nullptr;
    QLabel* mLoadError = nullptr;

    QList<QLabel*> mCaptions; // secondary text, recoloured with the palette
    QList<QLabel*> mErrors;
};
