#include <QtTest>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTemporaryDir>

class LoggingTests : public QObject
{
    Q_OBJECT
private:
    struct Result { QByteArray console; QByteArray file; int exitCode; QProcess::ExitStatus status; };
    Result run(const QString &scenario, const QString &path,
               const QString &rules = {}, const QString &pattern = {},
               const QString &color = {})
    {
        QProcess process;
        auto environment = QProcessEnvironment::systemEnvironment();
        environment.remove(QStringLiteral("QT_MESSAGE_PATTERN"));
        environment.remove(QStringLiteral("QT_LOGGING_CONF"));
        environment.remove(QStringLiteral("QT_FATAL_WARNINGS"));
        environment.remove(QStringLiteral("QT_FATAL_CRITICALS"));
        environment.remove(QStringLiteral("CMQT_LOG_COLOR"));
        if (!color.isEmpty()) environment.insert(QStringLiteral("CMQT_LOG_COLOR"), color);
        environment.insert(QStringLiteral("QT_LOGGING_RULES"), rules);
        environment.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("offscreen"));
        if (!pattern.isEmpty()) environment.insert(QStringLiteral("QT_MESSAGE_PATTERN"), pattern);
        process.setProcessEnvironment(environment);
        process.setProcessChannelMode(QProcess::MergedChannels);
        process.start(QCoreApplication::applicationDirPath() + QStringLiteral("/CaliburnLoggingProbe"),
                      {scenario, path});
        if (!process.waitForFinished(15000)) {
            process.kill();
            process.waitForFinished();
            QTest::qFail("日志探针没有在限定时间内结束", __FILE__, __LINE__);
        }
        QFile file(path);
        const auto bytes = file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray{};
        return {process.readAll(), bytes, process.exitCode(), process.exitStatus()};
    }

