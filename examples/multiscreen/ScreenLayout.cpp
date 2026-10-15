#include "ScreenLayout.h"
#include <algorithm>

namespace MultiScreen {
std::optional<int> parseAutoClose(const QString &value)
{
    bool ok = false;
    const int seconds = value.toInt(&ok);
    if (!ok || seconds < 3 || seconds > 86400) return std::nullopt;
    return seconds;
}
std::optional<ScreenLayout> selectLayout(const QString &platform,
                                         const QList<ScreenMetrics> &screens, bool strict)
{
    if (screens.isEmpty()) return std::nullopt;
    bool four = platform == QStringLiteral("xcb") && screens.size() == 4;
    for (int i = 0; four && i < screens.size(); ++i)
        four = screens[i].geometry == QRect(i * 1920, 0, 1920, 1080)
            && qFuzzyCompare(screens[i].dpr, qreal(1));
    if (strict && !four) return std::nullopt;
    const QRect area = screens.front().available;
    if (!area.isValid()) return std::nullopt;
    if (four) return ScreenLayout{true,
        QRect(area.x() + (area.width()-1440)/2, area.y() + (area.height()-810)/2, 1440, 810),
        QRect(1920, 0, 5760, 1080)};
    const QSize mainSize(std::max(1, std::min(1440, area.width()*65/100)),
                         std::max(1, std::min(810, area.height()*65/100)));
    const QSize displaySize(std::max(1, std::min(1152, area.width()*75/100)),
                            std::max(1, std::min(288, area.height()*30/100)));
    return ScreenLayout{false,
        QRect(area.topLeft() + QPoint(area.width()/20, area.height()/20), mainSize),
        QRect(area.topLeft() + QPoint((area.width()-displaySize.width())*4/5,
                                      (area.height()-displaySize.height())*4/5), displaySize)};
}
}
