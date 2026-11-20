#include "MoodLib.h"

#include <QDateTime>
#include <QSettings>
#include <QVariantList>
#include <QVariantMap>

namespace {

// 和 AppSettings 用同一套配置，用户换电脑前数据都在一个地方
QSettings &cfg()
{
    static QSettings s(QStringLiteral("RelaxAssistant"),
                       QStringLiteral("RelaxAssistant"));
    return s;
}

// 自定义内容的长度上限：类型名短一点，正文一段话就够，不要太长
const int kMaxNameLen = 12;
const int kMaxTextLen = 500;

// 自定义类型在 QSettings 里的 key
const char *kCustomKey = "mood/customs";

} // namespace

namespace MoodLib {

// ============================================================
// 内置的六类情绪陪伴话术
// ------------------------------------------------------------
// 每一条都遵循三个原则：
//   1) 先接纳情绪（不评判、不急着纠正）
//   2) 给一个具体的身体动作（放肩、呼气、感受支撑）
//   3) 给一个可以带走的小观点
// 这样读完是"被理解 + 有事可做"，而不是空泛的安慰。
// ============================================================
QList<MoodEntry> builtin()
{
    QList<MoodEntry> out;

    MoodEntry stress;
    stress.id = QStringLiteral("mood_stress");
    stress.name = QStringLiteral("压力很大");
    stress.builtin = true;
    stress.text = QStringLiteral(
        "肩膀是不是不自觉地耸着、像扛了什么？脖子也有点僵？\n\n"
        "好，先不要去想那一堆事。就现在这一秒，深呼吸一次——"
        "吸气的时候肩膀悄悄落下来，呼气的时候让它们离耳朵远一点。\n\n"
        "压力不会一下消失，但你可以一次只扛一件。"
        "现在把手头要做的事里，圈出最紧急的那一件，其他的，先放一放。\n\n"
        "再呼吸三次。吸气告诉自己「我在」，呼气告诉自己「可以慢一点」。");
    out << stress;

    MoodEntry irritated;
    irritated.id = QStringLiteral("mood_irritated");
    irritated.name = QStringLiteral("心里烦躁");
    irritated.builtin = true;
    irritated.text = QStringLiteral(
        "胸口堵得慌、想摔东西或者怼人？明明没多大事儿，就是压不住？\n\n"
        "没关系，不用立刻压下去。烦躁来了，就让它在那儿待一会儿——"
        "就像一阵风，你越抓它跑得越快。\n\n"
        "现在慢慢吐一口长气，比吸气再长一点。"
        "感受脚底踩在地面上的温度，感受椅子稳稳地在托住你。\n\n"
        "喝一口水，或者抬头看窗外三秒钟。等这口气呼完，你再决定要不要说、要不要做。");
    out << irritated;

    MoodEntry tired;
    tired.id = QStringLiteral("mood_tired");
    tired.name = QStringLiteral("感觉很累");
    tired.builtin = true;
    tired.text = QStringLiteral(
        "眼皮抬不动了？盯着屏幕看东西发虚？脑子转一下要花半秒？\n\n"
        "累不是因为你不够努力，是身体在跟你说「我要歇一下」。\n\n"
        "现在就让自己松下来，不用把状态调整到最好。"
        "让眼皮的重量自然落下，感受身体一点点陷进椅子里，背可以靠回去一点。\n\n"
        "如果现在能起身接一杯水，就起身接一杯。"
        "如果五分钟什么都不做，那就什么都不做。这不是浪费时间，这是在给身体充电。");
    out << tired;

    MoodEntry anxious;
    anxious.id = QStringLiteral("mood_anxious");
    anxious.name = QStringLiteral("有点焦虑");
    anxious.builtin = true;
    anxious.text = QStringLiteral(
        "心里慌慌的、一直在想「万一…」对吗？手心出汗？或者脑子里的念头一个接一个停不下来？\n\n"
        "大脑有个毛病：它很擅长在心里预演还没发生的坏事，有时预演得太逼真，你都分不清是过去还是现在了。\n\n"
        "试着把注意力拉回此刻——看看眼前是什么颜色，听听现在有什么声音，"
        "摸一下手边的东西是什么质感。你担心的那件事，此刻并没有发生。\n\n"
        "一次只想二十四小时以内的事。明天的，交给明天的你。");
    out << anxious;

    MoodEntry low;
    low.id = QStringLiteral("mood_low");
    low.name = QStringLiteral("情绪低落");
    low.builtin = true;
    low.text = QStringLiteral(
        "今天不太想说话？什么都提不起劲？或者心里闷闷的，说不上为什么，但就是开心不起来？\n\n"
        "不用急着振作，不用逼自己「快好起来」。"
        "情绪低落就像下雨——你没法让雨停，只能撑一把伞、或者找个屋檐先躲一躲。\n\n"
        "现在慢慢吸一口气，再慢慢呼出来。就做这一件事就够了。\n\n"
        "如果愿意，起身去接一杯水，或者走到窗边看看天。"
        "光是慢慢来的，你不需要现在就抓住它。");
    out << low;

    MoodEntry wronged;
    wronged.id = QStringLiteral("mood_wronged");
    wronged.name = QStringLiteral("受了委屈");
    wronged.builtin = true;
    wronged.text = QStringLiteral(
        "明明没做错，或者已经很努力了，还是被说、被误会、被责怪？"
        "心里堵得慌，想辩解又不想开口？\n\n"
        "你的感受是真实的，不用向谁证明你委屈，也不用立刻让自己「大度」。\n\n"
        "现在把手轻轻放在胸口，感受一下心跳。对自己说一句："
        "我知道你难受，我陪着你。\n\n"
        "等你缓过来，再决定要不要说、要怎么说。现在，先把自己照顾好。");
    out << wronged;

    return out;
}

// ============================================================
// 用户自定义的类型
// ============================================================
QList<MoodEntry> customs()
{
    QList<MoodEntry> out;
    const QVariantList list = cfg().value(QLatin1String(kCustomKey)).toList();
    for (const QVariant &v : list) {
        const QVariantMap m = v.toMap();
        MoodEntry e;
        e.id = m.value(QStringLiteral("id")).toString();
        e.name = m.value(QStringLiteral("name")).toString();
        e.text = m.value(QStringLiteral("text")).toString();
        e.builtin = false;
        // 缺字段的脏数据直接跳过
        if (!e.id.isEmpty() && !e.name.isEmpty() && !e.text.isEmpty())
            out << e;
    }
    return out;
}

QList<MoodEntry> all()
{
    QList<MoodEntry> out = builtin();
    out += customs();
    return out;
}

bool addCustom(const QString &name, const QString &text)
{
    // 去掉首尾空白后再判断，避免用户只敲了空格
    const QString n = name.trimmed();
    const QString t = text.trimmed();
    if (n.isEmpty() || t.isEmpty()) return false;
    if (n.size() > kMaxNameLen || t.size() > kMaxTextLen) return false;

    QVariantList list = cfg().value(QLatin1String(kCustomKey)).toList();

    QVariantMap m;
    m[QStringLiteral("id")] = QStringLiteral("mood_%1")
                                  .arg(QDateTime::currentMSecsSinceEpoch());
    m[QStringLiteral("name")] = n;
    m[QStringLiteral("text")] = t;
    list << m;

    cfg().setValue(QLatin1String(kCustomKey), list);
    cfg().sync();
    return true;
}

void removeCustom(const QString &id)
{
    QVariantList list = cfg().value(QLatin1String(kCustomKey)).toList();
    for (int i = 0; i < list.size(); ++i) {
        if (list.at(i).toMap().value(QStringLiteral("id")).toString() == id) {
            list.removeAt(i);
            break;
        }
    }
    cfg().setValue(QLatin1String(kCustomKey), list);
    cfg().sync();
}

} // namespace MoodLib
