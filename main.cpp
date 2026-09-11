#include "core/RunCacheDir.h"
#include "mega/MegaAccountSource.h"
#include "mock/MockAccountSource.h"
#include "ui/MainWindow.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QIcon>
#include <QMessageBox>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QStyleHints>

#include <memory>

namespace
{

// Lets the light/dark look be checked without flipping the OS theme.
void applyColorSchemeOverride()
{
    const QByteArray scheme = qgetenv("MEGADIRSTAT_COLOR_SCHEME");
    if (scheme == "dark")
    {
        QGuiApplication::styleHints()->setColorScheme(Qt::ColorScheme::Dark);
    }
    else if (scheme == "light")
    {
        QGuiApplication::styleHints()->setColorScheme(Qt::ColorScheme::Light);
    }
}

} // namespace

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("MegaDirStat"));
    QApplication::setApplicationVersion(QStringLiteral(MEGADIRSTAT_VERSION));
    applyColorSchemeOverride();

    // Windows asks for a small and a large icon separately (title bar vs.
    // taskbar), scaled by the monitor's DPI, so one pixmap would be resampled
    // for most of them. resources/appicon.rc covers what Explorer and a pinned
    // taskbar entry show before the process exists.
    {
        QIcon windowIcon;
        for (int size : {16, 24, 32, 48, 64, 256})
        {
            windowIcon.addFile(QStringLiteral(":/resources/appicon-%1.png").arg(size));
        }
        QApplication::setWindowIcon(windowIcon);
    }

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Disk usage treemap for MEGA cloud storage."));
    parser.addHelpOption();
    parser.addVersionOption();
    const QCommandLineOption mockOption(QStringLiteral("mock"),
                                        QStringLiteral("Load a JSON fixture instead of MEGA."),
                                        QStringLiteral("file"));
    const QCommandLineOption generateOption(
        QStringLiteral("mock-generate"),
        QStringLiteral("Generate a mock account with about <count> files instead of MEGA."),
        QStringLiteral("count"));
    const QCommandLineOption seedOption(QStringLiteral("seed"),
                                        QStringLiteral("Seed for --mock-generate (default 1)."),
                                        QStringLiteral("n"),
                                        QStringLiteral("1"));
    const QCommandLineOption delayOption(QStringLiteral("mock-delay"),
                                         QStringLiteral("Simulated mock load time."),
                                         QStringLiteral("ms"));
    const QCommandLineOption failOption(QStringLiteral("mock-fail"),
                                        QStringLiteral("Make the mock load fail."));
    const QCommandLineOption loginOption(
        QStringLiteral("mock-login"),
        QStringLiteral("Show the sign-in page first (any credentials; password \"wrong\" fails)."));
    const QCommandLineOption twoFactorOption(
        QStringLiteral("mock-2fa"),
        QStringLiteral("Like --mock-login, then ask for a two-factor code (123456)."));
    const QCommandLineOption sizeOption(QStringLiteral("window-size"),
                                        QStringLiteral("Initial window size, e.g. 1200x800."),
                                        QStringLiteral("WxH"));
    parser.addOptions({mockOption,
                       generateOption,
                       seedOption,
                       delayOption,
                       failOption,
                       loginOption,
                       twoFactorOption,
                       sizeOption});
    parser.process(app);

    const bool mock = parser.isSet(mockOption) || parser.isSet(generateOption) ||
                      parser.isSet(delayOption) || parser.isSet(failOption) ||
                      parser.isSet(loginOption) || parser.isSet(twoFactorOption);

    std::unique_ptr<IAccountSource> source;
    if (mock)
    {
        MockOptions options;
        options.fixturePath = parser.value(mockOption);
        options.generateCount = parser.value(generateOption).toLongLong();
        if (options.fixturePath.isEmpty() && options.generateCount <= 0)
        {
            options.generateCount = 10000;
        }
        options.seed = parser.value(seedOption).toULongLong();
        options.delayMs = parser.value(delayOption).toInt();
        options.fail = parser.isSet(failOption);
        options.requireLogin = parser.isSet(loginOption);
        options.twoFactor = parser.isSet(twoFactorOption);
        source = std::make_unique<MockAccountSource>(options);
    }
    else
    {
        // AppLocalDataLocation: on Windows AppDataLocation is the roaming profile.
        const QString cacheBase = QDir(QStandardPaths::writableLocation(
                                           QStandardPaths::AppLocalDataLocation))
                                      .filePath(QStringLiteral("sdk-cache"));
        QString error;
        auto cacheDir = RunCacheDir::create(cacheBase, &error);
        if (!cacheDir)
        {
            QMessageBox::critical(nullptr,
                                  QStringLiteral("MegaDirStat"),
                                  QObject::tr("Could not create the cache directory:\n%1").arg(error));
            return 1;
        }
        source = std::make_unique<MegaAccountSource>(
            std::move(cacheDir), QStringLiteral("MegaDirStat/" MEGADIRSTAT_VERSION));
    }

    MainWindow window(*source);
    const QRegularExpressionMatch size =
        QRegularExpression(QStringLiteral("^(\\d+)[xX](\\d+)$")).match(parser.value(sizeOption));
    if (size.hasMatch())
    {
        window.resize(size.captured(1).toInt(), size.captured(2).toInt());
    }
    window.show();
    window.start();
    const int status = app.exec();
    // After the window has gone, so a slow server-side logout is not a frozen window.
    source->logout();
    return status;
}
