#ifndef STRETCHLIB_H
#define STRETCHLIB_H

#include <QString>
#include <QVector>

// ============================================================
// 身体放松 · 动作库
// ------------------------------------------------------------
// 这里只放"数据"：每个动作由若干步骤组成，每一步包含
//   标题、口语化说明、保持秒数、姿势参数、叠加箭头类型
// 具体怎么画由 ui/BodyPoseView 负责（它按 Pose 画简笔人）。
//
// 视角约定：
//   肩颈 3 个 + 转体 + 侧屈 → 正面（front）
//   髋屈伸 → 侧面（side）
// ============================================================

// 一个"姿势"的参数。数值都是"度"或 0~1 的强度，含义写在各字段后面。
struct Pose {
    double headTilt = 0.0;      // 头部侧屈角度，正数 = 向画面右侧倾（耳朵找右肩）
    double torsoBend = 0.0;     // 躯干侧屈角度，正数 = 向画面右侧弯
    double shoulderUp = 0.0;    // 双肩上提强度 0~1（肩绕环"向上向前"那一步）
    double shoulderBack = 0.0;  // 双肩后收下沉强度 0~1（肩绕环"向后向下"、胸廓打开）
    int arms = 0;               // 手臂姿势：
                                //   0 = 自然下垂
                                //   1 = 双手抱脑后（胸廓打开）
                                //   2 = 单臂举过头顶（侧屈）
                                //   3 = 一手放对侧膝、一手扶椅背（转体）
    double kneeLift = 0.0;      // 侧面视角：抬膝角度（0~90）
    bool holdShin = false;      // 侧面视角：双手抱小腿（髋屈伸）
};

// 一个步骤
struct StretchStep {
    QString title;          // "第 1 步 · 准备"
    QString text;           // 口语化步骤说明
    QString arrow;          // 叠加箭头：none / tilt-right / tilt-left /
                            // circle / open / twist-left / twist-right /
                            // bend-left / bend-right / lift
    int seconds = 10;       // 保持多少秒
    Pose pose;              // 这一步身体要摆成什么样
    QString image;          // 本步的图示（qrc 路径）；空 = 用程序画的简笔人
    QString image2;         // 可选：和 image 交替显示，表现循环动作
    int swapMs = 2000;      // 两张图的切换周期（毫秒），应配合动作/呼吸节奏
    QString phaseA;         // 显示 image 时的动作相位名（如"打开"）
    QString phaseB;         // 显示 image2 时的动作相位名（如"收回"）
    bool mirror = false;    // 图只画了单侧，另一侧程序里自动镜像
};

// 一个完整动作
struct StretchAction {
    QString code;           // 内部代号，菜单和设置里都用它
    QString name;           // 显示名
    QString view;           // "front" 正面 / "side" 侧面
    QString note;           // 全程注意事项（显示在窗口底部）
    QVector<StretchStep> steps;
};

namespace StretchLib {

// 全部 6 个动作
QVector<StretchAction> all();

// 按代号取一个动作，找不到返回 code 为空的默认值
StretchAction find(const QString &code);

} // namespace StretchLib

#endif // STRETCHLIB_H
