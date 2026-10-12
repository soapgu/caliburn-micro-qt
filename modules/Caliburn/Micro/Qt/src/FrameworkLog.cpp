#include "FrameworkLog.h"

namespace FrameworkLog {
#ifdef QT_NO_DEBUG
constexpr QtMsgType defaultLevel = QtInfoMsg;
#else
constexpr QtMsgType defaultLevel = QtDebugMsg;
#endif
Q_LOGGING_CATEGORY(core, "caliburn.microqt.core", defaultLevel)
Q_LOGGING_CATEGORY(lifecycle, "caliburn.microqt.lifecycle", defaultLevel)
Q_LOGGING_CATEGORY(conductor, "caliburn.microqt.conductor", defaultLevel)
Q_LOGGING_CATEGORY(view, "caliburn.microqt.view", defaultLevel)
Q_LOGGING_CATEGORY(window, "caliburn.microqt.window", defaultLevel)
Q_LOGGING_CATEGORY(bootstrapper, "caliburn.microqt.bootstrapper", defaultLevel)

QString identity(const QObject *object)
{
    if (!object) return QStringLiteral("null");
    return QStringLiteral("%1@0x%2")
        .arg(QString::fromLatin1(object->metaObject()->className()))
        .arg(reinterpret_cast<quintptr>(object), 0, 16);
}

QString exceptionMessage(std::exception_ptr error)
{
    if (!error) return {};
    try { std::rethrow_exception(error); }
    catch (const std::exception &exception) { return QString::fromUtf8(exception.what()); }
    catch (...) { return QStringLiteral("Unknown exception"); }
}
}
