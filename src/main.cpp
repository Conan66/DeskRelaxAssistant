#include <QAction>
#include <QApplication>
#include <QDateTime>       // 右键提示里要用到"现在几点"
#include <QFont>
#include <QGuiApplication>
#include <QIcon>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPixmap>
#include <QRadialGradient>
#include <QScreen>
#include <QStringList>
#include <QSystemTrayIcon>
#include <QTimer>          // QTimer::singleShot：等事件循环启动后再开引导窗

#include "core/AppSettings.h"
#include "core/SitReminder.h"
#include "core/StretchLib.h"
#include "ui/DesktopBallWindow.h"
#include "ui/AmbientView.h"
#include "ui/GuideWindow.h"
#include "ui/MoodDialog.h"
#include "ui/PlanDialog.h"
#include "ui/RecordsDialog.h"
#include "ui/ReminderOverlay.h"

#include <functional>
#include "ui/RhythmDialog.h"
#include "ui/SitReminderDialog.h"
#include "ui/StretchWindow.h"

namespace {
// 画一张"和桌面球同款"的绿色小球图，当托盘图标用。
// 这样做不用额外准备 png 文件，颜色也能一直和球保持一致。
QPixmap ballIconPixmap(int size)
{
    QPixmap pm(size, size);
    pm.fill(Qt::transparent);

    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(Qt::NoPen);

    const QPointF c(size / 2.0, size / 2.0);
    const double ballR = size * 0.38;      // 球体半径
    const double haloR = size * 0.50;      // 外面一圈柔光

    // ① 柔光：让图标在深色/浅色任务栏上都看得清
    QRadialGradient halo(c, haloR);
    halo.setColorAt(0.60, QColor(111, 175, 147, 90));
    halo.setColorAt(1.00, QColor(111, 175, 147, 0));
    p.setBrush(halo);
    p.drawEllipse(c, haloR, haloR);

    // ② 球体：左上偏亮、右下偏暗，和桌面球一致
    QRadialGradient body(c - QPointF(ballR * 0.30, ballR * 0.34), ballR * 1.85);
    body.setColorAt(0.00, QColor(QStringLiteral("#7FBF9F")));
    body.setColorAt(0.55, QColor(QStringLiteral("#6FAF93")));
    body.setColorAt(1.00, QColor(QStringLiteral("#4F8E73")));
    p.setBrush(body);
    p.drawEllipse(c, ballR, ballR);

    return pm;
}

// 托盘图标要准备多种尺寸，任务栏放大缩小时才不会糊
QIcon makeBallIcon()
{
    QIcon icon;
    const int sizes[] = {16, 20, 24, 32, 40, 48, 64};
    for (int s : sizes) icon.addPixmap(ballIconPixmap(s));
    return icon;
}
}   // namespace

