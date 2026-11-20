#ifndef BREATHTYPES_H
#define BREATHTYPES_H

#include <QColor>
#include <QString>
#include <QVector>

// ============================================================
// 呼吸阶段类型（对应 PRD 4.0.2 的三种球体状态）
//   Inhale 吸气：球从基准涨到最大
//   Hold   屏息：球完全静止（PRD 硬规定，不缩不抖不亮不暗）
//   Exhale 呼气：球从最大缩回基准
// ============================================================
enum class PhaseType {
    Inhale,
    Hold,
    Exhale
};

// 示意图上的气流路线。
// 大部分呼吸是"鼻 → 胸 → 腹"，但涌泉、扎根这类要想象气从脚底走，
// 肩颈和侧肋也各有自己的重点，所以不能共用同一条箭头路线。
enum class FlowRoute {
    NoseBelly,   // 鼻子 → 胸腔 → 腹部（默认）
    FootBelly,   // 脚底 → 小腿 → 大腿 → 腹部（涌泉 / 扎根）
    Shoulder,    // 肩膀随吸气轻轻上提、呼气下沉（肩颈）
    SideRib      // 肋骨向两侧打开、收回（侧肋）
};

// ============================================================
// 一个阶段：持续几秒 + 用哪一句语音 + 一句引导语
// ============================================================
struct BreathPhase {
    PhaseType type = PhaseType::Inhale;
    int seconds = 4;          // 该阶段持续秒数
    QString audioKey;         // 语音文件名后缀，如 "inhale" / "left_inhale"
    QString cue;              // 这一段的引导语（显示在球下方，教用户"怎么想"）
};

// ============================================================
// 一种呼吸方式（12 种之一）
// ============================================================
struct BreathType {
    QString code;                  // 代码，如 "fushi"，用于拼语音文件路径
    QString name;                  // 中文名，如 "腹式呼吸"
    QString intent;                // 一句话说明，显示在列表里
    QString intro;                 // 两三句的详细说明（做法 + 想象画面 + 适用场景）
    QString posture;               // 坐姿要求（靠不靠椅背、手放哪），界面上单独显示一行
    QVector<BreathPhase> phases;   // 一轮的完整阶段序列
    int defaultRounds = 12;        // 默认做几轮
    bool useFrontDiagram = false;  // 示意图朝向：true 正面 / false 侧面
    FlowRoute route = FlowRoute::NoseBelly; // 示意图气流箭头走哪条路

    // true = 秒数是"死"的，不允许用户在节奏设置里改。
    // 4-7-8 这类配方，比例本身就是它有效的原因，改了就不是这个练习了。
    bool fixedRhythm = false;

    // 一轮总秒数
    int cycleSeconds() const {
        int s = 0;
        for (const BreathPhase &p : phases) s += p.seconds;
        return s;
    }
    // 默认总时长（秒）
    int defaultTotalSeconds() const { return cycleSeconds() * defaultRounds; }
};

// ============================================================
// 12 种呼吸的默认参数库（数值来自 PRD 附录 A.2.1 的科学标准）
// ============================================================
namespace BreathLib {
    QVector<BreathType> all();
    BreathType find(const QString &code);
}

#endif // BREATHTYPES_H
