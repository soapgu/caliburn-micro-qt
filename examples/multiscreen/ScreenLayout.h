#pragma once
#include <QList>
#include <QRect>
#include <QString>
#include <optional>

namespace MultiScreen {
struct ScreenMetrics { QRect geometry; QRect available; qreal dpr = 1; };
struct ScreenLayout {
    bool fourScreen = false;
    QRect mainGeometry;
    QRect displayGeometry;
};
// 输入按横坐标排列；不改变系统显示设置。
std::optional<ScreenLayout> selectLayout(const QString &platform,
                                         const QList<ScreenMetrics> &screens, bool strict);
std::optional<int> parseAutoClose(const QString &value);
}
