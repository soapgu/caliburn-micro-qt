#include "ViewHostState.h"
#include <QThread>
#include <QVariant>

bool ViewHostState::matchesView(QObject *view) const
{
    return m_model && view && view->property("viewModel").value<QObject *>() == m_model.data();
}

void ViewHostState::setModel(ViewModelBase *model)
{
    if (QThread::currentThread() != thread() || (model && model->thread() != thread())) {
        qWarning("ViewHost：模型与宿主必须位于同一线程");
        return;
    }
    if (m_model == model)
        return;
    QObject::disconnect(m_destroyed);
    m_model = model;
    if (model) {
        m_destroyed = connect(model, &QObject::destroyed, this, [this] {
            // destroyed 到达时 QPointer 已经清空，仍必须显式通知宿主卸载。
            m_model.clear();
            emit modelChanged();
        });
    }
    emit modelChanged();
}
