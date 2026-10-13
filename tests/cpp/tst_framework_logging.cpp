#include <QtTest>
#include <QFile>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTemporaryDir>

class FrameworkLoggingTests : public QObject
{
    Q_OBJECT
    struct Result { QByteArray console; QByteArray file; };
    Result run(const QString &scenario, const QString &rules = {}, const QString &pattern = {})
    {
        QTemporaryDir directory;
        const auto path = directory.filePath(QStringLiteral("framework.log"));
        QProcess process;
        auto environment = QProcessEnvironment::systemEnvironment();
        for (const auto *name : {"QT_MESSAGE_PATTERN", "QT_LOGGING_CONF", "QT_FATAL_WARNINGS", "QT_FATAL_CRITICALS", "CMQT_LOG_COLOR"})
            environment.remove(QString::fromLatin1(name));
        environment.insert(QStringLiteral("QT_LOGGING_RULES"), rules);
        environment.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("offscreen"));
        environment.insert(QStringLiteral("QT_QUICK_BACKEND"), QStringLiteral("software"));
        if (!pattern.isEmpty()) environment.insert(QStringLiteral("QT_MESSAGE_PATTERN"), pattern);
        process.setProcessEnvironment(environment);
        process.setProcessChannelMode(QProcess::MergedChannels);
        process.start(QCoreApplication::applicationDirPath() + QStringLiteral("/CaliburnFrameworkLoggingProbe"), {scenario, path});
        if (!process.waitForFinished(15000)) {
            process.kill(); process.waitForFinished();
            QTest::qFail("框架日志探针超时", __FILE__, __LINE__);
        }
        const auto console = process.readAll();
        if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0)
            QTest::qFail(console.constData(), __FILE__, __LINE__);
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) QTest::qFail("无法读取框架日志", __FILE__, __LINE__);
        return {console, file.readAll()};
    }
    void onceInBoth(const Result &result, const QByteArray &marker)
    {
        QCOMPARE(result.console.count(marker), 1);
        QCOMPARE(result.file.count(marker), 1);
    }
