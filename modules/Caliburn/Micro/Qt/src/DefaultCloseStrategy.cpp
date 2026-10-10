#include <CaliburnMicroQt/DefaultCloseStrategy.h>
#include <CaliburnMicroQt/ViewModelBase.h>
#include <QPointer>
#include <memory>

namespace {
struct Evaluation : std::enable_shared_from_this<Evaluation>
{
    QList<QPointer<ViewModelBase>> items;
    CloseCallback callback;
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
                complete(result);
                break;
            }
            auto current = items.at(index++);
            if (!current) { result = false; continue; }
            auto *guard = qobject_cast<IGuardClose *>(current.data());
            if (!guard) continue;
            waiting = true;
            auto self = shared_from_this();
            guard->canClose([self, current](bool allowed) {
                self->result = self->result && current && allowed;
                self->waiting = false;
                self->advance();
            });
        }
        pumping = false;
    }
};
}

void DefaultCloseStrategy::execute(const QList<ViewModelBase *> &items, CloseCallback callback)
{
    auto state = std::make_shared<Evaluation>();
    state->callback = std::move(callback);
    for (auto *item : items)
        if (item) state->items.append(item);
    state->advance();
}
