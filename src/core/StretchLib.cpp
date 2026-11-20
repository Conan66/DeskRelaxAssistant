#include "StretchLib.h"

namespace {

// 小工具：少写一点重复代码
// 计时依据（ACSM/CSEP 办公拉伸建议）：
//   静态保持 10~30 秒；动态动作用呼吸节奏计数；
//   "轻微紧绷、不疼痛"；全程均匀呼吸不憋气。
StretchStep step(const QString &title, const QString &text,
                 const QString &arrow, int seconds, const Pose &pose,
                 const QString &image = QString(), bool mirror = false)
{
    StretchStep s;
    s.title = title;
    s.text = text;
    s.arrow = arrow;
    s.seconds = seconds;
    s.pose = pose;
    s.image = image;
    s.mirror = mirror;
    return s;
}

// ---------- 1) 颈侧屈（正面） ----------
// 图里的动作帧是"头倒向画面右侧"（= 用户的左侧），
// 所以：向左侧弯用原图，向右侧弯用镜像。
StretchAction makeNeckSide()
{
    StretchAction a;
    a.code = QStringLiteral("neck_side");
    a.name = QStringLiteral("颈侧屈（脖子放松）");
    a.view = QStringLiteral("front");
    a.note = QStringLiteral("头只往侧边倒，不低头、不转；对侧肩膀往下沉，千万别耸肩；全程自然呼吸，不要憋气。");
    a.steps << step(QStringLiteral("第 1 步 · 准备"),
                    QStringLiteral("坐直，双脚踩稳地面，双手放在大腿上，先做两次深呼吸。"),
                    QStringLiteral("none"), 5, Pose(),
                    QStringLiteral("assets/stretch/neck_side_1.png"));

    a.steps << step(QStringLiteral("第 2 步 · 头倒向右侧（保持 20 秒）"),
                    QStringLiteral("呼气时，慢慢把头顶向右侧倒，右耳去找右肩。保持自然呼吸：吸气时别用力，每一次呼气都让脖子再松一点。"),
                    QStringLiteral("tilt-right"), 20, Pose(),
                    QStringLiteral("assets/stretch/neck_side_2.png"), true);

    a.steps << step(QStringLiteral("第 3 步 · 慢慢回正"),
                    QStringLiteral("吸气，慢慢把头带回正中，别甩。"),
                    QStringLiteral("none"), 5, Pose(),
                    QStringLiteral("assets/stretch/neck_side_1.png"));

    a.steps << step(QStringLiteral("第 4 步 · 头倒向左侧（保持 20 秒）"),
                    QStringLiteral("换另一边：呼气时头顶向左侧倒，左耳找左肩；右肩往下沉。"),
                    QStringLiteral("tilt-left"), 20, Pose(),
                    QStringLiteral("assets/stretch/neck_side_2.png"));

    a.steps << step(QStringLiteral("第 5 步 · 结束"),
                    QStringLiteral("回正，轻轻转一下脖子，慢慢睁眼。"),
                    QStringLiteral("none"), 5, Pose(),
                    QStringLiteral("assets/stretch/neck_side_1.png"));
    return a;
}

// ---------- 2) 肩绕环（正面） ----------
StretchAction makeShoulderCircle()
{
    StretchAction a;
    a.code = QStringLiteral("shoulder_circle");
    a.name = QStringLiteral("肩绕环（放松肩颈）");
    a.view = QStringLiteral("front");
    a.note = QStringLiteral("是画完整的圆，不是耸肩；速度越慢越好，别甩；肩膀发酸就停下来休息。");
    a.steps << step(QStringLiteral("第 1 步 · 准备"),
                    QStringLiteral("坐直，双臂自然下垂，肩膀放平，先做一次深呼吸。"),
                    QStringLiteral("none"), 5, Pose(),
                    QStringLiteral("assets/stretch/shoulder_1.png"));

    a.steps << step(QStringLiteral("第 2 步 · 吸气：向上、向前"),
                    QStringLiteral("用约 4 秒吸气，双肩一起向上抬、再往前送，走到圆圈的最高点。注意不是耸肩。"),
                    QStringLiteral("circle"), 6, Pose(),
                    QStringLiteral("assets/stretch/shoulder_2.png"));

    a.steps << step(QStringLiteral("第 3 步 · 呼气：向后、向下"),
                    QStringLiteral("用约 4 秒呼气，双肩向后、向下沉到最低点，肩胛骨轻轻靠拢。"),
                    QStringLiteral("circle"), 6, Pose(),
                    QStringLiteral("assets/stretch/shoulder_3.png"));

    a.steps << step(QStringLiteral("第 4 步 · 再画两圈（跟图切换节奏）"),
                    QStringLiteral("再慢慢画两圈，每圈约 8 秒：吸气抬肩向前，呼气沉肩向后。图会跟着节奏切换。"),
                    QStringLiteral("circle"), 16, Pose(),
                    QStringLiteral("assets/stretch/shoulder_2.png"));
    // 循环动作：抬肩图 ↔ 沉肩图交替，周期和一次呼吸（8 秒）对齐
    a.steps.last().image2 = QStringLiteral("assets/stretch/shoulder_3.png");
    a.steps.last().swapMs = 4000;
    a.steps.last().phaseA = QStringLiteral("抬起 · 吸气");
    a.steps.last().phaseB = QStringLiteral("沉下 · 呼气");
    return a;
}

// ---------- 3) 胸廓打开（正面） ----------
StretchAction makeChestOpen()
{
    StretchAction a;
    a.code = QStringLiteral("chest_open");
    a.name = QStringLiteral("胸廓打开（打开胸口）");
    a.view = QStringLiteral("front");
    a.note = QStringLiteral("腰保持中立，不塌腰、不挺肚子；手只是轻轻托在脑后，别压头；觉得头晕就减小幅度。");
    a.steps << step(QStringLiteral("第 1 步 · 双手抱脑后"),
                    QStringLiteral("十指交叉放在脑后，两肘朝前收，背部坐直。"),
                    QStringLiteral("none"), 5, Pose(),
                    QStringLiteral("assets/stretch/chest_1.png"));

    a.steps << step(QStringLiteral("第 2 步 · 吸气：两肘打开"),
                    QStringLiteral("吸气，慢慢把两肘向两侧打开，胸口往前挺，肩胛骨靠拢。"),
                    QStringLiteral("open"), 10, Pose(),
                    QStringLiteral("assets/stretch/chest_2.png"));

    a.steps << step(QStringLiteral("第 3 步 · 保持（三次深呼吸）"),
                    QStringLiteral("保持打开，做三次深呼吸：吸 4 拍、呼 6 拍，呼气比吸气长，才更容易松下来。"),
                    QStringLiteral("none"), 12, Pose(),
                    QStringLiteral("assets/stretch/chest_2.png"));

    a.steps << step(QStringLiteral("第 4 步 · 收回再打开（跟图切换节奏）"),
                    QStringLiteral("配合呼吸来回两次：呼气收回、吸气打开，一个来回约 8 秒，跟着图切换的节奏走。"),
                    QStringLiteral("open"), 16, Pose(),
                    QStringLiteral("assets/stretch/chest_2.png"));
    // 循环动作：打开图 ↔ 收回图交替，4 秒一换 = 一次吸/呼各 4 秒
    a.steps.last().image2 = QStringLiteral("assets/stretch/chest_1.png");
    a.steps.last().swapMs = 4000;
    a.steps.last().phaseA = QStringLiteral("打开 · 吸气");
    a.steps.last().phaseB = QStringLiteral("收回 · 呼气");
    return a;
}

// ---------- 4) 坐姿转体（正面） ----------
// 图里的动作帧是"向画面右侧转"（= 用户的左侧），
// 所以：向左转用原图，向右转用镜像。
StretchAction makeTwist()
{
    StretchAction a;
    a.code = QStringLiteral("twist");
    a.name = QStringLiteral("坐姿转体（放松腰背）");
    a.view = QStringLiteral("front");
    a.note = QStringLiteral("脊柱保持直立，从腰部开始转，别用肩膀硬拧；骨盆和膝盖不要跟着转；转身时别憋气。");
    a.steps << step(QStringLiteral("第 1 步 · 坐椅子前缘"),
                    QStringLiteral("坐到椅子前半部分，双脚平放与肩同宽，脊柱坐直，双手放腿上。"),
                    QStringLiteral("none"), 5, Pose(),
                    QStringLiteral("assets/stretch/twist_1.png"));

    a.steps << step(QStringLiteral("第 2 步 · 向右转（保持 15 秒）"),
                    QStringLiteral("吸气坐高脊柱，呼气从腰部慢慢向右转：左手按在右腿膝盖上，轻轻往下压提供阻力；右手向后扶住右侧椅背稳住身体。每一次呼气再多转一点点。"),
                    QStringLiteral("twist-right"), 15, Pose(),
                    QStringLiteral("assets/stretch/twist_2.png"), true);

    a.steps << step(QStringLiteral("第 3 步 · 回正"),
                    QStringLiteral("吸气，慢慢转回中间。"),
                    QStringLiteral("none"), 5, Pose(),
                    QStringLiteral("assets/stretch/twist_1.png"));

    a.steps << step(QStringLiteral("第 4 步 · 向左转（保持 15 秒）"),
                    QStringLiteral("换另一边：吸气坐高，呼气从腰部慢慢向左转：右手按在左腿膝盖上提供阻力；左手向后扶住左侧椅背稳住身体。每一次呼气再多转一点点。"),
                    QStringLiteral("twist-left"), 15, Pose(),
                    QStringLiteral("assets/stretch/twist_2.png"));
    return a;
}

// ---------- 5) 坐姿侧屈（正面） ----------
// 图里的动作帧是"右臂上举、身体向画面右侧弯"（= 用户的左侧），
// 所以：向左侧弯用原图，向右侧弯用镜像。
StretchAction makeSideBend()
{
    StretchAction a;
    a.code = QStringLiteral("side_bend");
    a.name = QStringLiteral("坐姿侧屈（拉伸腰侧）");
    a.view = QStringLiteral("front");
    a.note = QStringLiteral("身体只往侧边弯，不往前、不旋转；另一侧肩膀别耸起来；手肘别锁死。");
    a.steps << step(QStringLiteral("第 1 步 · 准备"),
                    QStringLiteral("坐直，双脚平放，双手放在大腿上，肩膀放平。"),
                    QStringLiteral("none"), 5, Pose(),
                    QStringLiteral("assets/stretch/sidebend_1.png"));

    a.steps << step(QStringLiteral("第 2 步 · 向右侧弯（保持 15 秒）"),
                    QStringLiteral("吸气，左手举过头顶向上延展；呼气，身体慢慢向右侧弯，右手抓住椅子边。保持自然呼吸，每次呼气再弯深一点。"),
                    QStringLiteral("bend-right"), 15, Pose(),
                    QStringLiteral("assets/stretch/sidebend_2.png"), true);

    a.steps << step(QStringLiteral("第 3 步 · 回正"),
                    QStringLiteral("吸气，慢慢回到正中。"),
                    QStringLiteral("none"), 5, Pose(),
                    QStringLiteral("assets/stretch/sidebend_1.png"));

    a.steps << step(QStringLiteral("第 4 步 · 向左侧弯（保持 15 秒）"),
                    QStringLiteral("换另一边：吸气，右手举过头顶向上延展；呼气，身体向左侧弯，左手抓住椅子边。"),
                    QStringLiteral("bend-left"), 15, Pose(),
                    QStringLiteral("assets/stretch/sidebend_2.png"));
    return a;
}

// ---------- 6) 髋屈伸（侧面） ----------
StretchAction makeKneeChest()
{
    StretchAction a;
    a.code = QStringLiteral("knee_chest");
    a.name = QStringLiteral("髋屈伸（抱膝放松腰）");
    a.view = QStringLiteral("side");
    a.note = QStringLiteral("背保持直立，不要往后仰；另一只脚踩稳地面；腰有疼痛感就减小幅度，别硬拉。");
    a.steps << step(QStringLiteral("第 1 步 · 准备"),
                    QStringLiteral("坐在椅子上，双脚平放地面，背坐直，双手放腿上。"),
                    QStringLiteral("none"), 5, Pose(),
                    QStringLiteral("assets/stretch/knee_1.png"));

    a.steps << step(QStringLiteral("第 2 步 · 抱膝（保持 20 秒）"),
                    QStringLiteral("呼气时，一条腿的膝盖慢慢拉向胸口，双手抱住小腿；吸气时背挺直，每一次呼气再拉近一点点。"),
                    QStringLiteral("lift"), 20, Pose(),
                    QStringLiteral("assets/stretch/knee_2.png"));

    a.steps << step(QStringLiteral("第 3 步 · 放下"),
                    QStringLiteral("呼气，慢慢把脚放回地面，感受腰后面松开。"),
                    QStringLiteral("none"), 5, Pose(),
                    QStringLiteral("assets/stretch/knee_1.png"));

    a.steps << step(QStringLiteral("第 4 步 · 换另一条腿（保持 20 秒）"),
                    QStringLiteral("换另一条腿：呼气拉向胸口，双手抱小腿，背保持直立。"),
                    QStringLiteral("lift"), 20, Pose(),
                    QStringLiteral("assets/stretch/knee_2.png"), true);
    return a;
}

} // namespace

namespace StretchLib {

QVector<StretchAction> all()
{
    QVector<StretchAction> list;
    list << makeNeckSide()
         << makeShoulderCircle()
         << makeChestOpen()
         << makeTwist()
         << makeSideBend()
         << makeKneeChest();
    return list;
}

StretchAction find(const QString &code)
{
    for (const StretchAction &a : all()) {
        if (a.code == code) return a;
    }
    return StretchAction();
}

} // namespace StretchLib
