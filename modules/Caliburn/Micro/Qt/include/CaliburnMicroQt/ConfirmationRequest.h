#pragma once

#include <QString>

struct ConfirmationRequest
{
    QString title;
    QString message;
    QString confirmText = QStringLiteral("确认");
    QString cancelText = QStringLiteral("取消");
};
