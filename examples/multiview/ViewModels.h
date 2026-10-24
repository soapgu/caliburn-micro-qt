#pragma once
#include <CaliburnMicroQt/ScreenViewModel.h>
#include <QtQml/qqmlregistration.h>

namespace MultiView {
class HomeViewModel : public ScreenViewModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("由 Shell 创建")
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)
public:
    using ScreenViewModel::ScreenViewModel;
    QString name() const { return m_name; }
    void setName(const QString &name) { setAndNotify(m_name, name, &HomeViewModel::nameChanged); }
    int count() const { return m_count; }
    Q_INVOKABLE void increment() { ++m_count; emit countChanged(); }
signals:
    void nameChanged();
    void countChanged();
private:
    QString m_name = QStringLiteral("同一个 HomeViewModel");
    int m_count = 0;
};

class ShellViewModel : public ScreenViewModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("由 Bootstrapper 创建")
    Q_PROPERTY(MultiView::HomeViewModel *home READ home CONSTANT)
public:
    using ScreenViewModel::ScreenViewModel;
    HomeViewModel *home() { return &m_home; }
    Q_INVOKABLE void exitApplication() { tryClose(); }
protected:
    void onActivate() override { m_home.activate(); }
    void onDeactivate(bool close) override { m_home.deactivate(close); }
private:
    HomeViewModel m_home{this};
};
}
