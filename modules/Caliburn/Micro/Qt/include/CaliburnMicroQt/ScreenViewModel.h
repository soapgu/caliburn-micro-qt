#pragma once

#include <CaliburnMicroQt/ViewModelBase.h>

class ScreenViewModel : public ViewModelBase
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Screen 由 C++ 应用装配层创建")
    Q_PROPERTY(bool isInitialized READ isInitialized NOTIFY isInitializedChanged)
    Q_PROPERTY(bool isActive READ isActive NOTIFY isActiveChanged)

public:
    explicit ScreenViewModel(QObject *parent = nullptr);
    bool isInitialized() const { return m_initialized; }
    bool isActive() const { return m_active; }
    void initialize();
    void activate();
    void deactivate(bool close = false);

signals:
    void isInitializedChanged();
    void isActiveChanged();

protected:
    virtual void onInitialize() {}
    virtual void onActivate() {}
    virtual void onDeactivate(bool close) { Q_UNUSED(close); }

private:
    bool canTransition() const;
    void initializeOnce();
    bool m_initialized = false;
    bool m_active = false;
    bool m_closed = false;
    bool m_transitioning = false;
};
