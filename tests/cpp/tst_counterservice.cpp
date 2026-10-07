#include <CounterService.h>
#include <QtTest>
#include <limits>

class CounterServiceTests : public QObject
{
    Q_OBJECT
private slots:
    void boundaries_data()
    {
        QTest::addColumn<int>("initial");
        QTest::addColumn<int>("delta");
        for (int initial = 0; initial <= 5; ++initial) {
            for (int delta : {std::numeric_limits<int>::min(), -1, 0, 1, 2, 3, 5, 6,
                              std::numeric_limits<int>::max()}) {
                const auto row = QByteArray::number(initial) + "/" + QByteArray::number(delta);
                QTest::newRow(row.constData()) << initial << delta;
            }
        }
    }

    void boundaries()
    {
        QFETCH(int, initial);
        QFETCH(int, delta);
        CounterService service;
        QVERIFY(!service.parent());
        QCOMPARE(service.count(), 0);
        if (initial)
            service.add(initial);
        QSignalSpy changed(&service, &CounterService::countChanged);
        const bool accepted = delta > 0 && delta <= 5 - initial;
        QCOMPARE(service.canAdd(delta), accepted);
        service.add(delta);
        QCOMPARE(service.count(), accepted ? initial + delta : initial);
        QCOMPARE(changed.count(), int(accepted));
        const bool resetChanges = service.count() != 0;
        service.reset();
        QCOMPARE(service.count(), 0);
        QCOMPARE(changed.count(), int(accepted) + int(resetChanges));
        service.reset();
        QCOMPARE(changed.count(), int(accepted) + int(resetChanges));
    }
};

QTEST_GUILESS_MAIN(CounterServiceTests)
#include "tst_counterservice.moc"
