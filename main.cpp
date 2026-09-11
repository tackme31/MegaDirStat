#include "mock/MockAccountSource.h"
#include "ui/MainWindow.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QMessageBox>
#include <QRegularExpression>
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
    const QCommandLineOption sizeOption(QStringLiteral("window-size"),
                                        QStringLiteral("Initial window size, e.g. 1200x800."),
                                        QStringLiteral("WxH"));
    parser.addOptions(
        {mockOption, generateOption, seedOption, delayOption, failOption, sizeOption});
    parser.process(app);

    const bool mock = parser.isSet(mockOption) || parser.isSet(generateOption) ||
                      parser.isSet(delayOption) || parser.isSet(failOption);
    if (!mock)
    {
        QMessageBox::information(nullptr,
                                 QStringLiteral("MegaDirStat"),
                                 QStringLiteral("Signing in to MEGA is not implemented yet.\n"
                                                "Run with --mock <file> or --mock-generate <count>."));
        return 1;
    }

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
    auto source = std::make_unique<MockAccountSource>(options);

    MainWindow window(*source);
    const QRegularExpressionMatch size =
        QRegularExpression(QStringLiteral("^(\\d+)[xX](\\d+)$")).match(parser.value(sizeOption));
    if (size.hasMatch())
    {
        window.resize(size.captured(1).toInt(), size.captured(2).toInt());
    }
    window.show();
    window.start();
    return app.exec();
}
