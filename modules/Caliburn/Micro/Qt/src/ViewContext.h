#pragma once

#include <QString>
#include <QVariant>
#include <optional>

namespace ViewContext {
inline QString describe(const std::optional<QString> &context)
{
    return context ? QStringLiteral("\"%1\"").arg(*context) : QStringLiteral("<default>");
}

inline bool valid(const std::optional<QString> &context)
{
    // 空字符串是独立的合法登记键，但无法形成约定视图名称。
    if (!context || context->isEmpty()) return true;
    if (context->contains(QLatin1Char('/')) || context->contains(QLatin1Char('\\'))) return false;
    for (const auto &part : context->split(QLatin1Char('.')))
        if (part.isEmpty()) return false;
    return true;
}

inline bool fromQml(const QVariant &value, std::optional<QString> &context)
{
    if (!value.isValid() || value.isNull()) {
        context.reset();
        return true;
    }
    if (value.metaType().id() != QMetaType::QString) return false;
    context = value.toString();
    return valid(context);
}

inline QString describeQml(const QVariant &value)
{
    std::optional<QString> context;
    return fromQml(value, context) ? describe(context) : QStringLiteral("<invalid>");
}
}
