#pragma once
#include <QHash>
#include <QMap>
#include <QMetaObject>
#include <QUrl>
#include <optional>

// 内部精确映射表；ViewLocator 负责线程、配置与冻结检查。
class ViewRegistry
{
public:
    bool registerType(const QMetaObject &type, const QUrl &url, const std::optional<QString> &context);
    QUrl viewUrl(const QObject *model, const std::optional<QString> &context) const;
private:
    QHash<const QMetaObject *, QMap<std::optional<QString>, QUrl>> m_views;
};
