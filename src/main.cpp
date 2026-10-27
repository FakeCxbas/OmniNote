#include <QApplication>
#include <QLoggingCategory>
#include <QSurfaceFormat>
#include "app/MainWindow.hpp"
#include "input/TabletEventFilter.hpp"

int main(int argc, char *argv[]) {
    // Silence benign QtWayland textinput focus warnings when switching between canvas and GL viewport
    QLoggingCategory::setFilterRules(QStringLiteral("qt.qpa.wayland.textinput*=false"));

    // Configure default OpenGL surface format for buttery-smooth antialiasing
    QSurfaceFormat format;
    format.setSamples(4); // 4x Multisampling
    format.setSwapInterval(1); // VSync enabled
    QSurfaceFormat::setDefaultFormat(format);

    QApplication app(argc, argv);
    app.setApplicationName("OmniNote");
    app.setApplicationDisplayName("OmniNote STEM");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("OmniNote");
    app.setOrganizationDomain("omninote.org");
    app.setDesktopFileName("org.omninote.OmniNote");
    app.setWindowIcon(QIcon(":/icons/app_icon.svg"));

    // Install global tablet event filter for low-latency Wacom tablet tracking
    auto *tabletFilter = new OmniNote::TabletEventFilter(&app);
    app.installEventFilter(tabletFilter);

    OmniNote::MainWindow mainWindow;
    mainWindow.show();

    if (app.arguments().contains("--screenshot")) {
        int idx = app.arguments().indexOf("--screenshot");
        QString path = (idx + 1 < app.arguments().size()) ? app.arguments().at(idx + 1) : "omninote_preview.png";
        mainWindow.resize(1280, 800);
        mainWindow.show();
        QApplication::processEvents();
        QPixmap shot = mainWindow.grab();
        shot.save(path);
        return 0;
    }

    return app.exec();
}

