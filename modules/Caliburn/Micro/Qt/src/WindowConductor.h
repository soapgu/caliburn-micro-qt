#pragma once

#include <QObject>
#include <QPointer>

class QQuickWindow;
class ScreenViewModel;

// 根窗口与根 VM 的私有桥接，不接管两者的所有权。
class WindowConductor final : public QObject
{
    Q_OBJECT
public:
    WindowConductor(QQuickWindow *window, ScreenViewModel *model, QObject *parent = nullptr);
    ~WindowConductor() override;
    void detach();

signals:
    void windowClosed();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void checkClose();
    void closeWindow();

    QPointer<QQuickWindow> m_window;
    QPointer<ScreenViewModel> m_model;
    bool m_enabled = true;
    bool m_checking = false;
    bool m_allowClose = false;
    bool m_modelClosed = false;
    bool m_windowClosed = false;
};