private slots:
    void colorModes_data()
    {
        QTest::addColumn<QString>("scenario");
        QTest::addColumn<QString>("mode");
        QTest::addColumn<bool>("colored");
        QTest::addColumn<bool>("invalid");
        QTest::newRow("default-pipe") << "normal" << "" << false << false;
        QTest::newRow("auto-pipe") << "normal" << "auto" << false << false;
        QTest::newRow("never") << "normal" << "never" << false << false;
        QTest::newRow("invalid-default") << "normal" << "invalid" << false << true;
        QTest::newRow("options-never") << "optionsNever" << "" << false << false;
#ifdef Q_OS_UNIX
        // ANSI sink 可以在管道上强制着色；Windows 原生控制台 sink 需真实控制台。
        QTest::newRow("always") << "normal" << "always" << true << false;
        QTest::newRow("normalized") << "normal" << " Always " << true << false;
        QTest::newRow("options-always") << "optionsAlways" << "" << true << false;
        QTest::newRow("env-overrides-always") << "optionsAlways" << "never" << false << false;
        QTest::newRow("env-overrides-never") << "optionsNever" << "always" << true << false;
        QTest::newRow("env-auto-overrides-always") << "optionsAlways" << "auto" << false << false;
        QTest::newRow("invalid-options") << "optionsAlways" << "invalid" << true << true;
#endif
    }

    void colorModes()
    {
        QFETCH(QString, scenario);
        QFETCH(QString, mode);
        QFETCH(bool, colored);
        QFETCH(bool, invalid);
        QTemporaryDir directory;
        const auto result = run(scenario, directory.filePath(QStringLiteral("color.log")),
                                QStringLiteral("caliburn.example.debug=true"), {}, mode);
        QCOMPARE(result.exitCode, 0);
        QVERIFY(!result.file.contains('\x1b'));
        QCOMPARE(result.console.contains("Invalid CMQT_LOG_COLOR"), invalid);
        QCOMPARE(result.console.contains("\x1b["), colored);
        // 去掉组件生成的 SGR 后，两路输出应完全一致（降级提示只在控制台）。
        const auto sgr = QRegularExpression(QStringLiteral("\\x1b\\[[0-9;]*m"));
        const auto lines = QString::fromUtf8(result.console).split('\n');
        QStringList clean;
        for (auto line : lines) {
            if (line.startsWith(QStringLiteral("Logging output error:"))) continue;
            line.remove(sgr);
            clean.push_back(line);
        }
        QCOMPARE(clean.join('\n').toUtf8(), result.file);
        if (colored) {
            for (const auto &entry : {qMakePair(QByteArray("[debug]"), QByteArray("\x1b[36m")),
                                      qMakePair(QByteArray("[info]"), QByteArray("\x1b[32m")),
                                      qMakePair(QByteArray("[warning]"), QByteArray("\x1b[33m\x1b[1m")),
                                      qMakePair(QByteArray("[critical]"), QByteArray("\x1b[31m"))}) {
                bool found = false;
                for (const auto &line : result.console.split('\n')) {
                    if (!line.contains(entry.first)) continue;
                    QVERIFY(line.startsWith(entry.second));
                    QVERIFY(line.endsWith("\x1b[m"));
                    found = true;
                }
                QVERIFY(found);
            }
        }
    }

    void coloredCustomPattern()
    {
        QTemporaryDir directory;
        const auto result = run(QStringLiteral("normal"), directory.filePath(QStringLiteral("pattern.log")),
                                {}, QStringLiteral("%{type}|%{category}|%{message}"), QStringLiteral("always"));
        QCOMPARE(result.exitCode, 0);
        QVERIFY(result.file.contains("info|caliburn.example|中文正文 {} %1 %s"));
        QVERIFY(!result.file.contains('\x1b'));
        QVERIFY(!result.file.contains("LoggingProbe.cpp"));
        QCOMPARE(result.console.count("中文正文 {} %1 %s"), 1);
    }

    void outputAndLocation()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto result = run(QStringLiteral("normal"), directory.filePath(QStringLiteral("日志.log")));
        QCOMPARE(result.status, QProcess::NormalExit);
        QCOMPARE(result.exitCode, 0);
        for (const auto *message : {"中文正文 {} %1 %s", "信息消息", "警告消息", "错误消息",
                                    "Qt 原生消息", "QML 原生消息", "应用析构日志"}) {
            const auto bytes = QByteArray(message);
            QCOMPARE(result.file.count(bytes), 1);
            QCOMPARE(result.console.count(bytes), 1);
        }
        QVERIFY(result.file.contains("LoggingLocationProbe::record"));
        QVERIFY(result.file.contains("LoggingProbe.cpp:"));
        QVERIFY(result.file.contains("[caliburn.example]"));
        QVERIFY(QRegularExpression(QStringLiteral("\\[tid=\\d+\\]")).match(QString::fromUtf8(result.file)).hasMatch());
        QVERIFY(result.file.contains("[warning]"));
        QVERIFY(result.file.contains("[critical]"));
        QVERIFY(!result.file.contains("Qt 内部调试消息"));
#ifdef QT_NO_DEBUG
        QVERIFY(!result.file.contains("调试消息"));
#else
        QCOMPARE(result.file.count("调试消息"), 1);
