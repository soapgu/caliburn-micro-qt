#pragma once

#include <QObject>
#include <QPointer>
#include <QList>
#include <CaliburnMicroQt/DialogResult.h>
#include "WindowViewBinding.h"

class QQuickWindow;

// 窗口与 VM 的私有桥接，不接管两者的所有权。
class WindowConductor final : public QObject
{
    Q_OBJECT
public:
    WindowConductor(QQuickWindow *window, QObject *model, QObject *parent = nullptr,
                    bool isDialog = false);
    ~WindowConductor() override;
    void detach();
    bool isClosing() const { return m_checking || m_allowClose || m_windowClosed; }
    bool isClosed() const { return m_windowClosed; }

public slots:
    void requestClose();

signals:
    void windowClosed(DialogResult result);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void checkClose();
    void closeWindow();
    void rejectClose();

    WindowViewBinding m_binding;
    QPointer<QQuickWindow> m_window;
    QPointer<QObject> m_model;
    QList<QMetaObject::Connection> m_connections;
    DialogResult m_result;
    bool m_isDialog = false;
    bool m_enabled = true;
    bool m_checking = false;
    bool m_allowClose = false;
    bool m_modelClosed = false;
    bool m_windowClosed = false;
};
