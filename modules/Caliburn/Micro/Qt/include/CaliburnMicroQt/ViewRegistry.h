#pragma once

#include <CaliburnMicroQt/ViewModelBase.h>
#include <QUrl>

class ViewRegistry : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    explicit ViewRegistry(QObject *parent = nullptr) : QObject(parent) {}

    template<class T>
    static bool registerView(const QUrl &url)
    {
        static_assert(std::is_base_of_v<ViewModelBase, T>, "登记类型必须继承 ViewModelBase");
        return registerType(T::staticMetaObject, url);
    }

    // 在应用主线程显式结束映射登记；重复冻结返回 true。
    [[nodiscard]] static bool freeze();
    static QUrl viewUrl(const ViewModelBase *model);
    Q_INVOKABLE QUrl resolve(ViewModelBase *model) const { return viewUrl(model); }

private:
    static bool registerType(const QMetaObject &type, const QUrl &url);
};
