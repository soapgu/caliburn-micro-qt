#include "ShellViewModel.h"
#include <QCoreApplication>
#include <QThread>
#include <stdexcept>

ShellViewModel::ShellViewModel(std::unique_ptr<HomeViewModel> home)
{
    auto *app = QCoreApplication::instance();
    if (!home || home->parent() || !app || thread() != app->thread()
            || QThread::currentThread() != thread() || home->thread() != thread())
        throw std::invalid_argument("Shell 接管 Home 要求非空、无父对象且位于同一应用主线程");

    home->setParent(this);
    if (home->parent() != this)
        throw std::runtime_error("Shell 接管 Home：QObject 父关系建立失败");
    m_home = home.release();
    connect(m_home.data(), &QObject::destroyed, this, [this] {
        m_home.clear();
        emit homeChanged();
    });
}

void ShellViewModel::onInitialize()
{
    if (m_home)
        m_home->initialize();
}

void ShellViewModel::onActivate()
{
    if (m_home)
        m_home->activate();
}

void ShellViewModel::onDeactivate(bool close)
{
    if (m_home)
        m_home->deactivate(close);
}
