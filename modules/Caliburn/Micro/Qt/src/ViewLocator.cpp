#include <CaliburnMicroQt/ViewLocator.h>
#include "ViewRegistry.h"
#include "FrameworkLog.h"
#include <QCoreApplication>
#include <QFileInfo>
#include <QSet>
#include <QThread>
#include <algorithm>

namespace {
struct LocatorData {
    ViewRegistry registry;
    // 内置根始终位于末尾，与应用根共用同一套候选生成及存在性检查。
    QList<ViewSearchRoot> roots{{QUrl(QStringLiteral("qrc:/qt/qml/Caliburn/Micro/Qt/"))}};
    bool frozen = false;
};
LocatorData &data()
{
    static LocatorData value;
    return value;
}
bool applicationThread()
{
    const auto *app = QCoreApplication::instance();
    if (app && QThread::currentThread() == app->thread())
        return true;
    qCWarning(FrameworkLog::view, "ViewLocator: registration, freeze and lookup require the application thread");
    return false;
}
bool configurable()
{
    if (!applicationThread())
        return false;
    if (!data().frozen)
        return true;
    qCWarning(FrameworkLog::view, "ViewLocator: registration rejected after freeze");
    return false;
}
bool exists(const QUrl &url)
{
    const QString path = url.scheme() == QStringLiteral("qrc")
        ? QStringLiteral(":") + url.path() : url.toLocalFile();
    return QFileInfo(path).isFile();
}
}

bool ViewLocator::registerType(const QMetaObject &type, const QUrl &url)
{
    if (!configurable())
        return false;
    if (url.isEmpty() || !url.isValid() || url.isRelative()) {
        qCWarning(FrameworkLog::view, "ViewLocator: view URL must be valid, non-empty and absolute");
        return false;
    }
    return data().registry.registerType(type, url);
}

bool ViewLocator::addSearchRoot(const ViewSearchRoot &root)
{
    if (!configurable())
        return false;
    ViewSearchRoot normalized = root;
    const auto &url = root.url;
    const auto &directory = root.viewsDirectory;
    const bool supported = (url.scheme() == QStringLiteral("qrc") && url.authority().isEmpty())
        || (url.isLocalFile() && url.authority().isEmpty() && QFileInfo(url.toLocalFile()).isAbsolute());
    const auto segments = root.modelNamespacePrefix.split(QStringLiteral("::"));
    const bool validPrefix = root.modelNamespacePrefix.isEmpty()
        || std::all_of(segments.cbegin(), segments.cend(), [](const QString &part) {
               return !part.isEmpty() && !part.contains(QLatin1Char(':'))
                   && !part.contains(QLatin1Char('/')) && !part.contains(QLatin1Char('\\'));
           });
    if (!url.isValid() || url.isEmpty() || url.isRelative() || !supported
        || !url.path().startsWith(QLatin1Char('/')) || url.hasQuery() || url.hasFragment()
        || directory.isEmpty() || directory == QStringLiteral(".") || directory == QStringLiteral("..")
        || directory.contains(QLatin1Char('/')) || directory.contains(QLatin1Char('\\')) || !validPrefix) {
        qCWarning(FrameworkLog::view, "ViewLocator: invalid search root configuration");
        return false;
    }
    if (!normalized.url.path().endsWith(QLatin1Char('/')))
        normalized.url.setPath(normalized.url.path() + QLatin1Char('/'));
    for (const auto &existing : data().roots) {
        if (existing.url == normalized.url && existing.modelNamespacePrefix == normalized.modelNamespacePrefix
            && existing.viewsDirectory == normalized.viewsDirectory)
            return true;
    }
    data().roots.insert(data().roots.size() - 1, normalized);
    qCDebug(FrameworkLog::view) << "View search root added:" << normalized.url << normalized.modelNamespacePrefix << normalized.viewsDirectory;
    return true;
}

bool ViewLocator::freeze()
{
    if (!applicationThread())
        return false;
    if (!data().frozen)
        qCDebug(FrameworkLog::view) << "View locator frozen; search roots:" << data().roots.size();
    data().frozen = true;
    return true;
}

QUrl ViewLocator::viewUrl(const QObject *model)
{
    if (!applicationThread() || !model)
        return {};
    if (model->thread() != QThread::currentThread()) {
        qCWarning(FrameworkLog::view, "ViewLocator: model must belong to the application thread");
        return {};
    }
    const QUrl registered = data().registry.viewUrl(model);
    if (!registered.isEmpty())
        return registered;

    const QString type = QString::fromUtf8(model->metaObject()->className());
    QStringList parts = type.split(QStringLiteral("::"));
    QString name = parts.takeLast();
    QList<QUrl> mapped, sameNamespace;
    if (name.endsWith(QStringLiteral("ViewModel")) && name.size() > 9) {
        name.chop(5);
        name += QStringLiteral(".qml");
        for (const auto &root : data().roots) {
            QStringList remaining = parts;
            if (!root.modelNamespacePrefix.isEmpty()) {
                const auto prefix = root.modelNamespacePrefix.split(QStringLiteral("::"));
                if (remaining.size() < prefix.size() || remaining.mid(0, prefix.size()) != prefix)
                    continue;
                remaining = remaining.mid(prefix.size());
            }
            qCDebug(FrameworkLog::view).noquote() << "View search scope; type:" << type << "root:" << root.url.toString();
            auto append = [&](QList<QUrl> &urls, const QStringList &directories) {
                QUrl candidate = root.url;
                candidate.setPath(root.url.path() + (directories + QStringList{name}).join(QLatin1Char('/')));
                urls.append(candidate);
            };
            append(sameNamespace, remaining);
            const auto index = remaining.lastIndexOf(QStringLiteral("ViewModels"));
            if (index >= 0) {
                remaining[index] = root.viewsDirectory;
                append(mapped, remaining);
            } else if (remaining.isEmpty()) {
                append(mapped, QStringList{root.viewsDirectory});
            }
        }
    }
    QSet<QUrl> visited;
    for (const auto &candidate : mapped + sameNamespace) {
        if (visited.contains(candidate))
            continue;
        visited.insert(candidate);
        qCDebug(FrameworkLog::view).noquote() << "View candidate; type:" << type << "URL:" << candidate.toString();
        if (exists(candidate)) {
            qCDebug(FrameworkLog::view).noquote() << "Convention view found; type:" << type << "URL:" << candidate.toString();
            return candidate;
        }
    }
    qCWarning(FrameworkLog::view).noquote() << "ViewLocator: no view registered for type" << type;
    return {};
}
