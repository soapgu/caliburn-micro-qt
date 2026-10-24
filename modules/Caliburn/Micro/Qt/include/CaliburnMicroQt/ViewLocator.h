#pragma once
#include <QObject>
#include <QUrl>
#include <QVariant>
#include <QtQml/qqmlregistration.h>
#include <optional>
#include <type_traits>

struct ViewSearchRoot {
    QUrl url;
    QString modelNamespacePrefix;
    QString viewsDirectory = QStringLiteral("views");
};

// 配置在进程内共享；配置、冻结和查询均须在应用主线程调用。
class ViewLocator : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
public:
    explicit ViewLocator(QObject *parent = nullptr) : QObject(parent) {}
    template<class T>
    static bool registerView(const QUrl &url, const std::optional<QString> &context = std::nullopt)
    {
        static_assert(std::is_base_of_v<QObject, T>, "登记类型必须继承 QObject");
        static_assert(std::is_same_v<decltype(&T::qt_metacast), void *(T::*)(const char *)>,
                      "具体登记类型必须声明自己的 Q_OBJECT");
        return registerType(T::staticMetaObject, url, context);
    }
    [[nodiscard]] static bool addSearchRoot(const ViewSearchRoot &root);
    [[nodiscard]] static bool freeze();
    static QUrl viewUrl(const QObject *model, const std::optional<QString> &context = std::nullopt);
    // QVariant 仅用于 QML 空值边界；非空参数必须为字符串，不做隐式转换。
    Q_INVOKABLE QUrl resolve(QObject *model, const QVariant &context = {}) const;
private:
    static bool registerType(const QMetaObject &type, const QUrl &url,
                             const std::optional<QString> &context);
};
