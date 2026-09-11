#include "ui/LoginView.h"

#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpressionValidator>
#include <QStackedWidget>
#include <QVBoxLayout>

#include <algorithm>

namespace
{

constexpr int kColumnWidth = 320;
constexpr int kCodeLength = 6;
constexpr int kControlHeight = 32;

QLabel* makeTitle(const QString& text)
{
    auto* label = new QLabel(text);
    QFont font = label->font();
    font.setPointSizeF(font.pointSizeF() * 1.5);
    font.setWeight(QFont::DemiBold);
    label->setFont(font);
    label->setAlignment(Qt::AlignHCenter);
    label->setWordWrap(true);
    return label;
}

QLabel* makeText(const QString& text = {})
{
    auto* label = new QLabel(text);
    label->setAlignment(Qt::AlignHCenter);
    label->setWordWrap(true);
    return label;
}

// Every page adds stretch above and below its content: QStackedWidget gives each
// page the tallest page's height, and the content should sit in its middle.
QVBoxLayout* pageLayout(QWidget* page)
{
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);
    return layout;
}

bool isDark(const QPalette& palette)
{
    return palette.color(QPalette::Window).lightness() < 128;
}

} // namespace

LoginView::LoginView(QWidget* parent) : QWidget(parent)
{
    mPages = new QStackedWidget;
    mPages->setFixedWidth(kColumnWidth);
    mSignInPage = buildSignInPage();
    mTwoFactorPage = buildTwoFactorPage();
    mProgressPage = buildProgressPage();
    mErrorPage = buildErrorPage();
    for (QWidget* page : {mSignInPage, mTwoFactorPage, mProgressPage, mErrorPage})
    {
        mPages->addWidget(page);
    }

    auto* row = new QHBoxLayout;
    row->addStretch();
    row->addWidget(mPages);
    row->addStretch();
    auto* outer = new QVBoxLayout(this);
    outer->addStretch(2);
    outer->addLayout(row);
    outer->addStretch(3); // a little above centre reads as centred

    // The style's default control height reads cramped on a page this sparse.
    for (QWidget* control : findChildren<QWidget*>())
    {
        if (qobject_cast<QLineEdit*>(control) || qobject_cast<QPushButton*>(control))
        {
            control->setMinimumHeight(kControlHeight);
        }
    }

    updateColors();
    updateButtons();
}

QWidget* LoginView::buildSignInPage()
{
    auto* page = new QWidget;
    auto* layout = pageLayout(page);
    layout->addStretch();
    layout->addWidget(makeTitle(tr("Sign in to MEGA")));
    layout->addSpacing(8);

    mEmail = new QLineEdit;
    mEmail->setPlaceholderText(tr("Email"));
    mEmail->setInputMethodHints(Qt::ImhEmailCharactersOnly | Qt::ImhNoAutoUppercase);
    layout->addWidget(mEmail);

    mPassword = new QLineEdit;
    mPassword->setPlaceholderText(tr("Password"));
    mPassword->setEchoMode(QLineEdit::Password);
    layout->addWidget(mPassword);

    mSignInError = makeText();
    mSignInError->setVisible(false);
    mErrors.append(mSignInError);
    layout->addWidget(mSignInError);

    mSignInButton = new QPushButton(tr("Sign in"));
    mSignInButton->setDefault(true);
    layout->addWidget(mSignInButton);

    layout->addSpacing(8);
    auto* note = makeText(tr("Your session is not saved. You will sign in again next time."));
    mCaptions.append(note);
    layout->addWidget(note);
    layout->addStretch();

    connect(mEmail, &QLineEdit::textChanged, this, &LoginView::updateButtons);
    connect(mPassword, &QLineEdit::textChanged, this, &LoginView::updateButtons);
    connect(mEmail, &QLineEdit::returnPressed, mPassword, qOverload<>(&QWidget::setFocus));
    connect(mPassword, &QLineEdit::returnPressed, this, &LoginView::submitSignIn);
    connect(mSignInButton, &QPushButton::clicked, this, &LoginView::submitSignIn);
    return page;
}

QWidget* LoginView::buildTwoFactorPage()
{
    auto* page = new QWidget;
    auto* layout = pageLayout(page);
    layout->addStretch();
    layout->addWidget(makeTitle(tr("Two-factor authentication")));
    layout->addWidget(makeText(tr("Enter the 6-digit code from your authenticator app.")));
    layout->addSpacing(8);

    mCode = new QLineEdit;
    mCode->setAlignment(Qt::AlignHCenter);
    mCode->setMaxLength(kCodeLength);
    mCode->setPlaceholderText(QStringLiteral("000000"));
    mCode->setInputMethodHints(Qt::ImhDigitsOnly);
    mCode->setValidator(new QRegularExpressionValidator(
        QRegularExpression(QStringLiteral("[0-9]{0,%1}").arg(kCodeLength)), mCode));
    QFont codeFont = mCode->font();
    codeFont.setPointSizeF(codeFont.pointSizeF() * 1.6);
    codeFont.setLetterSpacing(QFont::AbsoluteSpacing, 6);
    mCode->setFont(codeFont);
    layout->addWidget(mCode);

    mCodeError = makeText();
    mCodeError->setVisible(false);
    mErrors.append(mCodeError);
    layout->addWidget(mCodeError);

    auto* buttons = new QHBoxLayout;
    auto* back = new QPushButton(tr("Back"));
    mConfirmButton = new QPushButton(tr("Confirm"));
    mConfirmButton->setDefault(true);
    buttons->addWidget(back);
    buttons->addWidget(mConfirmButton);
    layout->addLayout(buttons);
    layout->addStretch();

    connect(mCode, &QLineEdit::textChanged, this, &LoginView::updateButtons);
    connect(mCode, &QLineEdit::returnPressed, this, &LoginView::submitCode);
    connect(mConfirmButton, &QPushButton::clicked, this, &LoginView::submitCode);
    connect(back, &QPushButton::clicked, this, &LoginView::twoFactorCancelled);
    return page;
}

