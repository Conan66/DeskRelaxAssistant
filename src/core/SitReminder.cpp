#include "SitReminder.h"

#include <QDateTime>
#include <QTimer>
#include <QTime>

#include "AppSettings.h"

// Windows 的头文件：OpenInputDesktop 用来判断"现在是不是锁屏了"
#ifndef NOMINMAX
#define NOMINMAX          // 防止 windows.h 里的 min/max 宏和 Qt 打架
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace {
// 把 "12:00-13:30" 这种字符串拆成两个时间点
// 格式不对就返回 false，交给调用方跳过这一条
bool parsePeriod(const QString &one, QTime *from, QTime *to)
{
    const QStringList parts = one.split(QLatin1Char('-'));
    if (parts.size() != 2) return false;

    const QTime f = QTime::fromString(parts.at(0).trimmed(), QStringLiteral("HH:mm"));
    const QTime t = QTime::fromString(parts.at(1).trimmed(), QStringLiteral("HH:mm"));
    if (!f.isValid() || !t.isValid()) return false;

    *from = f;
    *to = t;
    return true;
}
}

SitReminder::SitReminder(QObject *parent)
    : QObject(parent)
{
    // 1 秒走一格：既用来倒数，也顺便做锁屏/睡眠检测
    m_timer = new QTimer(this);
    m_timer->setInterval(1000);
    connect(m_timer, &QTimer::timeout, this, &SitReminder::onTick);
}

// ============================================================
// 开始 / 重新开始
// ============================================================
void SitReminder::start()
{
    enterWorking();
    m_lastTickMs = QDateTime::currentMSecsSinceEpoch();

    // 用户在设置里关掉了总开关：定时器都不用跑
    if (!AppSettings::sitReminderEnabled()) {
        m_timer->stop();
        return;
    }
    m_timer->start();
}

// 手动重新计时：和 start() 一样从头坐满一轮。
// 单独起个名字，让"右键 → 重新计时"的调用读起来更明白。
void SitReminder::restart()
{
    start();
}

void SitReminder::reloadSettings()
{
    // 用户在设置里改了时长，最简单的做法是从头开始计
    start();
}

// 用户点掉提醒：进入活动期
void SitReminder::dismiss()
{
    if (m_state != State::Reminding) return;

    m_state = State::Standing;
    m_remainSec = AppSettings::standMinutes() * 60;
    emit standingStarted(m_remainSec);
}

// ============================================================
// 提醒挂了太久没人点（比如直接去开会了，没锁屏）
// ------------------------------------------------------------
// 这时候按"人已经离开"处理：扔掉这次提醒，从头计时。
// 不能一直停在 Reminding，否则遮罩会永远盖着屏幕。
// ============================================================
void SitReminder::ignoreReminder()
{
    if (m_state != State::Reminding) return;

    enterWorking();
    emit countdownChanged(m_remainSec, int(m_state));
}

// 回到"坐着计时"状态
void SitReminder::enterWorking()
{
    m_state = State::Working;
    m_remainSec = AppSettings::sitMinutes() * 60;
}