// ============================================================
// 程序入口
// ------------------------------------------------------------
// 正常流程：
//   1. 启动后在屏幕右下角出现一个呼吸球（始终置顶、可拖动）
//   2. 点球 → 选一种呼吸 → 弹出引导窗
//   3. 引导窗打开期间，桌面球自动隐藏（同屏只留一个球）
//   4. 关掉引导窗 → 桌面球重新出现
//
// 命令行参数（方便做桌面快捷方式）：
//   RelaxAssistant.exe --breath fushi    直接开始"腹式呼吸"
//   RelaxAssistant.exe --breath 478      直接开始"4-7-8 呼吸"
// ============================================================
int main(int argc, char *argv[])
{
    // 高 DPI 支持必须在 QApplication 构造之前设置，
    // 否则 4K / 缩放屏上球会糊
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling, true);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps, true);

    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("桌面放松助手"));
    app.setOrganizationName(QStringLiteral("RelaxAssistant"));

    // 关掉最后一个窗口不退出程序——桌面球是常驻的
    app.setQuitOnLastWindowClosed(false);

    // 全局字体：微软雅黑，中文显示更舒服
    QFont f(QStringLiteral("Microsoft YaHei"), 9);
    app.setFont(f);

    // 全局窗口图标：和托盘用的是同一颗"放松小球"。
    // 设置一次，之后所有窗口 / 对话框（引导窗、拉伸窗、情绪陪伴、
    // 定时计划、我的坚持、氛围放空……）标题栏和任务栏都会用这颗球，
    // 不用再给每个子窗口单独 setWindowIcon。
    app.setWindowIcon(makeBallIcon());

    // ---------- 桌面常驻球 ----------
    DesktopBallWindow ball;

    // 引导窗用指针管理，按需创建（关闭时 deleteLater）
    GuideWindow *guide = nullptr;

    // 打开引导窗的统一入口（菜单点击和命令行参数都走这里）
    auto openGuide = [&ball, &guide](const QString &code) {
        if (!guide) {
            guide = new GuideWindow();
            QObject::connect(guide, &GuideWindow::closed, [&ball, &guide]() {
                ball.setSuppressed(false);   // 恢复桌面球
                guide->deleteLater();
                guide = nullptr;
            });
        }
        ball.setSuppressed(true);            // 同屏只留一个球
        guide->startBreath(code);
        guide->show();
        guide->raise();
        guide->activateWindow();
    };

    // 点了某种呼吸
    QObject::connect(&ball, &DesktopBallWindow::breathChosen,
                     [openGuide](const QString &code) { openGuide(code); });

    // ---------- 身体放松引导窗 ----------
    // 和呼吸引导窗一样：按需创建，关掉就 deleteLater，期间桌面球藏起来
    StretchWindow *stretch = nullptr;
    // 连续做多个动作：定时计划里同一时间有多条时，点一次「开始」依次做完。
    // 单项做完会停 5 秒，自动进下一项。
    auto openStretchQueue = [&ball, &stretch](const QStringList &codes) {
        if (codes.isEmpty()) return;
        if (!stretch) {
            stretch = new StretchWindow();
            QObject::connect(stretch, &StretchWindow::closed, [&ball, &stretch]() {
                ball.setSuppressed(false);   // 恢复桌面球
                stretch->deleteLater();
                stretch = nullptr;
            });
        }
        ball.setSuppressed(true);
        stretch->startQueue(codes);
        stretch->raise();
        stretch->activateWindow();
    };
    // 单个动作就是"只有一项的队列"，复用同一套展示逻辑
    auto openStretch = [openStretchQueue](const QString &code) {
        openStretchQueue(QStringList() << code);
    };
    QObject::connect(&ball, &DesktopBallWindow::stretchChosen,
                     [openStretch](const QString &code) { openStretch(code); });

    // 点了"定时计划…"：打开计划编辑对话框。
    // 保存后主程序每分钟扫描时会自动读到新计划，不用重启。
    auto openPlanSettings = []() {
        PlanDialog dlg;
        dlg.exec();
    };
    QObject::connect(&ball, &DesktopBallWindow::planRequested, openPlanSettings);

    // 点了"我的坚持…"：看今天做了几次、连续几天
    auto openRecords = []() {
        RecordsDialog dlg;
        dlg.exec();
    };
    QObject::connect(&ball, &DesktopBallWindow::recordsRequested, openRecords);

    // 点了"情绪陪伴…"：选一种感受，读一段话
    auto openMood = []() {
        MoodDialog dlg;
        dlg.exec();
    };
    QObject::connect(&ball, &DesktopBallWindow::moodRequested, openMood);

    // 点了"氛围放空…"：看图放空 / 冥想放空
    // AmbientView 是 QWidget（不是 QDialog），用 .show() 非模态显示，
    // 这样用户可以一边看图一边点桌面球弹呼吸，互不阻塞。
    auto openAmbient = []() {
        static AmbientView *win = nullptr;
        if (!win) win = new AmbientView();
        // 打开前先校正大小和位置：
        // 长时间使用后窗口可能被跨屏 DPI 放大，或留在另一块屏幕上，
        // prepareToShow() 会把这些状态恢复成正常大小、并跟随当前鼠标所在屏幕。
        win->prepareToShow();
        win->show();
        win->raise();
        win->activateWindow();
    };
    QObject::connect(&ball, &DesktopBallWindow::ambientRequested, openAmbient);

    // ---------- 关于：简单的程序简介 ----------
    auto openAbout = []() {
        QMessageBox dlg;
        dlg.setWindowTitle(QStringLiteral("关于桌面放松助手"));
        dlg.setIconPixmap(ballIconPixmap(64));
        dlg.setText(QStringLiteral("桌面放松助手"));
        dlg.setInformativeText(QStringLiteral(
            "一个常驻桌面的放松小伙伴。\n\n"
            "呼吸练习、身体放松、久坐提醒、情绪陪伴和氛围放空，"
            "都可以从小球的菜单里开始。"));
        dlg.exec();
    };
    QObject::connect(&ball, &DesktopBallWindow::aboutRequested, openAbout);

    // ---------- 新手导引 ----------
    // 单独用一个气泡，不能和久坐提醒、定时计划共用：
    // 否则点“跳过”可能会被误当成确认久坐提醒，或启动某条计划。
    ReminderBubble onboardingBubble;
    int onboardingStep = 0;

    auto finishOnboarding = [&onboardingBubble]() {
        onboardingBubble.hideBubble();
        // 看完和主动跳过都算“已经知道怎么使用”，下次启动不再自动弹。
        AppSettings::setGuideShown(true);
    };

    std::function<void()> showOnboardingStep;
    showOnboardingStep = [&ball, &onboardingBubble, &onboardingStep,
                          finishOnboarding]() {
        static const QStringList titles = {
            QStringLiteral("认识小球"),
            QStringLiteral("呼吸与身体放松"),
            QStringLiteral("提醒与计划"),
            QStringLiteral("情绪与放空"),
            QStringLiteral("随时能找到我"),
        };
        static const QStringList bodies = {
            QStringLiteral("左键点我打开功能菜单；按住拖动可以换位置，松手后我会自动贴到屏幕边缘。右键点我，可以查看下一次久坐提醒。"),
            QStringLiteral("在「开始呼吸引导」里挑一种呼吸；「身体放松」里有肩颈、腰部等动作，可以跟着图示慢慢做。"),
            QStringLiteral("「久坐提醒」会按时叫你起身活动；「定时计划」可以安排固定时间做呼吸或拉伸，具体节奏都能在对应设置里调整。"),
            QStringLiteral("「情绪陪伴」可以按当下的感受读一段话；「氛围放空」可以看风景、听白噪声，或闭眼安静一会儿。"),
            QStringLiteral("如果小球被隐藏，双击屏幕右下角的托盘图标就能叫回来。以后想重看这份导引，在「关于 → 使用导引」里打开即可。"),
        };

        // 最后一步再点一次，就结束整个导引。
        if (onboardingStep >= titles.size()) {
            finishOnboarding();
            return;
        }

        const bool isLast = (onboardingStep == titles.size() - 1);
        onboardingBubble.showGuideStep(
            QStringLiteral("新手导引 %1/%2 · %3")
                .arg(onboardingStep + 1)
                .arg(titles.size())
                .arg(titles.at(onboardingStep)),
            bodies.at(onboardingStep),
            ball.frameGeometry(),
            isLast ? QStringLiteral("开始使用") : QStringLiteral("下一步"));
    };

    auto startOnboarding = [&onboardingStep, showOnboardingStep]() {
        onboardingStep = 0;
        // 导引只要真正显示出来，就记为"已经看过"。
        // 这样用户第一次启动时中途直接关掉程序，下次也不会再自动弹；
        // 想重看随时可以从「关于 → 使用导引」打开。
        AppSettings::setGuideShown(true);
        showOnboardingStep();
    };
    QObject::connect(&ball, &DesktopBallWindow::onboardingRequested, startOnboarding);

    QObject::connect(&onboardingBubble, &ReminderBubble::actionRequested,
                     [&onboardingStep, showOnboardingStep]() {
        ++onboardingStep;
        // 等本次鼠标点击处理完再画下一条，避免同一个气泡反复切页时闪烁。
        QTimer::singleShot(0, showOnboardingStep);
    });
    QObject::connect(&onboardingBubble, &ReminderBubble::dismissed, finishOnboarding);

    // 点了"节奏设置"
    // 对话框关掉后立刻让桌面球重新读一次节奏：
    // 这样在设置里把秒数改完，桌面球马上按新时长动，不用再点开菜单。
    // （托盘菜单里的"节奏设置…"也复用这个 lambda）
    auto openRhythmSettings = [&ball]() {
        RhythmDialog dlg;
        dlg.exec();
        ball.applyDeskRhythm();
    };
    QObject::connect(&ball, &DesktopBallWindow::settingsRequested, openRhythmSettings);

    // 点了"退出"
    QObject::connect(&ball, &DesktopBallWindow::quitRequested, &app, &QApplication::quit);

    // ---------- 久坐提醒 ----------
    // reminder 负责计时，overlay / bubble 只负责显示，
    // 两者通过信号连起来，各管一件事。
    SitReminder reminder;
    // 每块屏幕各建一个全屏遮罩，而不是用一个窗口横跨多屏：
    // 主屏 100%、副屏 200% 时，跨屏窗口只有一个缩放比，物理尺寸算不准，
    // 副屏只能盖住一半。一屏一个就各用各的缩放比，不会漏。
    QList<ReminderOverlay*> overlays;
    for (int i = 0; i < QGuiApplication::screens().size(); ++i)
        overlays.append(new ReminderOverlay());
    ReminderBubble bubble;

    // 把所有屏幕上的全屏遮罩一起收掉
    auto hideAllOverlays = [&overlays]() {
        for (ReminderOverlay *o : overlays) o->hide();
    };

    // 气泡模式下"8 秒后自动藏起来 → 2 分钟没反应再弹一次"用的计时器
    QTimer bubbleRetry;
    bubbleRetry.setSingleShot(true);
    int bubbleRetryLeft = 0;            // 还能重弹几次

    // 兜底：提醒挂出来 5 分钟都没人点（甚至人已经走了，只是没锁屏），
    // 就把遮罩收起来，按"离开了"处理，免得一直挡着屏幕
    QTimer reminderGiveUp;
    reminderGiveUp.setSingleShot(true);
    reminderGiveUp.setInterval(5 * 60 * 1000);

    // 右键查"下次提醒"时，让那条提示多停一会儿，别被倒计时文字马上顶掉
    qint64 infoShownMs = 0;

    // 弹一次提醒（气泡模式的重弹也走这里，保证两边文案一致）
    auto showReminder = [&ball, &overlays, &bubble, &bubbleRetry]() {
        if (AppSettings::sitReminderStyle() == 0) {
            // 给每块屏幕各弹一个遮罩
            const QList<QScreen*> screens = QGuiApplication::screens();
            for (int i = 0; i < overlays.size() && i < screens.size(); ++i)
                overlays[i]->popup(AppSettings::sitMinutes(), screens.at(i));
        } else {
            // 气泡多停 4 秒（共 12 秒），专注时更容易被看到
            bubble.showNotice(
                QStringLiteral("该起来活动一下了"),
                QStringLiteral("已经连续坐了 %1 分钟，起来走两步吧")
                    .arg(AppSettings::sitMinutes()),
                ball.frameGeometry(), 12000);
            // 2 分钟后如果还是没人点，就再弹一次
            bubbleRetry.start(2 * 60 * 1000);
        }
    };

    // 到点了：按设置决定用"全屏遮罩"还是"小气泡"
    QObject::connect(&reminder, &SitReminder::reminderDue,
                     [&reminderGiveUp, &bubbleRetryLeft, showReminder]() {
        bubbleRetryLeft = 2;        // 气泡模式最多再重弹 2 次（一共出现 3 次）
        reminderGiveUp.start();
        showReminder();
    });

    // 气泡到点重弹
    QObject::connect(&bubbleRetry, &QTimer::timeout,
                     [&reminder, &bubbleRetry, &bubbleRetryLeft, showReminder]() {
        if (reminder.state() != SitReminder::State::Reminding) return;
        if (bubbleRetryLeft <= 0) return;      // 弹够了，交给下面的 5 分钟兜底
        --bubbleRetryLeft;
        showReminder();
    });

    // 5 分钟都没人点 → 收掉提醒，重新开始计时
    QObject::connect(&reminderGiveUp, &QTimer::timeout,
                     [&reminder, &hideAllOverlays, &bubble, &bubbleRetry]() {
        if (reminder.state() != SitReminder::State::Reminding) return;
        bubbleRetry.stop();
        hideAllOverlays();
        bubble.hideBubble();
        reminder.ignoreReminder();
    });

    // 用户点掉提醒 → 进入活动期（点掉时把两个计时器都停掉）
    auto ack = [&reminder, &bubbleRetry, &reminderGiveUp]() {
        bubbleRetry.stop();
        reminderGiveUp.stop();
        reminder.dismiss();
    };
    // 任意一块屏幕上的遮罩被点掉，都算"确认了这次提醒"
    for (ReminderOverlay *o : overlays)
        QObject::connect(o, &ReminderOverlay::dismissed, ack);
    QObject::connect(&bubble, &ReminderBubble::dismissed, ack);

    // 提醒还挂着但已经没意义了（锁屏、电脑休眠、进入午休等免打扰时段）：
    // 直接把遮罩/气泡收掉，等回到正常计时的状态再说
    QObject::connect(&reminder, &SitReminder::reminderCancelled,
                     [&hideAllOverlays, &bubble, &bubbleRetry, &reminderGiveUp]() {
        bubbleRetry.stop();
        reminderGiveUp.stop();
        hideAllOverlays();
        bubble.hideBubble();
    });

    // 每秒的状态同步：
    //   活动期 → 球旁边显示倒计时
    //   其他状态 → 把倒计时气泡收起来（提醒气泡只在"提醒中"出现）
    QObject::connect(&reminder, &SitReminder::countdownChanged,
                     [&ball, &bubble, &infoShownMs](int seconds, int state) {
        // 刚右键查过"下次提醒"，先让那条提示在屏幕上待够 8 秒
        if (infoShownMs > 0) {
            if (QDateTime::currentMSecsSinceEpoch() - infoShownMs < 8000) return;
            infoShownMs = 0;
        }

        if (state == int(SitReminder::State::Standing)) {
            bubble.showCountdown(seconds, ball.frameGeometry());
        } else if (state == int(SitReminder::State::Working)) {
            bubble.hideBubble();
        }
    });

    // 右键点球：冒个泡，告诉你下次什么时候提醒
    QObject::connect(&ball, &DesktopBallWindow::sitInfoRequested,
                     [&ball, &bubble, &reminder, &infoShownMs]() {
        infoShownMs = QDateTime::currentMSecsSinceEpoch();

        if (!AppSettings::sitReminderEnabled()) {
            bubble.showNotice(QStringLiteral("久坐提醒已关闭"),
                              QStringLiteral("左键点球 →「久坐提醒…」可以重新打开。"),
                              ball.frameGeometry());
            return;
        }
        bubble.showRestartAction(QStringLiteral("下一次久坐提醒"),
                                 reminder.nextReminderText(),
                                 ball.frameGeometry());
    });

    // 右键气泡里点了「重新计时」：把已经坐的时间清零，从头再坐满一轮。
    // 重置后再弹一条普通提示，告诉用户新的提醒时间。
    QObject::connect(&bubble, &ReminderBubble::actionRequested,
                     [&ball, &bubble, &reminder]() {
        reminder.restart();
        bubble.showNotice(QStringLiteral("已重新计时"),
                          QStringLiteral("下次提醒 %1").arg(reminder.nextReminderText()),
                          ball.frameGeometry());
    });

    // 点了"久坐提醒…"：改完设置后立刻按新设置重新计时
    // （托盘菜单里的"久坐提醒…"也复用这个 lambda）
    auto openSitSettings =
        [&reminder, &hideAllOverlays, &bubble, &bubbleRetry, &reminderGiveUp]() {
        // 打开设置前先记一份当前配置。
        // 用户往往只是点开看一眼就关掉，这种情况下不能把正在走的计时清零，
        // 否则再右键查看"下次提醒"，时间就会莫名其妙往后跳。
        const bool oldEnabled = AppSettings::sitReminderEnabled();
        const int oldSit      = AppSettings::sitMinutes();
        const int oldStand    = AppSettings::standMinutes();
        const int oldStyle    = AppSettings::sitReminderStyle();
        const QStringList oldPeriods = AppSettings::skipPeriods();

        SitReminderDialog dlg;
        dlg.exec();

        // 关掉提醒时，别留着已经弹出来的遮罩/气泡
        if (!AppSettings::sitReminderEnabled()) {
            bubbleRetry.stop();
            reminderGiveUp.stop();
            hideAllOverlays();
            bubble.hideBubble();
        }

        // 只有真的改了设置才重新计时；只是看一眼就关掉，保持原来的进度。
        const bool changed =
            oldEnabled != AppSettings::sitReminderEnabled()
            || oldSit   != AppSettings::sitMinutes()
            || oldStand != AppSettings::standMinutes()
            || oldStyle != AppSettings::sitReminderStyle()
            || oldPeriods != AppSettings::skipPeriods();

        if (changed) reminder.reloadSettings();
    };
    QObject::connect(&ball, &DesktopBallWindow::sitReminderRequested, openSitSettings);

    // ---------- 身体放松 · 定时计划 ----------
    // 每分钟扫一遍用户设的计划；到点弹气泡，点「开始」直接进跟练。
    QTimer planTimer;
    planTimer.setInterval(60 * 1000);
    planTimer.setSingleShot(false);
    // 防重记录：今天已经弹过的计划 id（跨分钟只弹一次）
    QStringList todayPlannedNotified;
    QDate planLastDate = QDate::currentDate();

    // 计划专用的气泡：不和久坐提醒的 bubble 共用，
    // 否则"点掉计划提醒"会被当成"确认了久坐提醒"，两套状态互相干扰
    ReminderBubble planBubble;
    // 待执行的计划项：(类型, 代号)。类型是 stretch（身体放松）或 breath（呼吸练习）
    QList<QPair<QString, QString>> pendingPlans;

    auto scanPlansOnce = [&ball, &planBubble, &todayPlannedNotified,
                          &planLastDate, &pendingPlans]() {
        const QDate today = QDate::currentDate();
        // 跨天了，清空记录，不然昨天弹过的今天就不弹了
        if (today != planLastDate) {
            todayPlannedNotified.clear();
            planLastDate = today;
        }

        const QTime now = QTime::currentTime();
        // 只精确到分钟，秒数不管
        const QString hm = QStringLiteral("%1:%2")
                               .arg(now.hour(), 2, 10, QLatin1Char('0'))
                               .arg(now.minute(), 2, 10, QLatin1Char('0'));
        const int dow = today.dayOfWeek();   // Qt 1=周一, 7=周日，和 repeat 约定一致

        // 先收集"这一分钟到点的所有计划"
        QList<QPair<QString, QString>> dueItems;   // (kind, code)
        QStringList dueIds;
        for (const AppSettings::RelaxPlan &p : AppSettings::relaxPlans()) {
            if (!p.enabled) continue;
            if (p.time != hm) continue;
            // repeat 为空 = 每天都弹；否则只在指定的日子弹
            if (!p.repeat.isEmpty() && !p.repeat.contains(QString::number(dow)))
                continue;
            // 今天已经弹过这条了 → 跳过
            if (todayPlannedNotified.contains(p.id)) continue;

            dueItems << qMakePair(p.kind, p.action);
            dueIds << p.id;
        }
        if (dueItems.isEmpty()) return;

        // 同一分钟有多条计划：合并成一条提示，不连弹多个气泡
        QStringList names;
        for (const QPair<QString, QString> &item : dueItems) {
            if (item.first == QLatin1String("breath")) {
                const BreathType b = BreathLib::find(item.second);
                names << QStringLiteral("呼吸 · %1")
                             .arg(b.name.isEmpty() ? item.second : b.name);
            } else {
                const StretchAction a = StretchLib::find(item.second);
                names << (a.name.isEmpty() ? item.second : a.name);
            }
        }
        const QString body = (names.size() == 1)
            ? QStringLiteral("该做「%1」了").arg(names.first())
            : QStringLiteral("这一分钟有 %1 项放松：\n%2")
                  .arg(names.size())
                  .arg(names.join(QStringLiteral("\n")));

        pendingPlans = dueItems;
        planBubble.showNoticeWithAction(QStringLiteral("到点啦"), body,
                                        ball.frameGeometry(), 30000);
        for (const QString &id : dueIds) todayPlannedNotified << id;
    };
    QObject::connect(&planTimer, &QTimer::timeout, scanPlansOnce);
    planTimer.start();

    // ---------- 点「开始」后怎么执行 ----------
    // 规则：先把开头连续的"身体放松"项合成一个队列，一项接一项做完；
    //       动作全部做完（窗口关闭）后，如果后面还有呼吸项，自动打开呼吸。
    //       呼吸本身包含多轮，所以一次只开一个。
    std::function<void()> runPending = nullptr;
    runPending = [&pendingPlans, &stretch, openGuide, openStretchQueue, &runPending]() {
        if (pendingPlans.isEmpty()) return;

        // 1) 取出开头连续的"身体放松"项
        QStringList stretchCodes;
        while (!pendingPlans.isEmpty()
               && pendingPlans.first().first != QLatin1String("breath")) {
            stretchCodes << pendingPlans.takeFirst().second;
        }

        if (!stretchCodes.isEmpty()) {
            openStretchQueue(stretchCodes);
            // 动作队列做完、窗口关掉之后，继续执行后面的呼吸项
            if (!pendingPlans.isEmpty() && stretch) {
                QObject::connect(stretch, &StretchWindow::closed,
                                 [&runPending]() { runPending(); });
            }
            return;
        }

        // 2) 只剩呼吸项：打开第一个
        const QPair<QString, QString> item = pendingPlans.takeFirst();
        openGuide(item.second);
    };

    // 气泡上点「开始」→ 直接进入跟练/呼吸
    QObject::connect(&planBubble, &ReminderBubble::actionRequested,
                     [&runPending]() { runPending(); });

    // ---------- 托盘图标 ----------
    // 桌面球不进任务栏，托盘图标就是"第二条命"：
    // 球被挡住、被拖到屏幕外，或者你想让它安静一会儿，都靠这里。
    QSystemTrayIcon tray(makeBallIcon());
    tray.setToolTip(QStringLiteral("桌面放松助手（双击把小球叫回来）"));

    QMenu trayMenu;
    QAction *showAct = trayMenu.addAction(QStringLiteral("显示小球"));
    showAct->setCheckable(true);
    showAct->setChecked(true);
    QObject::connect(showAct, &QAction::toggled, [&ball, &tray](bool on) {
        ball.setUserHidden(!on);
        if (!on) {
            // 第一次隐藏时提示一下怎么找回来，免得以为程序退了
            tray.showMessage(QStringLiteral("桌面放松助手"),
                             QStringLiteral("小球已隐藏，双击托盘图标可以叫回来"),
                             QSystemTrayIcon::Information, 3000);
        }
    });

    trayMenu.addSeparator();
    QAction *traySitAct = trayMenu.addAction(QStringLiteral("久坐提醒…"));
    QObject::connect(traySitAct, &QAction::triggered, openSitSettings);

    QAction *trayRhythmAct = trayMenu.addAction(QStringLiteral("节奏设置…"));
    QObject::connect(trayRhythmAct, &QAction::triggered, openRhythmSettings);

    // 定时计划也放托盘一份，不用先点球
    QAction *trayPlanAct = trayMenu.addAction(QStringLiteral("定时计划…"));
    QObject::connect(trayPlanAct, &QAction::triggered, openPlanSettings);

    QAction *trayRecordsAct = trayMenu.addAction(QStringLiteral("我的坚持…"));
    QObject::connect(trayRecordsAct, &QAction::triggered, openRecords);

    QAction *trayAmbientAct = trayMenu.addAction(QStringLiteral("氛围放空…"));
    QObject::connect(trayAmbientAct, &QAction::triggered, openAmbient);

    // 托盘也保留同一份「关于」，小球隐藏时仍能重新打开导引。
    QMenu *trayAboutMenu = trayMenu.addMenu(QStringLiteral("关于"));
    QAction *trayOnboardingAct = trayAboutMenu->addAction(QStringLiteral("使用导引"));
    QObject::connect(trayOnboardingAct, &QAction::triggered, startOnboarding);
    QAction *trayAboutAct = trayAboutMenu->addAction(QStringLiteral("关于桌面放松助手…"));
    QObject::connect(trayAboutAct, &QAction::triggered, openAbout);

    trayMenu.addSeparator();
    QAction *trayQuitAct = trayMenu.addAction(QStringLiteral("退出"));
    QObject::connect(trayQuitAct, &QAction::triggered, &app, &QApplication::quit);

    tray.setContextMenu(&trayMenu);

    // 双击托盘图标：显示小球，并把它搬回右下角（球找不到时最有用）
    QObject::connect(&tray, &QSystemTrayIcon::activated,
                     [&ball, showAct](QSystemTrayIcon::ActivationReason reason) {
        if (reason != QSystemTrayIcon::DoubleClick) return;
        if (!showAct->isChecked()) showAct->setChecked(true);   // 顺带把球显示出来
        ball.setUserHidden(false);
        ball.resetToCorner();
        ball.raise();
    });

    // 极少数系统没有托盘区（被策略关掉），这时只靠球也能用
    if (QSystemTrayIcon::isSystemTrayAvailable()) tray.show();

    reminder.start();   // 按设置开始计时（关着的话它自己不会启动）

    ball.show();

    // ---------- 解析命令行参数 ----------
    const QStringList args = app.arguments();
    const int idx = args.indexOf(QStringLiteral("--breath"));
    if (idx >= 0 && idx + 1 < args.size()) {
        const QString code = args.at(idx + 1);
        // 等事件循环跑起来再开窗，保证窗口能正常显示
        QTimer::singleShot(0, [openGuide, code]() { openGuide(code); });
    }

    // --stretch neck_side：直接开始某个身体放松动作（方便做快捷方式/测试）
    const int sidx = args.indexOf(QStringLiteral("--stretch"));
    if (sidx >= 0 && sidx + 1 < args.size()) {
        const QString code = args.at(sidx + 1);
        QTimer::singleShot(0, [openStretch, code]() { openStretch(code); });
    }

    // --plan：直接打开「定时计划」对话框（方便做快捷方式，也方便测试）
    if (args.contains(QStringLiteral("--plan"))) {
        QTimer::singleShot(0, [openPlanSettings]() { openPlanSettings(); });
    }

    // --records：直接打开「我的坚持」面板（方便测试）
    if (args.contains(QStringLiteral("--records"))) {
        QTimer::singleShot(0, [openRecords]() { openRecords(); });
    }

    // --mood：直接打开「情绪陪伴」窗口（方便测试）
    if (args.contains(QStringLiteral("--mood"))) {
        QTimer::singleShot(0, [openMood]() { openMood(); });
    }

    // --ambient：直接打开「氛围放空」窗口（方便测试）
    // 用前面已经定义好的 openAmbient lambda（菜单入口也复用它）
    if (args.contains(QStringLiteral("--ambient"))) {
        QTimer::singleShot(100, openAmbient);
    }

    // 只有普通的首次启动才自动播导引。
    // 如果用户通过快捷方式直达某个功能，就先尊重他的目的，不额外打断。
    const bool directFeature =
        (idx >= 0) || (sidx >= 0)
        || args.contains(QStringLiteral("--plan"))
        || args.contains(QStringLiteral("--records"))
        || args.contains(QStringLiteral("--mood"))
        || args.contains(QStringLiteral("--ambient"));
    if (!directFeature && !AppSettings::guideShown()) {
        QTimer::singleShot(600, startOnboarding);
    }

    return app.exec();
}
