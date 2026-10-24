#include "DialogWindow.h"
#include <QEvent>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QPalette>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <algorithm>
#include <memory>
#include <stdexcept>

namespace {
constexpr int contentMargin = 20;
}

DialogWindow::DialogWindow()
{
    setObjectName(QStringLiteral("dialogWindow"));
    setFlags(Qt::Dialog);
    setModality(Qt::ApplicationModal);
    setColor(QGuiApplication::palette().color(QPalette::Window));
    qGuiApp->installEventFilter(this);
    connect(this, &QQuickWindow::widthChanged, this, &DialogWindow::updateContentGeometry);
    connect(this, &QQuickWindow::heightChanged, this, &DialogWindow::updateContentGeometry);
}

bool DialogWindow::event(QEvent *event)
{
    // 在窗口边界处理无修饰键的 Esc，焦点位于任意业务控件时均走关闭守卫。
    if (event->type() == QEvent::ShortcutOverride || event->type() == QEvent::KeyPress) {
        const auto *key = static_cast<QKeyEvent *>(event);
        if (key->key() == Qt::Key_Escape && key->modifiers() == Qt::NoModifier) {
            event->accept();
            if (event->type() == QEvent::KeyPress) emit dismissRequested();
            return true;
        }
    }
    return QQuickWindow::event(event);
}

bool DialogWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == qGuiApp && event->type() == QEvent::ApplicationPaletteChange)
        setColor(QGuiApplication::palette().color(QPalette::Window));
    return QQuickWindow::eventFilter(watched, event);
}

void DialogWindow::loadView(QQmlEngine *engine, QObject *model, const std::optional<QString> &context)
{
    QQmlEngine::setContextForObject(this, engine->rootContext());
    QQmlComponent component(engine, QUrl(QStringLiteral(
        "qrc:/qt/qml/Caliburn/Micro/Qt/ViewHost.qml")));
    std::unique_ptr<QObject> object(component.beginCreate(engine->rootContext()));
    auto *host = qobject_cast<QQuickItem *>(object.get());
    if (!host) throw std::runtime_error(component.errorString().toStdString());
    QQmlEngine::setObjectOwnership(host, QQmlEngine::CppOwnership);
    host->setParent(contentItem());
    host->setParentItem(contentItem());
    m_host = host;
    updateContentGeometry();
    host->setFocus(true);
    component.setInitialProperties(host, {{QStringLiteral("model"), QVariant::fromValue(model)},
                                           {QStringLiteral("context"), context ? QVariant(*context) : QVariant()}});
    component.completeCreate();
    if (component.isError()) throw std::runtime_error(component.errorString().toStdString());
    const auto error = host->property("errorString").toString();
    if (!error.isEmpty()) throw std::runtime_error(error.toStdString());
    if (!viewItem()) throw std::runtime_error("DialogWindow: view host did not create an Item");
    object.release();
}

QQuickItem *DialogWindow::viewItem() const
{
    return m_host ? m_host->property("item").value<QQuickItem *>() : nullptr;
}

QSize DialogWindow::preferredSize() const
{
    const auto *view = viewItem();
    return QSize(qRound(view && view->implicitWidth() > 0 ? view->implicitWidth() : 360) + 2 * contentMargin,
                 qRound(view && view->implicitHeight() > 0 ? view->implicitHeight() : 220) + 2 * contentMargin);
}

void DialogWindow::updateContentGeometry()
{
    if (!m_host) return;
    m_host->setPosition(QPointF(contentMargin, contentMargin));
    m_host->setSize(QSizeF(std::max(0, width() - 2 * contentMargin),
                          std::max(0, height() - 2 * contentMargin)));
}
