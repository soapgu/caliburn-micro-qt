#include "AppBootstrapper.h"
#include <CaliburnMicroQt/ViewLocator.h>
#include <QGuiApplication>
#include <QQmlExtensionPlugin>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTimer>
#include <QtTest>

Q_IMPORT_QML_PLUGIN(CaliburnMicroQtPlugin)
Q_IMPORT_QML_PLUGIN(CaliburnMultiScreenModulePlugin)

using namespace MultiScreen;
static QQuickWindow *findWindow(bool display)
{
    for (auto *candidate : QGuiApplication::allWindows()) {
        auto *window = qobject_cast<QQuickWindow *>(candidate);
        if (!window) continue;
        auto *model = window->property("viewModel").value<QObject *>();
        if (display ? bool(qobject_cast<DisplayViewModel *>(model)) : bool(qobject_cast<ShellViewModel *>(model)))
            return window;
    }
    return nullptr;
}
class MultiScreenTests : public QObject
{
    Q_OBJECT
private slots:
    void layoutDecisions() {
        QList<ScreenMetrics> screens;
        for (int i=0; i<4; ++i) screens.append({QRect(i*1920,0,1920,1080), QRect(i*1920,0,1920,1080), 1});
        auto four = selectLayout("xcb", screens, true);
        QVERIFY(four && four->fourScreen);
        QCOMPARE(four->mainGeometry, QRect(240,135,1440,810));
        QCOMPARE(four->displayGeometry, QRect(1920,0,5760,1080));
        QVERIFY(!selectLayout("cocoa", screens, true));
        QVERIFY(!selectLayout("xcb", {}, false));
        screens[2].dpr = 2;
        QVERIFY(!selectLayout("xcb", screens, true));
        QVERIFY(!selectLayout("xcb", screens, false)->fourScreen);
        screens[2].dpr = 1; screens[2].geometry.translate(1, 0);
        QVERIFY(!selectLayout("xcb", screens, true));
        const QRect available(-1280, 25, 1280, 695);
        auto preview = selectLayout("cocoa", {{QRect(-1280,0,1280,720),available,2}}, false);
        QVERIFY(preview && !preview->fourScreen);
        QVERIFY(available.contains(preview->mainGeometry));
        QVERIFY(available.contains(preview->displayGeometry));
    }
    void autoCloseParsing() {
        for (const auto &value : {"", "abc", "2", "86401", "3.5", "-3", "999999999999"})
            QVERIFY(!parseAutoClose(value));
        QCOMPARE(*parseAutoClose("3"), 3);
        QCOMPARE(*parseAutoClose("86400"), 86400);
    }
    void windowsAndExit_data() {
        QTest::addColumn<QString>("action");
        for (auto *action : {"auxiliary", "button", "primary", "early", "auto", "destroy"})
            QTest::newRow(action) << QString(action);
    }
    void windowsAndExit() {
        QFETCH(QString, action);
        auto *app = qobject_cast<QGuiApplication *>(QCoreApplication::instance());
        const bool previous = app->quitOnLastWindowClosed();
        const ScreenLayout layout{false, QRect(30,30,500,350), QRect(70,90,600,180)};
        AppBootstrapper bootstrapper(*app, layout, action == "auto" ? 3 : 0, false);
        QPointer<QQuickWindow> main, display;
        int displayClosures = 0;
        bool exercised = false;
        QTimer watchdog; watchdog.setSingleShot(true);
        connect(&watchdog, &QTimer::timeout, this, [] { QCoreApplication::exit(99); });
        watchdog.start(6000);
        connect(&bootstrapper, &AppBootstrapper::windowsReady, this, [&] {
            main = findWindow(false); display = findWindow(true);
            QVERIFY(main && display);
            QVERIFY(!display->parent() && !display->transientParent());
            auto *shell = qobject_cast<ShellViewModel *>(main->property("viewModel").value<QObject *>());
            auto *displayModel = qobject_cast<DisplayViewModel *>(display->property("viewModel").value<QObject *>());
            QVERIFY(shell && displayModel);
            connect(displayModel, &ScreenViewModel::deactivated, this, [&](bool close) { if (close) ++displayClosures; });
            if (action == "early") { exercised = true; shell->exitApplication(); return; }
            QTimer::singleShot(0, &bootstrapper, [&, shell] {
                QTest::keyClick(main, Qt::Key_Escape);
                QTest::keyClick(display, Qt::Key_Escape);
                QVERIFY(main->isVisible() && display->isVisible());
                exercised = true;
                if (action == "auxiliary") {
                    display->close();
                    QTimer::singleShot(100, &bootstrapper, [&, shell] {
                        QVERIFY(!display);
                        QVERIFY(main && main->isVisible() && shell->isActive());
                        QCOMPARE(displayClosures, 1);
                        shell->exitApplication();
                    });
                } else if (action == "button") {
                    auto *button = main->findChild<QQuickItem *>("exitButton");
                    QVERIFY(button);
                    const QPoint position = button->mapToScene(QPointF(button->width()/2,button->height()/2)).toPoint();
                    QTest::mouseClick(main, Qt::LeftButton, Qt::NoModifier, position);
                } else if (action == "primary") main->close();
                else if (action == "destroy") delete main.data();
            });
        });
        QCOMPARE(bootstrapper.run(), action == "destroy" ? 1 : 0);
        QVERIFY(exercised);
        QVERIFY(!main && !display);
        QCOMPARE(displayClosures, 1);
        QCOMPARE(app->quitOnLastWindowClosed(), previous);
        QVERIFY(QGuiApplication::allWindows().isEmpty());
    }
    void startupFailureRollback() {
        auto *app = qobject_cast<QGuiApplication *>(QCoreApplication::instance());
        QVERIFY(ViewLocator::registerView<DisplayViewModel>(QUrl("qrc:/missing-display.qml")));
        const bool previous = app->quitOnLastWindowClosed();
        AppBootstrapper bootstrapper(*app, {false,QRect(20,20,500,350),QRect(40,40,600,180)}, 0, false);
        QCOMPARE(bootstrapper.run(), 1);
        QCOMPARE(app->quitOnLastWindowClosed(), previous);
        QVERIFY(QGuiApplication::allWindows().isEmpty());
    }
};
QTEST_MAIN(MultiScreenTests)
#include "tst_multiscreen.moc"
