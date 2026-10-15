#pragma once
#include <CaliburnMicroQt/ScreenViewModel.h>

namespace MultiScreen {
class ShellViewModel final : public ScreenViewModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Created by the application")
    Q_PROPERTY(QString modeLabel READ modeLabel CONSTANT)
public:
    explicit ShellViewModel(bool fourScreen) : m_modeLabel(fourScreen ? QStringLiteral("四屏模式") : QStringLiteral("预览模式")) {}
    QString modeLabel() const { return m_modeLabel; }
    Q_INVOKABLE void exitApplication() { tryClose(); }
private:
    QString m_modeLabel;
};
class DisplayViewModel final : public ScreenViewModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Created by the application")
    Q_PROPERTY(QString modeLabel READ modeLabel CONSTANT)
public:
    explicit DisplayViewModel(bool fourScreen) : m_modeLabel(fourScreen ? QStringLiteral("四屏模式") : QStringLiteral("预览模式")) {}
    QString modeLabel() const { return m_modeLabel; }
private:
    QString m_modeLabel;
};
}
