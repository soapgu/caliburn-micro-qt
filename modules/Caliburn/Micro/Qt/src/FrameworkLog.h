#pragma once

#include <QLoggingCategory>
#include <QObject>
#include <QString>
#include <exception>

namespace FrameworkLog {
Q_DECLARE_LOGGING_CATEGORY(core)
Q_DECLARE_LOGGING_CATEGORY(lifecycle)
Q_DECLARE_LOGGING_CATEGORY(conductor)
Q_DECLARE_LOGGING_CATEGORY(view)
Q_DECLARE_LOGGING_CATEGORY(window)
Q_DECLARE_LOGGING_CATEGORY(bootstrapper)

// 必须在对象仍然有效时取快照；回调后只使用字符串，不再解引用对象。
QString identity(const QObject *object);
QString exceptionMessage(std::exception_ptr error);
}
