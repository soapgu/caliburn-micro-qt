#pragma once

#include <CaliburnMicroQt/WindowManager.h>

// DialogHost 的 QML/C++ 适配；业务通过 IWindowManager 发起请求。
class DialogHostState : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(bool available READ available WRITE setAvailable NOTIFY availableChanged)
    Q_PROPERTY(IWindowManager *manager READ manager WRITE setManager NOTIFY managerChanged)
    Q_PROPERTY(ScreenViewModel *model READ model NOTIFY modelChanged)
    Q_PROPERTY(QString requestId READ requestId NOTIFY modelChanged)
public:
    explicit DialogHostState(QObject *parent = nullptr) : QObject(parent) {}
    ~DialogHostState() override;
    bool available() const { return m_available; }
    void setAvailable(bool available);
    IWindowManager *manager() const { return m_manager.data(); }
    void setManager(IWindowManager *manager);
    ScreenViewModel *model() const;
    QString requestId() const;
    Q_INVOKABLE void failed(const QString &id, const QString &message);
    Q_INVOKABLE void released(const QString &id);
    Q_INVOKABLE void dismiss(const QString &id);
signals:
    void availableChanged();
    void managerChanged();
    void modelChanged();
    void hideRequested(const QString &id);
private:
    friend class WindowManager;
    bool m_available = false;
    QPointer<WindowManager> m_manager;
};
