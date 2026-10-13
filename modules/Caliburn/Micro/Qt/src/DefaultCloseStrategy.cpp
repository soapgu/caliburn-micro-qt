#include "FrameworkLog.h"
#include <CaliburnMicroQt/DefaultCloseStrategy.h>
#include <QObject>
#include <QPointer>
#include <memory>

namespace {
struct Evaluation : std::enable_shared_from_this<Evaluation>
{
    QList<QPointer<QObject>> items;
    CloseStrategyCallback callback;
    QList<QPointer<QObject>> closable;
    bool partialClose = false;
    int index = 0;
    bool result = true;
    bool waiting = false;
    bool pumping = false;

    void advance()
    {
        if (pumping) return;
        pumping = true;
        while (!waiting) {
            if (index == items.size()) {
                auto complete = std::move(callback);
                QList<QObject *> available;
                if (partialClose)
                    for (const auto &item : closable)
                        if (item) available.append(item.data());
                qCDebug(FrameworkLog::conductor) << "Close strategy completed; allowed:" << result << "closable items:" << available.size();
                complete(result, available);
                break;
            }
            auto current = items.at(index++);
            if (!current) { result = false; continue; }
            auto *guard = qobject_cast<IGuardClose *>(current.data());
            if (!guard) { closable.append(current); continue; }
            waiting = true;
            auto self = shared_from_this();
            const auto identity = FrameworkLog::identity(current.data());
            qCDebug(FrameworkLog::conductor).noquote() << "Evaluating close guard:" << identity;
            guard->canClose([self, current, identity](bool allowed) {
                qCDebug(FrameworkLog::conductor).noquote() << "Close guard resolved:" << identity << "alive:" << bool(current) << "allowed:" << allowed;
                self->result = self->result && current && allowed;
                if (current && allowed) self->closable.append(current);
                self->waiting = false;
                self->advance();
            });
        }
        pumping = false;
    }
};
}

void DefaultCloseStrategy::execute(const QList<QObject *> &items, CloseCallback callback)
{
    executeWithResult(items, [callback = std::move(callback)](bool allowed, const auto &) { callback(allowed); });
}

void DefaultCloseStrategy::executeWithResult(const QList<QObject *> &items, CloseStrategyCallback callback)
{
    auto state = std::make_shared<Evaluation>();
    state->callback = std::move(callback);
    state->partialClose = m_partialClose;
    for (auto *item : items)
        if (item) state->items.append(item);
    state->advance();
}
