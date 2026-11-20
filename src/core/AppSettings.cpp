#include "AppSettings.h"

#include <QCoreApplication>
#include <QDate>
#include <QDir>
#include <QSettings>
#include <QStringList>

namespace {
// 统一用这一组 key，避免各处手写字符串写错
QString keyRhythm(const QString &code, const QString &field)
{
    return QStringLiteral("rhythm/%1/%2").arg(code, field);
}

QSettings &cfg()
{
    static QSettings s(QStringLiteral("RelaxAssistant"),
                       QStringLiteral("RelaxAssistant"));
    return s;
}

// 秒数范围限制（PRD FR-22：吸 2–8 / 屏 0–15 / 呼 3–12）
int clampInhale(int v) { return qBound(2, v, 8); }
int clampHold(int v)   { return qBound(0, v, 15); }
int clampExhale(int v) { return qBound(3, v, 12); }
}

namespace AppSettings {

// ============================================================
// 读取某种呼吸的自定义节奏
// ============================================================
Rhythm rhythmFor(const BreathType &type)
{
    Rhythm r;
    // 先用它自己的默认值打底。
    // 一轮里可能有两次屏息（箱式），第一次给 hold，第二次给 hold2，
    // 这样设置面板就能把两次屏息分开调。
    int holdCount = 0;
    for (const BreathPhase &p : type.phases) {
        switch (p.type) {
        case PhaseType::Inhale: r.inhale = p.seconds; break;
        case PhaseType::Hold:
            if (holdCount == 0) r.hold = p.seconds;
            else                r.hold2 = p.seconds;
            ++holdCount;
            break;
        case PhaseType::Exhale: r.exhale = p.seconds; break;
        }
    }

    // 有自定义就覆盖
    // 但像 4-7-8 这种"固定配方"，秒数是它有效的前提，一律用默认值，
    // 不读以前保存过的自定义（旧版本可能存过），也不让界面去改。
    if (type.fixedRhythm) return r;

    const QString prefix = QStringLiteral("rhythm/%1/").arg(type.code);
    if (cfg().contains(prefix + "inhale")) {
        r.inhale = clampInhale(cfg().value(prefix + "inhale", r.inhale).toInt());
        r.hold   = clampHold(cfg().value(prefix + "hold",   r.hold).toInt());
        r.exhale = clampExhale(cfg().value(prefix + "exhale", r.exhale).toInt());
        r.hold2  = clampHold(cfg().value(prefix + "hold2",  r.hold2).toInt());
    }
    return r;
}

void saveRhythm(const QString &code, const Rhythm &r)
{
    cfg().setValue(keyRhythm(code, "inhale"), clampInhale(r.inhale));
    cfg().setValue(keyRhythm(code, "hold"),   clampHold(r.hold));
    cfg().setValue(keyRhythm(code, "exhale"), clampExhale(r.exhale));
    cfg().setValue(keyRhythm(code, "hold2"),  clampHold(r.hold2));
    cfg().sync();
}

void resetRhythm(const QString &code)
{
    cfg().remove(QStringLiteral("rhythm/%1").arg(code));
    cfg().sync();
}

bool hasCustomRhythm(const QString &code)
{
    return cfg().contains(QStringLiteral("rhythm/%1/inhale").arg(code));
}

// ============================================================
// 把自定义节奏套到呼吸上
// 按阶段类型统一改写秒数，所以四段式的呼吸也能用
// ============================================================
BreathType applyRhythm(const BreathType &type, const Rhythm &r)
{
    BreathType t = type;
    int holdCount = 0;
    for (BreathPhase &p : t.phases) {
        switch (p.type) {
        case PhaseType::Inhale: p.seconds = r.inhale; break;
        case PhaseType::Hold:
            // 第一次屏息用 hold，第二次用 hold2
            p.seconds = (holdCount == 0) ? r.hold : r.hold2;
            ++holdCount;
            break;
        case PhaseType::Exhale: p.seconds = r.exhale; break;
        }
    }
    return t;
}

BreathType tunedBreath(const BreathType &type)
{
    // 固定配方的呼吸（4-7-8）不允许被改，直接原样返回
    if (type.fixedRhythm) return type;
    if (!hasCustomRhythm(type.code)) return type;
    return applyRhythm(type, rhythmFor(type));
}

// ============================================================
// 四个开关（默认全开）
// ============================================================
bool voiceEnabled() { return cfg().value("toggle/voice", true).toBool(); }
void setVoiceEnabled(bool v) { cfg().setValue("toggle/voice", v); }

bool tickEnabled() { return cfg().value("toggle/tick", true).toBool(); }
void setTickEnabled(bool v) { cfg().setValue("toggle/tick", v); }

bool diagramEnabled() { return cfg().value("toggle/diagram", true).toBool(); }
void setDiagramEnabled(bool v) { cfg().setValue("toggle/diagram", v); }

bool animationEnabled() { return cfg().value("toggle/animation", true).toBool(); }
void setAnimationEnabled(bool v) { cfg().setValue("toggle/animation", v); }

// ============================================================
// 音色
// ============================================================
QString voiceId()
{
    const QString v = cfg().value("voice/id", QStringLiteral("yunxi")).toString();
    return (v == QLatin1String("xiaoxiao")) ? v : QStringLiteral("yunxi");
}

void setVoiceId(const QString &id)
{
    cfg().setValue("voice/id", (id == QLatin1String("xiaoxiao")) ? id
                                                                 : QStringLiteral("yunxi"));
}

// ============================================================
// 背景音乐：默认打开、默认"羽音·沉静"，音量只有 18%
// ============================================================
bool musicEnabled() { return cfg().value("music/enabled", true).toBool(); }
void setMusicEnabled(bool v) { cfg().setValue("music/enabled", v); }

QString musicTrack()
{
    const QString id = cfg().value("music/track", QStringLiteral("yu")).toString();
    // 以 file: 开头的是用户自己放进 music 文件夹的音乐，不做白名单限制
    if (id.startsWith(QStringLiteral("file:"))) return id;

    static const QStringList valid = {QStringLiteral("gong"), QStringLiteral("shang"),
                                      QStringLiteral("jue"), QStringLiteral("zhi"),
                                      QStringLiteral("yu")};
    return valid.contains(id) ? id : QStringLiteral("yu");
}

void setMusicTrack(const QString &id)
{
    if (id.startsWith(QStringLiteral("file:"))) {
        cfg().setValue("music/track", id);
        return;
    }

    static const QStringList valid = {QStringLiteral("gong"), QStringLiteral("shang"),
                                      QStringLiteral("jue"), QStringLiteral("zhi"),
                                      QStringLiteral("yu")};
    cfg().setValue("music/track", valid.contains(id) ? id : QStringLiteral("yu"));
}

// ============================================================
// 桌面球的呼吸节奏（默认腹式呼吸）
// ============================================================
QString deskBreath()
{
    return cfg().value("desk/breath", QStringLiteral("fushi")).toString();
}

void setDeskBreath(const QString &code)
{
    cfg().setValue("desk/breath", code);
}

// ============================================================
// 新手导引：第一次正常启动时自动显示，看完或跳过后记住状态
// ============================================================
bool guideShown()
{
    return cfg().value(QStringLiteral("guide/shown"), false).toBool();
}

void setGuideShown(bool v)
{
    cfg().setValue(QStringLiteral("guide/shown"), v);
    cfg().sync();   // 立即保存，避免看完导引后马上退出导致下次又弹一次
}

// ============================================================
// 久坐提醒
// ============================================================
bool sitReminderEnabled() { return cfg().value("sit/enabled", true).toBool(); }
void setSitReminderEnabled(bool v) { cfg().setValue("sit/enabled", v); }

int sitMinutes()
{
    // 夹在 20~120 分钟之间，防止配置被改坏后出现 0 分钟疯狂提醒
    return qBound(20, cfg().value("sit/minutes", 45).toInt(), 120);
}
void setSitMinutes(int v) { cfg().setValue("sit/minutes", qBound(20, v, 120)); }

int standMinutes()
{
    return qBound(1, cfg().value("sit/standMinutes", 5).toInt(), 30);
}
void setStandMinutes(int v) { cfg().setValue("sit/standMinutes", qBound(1, v, 30)); }

int sitReminderStyle()
{
    const int s = cfg().value("sit/style", 0).toInt();
    return (s == 1) ? 1 : 0;
}
void setSitReminderStyle(int style) { cfg().setValue("sit/style", (style == 1) ? 1 : 0); }

QStringList skipPeriods()
{
    // 存成 "12:00-13:30,18:30-09:00" 这种一行字符串，读出来再拆开
    const QString raw = cfg().value("sit/skip").toString();
    QStringList out;
    for (const QString &one : raw.split(QLatin1Char(','), Qt::SkipEmptyParts)) {
        const QString s = one.trimmed();
        if (!s.isEmpty()) out << s;
    }
    return out;
}

void setSkipPeriods(const QStringList &periods)
{
    cfg().setValue("sit/skip", periods.join(QLatin1Char(',')));
}

// ============================================================
// 开机自启
// ------------------------------------------------------------
// 用的是 Windows 的"当前用户"启动项：
//   HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Run
// 在里面放一条 RelaxAssistant = "程序完整路径"。
// 注册表里有没有这条值，就是唯一真相（用户手动删掉也能正确显示）。
// ============================================================
namespace {
// 这个 Run 键的路径，单独抽出来避免两处写不一致
const char *kRunKey =
    "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run";
const char *kRunValue = "RelaxAssistant";
}

bool autoStart()
{
    // NativeFormat 才能直接读写注册表（而不是写成 ini 文件）
    QSettings run(QString::fromLatin1(kRunKey), QSettings::NativeFormat);
    return run.contains(QString::fromLatin1(kRunValue));
}

void setAutoStart(bool on)
{
    QSettings run(QString::fromLatin1(kRunKey), QSettings::NativeFormat);

    if (on) {
        // 路径可能带空格（比如 "f:\my Apps\..."），必须用引号包起来，
        // 否则 Windows 会把它当成"程序 + 参数"来解析
        const QString exe = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
        run.setValue(QString::fromLatin1(kRunValue), QStringLiteral("\"%1\"").arg(exe));
    } else {
        run.remove(QString::fromLatin1(kRunValue));
    }
    run.sync();
}

// ============================================================
// 身体放松定时计划（A 方案：轻量气泡提醒）
// ------------------------------------------------------------
// 存成 QVariantList，每条计划一个 QVariantMap，字段：
//   id / time / action / repeat / enabled
// 这样未来要在设置界面加"编辑计划"时直接读出来就能填表。
// ============================================================
QList<RelaxPlan> relaxPlans()
{
    QList<RelaxPlan> out;
    const QVariantList list = cfg().value("stretch/plans", QVariantList()).toList();
    for (const QVariant &v : list) {
        const QVariantMap m = v.toMap();
        RelaxPlan p;
        p.id      = m.value("id").toString();
        p.time    = m.value("time").toString();
        p.kind    = m.value("kind", QStringLiteral("stretch")).toString();
        p.action  = m.value("action").toString();
        p.enabled = m.value("enabled", true).toBool();
        // repeat 存的是逗号分隔的字符串（"1,3,5"），读出来拆成 QStringList
        const QString rawRepeat = m.value("repeat").toString();
        for (const QString &day : rawRepeat.split(QLatin1Char(','), Qt::SkipEmptyParts)) {
            const QString d = day.trimmed();
            if (!d.isEmpty()) p.repeat << d;
        }
        // 三个关键字段都得有，否则当脏数据丢掉
        if (!p.id.isEmpty() && !p.time.isEmpty() && !p.action.isEmpty()) {
            if (p.kind.isEmpty()) p.kind = QStringLiteral("stretch");   // 老数据兼容
            out << p;
        }
    }

    // 如果一个计划都没有，给用户塞两个示例，让界面打开时不会空空如也。
    // 注释里说明：用户可以删掉，或者直接改时间/动作。
    if (out.isEmpty()) {
        // 示例 1：工作日 10:00 做肩绕环（身体放松类）
        // 示例 2：工作日 15:00 做腹式呼吸（呼吸练习类）
        // 这样用户一眼能看出"计划和呼吸都能排进去"，可以直接改成自己想要的
        QStringList weekdays = {QStringLiteral("1"), QStringLiteral("2"),
                                QStringLiteral("3"), QStringLiteral("4"),
                                QStringLiteral("5")};
        RelaxPlan shoulder;
        shoulder.id     = QStringLiteral("demo_1");
        shoulder.time   = QStringLiteral("10:00");
        shoulder.kind   = QStringLiteral("stretch");
        shoulder.action = QStringLiteral("shoulder_circle");
        shoulder.repeat = weekdays;
        shoulder.enabled = true;

        RelaxPlan breath;
        breath.id     = QStringLiteral("demo_2");
        breath.time   = QStringLiteral("15:00");
        breath.kind   = QStringLiteral("breath");
        breath.action = QStringLiteral("fushi");
        breath.repeat = weekdays;
        breath.enabled = true;

        out << shoulder << breath;
    }
    return out;
}

void setRelaxPlans(const QList<RelaxPlan> &plans)
{
    QVariantList list;
    for (const RelaxPlan &p : plans) {
        QVariantMap m;
        m["id"]      = p.id;
        m["time"]    = p.time;
        m["kind"]    = p.kind;
        m["action"]  = p.action;
        m["enabled"] = p.enabled;
        // repeat 存成一行逗号分隔的字符串，读的时候拆
        m["repeat"]  = p.repeat.join(QLatin1Char(','));
        list << m;
    }
    cfg().setValue("stretch/plans", list);
    cfg().sync();
}

// ============================================================
// 完成记录（按天汇总，永久保存）
// ------------------------------------------------------------
// 存储结构：{"2026-11-18": {"breath": 2, "stretch": 1}, ...}
// 每天一条，天然去重、天然很小，所以不做定期清理。
// ============================================================
namespace {

// 日期统一转成 "yyyy-MM-dd" 作为 key
QString dayKey(const QDate &d)
{
    return d.toString(QStringLiteral("yyyy-MM-dd"));
}

// 读出所有天的记录
QVariantMap dailyRecords()
{
    return cfg().value(QStringLiteral("records/daily")).toMap();
}

void saveDailyRecords(const QVariantMap &m)
{
    cfg().setValue(QStringLiteral("records/daily"), m);
    cfg().sync();
}

// 某一天的总次数（不管呼吸还是拉伸，都加起来）
int dayTotal(const QVariantMap &day)
{
    int n = 0;
    for (auto it = day.constBegin(); it != day.constEnd(); ++it)
        n += it.value().toInt();
    return n;
}

} // namespace

void recordCompletion(const QString &kind)
{
    QVariantMap all = dailyRecords();
    const QString k = dayKey(QDate::currentDate());
    QVariantMap day = all.value(k).toMap();
    day[kind] = day.value(kind).toInt() + 1;   // 同类计数 +1
    all[k] = day;
    saveDailyRecords(all);
}

int todayCount(const QString &kind)
{
    const QVariantMap day = dailyRecords()
                                .value(dayKey(QDate::currentDate()))
                                .toMap();
    return day.value(kind).toInt();
}

int todayTotalCount()
{
    return dayTotal(dailyRecords().value(dayKey(QDate::currentDate())).toMap());
}

int totalCount()
{
    // 注意：必须先把 map 存进局部变量，
    // 否则 begin()/end() 来自两个不同的临时对象，迭代会出错
    const QVariantMap all = dailyRecords();
    int n = 0;
    for (auto it = all.constBegin(); it != all.constEnd(); ++it)
        n += dayTotal(it.value().toMap());
    return n;
}

int streakDays()
{
    const QVariantMap all = dailyRecords();
    int streak = 0;
    QDate d = QDate::currentDate();
    // 今天还没做的话，从昨天开始数——不打断已有的连续记录
    if (!all.contains(dayKey(d))) d = d.addDays(-1);
    while (all.contains(dayKey(d))) {
        if (dayTotal(all.value(dayKey(d)).toMap()) <= 0) break;
        ++streak;
        d = d.addDays(-1);
    }
    return streak;
}

QList<int> recentDayCounts(int n)
{
    const QVariantMap all = dailyRecords();
    QList<int> out;
    for (int i = n - 1; i >= 0; --i) {
        const QString k = dayKey(QDate::currentDate().addDays(-i));
        out << dayTotal(all.value(k).toMap());
    }
    return out;
}

} // namespace AppSettings
