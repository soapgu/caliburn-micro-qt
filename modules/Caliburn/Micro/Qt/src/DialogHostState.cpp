#include "DialogHostState.h"
#include <QDebug>

DialogHostState::~DialogHostState()
{
    if (m_manager)
        m_manager->detachHost(this);
}

void DialogHostState::setAvailable(bool available)
{
    if (m_available == available)
        return;
    m_available = available;
    if (!available && m_manager)
        m_manager->complete(std::nullopt);
    emit availableChanged();
}

void DialogHostState::setManager(IWindowManager *manager)
{
    if (manager == m_manager)
        return;
    if (m_manager) {
        m_manager->complete(std::nullopt);
        disconnect(m_manager, nullptr, this, nullptr);
        m_manager->detachHost(this);
    }
    m_manager = nullptr;
    auto *concrete = qobject_cast<WindowManager *>(manager);
    if (concrete && concrete->attachHost(this)) {
        m_manager = concrete;
        connect(concrete, &IWindowManager::currentDialogChanged, this, &DialogHostState::modelChanged);
        connect(concrete, &QObject::destroyed, this, [this] {
            m_manager = nullptr;
            emit managerChanged();
            emit modelChanged();
        });
    } else if (manager) {
        qWarning("DialogHost：窗口服务不受支持或已有宿主");
    }
    emit managerChanged();
    emit modelChanged();
}

ScreenViewModel *DialogHostState::model() const
{
    return m_manager ? m_manager->currentDialog() : nullptr;
}

QString DialogHostState::requestId() const
{
    return m_manager ? m_manager->requestId() : QString();
}

void DialogHostState::failed(const QString &id, const QString &message)
{
    if (m_manager)
        m_manager->fail(id, message);
}

void DialogHostState::released(const QString &id)
{
    if (m_manager)
        m_manager->release(id);
}

void DialogHostState::dismiss(const QString &id)
{
    if (m_manager && id == requestId())
        m_manager->complete(std::nullopt);
}
