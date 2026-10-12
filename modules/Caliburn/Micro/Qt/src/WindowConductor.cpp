#include "FrameworkLog.h"
#include "WindowConductor.h"
#include <CaliburnMicroQt/ScreenViewModel.h>
#include <QCloseEvent>
#include <QDebug>
#include <QQuickWindow>
#include <exception>

WindowConductor::WindowConductor(QQuickWindow *window, ScreenViewModel *model, QObject *parent)
    : QObject(parent), m_window(window), m_model(model)
{
    window->installEventFilter(this);
    connect(model, &ScreenViewModel::closeRequested, this, &WindowConductor::requestClose);
    connect(model, &ScreenViewModel::deactivated, this, [this](bool close) {
        if (!close) return;
        m_modelClosed = true;
        if (m_enabled && !m_windowClosed)
            QMetaObject::invokeMethod(this, [this] { closeWindow(); }, Qt::QueuedConnection);
    });
    connect(model, &QObject::destroyed, this, [this] { detach(); });
    connect(window, &QObject::destroyed, this, [this] { detach(); });
}

WindowConductor::~WindowConductor()
{
    detach();
}

void WindowConductor::detach()
{
    m_enabled = false;
    if (m_window) m_window->removeEventFilter(this);
    if (m_model) disconnect(m_model, nullptr, this, nullptr);
}

void WindowConductor::requestClose()
{
    if (!m_enabled || !m_window || isClosing()) return;
    qCDebug(FrameworkLog::window).noquote() << "Window close requested:" << FrameworkLog::identity(m_window.data())
                                         << "model:" << FrameworkLog::identity(m_model.data());
    try { m_window->close(); }
    catch (const std::exception &error) {
        qCWarning(FrameworkLog::window).noquote() << "WindowConductor: close request failed:" << error.what();
        emit closeRejected();
    } catch (...) {
        qCWarning(FrameworkLog::window, "WindowConductor: close request failed with an unknown exception");
        emit closeRejected();
    }
}

bool WindowConductor::eventFilter(QObject *watched, QEvent *event)
{
    if (!m_enabled || watched != m_window || event->type() != QEvent::Close || m_allowClose)
        return false;
    static_cast<QCloseEvent *>(event)->ignore();
    if (!m_checking && !m_windowClosed) {
        m_checking = true;
        // 原关闭栈退出后才能重新调用 QWindow::close()。
        QMetaObject::invokeMethod(this, [this] { checkClose(); }, Qt::QueuedConnection);
    }
    return true;
}

void WindowConductor::checkClose()
{
    if (!m_enabled || !m_window || !m_model || m_windowClosed) return;
    if (m_modelClosed) { closeWindow(); return; }
    QPointer<WindowConductor> owner = this;
    try {
        m_model->canClose([owner](bool allowed) {
            if (!owner || !owner->m_enabled || owner->m_windowClosed || owner->m_modelClosed) return;
            qCInfo(FrameworkLog::window).noquote() << "Window close permission resolved:" << FrameworkLog::identity(owner->m_window.data())
                                                  << "model:" << FrameworkLog::identity(owner->m_model.data()) << "allowed:" << allowed;
            owner->m_checking = false;
            if (allowed) owner->closeWindow();
            else emit owner->closeRejected();
        });
    } catch (const std::exception &error) {
        if (owner) { owner->m_checking = false; emit owner->closeRejected(); }
        qCWarning(FrameworkLog::window).noquote() << "WindowConductor: close guard failed:" << error.what();
    } catch (...) {
        if (owner) { owner->m_checking = false; emit owner->closeRejected(); }
        qCWarning(FrameworkLog::window, "WindowConductor: close guard failed with an unknown exception");
    }
}

void WindowConductor::closeWindow()
{
    if (!m_enabled || !m_window || m_windowClosed) return;
    const auto windowIdentity = FrameworkLog::identity(m_window.data());
    const auto modelIdentity = FrameworkLog::identity(m_model.data());
    m_checking = false;
    m_allowClose = true;
    QPointer<WindowConductor> owner = this;
    QPointer<QQuickWindow> window = m_window;
    bool closed = false;
    try {
        closed = window->close();
    } catch (const std::exception &error) {
        qCWarning(FrameworkLog::window).noquote() << "WindowConductor: window close failed:" << error.what();
    } catch (...) {
        qCWarning(FrameworkLog::window, "WindowConductor: window close failed with an unknown exception");
    }
    if (!owner) return;
    m_allowClose = false;
    if (closed && (!window || !window->isVisible())) {
        m_windowClosed = true;
        qCInfo(FrameworkLog::window).noquote() << "Window closed:" << windowIdentity << "model:" << modelIdentity;
        emit windowClosed();
    } else {
        qCInfo(FrameworkLog::window).noquote() << "Window close rejected:" << windowIdentity << "model:" << modelIdentity;
        emit closeRejected();
    }
}
