#include <CaliburnMicroQt/ViewLocator.h>
#include <CaliburnMicroQt/ConfirmActionViewModel.h>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QThread>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQmlExtensionPlugin>
#include <QQuickItem>
#include <QtTest>

Q_IMPORT_QML_PLUGIN(CaliburnMicroQtPlugin)

class PreviewViewModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString title READ title WRITE setTitle NOTIFY titleChanged)
public:
    QString title() const { return m_title; }
    void setTitle(const QString &value) { if (m_title != value) { m_title = value; emit titleChanged(); } }
signals:
    void titleChanged();
private:
    QString m_title = QStringLiteral("初始值");
};
class OtherViewModel : public QObject { Q_OBJECT };
class DerivedPreviewViewModel : public PreviewViewModel { Q_OBJECT };
class InheritedMetaObject : public PreviewViewModel {};
static_assert(!std::is_same_v<decltype(&InheritedMetaObject::qt_metacast), void *(InheritedMetaObject::*)(const char *)>);
class ViewModel : public QObject { Q_OBJECT };
class Preview : public QObject { Q_OBJECT };
namespace App {
namespace ViewModels {
class HomeViewModel : public QObject { Q_OBJECT };
namespace Feature { class DetailViewModel : public QObject { Q_OBJECT }; }
namespace ViewModels { class NestedViewModel : public QObject { Q_OBJECT }; }
}
namespace Feature { class HomeViewModel : public QObject { Q_OBJECT }; }
}
namespace Apple { class HomeViewModel : public QObject { Q_OBJECT }; }