#endif
    }

    void rulesAndPattern()
    {
        QTemporaryDir directory;
        const auto enabled = run(QStringLiteral("normal"), directory.filePath(QStringLiteral("enabled.log")),
                                 QStringLiteral("caliburn.example.debug=true"),
                                 QStringLiteral("%{type}|%{category}|%{message}"));
        QCOMPARE(enabled.exitCode, 0);
        QVERIFY(enabled.file.contains("debug|caliburn.example|调试消息"));
        QVERIFY(!enabled.file.contains("LoggingProbe.cpp"));
        const auto disabled = run(QStringLiteral("normal"), directory.filePath(QStringLiteral("disabled.log")),
                                  QStringLiteral("caliburn.example.debug=false;caliburn.example.info=false"));
        QCOMPARE(disabled.exitCode, 0);
        QVERIFY(!disabled.file.contains("调试消息"));
        QVERIFY(!disabled.file.contains("信息消息"));
        QVERIFY(disabled.file.contains("警告消息"));
    }

    void concurrency()
    {
        QTemporaryDir directory;
        const auto result = run(QStringLiteral("threads"), directory.filePath(QStringLiteral("threads.log")));
        QCOMPARE(result.exitCode, 0);
        for (int worker = 0; worker < 4; ++worker)
            for (int index = 0; index < 25; ++index) {
                const auto marker = QStringLiteral("并发-%1-%2\n").arg(worker).arg(index).toUtf8();
                QCOMPARE(result.file.count(marker), 1);
                QCOMPARE(result.console.count(marker), 1);
            }
    }

    void coloredConcurrency()
    {
        QTemporaryDir directory;
        const auto result = run(QStringLiteral("threads"), directory.filePath(QStringLiteral("threads-color.log")),
                                {}, {}, QStringLiteral("always"));
        QCOMPARE(result.exitCode, 0);
        QVERIFY(!result.file.contains('\x1b'));
        auto clean = QString::fromUtf8(result.console);
        clean.remove(QRegularExpression(QStringLiteral("\\x1b\\[[0-9;]*m")));
        // 并发时两路的全局顺序可能不同，逐条比较，确认没有拆行或重复。
        auto consoleLines = clean.split('\n');
        auto fileLines = QString::fromUtf8(result.file).split('\n');
        consoleLines.sort();
        fileLines.sort();
        QCOMPARE(consoleLines, fileLines);
        for (int worker = 0; worker < 4; ++worker)
            for (int index = 0; index < 25; ++index) {
                const auto marker = QStringLiteral("并发-%1-%2\n").arg(worker).arg(index).toUtf8();
                QCOMPARE(clean.toUtf8().count(marker), 1);
                QCOMPARE(result.file.count(marker), 1);
            }
    }

    void rotation()
    {
        QTemporaryDir directory;
        const auto result = run(QStringLiteral("rotation"), directory.filePath(QStringLiteral("rotate.log")));
        QCOMPARE(result.exitCode, 0);
        QCOMPARE(QDir(directory.path()).entryList({QStringLiteral("*.log")}, QDir::Files).size(), 4);
        QByteArray retained;
        for (const auto &entry : QDir(directory.path()).entryList({QStringLiteral("*.log")}, QDir::Files)) {
            QFile file(directory.filePath(entry));
            QVERIFY(file.size() <= 1024);
            QVERIFY(file.open(QIODevice::ReadOnly));
            retained += file.readAll();
        }
        QVERIFY(retained.contains("79"));
    }

    void fallback()
    {
        QTemporaryDir directory;
        const auto path = directory.filePath(QStringLiteral("block"));
        QFile blocker(path);
        QVERIFY(blocker.open(QIODevice::WriteOnly));
        blocker.close();
        const auto result = run(QStringLiteral("normal"), path + QStringLiteral("/output.log"));
        QCOMPARE(result.exitCode, 0);
        QVERIFY(result.file.isEmpty());
        QVERIFY(result.console.contains("Logging output error"));
        QCOMPARE(result.console.count("信息消息"), 1);
    }

    void restore()
    {
        QTemporaryDir directory;
        const auto result = run(QStringLiteral("restore"), directory.filePath(QStringLiteral("restore.log")));
        QCOMPARE(result.exitCode, 0);
        QVERIFY(result.file.contains("桥接中的消息"));
        QVERIFY(!result.console.contains("旧处理器：桥接中的消息"));
        QVERIFY(!result.file.contains("恢复后的消息"));
        QVERIFY(result.console.contains("旧处理器：恢复后的消息"));
    }

    void fatalFlush()
    {
        QTemporaryDir directory;
        const auto result = run(QStringLiteral("fatal"), directory.filePath(QStringLiteral("fatal.log")));
        QCOMPARE(result.status, QProcess::CrashExit);
        QCOMPARE(result.file.count("终止前日志"), 1);
        QVERIFY(result.file.contains("[fatal]"));
    }

    void coloredFatalFlush()
    {
        QTemporaryDir directory;
        const auto result = run(QStringLiteral("fatal"), directory.filePath(QStringLiteral("fatal-color.log")),
                                {}, {}, QStringLiteral("always"));
        QCOMPARE(result.status, QProcess::CrashExit);
        QCOMPARE(result.file.count("终止前日志"), 1);
        QCOMPARE(result.console.count("终止前日志"), 1);
        QVERIFY(!result.file.contains('\x1b'));
#ifdef Q_OS_UNIX
        for (const auto &line : result.console.split('\n')) {
            if (!line.contains("[fatal]")) continue;
            QVERIFY(line.startsWith("\x1b[31m\x1b[1m"));
            QVERIFY(line.endsWith("\x1b[m"));
        }
#endif
    }
};

QTEST_GUILESS_MAIN(LoggingTests)
#include "tst_logging.moc"
