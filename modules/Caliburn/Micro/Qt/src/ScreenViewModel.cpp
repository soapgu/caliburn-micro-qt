#include <CaliburnMicroQt/ScreenViewModel.h>
#include <QCoreApplication>
#include <QScopedValueRollback>
#include <QThread>

ScreenViewModel::ScreenViewModel(QObject *parent) : ViewModelBase(parent) {}

bool ScreenViewModel::canTransition() const
{
    auto *app = QCoreApplication::instance();
    if (!app || QThread::currentThread() != thread() || thread() != app->thread()) {
        qWarning("ScreenViewModel：生命周期必须在应用主线程调用");
        return false;
    }
    if (m_transitioning) {
        qWarning("ScreenViewModel：拒绝生命周期重入");
        return false;
    }
    return true;
}

void ScreenViewModel::initializeOnce()
{
    if (m_initialized)
        return;
    onInitialize();
    setAndNotify(m_initialized, true, &ScreenViewModel::isInitializedChanged);
}

void ScreenViewModel::initialize()
{
    if (!canTransition())
        return;
    QScopedValueRollback<bool> guard(m_transitioning, true);
    initializeOnce();
}

void ScreenViewModel::activate()
{
    if (!canTransition())
        return;
    QScopedValueRollback<bool> guard(m_transitioning, true);
    initializeOnce();
    if (m_active)
        return;
    onActivate();
    m_closed = false;
    setAndNotify(m_active, true, &ScreenViewModel::isActiveChanged);
}

void ScreenViewModel::deactivate(bool close)
{
    if (!canTransition())
        return;
    QScopedValueRollback<bool> guard(m_transitioning, true);
    if (!m_initialized || (close ? m_closed : !m_active))
        return;
    onDeactivate(close);
    if (close)
        m_closed = true;
    setAndNotify(m_active, false, &ScreenViewModel::isActiveChanged);
}
