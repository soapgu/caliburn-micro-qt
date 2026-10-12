#include "LoggingRuntime.h"
#include "ExampleLog.h"
#include "AppBootstrapper.h"
#include <CaliburnMicroQt/Conductor.h>
#include <CaliburnMicroQt/ConfirmActionViewModel.h>
#include <CaliburnMicroQt/ViewRegistry.h>
#include <CaliburnMicroQt/WindowManager.h>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QTimer>
#include <QtQml/qqmlextensionplugin.h>
#include <stdexcept>

Q_IMPORT_QML_PLUGIN(CaliburnExampleModulePlugin)

class ProbeScreen : public ScreenViewModel
{
    Q_OBJECT
public:
    bool failActivation = false;
    bool failClose = false;
    bool delayClose = false;
    CloseCallback pending;
    void canClose(CloseCallback callback) override
    {
        if (delayClose) pending = std::move(callback);
        else callback(true);
    }
protected:
    void onActivate() override
    {
        qCInfo(exampleLog) << "Activation hook reached";
        if (failActivation) throw std::runtime_error("Activation hook failed");
    }
    void onDeactivate(bool) override
    {
        qCInfo(exampleLog) << "Deactivation hook reached";
        if (failClose) throw std::runtime_error("Deactivation hook failed");
    }
};

class CoreProbe : public ViewModelBase
{
    Q_OBJECT
public:
    void invalidNotify() { int field = 0; setAndNotify(field, 1, static_cast<void (CoreProbe::*)()>(nullptr)); }
};

int main(int argc, char **argv)
{
    LoggingRuntime logging;
    QGuiApplication app(argc, argv);
    if (app.arguments().size() != 3) return 2;
    LoggingOptions options;
    options.filePath = app.arguments().at(2);
    logging.initialize(options);
    const auto scenario = app.arguments().at(1);
    if (scenario == QStringLiteral("bootstrapper")) {
        AppBootstrapper bootstrapper(app);
        QTimer::singleShot(0, &app, &QCoreApplication::quit);
        return bootstrapper.run();
    }
    if (scenario == QStringLiteral("lifecycle") || scenario == QStringLiteral("exception")) {
        ProbeScreen model;
        QObject::connect(&model, &ScreenViewModel::deactivated, [] {
            qCInfo(exampleLog) << "Deactivated signal reached";
        });
        if (scenario == QStringLiteral("exception")) {
            model.failActivation = true;
            try { model.activate(); } catch (const std::runtime_error &) {}
            if (!model.isActive()) return 3;
            model.failClose = true;
            try { model.deactivate(true); } catch (const std::runtime_error &) {}
            return model.isActive() ? 4 : 0;
        }
        model.activate();
        model.activate();
        model.deactivate(false);
        model.deactivate(false);
        model.activate();
        model.deactivate(true);
        CoreProbe{}.invalidNotify();
        return 0;
    }
    if (scenario == QStringLiteral("idempotent")) {
        Conductor<ProbeScreen> conductor;
        conductor.activate();
        conductor.activateItem(nullptr);
        conductor.activateItem(nullptr);
        conductor.activateItem(std::make_unique<ProbeScreen>());
        conductor.activateItem(conductor.activeItem());
        return 0;
    }
    if (scenario == QStringLiteral("conductor") || scenario == QStringLiteral("destroyed")) {
        auto conductor = std::make_unique<Conductor<ProbeScreen>>();
        conductor->activate();
        auto first = std::make_unique<ProbeScreen>();
        auto *guard = first.get();
        guard->delayClose = true;
        conductor->activateItem(std::move(first));
        conductor->activateItem(std::make_unique<ProbeScreen>());
        auto reply = std::move(guard->pending);
        if (!reply || conductor->activeItem() != guard) return 5;
        if (scenario == QStringLiteral("destroyed")) {
            conductor.reset();
            reply(true);
            return 0;
        }
        reply(false);
        if (conductor->activeItem() != guard) return 6;
        conductor->activateItem(std::make_unique<ProbeScreen>());
        reply = std::move(guard->pending);
        reply(true);
        return conductor->activeItem() == guard ? 7 : 0;
    }
    if (scenario == QStringLiteral("view") || scenario == QStringLiteral("viewFailure")) {
        const auto url = scenario == QStringLiteral("view")
            ? QUrl(QStringLiteral("qrc:/tests/fixtures/Probe.qml"))
            : QUrl(QStringLiteral("qrc:/missing-view.qml"));
        if (!ViewRegistry::registerView<ProbeScreen>(url) || !ViewRegistry::freeze()) return 8;
        ProbeScreen model;
        QQmlEngine engine;
        QQmlComponent component(&engine);
        component.setData("import QtQuick; import Caliburn.Micro.Qt 1.0; ViewHost {}", QUrl());
        std::unique_ptr<QObject> host(component.create());
        if (!host) return 9;
        host->setProperty("model", QVariant::fromValue(&model));
        const bool loaded = host->property("item").value<QObject *>() != nullptr;
        const bool failed = !host->property("errorString").toString().isEmpty();
        return (scenario == QStringLiteral("view") ? loaded && !failed : !loaded && failed) ? 0 : 10;
    }
    if (scenario == QStringLiteral("window") || scenario == QStringLiteral("dialogFailure")) {
        if (!ViewRegistry::registerView<ProbeScreen>(QUrl(QStringLiteral("qrc:/tests/fixtures/WindowProbe.qml")))) return 11;
        if (scenario == QStringLiteral("dialogFailure"))
            ViewRegistry::registerView<ConfirmActionViewModel>(QUrl(QStringLiteral("qrc:/missing-dialog.qml")));
        if (!ViewRegistry::freeze()) return 19;
        ProbeScreen model;
        WindowManager windows;
        if (!windows.showWindow(QVariant::fromValue(&model))) return 12;
        auto future = windows.showDialogAsync(std::make_unique<ConfirmActionViewModel>(
            ConfirmationRequest{QStringLiteral("Test"), QStringLiteral("Confirm?")}), &model);
        if (scenario == QStringLiteral("dialogFailure")) {
            if (!future.isFinished()) return 13;
            try { future.result(); return 14; } catch (const std::exception &) {}
            windows.releaseWindows();
            return 0;
        }
        if (!windows.currentDialog()) return 15;
        windows.currentDialog()->tryClose(true);
        QTimer timeout;
        timeout.setSingleShot(true);
        QObject::connect(&timeout, &QTimer::timeout, &app, [] { QCoreApplication::exit(16); });
        QTimer poll;
        QObject::connect(&poll, &QTimer::timeout, &app, [&] {
            if (future.isFinished()) app.quit();
        });
        timeout.start(3000);
        poll.start(10);
        const int result = app.exec();
        if (result || !future.result().value_or(false)) return 17;
        windows.releaseWindows();
        return 0;
    }
    return 18;
}

#include "FrameworkLoggingProbe.moc"