// ============================================================
// 每秒一次
// ============================================================
void SitReminder::onTick()
{
    // ---------- 1) 电脑睡过一觉？醒来就重新计时 ----------
    // 正常每 1 秒跳一次；如果两次之间差了 1 分钟以上，
    // 说明中间电脑休眠了（定时器被冻结），这段时间不该算作"坐着"。
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (m_lastTickMs > 0 && now - m_lastTickMs > 60 * 1000) {
        // 休眠前可能正好弹着提醒，醒来后这个提醒已经过期了，先让外面收掉
        if (m_state == State::Reminding) emit reminderCancelled();
        enterWorking();
        m_lastTickMs = now;
        emit countdownChanged(m_remainSec, int(m_state));
        return;
    }
    m_lastTickMs = now;

    // ---------- 2) 锁屏 = 人离开了，重新计时 ----------
    if (isSessionLocked()) {
        if (m_state != State::Working || m_remainSec != AppSettings::sitMinutes() * 60) {
            // 锁屏时遮罩是看不见的，收掉免得解锁后突然冒出来
            if (m_state == State::Reminding) emit reminderCancelled();
            enterWorking();
            emit countdownChanged(m_remainSec, int(m_state));
        }
        return;
    }

    // ---------- 3) 免打扰时段：不计时，一直停在满值 ----------
    if (inSkipPeriod()) {
        if (m_state != State::Working) {
            // 午休/下班时段开始了，提醒也没必要继续挂着
            if (m_state == State::Reminding) emit reminderCancelled();
            enterWorking();
        }
        m_remainSec = AppSettings::sitMinutes() * 60;
        emit countdownChanged(m_remainSec, int(m_state));
        return;
    }

    // ---------- 4) 正常倒数 ----------
    switch (m_state) {
    case State::Working:
        if (m_remainSec > 0) --m_remainSec;
        if (m_remainSec <= 0) {
            // 到点了：停在"提醒中"，等用户点掉
            m_state = State::Reminding;
            emit reminderDue();
        }
        break;

    case State::Standing:
        if (m_remainSec > 0) --m_remainSec;
        if (m_remainSec <= 0) {
            // 活动结束 → 回去接着坐（重新计满）
            enterWorking();
            emit standingFinished();
        }
        break;

    case State::Reminding:
        // 等用户点击，这一段时间不倒数
        break;
    }

    emit countdownChanged(m_remainSec, int(m_state));
}

// ============================================================
// 是否处于免打扰时段
// 支持多段，也支持跨夜（18:30-09:00）
// ============================================================
bool SitReminder::inSkipPeriod() const
{
    const QTime now = QTime::currentTime();
    const QStringList list = AppSettings::skipPeriods();

    for (const QString &one : list) {
        // 每段必须是 "开始-结束" 两个时间
        const QStringList parts = one.split(QLatin1Char('-'));
        if (parts.size() != 2) continue;

        const QTime from = QTime::fromString(parts.at(0).trimmed(), QStringLiteral("HH:mm"));
        const QTime to   = QTime::fromString(parts.at(1).trimmed(), QStringLiteral("HH:mm"));
        if (!from.isValid() || !to.isValid()) continue;

        if (from <= to) {
            // 同一天内，比如 12:00-13:30
            if (now >= from && now < to) return true;
        } else {
            // 跨夜，比如 18:30-09:00
            if (now >= from || now < to) return true;
        }
    }
    return false;
}

// ============================================================
// 现在是不是锁屏状态
// 原理：锁屏时输入桌面会被系统切到"登录桌面"，普通程序打不开它。
// 打不开 = 已锁屏；能打开 = 正常使用中。
// ============================================================
bool SitReminder::isSessionLocked()
{
    HDESK desk = OpenInputDesktop(0, FALSE, DESKTOP_READOBJECTS);
    if (desk) {
        CloseDesktop(desk);
        return false;
    }
    return true;
}

// ============================================================
// 下面几个函数只服务一件事：
// 右键点桌面球时，告诉用户"下一次大概什么时候提醒"
// ------------------------------------------------------------
// 难点在于免打扰时段：那段时间计时是暂停的，
// 所以不能简单地"现在 + 剩余分钟"，得把免打扰时段跳过去。
// ============================================================

// 某个时刻是否落在任意一段免打扰时段里
bool SitReminder::inSkipPeriodAt(const QDateTime &t) const
{
    const QTime now = t.time();
    for (const QString &one : AppSettings::skipPeriods()) {
        QTime from, to;
        if (!parsePeriod(one, &from, &to)) continue;

        if (from <= to) {
            // 同一天内，比如 12:00-13:30
            if (now >= from && now < to) return true;
        } else {
            // 跨夜，比如 18:30-09:00
            if (now >= from || now < to) return true;
        }
    }
    return false;
}

