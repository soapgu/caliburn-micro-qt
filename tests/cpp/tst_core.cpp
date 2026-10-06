#include <CaliburnMicroQt/ViewModelBase.h>
#include <ShellViewModel.h>
#include <QtTest>
#include <type_traits>
#include <memory>
#include <limits>

struct TrackedValue
{
    int value = 0;
    int assignments = 0;
    bool operator==(const TrackedValue &other) const { return value == other.value; }
    TrackedValue &operator=(const TrackedValue &other)
    {
        value = other.value;
        ++assignments;
        return *this;
    }
};

class NotifyVm : public ViewModelBase
{
    Q_OBJECT
public:
    using ViewModelBase::ViewModelBase;
    using ViewModelBase::setAndNotify;
    int value = 0;
    QString text;
    TrackedValue tracked;
signals:
    void changed();
};

// 通知辅助只接受无参数 void 成员指针，QObject 基类不可复制或移动。
using NotifyHelper = bool (ViewModelBase::*)(int &, const int &, void (NotifyVm::*)());
static_assert(std::is_same_v<decltype(&NotifyVm::setAndNotify<NotifyVm, int>), NotifyHelper>);
static_assert(!std::is_invocable_v<NotifyHelper, NotifyVm *, int &, const int &,
                                  void (NotifyVm::*)(int)>);
static_assert(!std::is_invocable_v<NotifyHelper, NotifyVm *, int &, const int &,
                                  int (NotifyVm::*)()>);
static_assert(!std::is_copy_constructible_v<ViewModelBase>);
static_assert(!std::is_move_constructible_v<ViewModelBase>);

class CoreTests : public QObject
{
    Q_OBJECT
private slots:
    void baseContract()
    {
        QObject parent;
        ViewModelBase base(&parent);
        QCOMPARE(base.parent(), &parent);
        QCOMPARE(base.metaObject()->propertyCount(), QObject::staticMetaObject.propertyCount());
        QCOMPARE(base.metaObject()->methodCount(), QObject::staticMetaObject.methodCount());
        ShellViewModel shell;
        QCOMPARE(qobject_cast<ViewModelBase *>(&shell), static_cast<ViewModelBase *>(&shell));
    }

    void typedNotifications()
    {
        NotifyVm vm;
        QSignalSpy spy(&vm, &NotifyVm::changed);
        int observed = -1;
        connect(&vm, &NotifyVm::changed, this, [&] { observed = vm.value; });
        QVERIFY(vm.setAndNotify(vm.value, 3, &NotifyVm::changed));
        QCOMPARE(observed, 3);
        QCOMPARE(spy.count(), 1);
        QVERIFY(!vm.setAndNotify(vm.value, 3, &NotifyVm::changed));
        QCOMPARE(spy.count(), 1);
        QVERIFY(vm.setAndNotify(vm.text, QStringLiteral("文字"), &NotifyVm::changed));
        QCOMPARE(vm.text, QStringLiteral("文字"));
        QCOMPARE(spy.count(), 2);
        QVERIFY(!vm.setAndNotify(vm.tracked, TrackedValue{0}, &NotifyVm::changed));
        QCOMPARE(vm.tracked.assignments, 0);
        QVERIFY(vm.setAndNotify(vm.tracked, TrackedValue{2}, &NotifyVm::changed));
        QCOMPARE(vm.tracked.assignments, 1);
        QCOMPARE(spy.count(), 3);
    }

    void parentOwnsChildren()
    {
        auto parent = std::make_unique<ViewModelBase>();
        auto *child = new NotifyVm(parent.get());
        QPointer<NotifyVm> weak = child;
        QSignalSpy destroyed(child, &QObject::destroyed);
        parent.reset();
        QVERIFY(!weak);
        QCOMPARE(destroyed.count(), 1);
    }

