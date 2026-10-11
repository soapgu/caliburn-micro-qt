#include "ExampleLog.h"

#ifdef QT_NO_DEBUG
Q_LOGGING_CATEGORY(exampleLog, "caliburn.example", QtInfoMsg)
#else
Q_LOGGING_CATEGORY(exampleLog, "caliburn.example", QtDebugMsg)
#endif