QWidget* LoginView::buildProgressPage()
{
    auto* page = new QWidget;
    auto* layout = pageLayout(page);
    layout->addStretch();
    mStage = makeText();
    layout->addWidget(mStage);

    mBar = new QProgressBar;
    mBar->setTextVisible(false);
    mBar->setRange(0, 1);
    layout->addWidget(mBar);

    mDetail = makeText();
    mCaptions.append(mDetail);
    layout->addWidget(mDetail);
    layout->addStretch();
    return page;
}

QWidget* LoginView::buildErrorPage()
{
    auto* page = new QWidget;
    auto* layout = pageLayout(page);
    layout->addStretch();
    layout->addWidget(makeTitle(tr("Couldn't load the account")));
    mLoadError = makeText();
    mErrors.append(mLoadError);
    layout->addWidget(mLoadError);
    auto* retry = new QPushButton(tr("Try again"));
    layout->addWidget(retry);
    layout->addStretch();
    connect(retry, &QPushButton::clicked, this, &LoginView::retryRequested);
    return page;
}

void LoginView::showSignIn(const QString& error)
{
    stopProgress();
    mSignInError->setText(error);
    mSignInError->setVisible(!error.isEmpty());
    mPages->setCurrentWidget(mSignInPage);
    if (mEmail->text().isEmpty())
    {
        mEmail->setFocus();
    }
    else
    {
        mPassword->setFocus();
        mPassword->selectAll();
    }
}

void LoginView::showTwoFactor(const QString& error)
{
    stopProgress();
    mCodeError->setText(error);
    mCodeError->setVisible(!error.isEmpty());
    mCode->clear();
    mPages->setCurrentWidget(mTwoFactorPage);
    mCode->setFocus();
}

void LoginView::showProgress(const QString& stage, const QString& detail, qint64 done, qint64 total)
{
    mStage->setText(stage);
    mDetail->setText(detail);
    mDetail->setVisible(!detail.isEmpty());
    if (total > 0)
    {
        // QProgressBar is int-based; scale so byte counts cannot overflow it.
        mBar->setRange(0, 1000);
        mBar->setValue(static_cast<int>(std::clamp<qint64>(done * 1000 / total, 0, 1000)));
    }
    else
    {
        mBar->setRange(0, 0);
    }
    mPages->setCurrentWidget(mProgressPage);
}

void LoginView::showLoadError(const QString& error)
{
    stopProgress();
    mLoadError->setText(error);
    mPages->setCurrentWidget(mErrorPage);
}

QString LoginView::email() const
{
    return mEmail->text().trimmed();
}

QString LoginView::password() const
{
    return mPassword->text();
}

void LoginView::clearPassword()
{
    mPassword->clear();
    mCode->clear();
}

void LoginView::stopProgress()
{
    mBar->setRange(0, 1);
    mBar->setValue(0);
}

void LoginView::changeEvent(QEvent* event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::PaletteChange)
    {
        updateColors();
    }
}

void LoginView::updateButtons()
{
    mSignInButton->setEnabled(!email().isEmpty() && !mPassword->text().isEmpty());
    mConfirmButton->setEnabled(mCode->text().size() == kCodeLength);
}

void LoginView::updateColors()
{
    const QPalette& base = palette();
    QPalette caption = base;
    caption.setColor(QPalette::WindowText, base.color(QPalette::PlaceholderText));
    for (QLabel* label : std::as_const(mCaptions))
    {
        label->setPalette(caption);
    }
    // Windows 11's error red, and a lighter one that keeps its contrast on dark.
    QPalette error = base;
    error.setColor(QPalette::WindowText,
                   isDark(base) ? QColor(0xff, 0x99, 0xa4) : QColor(0xc4, 0x2b, 0x1c));
    for (QLabel* label : std::as_const(mErrors))
    {
        label->setPalette(error);
    }
}

void LoginView::submitSignIn()
{
    if (mSignInButton->isEnabled())
    {
        emit signInRequested(email(), mPassword->text());
    }
}

void LoginView::submitCode()
{
    if (mConfirmButton->isEnabled())
    {
        emit twoFactorSubmitted(mCode->text());
    }
}