    void invalidNotificationDoesNotWrite()
    {
        NotifyVm vm;
        QSignalSpy spy(&vm, &NotifyVm::changed);
        void (NotifyVm::*nullSignal)() = nullptr;
        QTest::ignoreMessage(QtWarningMsg, "ViewModelBase::setAndNotify: 信号为空或对象类型不兼容");
        QVERIFY(!vm.setAndNotify(vm.value, 2, nullSignal));
        QTest::ignoreMessage(QtWarningMsg, "ViewModelBase::setAndNotify: 信号为空或对象类型不兼容");
        QVERIFY(!vm.setAndNotify(vm.value, 2, &ShellViewModel::countChanged));
        QCOMPARE(vm.value, 0);
        QCOMPARE(spy.count(), 0);
    }

    void shellNotificationsAndBoundaries()
    {
        ShellViewModel vm;
        QSignalSpy count(&vm, &ShellViewModel::countChanged);
        QSignalSpy increment(&vm, &ShellViewModel::canIncrementChanged);
        QSignalSpy addTwo(&vm, &ShellViewModel::canAddTwoChanged);
        QSignalSpy reset(&vm, &ShellViewModel::canResetChanged);
        QString observed;
        connect(&vm, &ShellViewModel::countChanged, this, [&] { observed = vm.message(); });
        QCOMPARE(vm.message(), QStringLiteral("已点击 0 次"));
        QCOMPARE(vm.incrementText(), QStringLiteral("增加"));
        vm.reset();
        QCOMPARE(count.count(), 0);
        vm.increment();
        QCOMPARE(observed, QStringLiteral("已点击 1 次"));
        QCOMPARE(count.count(), 1);
        QCOMPARE(increment.count(), 0);
        QCOMPARE(reset.count(), 1);
        for (int i = 0; i < 10; ++i)
            vm.increment();
        QCOMPARE(vm.count(), 5);
        QCOMPARE(count.count(), 5);
        QCOMPARE(increment.count(), 1);
        QVERIFY(!vm.canIncrement());
        QVERIFY(!vm.canAddTwo());
        QCOMPARE(addTwo.count(), 1);
        vm.reset();
        QCOMPARE(vm.count(), 0);
        QCOMPARE(count.count(), 6);
        QCOMPARE(increment.count(), 2);
        QCOMPARE(reset.count(), 2);
        QVERIFY(!vm.canReset());
        QVERIFY(vm.canAddTwo());
        QCOMPARE(addTwo.count(), 2);
    }

    void shellAddBoundaries_data()
    {
        QTest::addColumn<int>("initial");
        QTest::addColumn<int>("delta");
        for (int initial = 0; initial <= 5; ++initial) {
            for (int delta : {std::numeric_limits<int>::min(), -1, 0, 1, 2, 3, 5, 6,
                              std::numeric_limits<int>::max()}) {
                const QByteArray row = QByteArray::number(initial) + "/" + QByteArray::number(delta);
                QTest::newRow(row.constData()) << initial << delta;
            }
        }
    }

    void shellAddBoundaries()
    {
        QFETCH(int, initial);
        QFETCH(int, delta);
        ShellViewModel vm;
        for (int i = 0; i < initial; ++i)
            vm.increment();
        QSignalSpy count(&vm, &ShellViewModel::countChanged);
        QSignalSpy increment(&vm, &ShellViewModel::canIncrementChanged);
        QSignalSpy addTwo(&vm, &ShellViewModel::canAddTwoChanged);
        QSignalSpy reset(&vm, &ShellViewModel::canResetChanged);
        const bool accepted = delta > 0 && delta <= 5 - initial;
        const int expected = accepted ? initial + delta : initial;
        vm.add(delta);
        QCOMPARE(vm.count(), expected);
        QCOMPARE(vm.message(), QStringLiteral("已点击 %1 次").arg(expected));
        QCOMPARE(vm.canIncrement(), expected < 5);
        QCOMPARE(vm.canAddTwo(), expected <= 3);
        QCOMPARE(vm.canReset(), expected > 0);
        QCOMPARE(count.count(), accepted ? 1 : 0);
        QCOMPARE(increment.count(), int((initial < 5) != (expected < 5)));
        QCOMPARE(addTwo.count(), int((initial <= 3) != (expected <= 3)));
        QCOMPARE(reset.count(), int((initial > 0) != (expected > 0)));
    }

};

QTEST_GUILESS_MAIN(CoreTests)
#include "tst_core.moc"
