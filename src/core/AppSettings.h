#ifndef APPSETTINGS_H
#define APPSETTINGS_H

#include <QString>
#include <QStringList>

#include "BreathTypes.h"

// ============================================================
// 配置读写（全部存本地，QSettings 自动落到注册表）
//   · 每种呼吸各自的节奏（吸 / 屏 / 呼秒数）
//   · 四个开关：语音 / 示意图 / 动画 / 节拍
//   · 当前音色（yunxi / xiaoxiao）
// ============================================================
namespace AppSettings {

// 用户自定义的一套节奏
struct Rhythm {
    int inhale = 4;
    int hold   = 2;
    int exhale = 6;
    // 有些呼吸一轮里有两次屏息（比如箱式 吸-屏-呼-屏），
    // 第二次单独用 hold2，这样设置里可以分开调。
    int hold2  = 2;
};

// 读取某种呼吸的自定义节奏；没设置过就返回它的默认值
Rhythm rhythmFor(const BreathType &type);

// 保存自定义节奏（三个值都会做范围限制）
void saveRhythm(const QString &code, const Rhythm &r);

// 恢复某种呼吸的默认节奏（删除自定义记录）
void resetRhythm(const QString &code);

// 是否设置过自定义节奏
bool hasCustomRhythm(const QString &code);

// 把自定义节奏套到一种呼吸上，返回改好的副本。
// 原理：按阶段类型重新赋秒数（吸气的阶段统一用新吸气秒数），
//       这样 12 种呼吸（包括交替鼻孔那种四段式）都能统一处理。
BreathType applyRhythm(const BreathType &type, const Rhythm &r);

// 从配置里读出该呼吸最终的参数（有自定义用自定义，没有用默认）
BreathType tunedBreath(const BreathType &type);

// ---- 四个开关 ----
bool voiceEnabled();
void setVoiceEnabled(bool v);
bool tickEnabled();
void setTickEnabled(bool v);
bool diagramEnabled();
void setDiagramEnabled(bool v);
bool animationEnabled();
void setAnimationEnabled(bool v);

// ---- 音色 ----
QString voiceId();                 // "yunxi" / "xiaoxiao"
void setVoiceId(const QString &id);

// ---- 背景音乐 ----
bool musicEnabled();               // 默认开，音量很低，不盖语音
void setMusicEnabled(bool v);
// 曲目：内置五音用 gong/shang/jue/zhi/yu；
// 用户自己放进 music 文件夹的音乐用 "file:绝对路径" 形式保存
QString musicTrack();
void setMusicTrack(const QString &id);

// ---- 桌面球的呼吸节奏 ----
// 返回呼吸代码（fushi / 478 ...）；空字符串表示"关闭节奏，只轻微呼吸"
QString deskBreath();
void setDeskBreath(const QString &code);

// ---- 新手导引 ----
// 只记录“是否已经看过”，手动从「关于 → 使用导引」打开不受它限制。
bool guideShown();
void setGuideShown(bool v);

// ---- 久坐提醒 ----
bool sitReminderEnabled();                 // 总开关，默认开
void setSitReminderEnabled(bool v);

int sitMinutes();                          // 坐多久提醒一次，默认 45 分钟
void setSitMinutes(int v);                 // 允许 20 ~ 120

int standMinutes();                        // 提醒后活动多久，默认 5 分钟
void setStandMinutes(int v);               // 允许 1 ~ 30

// 提醒方式：0 = 全屏白色遮罩（默认，最抓眼球）
//           1 = 球旁边的小气泡（温和，不打断）
int sitReminderStyle();
void setSitReminderStyle(int style);

// 免打扰时段，每段是 "HH:mm-HH:mm" 的字符串，可以有多段。
// 开始时间比结束时间晚表示跨夜（比如 18:30-09:00）。
QStringList skipPeriods();
void setSkipPeriods(const QStringList &periods);

// ---- 开机自启 ----
// 实现方式：往注册表 HKCU\Software\Microsoft\Windows\CurrentVersion\Run
// 写一条名叫 RelaxAssistant 的值，值就是本程序的完整路径。
// 这样做不需要管理员权限，也不依赖"启动"文件夹。
bool autoStart();
void setAutoStart(bool on);

// ---- 身体放松定时计划（A 方案：轻量气泡提醒） ----
// 每条计划：到点了就弹一个气泡"到点啦 · 做 XX 拉伸"，不强制全屏，
// 点「开始跟练」就开拉伸窗，自动消失；叉掉就只是关掉。
// 同一分钟只弹一次（避免每分钟都扫到重复弹）。
//
// repeat = "" 表示每天都弹；
// repeat 是星期几列表（QStringList），"1"=周一 ... "7"=周日，
// 也可以 "1,3,5" 这样逗号分隔。
struct RelaxPlan {
    QString id;          // 唯一 id（随便什么字符串，比如时间戳）
    QString time;        // "HH:mm"
    QString kind;        // 计划类型："stretch"=身体放松，"breath"=呼吸练习
    QString action;      // 动作代号（kind=stretch）或呼吸代号（kind=breath）
    QStringList repeat;  // 星期几重复（空 = 每天）
    bool enabled = true;
};

QList<RelaxPlan> relaxPlans();
void setRelaxPlans(const QList<RelaxPlan> &plans);

// ---- 完成记录（按天汇总，永久保存） ----
// 只记"每天做了几次"，不记明细，所以数据极小（一年几 KB），不需要清理。
// kind："breath" = 呼吸练习，"stretch" = 身体放松
void recordCompletion(const QString &kind);
int todayCount(const QString &kind);        // 今天这类做了几次
int todayTotalCount();                      // 今天总共做了几次
int totalCount();                           // 累计做了几次
int streakDays();                           // 连续坚持了几天（今天没做就从昨天算）
// 最近 n 天每天的总次数，索引 0 = 最早的一天，最后一个是今天
QList<int> recentDayCounts(int n);

}

#endif // APPSETTINGS_H
