#pragma once

#include "HomeViewModel.h"
#include <CaliburnMicroQt/IWindowManager.h>
#include <CaliburnMicroQt/ScreenViewModel.h>
#include <memory>

class ShellViewModel : public ScreenViewModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Created by the consumer composition root")
    Q_PROPERTY(HomeViewModel *home READ home CONSTANT)
    Q_PROPERTY(bool resetPending READ resetPending NOTIFY resetPendingChanged)
public:
    explicit ShellViewModel(std::shared_ptr<IWindowManager> windows);
    HomeViewModel *home() { return &m_home; }
    bool resetPending() const { return m_resetPending; }
    Q_INVOKABLE void requestReset();
    Q_INVOKABLE void closeWindow() { tryClose(); }
signals:
    void resetPendingChanged();
private:
    HomeViewModel m_home{this};
    std::shared_ptr<IWindowManager> m_windows;
    bool m_resetPending = false;
};
