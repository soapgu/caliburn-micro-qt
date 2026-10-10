#pragma once

#include <QObject>
#include <QPointer>

class QQuickWindow;
class ScreenViewModel;

// 窗口与 VM 的私有桥接，不接管两者的所有权。
class WindowConductor final : public QObject
{
    Q_OBJECT
public:
    WindowConductor(QQuickWindow *window, ScreenViewModel *model, QObject *parent = nullptr);
    ~WindowConductor() override;
    void detach();
    bool isClosing() const { return m_checking || m_allowClose || m_windowClosed; }

public slots:
    void requestClose();

signals:
    void windowClosed();
    void closeRejected();

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