class ViewLocatorTests : public QObject
{
    Q_OBJECT
    static QUrl file(const QTemporaryDir &directory, const QString &path, const QByteArray &source = "import QtQuick; Item {}")
    {
        const QString full = directory.filePath(path);
        if (!QDir().mkpath(QFileInfo(full).absolutePath()))
            return {};
        QFile output(full);
        if (!output.open(QIODevice::WriteOnly) || output.write(source) != source.size())
            return {};
        return QUrl::fromLocalFile(full);
    }
private slots:
    void rules_data()
    {
        QTest::addColumn<QString>("scenario");
        for (const char *name : {"layouts", "rootOrder", "prefix", "case", "config", "mapping", "freeze", "threads", "qrc", "objectExplicit", "objectConvention", "noFallback", "nonVisual", "injection", "missingOverride"})
            QTest::newRow(name) << QString::fromLatin1(name);
    }
    void rules()
    {
        QFETCH(QString, scenario);
        QTemporaryDir first, second;
        QVERIFY(first.isValid() && second.isValid());
        const ViewSearchRoot root{QUrl::fromLocalFile(first.path()), QStringLiteral("App")};
        PreviewViewModel preview;
        if (scenario == "layouts") {
            App::ViewModels::HomeViewModel home;
            App::ViewModels::Feature::DetailViewModel detail;
            App::Feature::HomeViewModel feature;
            App::ViewModels::ViewModels::NestedViewModel nested;
            const auto homeUrl = file(first, "views/HomeView.qml");
            const auto detailUrl = file(first, "views/Feature/DetailView.qml");
            const auto featureUrl = file(first, "Feature/HomeView.qml");
            const auto nestedUrl = file(first, "ViewModels/views/NestedView.qml");
            QVERIFY(ViewLocator::addSearchRoot(root));
            QCOMPARE(ViewLocator::viewUrl(&home), homeUrl);
            QCOMPARE(ViewLocator::viewUrl(&detail), detailUrl);
            QCOMPARE(ViewLocator::viewUrl(&feature), featureUrl);
            QCOMPARE(ViewLocator::viewUrl(&nested), nestedUrl);
            QVERIFY(QFile::remove(detailUrl.toLocalFile()));
            const auto same = file(first, "ViewModels/Feature/DetailView.qml");
            QCOMPARE(ViewLocator::viewUrl(&detail), same); // 不缓存，回退同命名空间。
        } else if (scenario == "rootOrder") {
            const auto same = file(first, "PreviewView.qml");
            const auto mappedSecond = file(second, "views/PreviewView.qml");
            QVERIFY(ViewLocator::addSearchRoot({QUrl::fromLocalFile(first.path())}));
            QVERIFY(ViewLocator::addSearchRoot({QUrl::fromLocalFile(second.path())}));
            QCOMPARE(ViewLocator::viewUrl(&preview), mappedSecond);
            const auto mappedFirst = file(first, "views/PreviewView.qml");
            QCOMPARE(ViewLocator::viewUrl(&preview), mappedFirst);
            QVERIFY(QFile::remove(mappedFirst.toLocalFile()));
            QVERIFY(QFile::remove(mappedSecond.toLocalFile()));
            QCOMPARE(ViewLocator::viewUrl(&preview), same);
        } else if (scenario == "prefix") {
            App::ViewModels::HomeViewModel home;
            Apple::HomeViewModel apple;
            QVERIFY(!file(first, "le/HomeView.qml").isEmpty());
            QVERIFY(ViewLocator::addSearchRoot(root));
            QVERIFY(ViewLocator::viewUrl(&apple).isEmpty());
            QVERIFY(ViewLocator::addSearchRoot({QUrl::fromLocalFile(second.path()), "App::ViewModels"}));
            const auto url = file(second, "views/HomeView.qml");
            QCOMPARE(ViewLocator::viewUrl(&home), url);
            ViewModel empty;
            Preview noSuffix;
            QVERIFY(ViewLocator::viewUrl(&empty).isEmpty());
            QVERIFY(ViewLocator::viewUrl(&noSuffix).isEmpty());
        } else if (scenario == "case") {
            App::ViewModels::HomeViewModel home;
            QVERIFY(ViewLocator::addSearchRoot({root.url, "app", "Views"}));
            QVERIFY(!file(first, "Views/HomeView.qml").isEmpty());
            QVERIFY(ViewLocator::viewUrl(&home).isEmpty());
            QVERIFY(ViewLocator::addSearchRoot({root.url, "App", "Views"}));
            QCOMPARE(ViewLocator::viewUrl(&home), QUrl::fromLocalFile(first.filePath("Views/HomeView.qml")));
        } else if (scenario == "config") {
            for (const char *url : {"", "relative/", "qrc:relative", "https://host/path/", "qrc:/path?x", "qrc:/path#x", "file:relative", "qrc://host/path/"})
                QVERIFY(!ViewLocator::addSearchRoot({QUrl(QString::fromLatin1(url))}));
            for (const char *directory : {"", ".", "..", "a/b", "a\\b"})
                QVERIFY(!ViewLocator::addSearchRoot({root.url, {}, QString::fromLatin1(directory)}));
            QVERIFY(!ViewLocator::addSearchRoot({root.url, "App::"}));
            QVERIFY(ViewLocator::addSearchRoot({root.url}));
            QVERIFY(ViewLocator::addSearchRoot({QUrl::fromLocalFile(first.path() + "/")}));
            QVERIFY(ViewLocator::viewUrl(&preview).isEmpty());
            QVERIFY(ViewLocator::viewUrl(nullptr).isEmpty());
        } else if (scenario == "mapping") {
            ConfirmActionViewModel confirm(ConfirmationRequest{});
            const QUrl frameworkUrl("qrc:/qt/qml/Caliburn/Micro/Qt/ConfirmActionView.qml");
            QCOMPARE(ViewLocator::viewUrl(&confirm), frameworkUrl);
            QVERIFY(ViewLocator::addSearchRoot({QUrl("qrc:/qt/qml/Caliburn/Micro/Qt")}));
            const auto convention = file(first, "views/ConfirmActionView.qml");
            QVERIFY(ViewLocator::addSearchRoot({root.url}));
            QCOMPARE(ViewLocator::viewUrl(&confirm), convention);
            const QUrl absent("qrc:/missing-override.qml");
            QVERIFY(ViewLocator::registerView<ConfirmActionViewModel>(absent));
            QVERIFY(ViewLocator::registerView<ConfirmActionViewModel>(absent));
            QVERIFY(!ViewLocator::registerView<ConfirmActionViewModel>(convention));
            QCOMPARE(ViewLocator::viewUrl(&confirm), absent);
            QVERIFY(!ViewLocator::registerView<OtherViewModel>(QUrl()));
            QVERIFY(!ViewLocator::registerView<OtherViewModel>(QUrl("relative.qml")));
            QVERIFY(ViewLocator::registerView<PreviewViewModel>(absent));
            DerivedPreviewViewModel derived;
            QVERIFY(ViewLocator::viewUrl(&derived).isEmpty());
        } else if (scenario == "freeze") {
            QVERIFY(ViewLocator::viewUrl(&preview).isEmpty());
            QVERIFY(ViewLocator::addSearchRoot({root.url}));
            QVERIFY(ViewLocator::registerView<PreviewViewModel>(QUrl("qrc:/absent.qml")));
            QVERIFY(ViewLocator::freeze());
            QVERIFY(ViewLocator::freeze());
            QVERIFY(!ViewLocator::addSearchRoot({root.url}));
            QVERIFY(!ViewLocator::registerView<PreviewViewModel>(QUrl("qrc:/absent.qml")));
            QCOMPARE(ViewLocator::viewUrl(&preview), QUrl("qrc:/absent.qml"));
        } else if (scenario == "threads") {
            bool added=true, registered=true, frozen=true;
            QUrl resolved;
            QThread worker;
            QObject foreign;
            foreign.moveToThread(&worker);
            QVERIFY(ViewLocator::viewUrl(&foreign).isEmpty());
            auto runner = std::unique_ptr<QThread>(QThread::create([&] {
                added=ViewLocator::addSearchRoot({root.url});
                registered=ViewLocator::registerView<PreviewViewModel>(QUrl("qrc:/x.qml"));
                frozen=ViewLocator::freeze();
                resolved=ViewLocator::viewUrl(&preview);
            }));
            runner->start(); QVERIFY(runner->wait(5000));
            QVERIFY(!added && !registered && !frozen && resolved.isEmpty());
            QVERIFY(ViewLocator::addSearchRoot({root.url}));
        } else if (scenario == "qrc") {
            QVERIFY(ViewLocator::addSearchRoot({QUrl("qrc:/locator")}));
            QCOMPARE(ViewLocator::viewUrl(&preview), QUrl("qrc:/locator/views/PreviewView.qml"));
        } else {
            qmlRegisterUncreatableType<PreviewViewModel>("LocatorTest", 1, 0, "PreviewViewModel", "由 C++ 创建");
            const QUrl typed("qrc:/locator/views/PreviewView.qml");
            const QUrl local = file(first, "views/PreviewView.qml", "import QtQuick; import LocatorTest 1.0; Text { required property PreviewViewModel viewModel; text: viewModel.title }");
            QVERIFY(!local.isEmpty());
            if (scenario == "objectExplicit")
                QVERIFY(ViewLocator::registerView<PreviewViewModel>(typed));
            else
                QVERIFY(ViewLocator::addSearchRoot({root.url}));
            const bool fail = scenario == "noFallback" || scenario == "nonVisual"
                || scenario == "injection" || scenario == "missingOverride";
            if (fail) {
                QVERIFY(ViewLocator::addSearchRoot({QUrl("qrc:/locator/")}));
                QByteArray source = "import QtQuick; Item { broken syntax }";
                if (scenario == "nonVisual") source = "import QtQml; QtObject { required property QtObject viewModel }";
                if (scenario == "injection") source = "import QtQuick; Item { required property Item viewModel }";
                QVERIFY(!file(first, "views/PreviewView.qml", source).isEmpty());
                if (scenario == "missingOverride")
                    QVERIFY(ViewLocator::registerView<PreviewViewModel>(QUrl("qrc:/absent-override.qml")));
            }
            QQmlEngine engine;
            QQmlComponent component(&engine);
            component.setData("import Caliburn.Micro.Qt 1.0; ViewHost {}", QUrl());
            QVERIFY2(component.isReady(), qPrintable(component.errorString()));
            std::unique_ptr<QObject> host(component.create()); QVERIFY(host);
            auto model = std::make_unique<PreviewViewModel>();
            QQmlEngine::setObjectOwnership(model.get(), QQmlEngine::CppOwnership);
            QVERIFY(host->setProperty("model", QVariant::fromValue<QObject *>(model.get())));
            if (fail) {
                QTRY_VERIFY(!host->property("errorString").toString().isEmpty());
                QVERIFY(!host->property("item").value<QObject *>());
                QCOMPARE(ViewLocator::viewUrl(model.get()), scenario == "missingOverride" ? QUrl("qrc:/absent-override.qml") : local);
                return;
            }
            QTRY_VERIFY(host->property("item").value<QObject *>());
            QPointer<QObject> old = host->property("item").value<QObject *>();
            QCOMPARE(old->property("viewModel").value<QObject *>(), model.get());
            model->setTitle("通知刷新");
            QTRY_COMPARE(old->property("text").toString(), QStringLiteral("通知刷新"));
            QCOMPARE(host->property("item").value<QObject *>(), old.data());
            QVERIFY(host->setProperty("model", QVariant::fromValue<QObject *>(&preview)));
            QTRY_VERIFY(host->property("item").value<QObject *>());
            QTRY_VERIFY(old.isNull());
            model.reset(); // 旧连接已解绑，不影响新模型。
            QVERIFY(host->property("item").value<QObject *>());
            auto replacement = std::make_unique<PreviewViewModel>();
            QQmlEngine::setObjectOwnership(replacement.get(), QQmlEngine::CppOwnership);
            QVERIFY(host->setProperty("model", QVariant::fromValue<QObject *>(replacement.get())));
            replacement.reset();
            QTRY_VERIFY(!host->property("item").value<QObject *>());
            QVERIFY(!host->property("model").value<QObject *>());
            QVERIFY(host->property("errorString").toString().isEmpty());
            QVERIFY(ViewLocator::registerView<OtherViewModel>(typed));
            OtherViewModel wrong;
            QVERIFY(host->setProperty("model", QVariant::fromValue<QObject *>(&wrong)));
            QTRY_VERIFY(!host->property("errorString").toString().isEmpty());
            QVERIFY(!host->property("item").value<QObject *>());
        }
    }
};
QTEST_MAIN(ViewLocatorTests)
#include "tst_viewlocator.moc"
