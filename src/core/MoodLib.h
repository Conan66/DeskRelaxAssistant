#ifndef MOODLIB_H
#define MOODLIB_H

#include <QList>
#include <QString>

// ============================================================
// 情绪陪伴语料库
// ------------------------------------------------------------
// 按 PRD FR-05 / FR-60~62 的"默念语句引导"实现：
//   用户选一个此刻的感受 → 显示一整段话 → 自己慢慢读一遍。
//
// 内置几类固定话术（写在代码里，随程序更新）；
// 用户也能自己加一类（存进 QSettings，永久保留）。
//
// 注意：这里只做"引导"，不记录用户选了哪种情绪。
// 记录负面情绪容易引发反刍（反复回想），反而有害。
// ============================================================
struct MoodEntry
{
    QString id;       // 唯一标识（内置用 "mood_stress" 这种，自定义用时间戳）
    QString name;     // 类型名，如"压力很大"
    QString text;     // 要读的那段话
    bool builtin = false;
};

namespace MoodLib {

// 内置的情绪类型（每类固定一段话）
QList<MoodEntry> builtin();

// 用户自己添加的类型
QList<MoodEntry> customs();

// 内置 + 自定义，按"先内置后自定义"的顺序
QList<MoodEntry> all();

// 添加一条自定义：名称最多 12 字，正文最多 500 字。
// 返回 true 表示添加成功（空内容会被拒绝）。
bool addCustom(const QString &name, const QString &text);

// 删除一条自定义（内置的不受影响）
void removeCustom(const QString &id);

} // namespace MoodLib

#endif // MOODLIB_H
