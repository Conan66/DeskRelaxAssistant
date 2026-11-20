#ifndef SITREMINDER_H
#define SITREMINDER_H

#include <QDateTime>
#include <QObject>
#include <QString>

class QTimer;

// ============================================================
// 久坐提醒（计时大脑）
// ------------------------------------------------------------
// 它只管"什么时候该提醒"，不负责画界面：
//   坐 45 分钟 → 发出 reminderDue() 让外面弹提醒
//   用户点掉提醒 → 进入 5 分钟活动期，每秒发 countdownChanged()
//   活动期结束 → 自动回到 45 分钟计时
//
// 两个"人不在这"的判断：
//   ① 锁屏（OpenInputDesktop 失败）→ 视为离开，直接重新计时
//   ② 电脑睡眠/休眠过（两次 tick 墙钟差很大）→ 醒来后重新计时
//
// 注意：**不看"鼠标多久没动"**。
// 看视频、等程序跑完、低头看手机都会让鼠标不动，
// 但人其实还坐着，这时候恰恰更需要提醒，不能当成离开。
// ============================================================
class SitReminder : public QObject
{
    Q_OBJECT
public:
    // 三种状态
    enum class State {
        Working,    // 正在计"坐了多久"
        Reminding,  // 提醒已弹出，等用户点掉
        Standing    // 活动期，正在倒计时
    };

    explicit SitReminder(QObject *parent = nullptr);

    // 按当前设置开始/重新开始计时（启动时、改完设置时都会调用）
    void start();
    // 手动重新计时：把已经坐的时间清零，从头再坐满一轮。
    // 用在右键气泡里的「重新计时」按钮。
    void restart();
    // 改完设置后重新读一遍（会从头计时，避免旧的进度让人困惑）
    void reloadSettings();

    // 用户点掉了提醒 → 进入活动期
    void dismiss();

    // 提醒挂了很久都没人点 → 当作人已经离开，重新计满
    // （外面用 5 分钟的超时计时器调用它）
    void ignoreReminder();

    // 右键点桌面球时显示的一句话：下一次提醒大概什么时候
    QString nextReminderText() const;

    State state() const { return m_state; }
    int remainSeconds() const { return m_remainSec; }

signals:
    void reminderDue();                          // 该提醒了
    // 提醒还挂着的时候，电脑锁屏/休眠或进入了免打扰时段：
    // 这时提醒已经没意义了，外面收到这个信号就把遮罩/气泡收掉
    void reminderCancelled();
    void standingStarted(int totalSeconds);      // 进入活动期
    void standingFinished();                     // 活动结束，重新开始久坐计时
    void countdownChanged(int remainSeconds, int state); // 每秒一次，给气泡更新用

private slots:
    void onTick();

private:
    void enterWorking();
    bool inSkipPeriod() const;      // 现在是不是在免打扰时段里
    // 下面几个是给"下次什么时候提醒"算时间用的
    bool inSkipPeriodAt(const QDateTime &t) const;        // 某个时刻是否在免打扰时段
    QDateTime skipEndAfter(const QDateTime &t) const;     // 这个时刻所在时段的结束时间
    QDateTime nextSkipStartAfter(const QDateTime &t) const; // t 之后最近一段免打扰的开始时间
    static bool isSessionLocked();  // 现在是不是锁屏状态

    QTimer *m_timer = nullptr;

    State m_state = State::Working;
    int m_remainSec = 45 * 60;      // 当前状态还剩多少秒
    qint64 m_lastTickMs = 0;        // 上一次 tick 的墙钟时间（判断睡眠）
};

#endif // SITREMINDER_H
