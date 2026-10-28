#pragma once

#include <QPointer>
#include <QQuickWindow>
#include <QVariant>
#include <optional>

class QQmlEngine;
class QQuickItem;

// 模块私有的原生窗口外壳；业务内容仍由 ViewHost 装配。
class DialogWindow final : public QQuickWindow
{
    Q_OBJECT
    Q_PROPERTY(QVariant dialogResult MEMBER m_dialogResult)
public:
    explicit DialogWindow(const QColor &backgroundColor = {});
    void loadView(QQmlEngine *engine, QObject *model,
                  const std::optional<QString> &context = std::nullopt);
    QQuickItem *viewItem() const;
    QSize preferredSize() const;

signals:
    void dismissRequested();

protected:
    bool event(QEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void updateContentGeometry();
    const QColor m_backgroundColor;
    QPointer<QQuickItem> m_host;
    QVariant m_dialogResult;
};