private slots:
    void lifecycleAndLocation()
    {
        const auto result = run(QStringLiteral("lifecycle"));
        QCOMPARE(result.file.count("Activating: ProbeScreen@0x"), 2);
        onceInBoth(result, "Closed: ProbeScreen@0x");
        QVERIFY(result.file.contains("[caliburn.microqt.lifecycle]"));
        QVERIFY(result.file.contains("ScreenViewModel::activate"));
        QVERIFY(QRegularExpression(QStringLiteral("ScreenViewModel.cpp:\\d+")).match(QString::fromUtf8(result.file)).hasMatch());
        QVERIFY(result.file.indexOf("Activating:") < result.file.indexOf("Activation hook reached"));
        QVERIFY(result.file.indexOf("Deactivating:") < result.file.indexOf("Deactivation hook reached"));
        QVERIFY(result.file.lastIndexOf("Deactivated signal reached") < result.file.indexOf("Closed:"));
        onceInBoth(result, "[caliburn.microqt.core]");
        QVERIFY(!QRegularExpression(QStringLiteral("[\\x{4e00}-\\x{9fff}]")).match(QString::fromUtf8(result.file)).hasMatch());
#ifdef QT_NO_DEBUG
        QVERIFY(!result.file.contains("Initializing:"));
#else
        QVERIFY(result.file.contains("Initializing:"));
#endif
    }
    void exceptionsDoNotReportCompletion()
    {
        const auto result = run(QStringLiteral("exception"));
        onceInBoth(result, "Activating:");
        onceInBoth(result, "Deactivating:");
        QVERIFY(!result.file.contains("Closed:"));
        QVERIFY(!result.file.contains("Deactivated signal reached"));
    }
    void deferredCloseAndRejection()
    {
        const auto result = run(QStringLiteral("conductor"), QStringLiteral("caliburn.microqt.*.debug=false"));
        QCOMPARE(result.file.count("Active item changed:"), 2);
        QCOMPARE(result.file.count("Close permission resolved:"), 3);
        onceInBoth(result, "allowed: false");
        QVERIFY(result.file.contains("allowed: true"));
    }
    void idempotentSelection()
    {
        const auto result = run(QStringLiteral("idempotent"));
        onceInBoth(result, "Active item changed:");
    }
    void destroyedOwner()
    {
        const auto result = run(QStringLiteral("destroyed"), QStringLiteral("caliburn.microqt.*.debug=true"));
        onceInBoth(result, "Close result discarded; conductor destroyed:");
        QVERIFY(result.file.contains("alive: false"));
        QCOMPARE(result.file.count("Active item changed:"), 1);
    }
    void viewsAndFailures()
    {
        const auto success = run(QStringLiteral("view"));
        onceInBoth(success, "View assembled: ProbeScreen@0x");
        QVERIFY(success.file.contains("qrc:/tests/fixtures/Probe.qml"));
        const auto failure = run(QStringLiteral("viewFailure"));
        onceInBoth(failure, "ViewHost: QML view loading failed qrc:/missing-view.qml");
        QVERIFY(failure.file.contains("[warning]"));
        QVERIFY(failure.file.contains("[caliburn.microqt.view]"));
        QVERIFY(!failure.file.contains("View assembled:"));
    }
    void windowsAndDialogs()
    {
        const auto result = run(QStringLiteral("window"), QStringLiteral("caliburn.microqt.view.debug=true"));
        onceInBoth(result, "Window shown:");
        onceInBoth(result, "Dialog shown:");
        onceInBoth(result, "Window closed: DialogWindow_");
        onceInBoth(result, "Window closed: WindowProbe_");
        onceInBoth(result, "Dialog completed:");
        QVERIFY(result.file.contains("result: accepted"));
        QVERIFY(result.file.contains("Convention view found; type: ConfirmActionViewModel"));
        QVERIFY(result.file.contains("qrc:/qt/qml/Caliburn/Micro/Qt/ConfirmActionView.qml"));
        const auto failure = run(QStringLiteral("dialogFailure"));
        onceInBoth(failure, "Dialog failed:");
        QVERIFY(failure.file.contains("qrc:/missing-dialog.qml"));
        QVERIFY(!failure.file.contains("Dialog shown:"));
        QVERIFY(!failure.file.contains("Dialog completed:"));
    }
    void startupAndShutdown()
    {
        const auto result = run(QStringLiteral("bootstrapper"));
        onceInBoth(result, "Startup completed:");
        onceInBoth(result, "Bootstrapper exited; exit code: 0");
        QVERIFY(result.file.contains("[caliburn.microqt.bootstrapper]"));
        QVERIFY(result.file.indexOf("Startup completed:") < result.file.indexOf("Bootstrapper exited;"));
    }
    void locatorDiagnostics()
    {
        const auto enabled = run(QStringLiteral("bootstrapper"), QStringLiteral("caliburn.microqt.view.debug=true"));
        for (const auto *marker : {"View search root added:", "View locator frozen;", "View search scope; type: ShellViewModel",
                                  "View candidate; type: HomeViewModel", "Convention view found; type: HomeViewModel"}) {
            QVERIFY2(enabled.console.contains(marker), marker);
            QVERIFY2(enabled.file.contains(marker), marker);
        }
        const auto disabled = run(QStringLiteral("bootstrapper"), QStringLiteral("caliburn.microqt.view.debug=false"));
        QVERIFY(!disabled.file.contains("View candidate;"));
        QVERIFY(disabled.file.contains("View assembled: HomeViewModel@0x"));
    }
    void filteringAndPattern()
    {
        const auto enabled = run(QStringLiteral("lifecycle"), QStringLiteral("caliburn.microqt.*.debug=true"),
                                 QStringLiteral("%{type}|%{category}|%{message}"));
        QVERIFY(enabled.file.contains("debug|caliburn.microqt.lifecycle|Initializing:"));
        QVERIFY(!enabled.file.contains("ScreenViewModel.cpp"));
        const auto disabled = run(QStringLiteral("lifecycle"), QStringLiteral("caliburn.microqt.*.debug=false;caliburn.microqt.*.info=false"));
        QVERIFY(!disabled.file.contains("Initializing:"));
        QVERIFY(!disabled.file.contains("Activating:"));
        QVERIFY(disabled.file.contains("[caliburn.microqt.core]"));
        const auto selective = run(QStringLiteral("conductor"), QStringLiteral("caliburn.microqt.*.debug=false;caliburn.microqt.lifecycle.info=false"));
        QVERIFY(!selective.file.contains("Activating:"));
        QVERIFY(selective.file.contains("Active item changed:"));
    }
};
QTEST_GUILESS_MAIN(FrameworkLoggingTests)
#include "tst_framework_logging.moc"