// 某个时刻所在那段免打扰时段的"结束时刻"
// （传进来的时刻不在时段里时，原样返回）
QDateTime SitReminder::skipEndAfter(const QDateTime &t) const
{
    const QTime now = t.time();
    for (const QString &one : AppSettings::skipPeriods()) {
        QTime from, to;
        if (!parsePeriod(one, &from, &to)) continue;

        const bool inside = (from <= to) ? (now >= from && now < to)
                                         : (now >= from || now < to);
        if (!inside) continue;

        QDateTime end(t.date(), to);
        // 跨夜时段（18:30-09:00）：现在处在晚上那一截，结束时间就落在第二天
        if (to < from && now >= from) end = end.addDays(1);
        return end;
    }
    return t;
}

// 从 t 开始，往后最近一段免打扰时段的"开始时刻"
// （没有配置免打扰时段时返回无效的 QDateTime）
QDateTime SitReminder::nextSkipStartAfter(const QDateTime &t) const
{
    QDateTime best;
    for (const QString &one : AppSettings::skipPeriods()) {
        QTime from, to;
        if (!parsePeriod(one, &from, &to)) continue;

        // 先看今天这段的开始时间；已经过了，就说明要看明天的同一时刻
        QDateTime start(t.date(), from);
        if (start <= t) start = start.addDays(1);

        if (!best.isValid() || start < best) best = start;
    }
    return best;
}

// ============================================================
// 右键提示文案
// ============================================================
QString SitReminder::nextReminderText() const
{
    // 提醒已经弹出来了，没什么可算的
    if (m_state == State::Reminding) {
        return QStringLiteral("提醒已经弹出来了，就等你点一下～");
    }

    const bool standing = (m_state == State::Standing);

    // 活动期：先把活动剩下的一段时间走完，再坐满一轮
    int left = m_remainSec;
    if (standing) left += AppSettings::sitMinutes() * 60;

    const QDateTime now = QDateTime::currentDateTime();

    // "今天 / 明天 / 11月20日"：按目标时间的日期来写，跨天也不会说错
    auto dayText = [&now](const QDateTime &at) -> QString {
        const QDate today = now.date();
        if (at.date() == today)                return QStringLiteral("今天");
        if (at.date() == today.addDays(1))     return QStringLiteral("明天");
        return at.date().toString(QStringLiteral("M月d日"));
    };
    auto whenText = [&dayText](const QDateTime &at) -> QString {
        return QStringLiteral("%1 %2")
            .arg(dayText(at), at.time().toString(QStringLiteral("HH:mm")));
    };

    // 现在就处在免打扰时段里：时段内不计时，结束后重新坐满一整轮
    if (inSkipPeriodAt(now)) {
        const QDateTime at = skipEndAfter(now).addSecs(AppSettings::sitMinutes() * 60);
        return QStringLiteral("现在是免打扰时段，暂停计时。时段结束后坐满 %1 分钟，约 %2 提醒你")
            .arg(AppSettings::sitMinutes())
            .arg(whenText(at));
    }

    // 先按"一路都不受打扰"算出一个到点时间
    QDateTime at = now.addSecs(left);

    // 但如果这段时间中途会走进免打扰时段，实际的计时逻辑是：
    // 一进入时段就把这一轮清零，时段结束后重新坐满一整轮
    // （见 onTick 里免打扰分支的处理）。
    // 所以提示也要按这个规则改：到点时间 = 免打扰结束 + 坐满一整轮，
    // 而不是把没走完的剩余时间顺延到时段之后。
    QDateTime cur = now;
    for (int guard = 0; guard < 10; ++guard) {
        const QDateTime skipStart = nextSkipStartAfter(cur);
        if (!skipStart.isValid() || skipStart >= at) break;

        cur = skipEndAfter(skipStart);
        at = cur.addSecs(AppSettings::sitMinutes() * 60);
    }

    if (standing) {
        return QStringLiteral("活动还剩 %1 分钟，之后坐满 %2 分钟再提醒（约 %3）")
            .arg(qMax(1, m_remainSec / 60))
            .arg(AppSettings::sitMinutes())
            .arg(whenText(at));
    }

    return QStringLiteral("大约 %1 会提醒你起来活动").arg(whenText(at));
}
