#include "Automation.h"
#include "MainWindow.h"
#include "SingleInstance.h"

#include "core/AppInfo.h"
#include "core/AppSettings.h"
#include "core/ModuleRegistry.h"
#include "core/ThemeManager.h"
#include "modules/writer/WriterModule.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QLibraryInfo>
#include <QLocale>
#include <QStyleFactory>
#include <QTimer>
#include <QTranslator>

#include <memory>

namespace {

void registerModules()
{
    ModuleRegistry &registry = ModuleRegistry::instance();
    registry.add(std::make_unique<WriterModule>());
    for (Module *module : registry.modules())
        module->registerCommands();
}

void installTranslations(QApplication &app)
{
    const QLocale turkish(QLocale::Turkish, QLocale::Turkey);
    const QString dir = QLibraryInfo::path(QLibraryInfo::TranslationsPath);
    for (const QString &catalog : {QStringLiteral("qt"), QStringLiteral("qtbase")}) {
        auto *translator = new QTranslator(&app);
        if (translator->load(turkish, catalog, QStringLiteral("_"), dir)) {
            app.installTranslator(translator);
            return;
        }
        delete translator;
    }
}

} // namespace

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setOrganizationName(AppInfo::organizationName());
    QApplication::setApplicationName(AppInfo::applicationName());
    QApplication::setApplicationVersion(AppInfo::version());
    QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    QLocale::setDefault(QLocale(QLocale::Turkish, QLocale::Turkey));
    installTranslations(app);

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("OmniOffice"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument(QStringLiteral("dosyalar"), QStringLiteral("Açılacak dosyalar"), QStringLiteral("[dosyalar...]"));
    QCommandLineOption newInstance(QStringLiteral("new-instance"), QStringLiteral("Yeni bir pencere/işlem başlat"));
    parser.addOption(newInstance);
    QCommandLineOption script(QStringLiteral("script"), QStringLiteral("Geliştirici otomasyon betiği (bkz. Automation.h)"),
                              QStringLiteral("adımlar"));
    parser.addOption(script);
    parser.process(app);
    const bool automated = parser.isSet(script);
    const QStringList files = parser.positionalArguments();

    SingleInstance instance;
    if (!automated && !parser.isSet(newInstance) && instance.forwardToRunningInstance(files))
        return 0;
    if (!automated)
        instance.listen();

    ThemeManager::instance().initialize();
    registerModules();
    MainWindow::declareCommands();

    MainWindow window;
    QObject::connect(&instance, &SingleInstance::filesReceived, &window, [&window](const QStringList &received) {
        window.bringToFront();
        window.openFiles(received);
    });
    window.show();

    if (!files.isEmpty()) {
        window.openFiles(files);
    } else {
        window.restoreSession();
        if (window.editors().isEmpty() || AppSettings::instance().showStartPage())
            window.showStartPage();
    }
    if (automated) {
        auto *automation = new Automation(&window, parser.value(script), &window);
        automation->start();
    } else {
        QTimer::singleShot(0, &window, [&window] { window.checkRecovery(); });
    }
    return app.exec();
}
