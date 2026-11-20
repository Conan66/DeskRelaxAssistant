#include "DesktopBallWindow.h"

#include <QApplication>
#include <QCursor>
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QMenu>
#include <QMouseEvent>
#include <QScreen>
#include <QTimer>
#include <QtMath>

#include "../core/AppSettings.h"
#include "../core/BreathTypes.h"
#include "../core/StretchLib.h"
#include "BreathingBall.h"

namespace {
const int kWindowSize = 150;    // 窗口比球大一圈，给光晕留空间
const int kEdgeMargin = 24;     // 距屏幕边缘的距离（PRD 4.0.1）
const int kDragThreshold = 4;   // 移动超过这个距离才算拖拽，否则算点击
}

DesktopBallWindow::DesktopBallWindow(QWidget *parent)
    : QWidget(parent)
{
    // ---------- 窗口样式：无边框 + 透明 + 置顶 + 不进任务栏 ----------
    setWindowFlags(Qt::FramelessWindowHint
                   | Qt::WindowStaysOnTopHint
                   | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setFixedSize(kWindowSize, kWindowSize);
    setCursor(Qt::PointingHandCursor);
    setWindowTitle(QStringLiteral("放松小球"));

    // ---------- 球本体 ----------
    m_ball = new BreathingBall(this);
    m_ball->setGeometry(0, 0, kWindowSize, kWindowSize);
    m_ball->setTextVisible(false);   // 桌面球太小，不显示倒计时文字

    // ---------- 待机动画：4 秒一个循环 ----------
    m_idleClock = new QElapsedTimer();
    m_idleClock->start();

    m_idleTimer = new QTimer(this);
    m_idleTimer->setInterval(40);    // 25fps 足够，省电
    connect(m_idleTimer, &QTimer::timeout, this, &DesktopBallWindow::onIdleFrame);
    m_idleTimer->start();

    // 初始位置：屏幕右下角
    if (QScreen *scr = QApplication::primaryScreen()) {
        const QRect avail = scr->availableGeometry();
        move(avail.right() - kWindowSize - kEdgeMargin,
             avail.bottom() - kWindowSize - kEdgeMargin);
    }

    // 读用户上次选的桌面节奏（默认腹式呼吸），读完立刻生效
    applyDeskRhythm();
}

// ============================================================
// 读取"桌面球跟着哪种呼吸动"
// ------------------------------------------------------------
// 用户可以在球的菜单里选 12 种呼吸之一，也可以关掉。
// 这里会套用用户在"节奏设置"里改过的秒数，和引导窗保持一致。
// ============================================================
void DesktopBallWindow::applyDeskRhythm()
{
    const QString code = AppSettings::deskBreath();

    if (code.isEmpty()) {
        m_hasDeskRhythm = false;      // 用户选的是"关闭"
    } else {
        const BreathType t = BreathLib::find(code);
        if (t.code.isEmpty() || t.phases.isEmpty()) {
            m_hasDeskRhythm = false;  // 代码无效时退回轻微起伏，别让球停住
        } else {
            m_deskBreath = AppSettings::tunedBreath(t);
            m_hasDeskRhythm = true;
        }
    }

    // 重新计时，让新节奏从"吸气开始"走，而不是从半截接上
    m_idleClock->restart();
}

// ============================================================
// 待机 / 桌面呼吸动画
// ------------------------------------------------------------
// 有两种模式：
//   ① 没选节奏：原来的 4 秒轻微起伏（1.00 ↔ 1.04）
//   ② 选了节奏：严格按该呼吸的 吸 / 屏 / 呼 秒数走动
//      吸气放大、屏息停住并略亮、呼气缩小
// ============================================================
void DesktopBallWindow::onIdleFrame()
{
    if (m_suppressed || !isVisible()) return;

    // ---------- ① 没选节奏：保持原来的轻微起伏 ----------
    if (!m_hasDeskRhythm || m_deskBreath.phases.isEmpty()) {
        const double periodMs = 4000.0;
        const double t = double(m_idleClock->elapsed() % qint64(periodMs)) / periodMs;
        const double wave = 0.5 - 0.5 * qCos(2.0 * M_PI * t);   // 0→1→0

        const double scale = 1.0 + 0.04 * wave;
        m_ball->setVisual(scale, 1.0 + 0.3 * wave,
                          QColor(QStringLiteral("#5F9E83")));
        return;
    }

    // ---------- ② 按呼吸节奏走 ----------
    // 算出当前处在第几个阶段、以及阶段内走到百分之几
    const int cycleMs = qMax(1, m_deskBreath.cycleSeconds() * 1000);
    const qint64 inCycle = m_idleClock->elapsed() % cycleMs;

    PhaseType type = PhaseType::Inhale;
    double progress = 0.0;
    int accMs = 0;
    int phaseIndex = 0;      // 当前走到第几个阶段（判断屏息前后用）
    for (int i = 0; i < m_deskBreath.phases.size(); ++i) {
        const BreathPhase &ph = m_deskBreath.phases.at(i);
        const int ms = qMax(1, ph.seconds * 1000);
        if (inCycle < accMs + ms) {
            type = ph.type;
            progress = double(inCycle - accMs) / double(ms);
            phaseIndex = i;
            break;
        }
        accMs += ms;
        // 兜底：万一时钟超出总长，就用最后一段的末尾
        type = ph.type;
        progress = 1.0;
        phaseIndex = i;
    }

    // 这个屏息前面是吸气还是呼气？
    // 箱式呼吸是 吸-屏-呼-屏，第二个屏息前面是呼气，
    // 这时球应该停在"小"的状态，否则会出现"呼完反而鼓大"的怪现象。
    bool holdAfterExhale = false;
    if (type == PhaseType::Hold) {
        for (int i = phaseIndex - 1; i >= 0; --i) {
            if (m_deskBreath.phases.at(i).type != PhaseType::Hold) {
                holdAfterExhale = (m_deskBreath.phases.at(i).type == PhaseType::Exhale);
                break;
            }
        }
    }

    // 桌面球很小，幅度放大一点才看得出（用户选的是"明显"）
    const double kMax = 1.18;
    double scale = 1.0;
    double halo = 1.0;
    QColor color(QStringLiteral("#5F9E83"));

    switch (type) {
    case PhaseType::Inhale:
        // 吸气：球变大，光晕一起变宽
        scale = 1.0 + (kMax - 1.0) * progress;
        halo = 1.0 + 0.35 * progress;
        break;
    case PhaseType::Hold:
        // 屏息：球完全停住（不缩不抖），只把颜色调亮一点表示气还在
        // 但如果前面刚呼完（箱式的第二个屏息），就该停在"小"的状态，
        // 颜色也偏暗，这样和呼气结尾接得上，不会突然鼓大一下。
        if (holdAfterExhale) {
            scale = 1.0;
            halo = 1.0;
            color = QColor(QStringLiteral("#4F8E73"));
        } else {
            scale = kMax;
            halo = 1.35;
            color = QColor(QStringLiteral("#6FB396"));
        }
        break;
    case PhaseType::Exhale:
        // 呼气：球缩回基准
        scale = kMax - (kMax - 1.0) * progress;
        halo = 1.35 - 0.35 * progress;
        break;
    }

    m_ball->setVisual(scale, halo, color);
}

void DesktopBallWindow::setSuppressed(bool on)
{
    m_suppressed = on;
    // 引导窗要显示时藏起来；但用户自己在托盘里选了"隐藏"时，关掉引导窗也别冒出来
    setVisible(!on && !m_userHidden);
    if (!on) {
        // 重新显示时重置动画时钟，避免跳变
        m_idleClock->restart();
        raise();
    }
}

// 托盘菜单里的"隐藏/显示小球"
void DesktopBallWindow::setUserHidden(bool on)
{
    m_userHidden = on;
    // 引导窗正开着的话，这里不能把球强行显示出来
    setVisible(!on && !m_suppressed);
    if (!on) {
        m_idleClock->restart();
        raise();
    }
}

// 把球搬回屏幕右下角
// 用在托盘双击：万一球被拖到屏幕外、或者副屏拔掉了，用这个能叫回来
void DesktopBallWindow::resetToCorner()
{
    // 优先放回鼠标所在的那块屏，没有就放主屏
    QScreen *scr = QGuiApplication::screenAt(QCursor::pos());
    if (!scr) scr = QGuiApplication::primaryScreen();
    if (!scr) return;

    // 两块显示器的缩放比例不同时，直接移动一个"正在显示"的无边框窗口，
    // Windows 会按目标屏幕改写它的物理尺寸，Qt 5.14 偶尔同步不过来，
    // 结果球就只剩左上角一小块。先藏起来再搬，能避开这个时机问题。
    const bool needShow = isVisible() && !m_userHidden && !m_suppressed;
    if (needShow) hide();

    const QRect avail = scr->availableGeometry();
    const int x = avail.right() - kWindowSize - kEdgeMargin;
    const int y = avail.bottom() - kWindowSize - kEdgeMargin;

    // 用 setGeometry 同时指定位置和大小，而不是只 move：
    // 让 Qt 按目标屏幕的缩放比例重新算一遍窗口的物理尺寸。
    auto applyTargetGeometry = [this, x, y]() {
        setGeometry(x, y, kWindowSize, kWindowSize);
        if (m_ball) m_ball->setGeometry(0, 0, kWindowSize, kWindowSize);
    };
    applyTargetGeometry();

    if (needShow) {
        show();
        raise();

        // show() 之后，Windows 还可能继续发来目标屏幕的 DPI 变化消息。
        // 等这批消息处理完，再按 Qt 已更新的屏幕缩放比例校正一次。
        // 否则从 200% 副屏回到 100% 主屏时，150 的窗口可能被缩成 75。
        QTimer::singleShot(0, this, [this, applyTargetGeometry]() {
            // 如果这期间用户又隐藏了球，或打开了全屏引导，就不再把它改出来。
            if (!isVisible() || m_userHidden || m_suppressed) return;
            applyTargetGeometry();
            raise();
        });
    }
}

void DesktopBallWindow::paintEvent(QPaintEvent *)
{
    // 背景完全透明，所有绘制都在 BreathingBall 里完成
}

void DesktopBallWindow::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    raise();   // 保证在最上层
}

// ============================================================
// 鼠标交互：短按 = 弹菜单，长按拖动 = 移动窗口
// ============================================================
void DesktopBallWindow::mousePressEvent(QMouseEvent *event)
{
    // 右键：不弹菜单，而是在球旁边冒个小气泡，告诉你下次什么时候提醒
    if (event->button() == Qt::RightButton) {
        emit sitInfoRequested();
        event->accept();
        return;
    }

    if (event->button() == Qt::LeftButton) {
        m_pressed = true;
        m_dragging = false;
        m_pressGlobal = event->globalPos();
        m_pressOffset = event->globalPos() - frameGeometry().topLeft();
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void DesktopBallWindow::mouseMoveEvent(QMouseEvent *event)
{
    if (m_pressed && (event->buttons() & Qt::LeftButton)) {
        // 超过阈值才开始真正拖动，避免手抖导致误拖
        if (!m_dragging
            && (event->globalPos() - m_pressGlobal).manhattanLength() > kDragThreshold) {
            m_dragging = true;
            setCursor(Qt::ClosedHandCursor);
        }
        if (m_dragging) {
            move(event->globalPos() - m_pressOffset);
        }
        event->accept();
        return;
    }
    QWidget::mouseMoveEvent(event);
}

void DesktopBallWindow::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_pressed = false;
        setCursor(Qt::PointingHandCursor);

        if (m_dragging) {
            m_dragging = false;
            snapToNearestEdge();   // 松手吸附到最近的边
        } else {
            showMenu();            // 没拖动 = 一次点击
        }
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

// 吸附到屏幕边
// ------------------------------------------------------------
// 水平和垂直各判断一次：拖到角落时两条边都会贴上。
// 原来的写法只挑"最近的一条边"，所以角落位置总是差一条边没吸住。
void DesktopBallWindow::snapToNearestEdge()
{
    QScreen *scr = screen();
    if (!scr) return;

    const QRect avail = scr->availableGeometry();
    const QRect g = frameGeometry();

    const int distLeft   = qAbs(g.left() - avail.left());
    const int distRight  = qAbs(avail.right() - g.right());
    const int distTop    = qAbs(g.top() - avail.top());
    const int distBottom = qAbs(avail.bottom() - g.bottom());

    // 距离边小于一个球的大小，就认为"用户想让它靠这条边"
    const int snapRange = kWindowSize;
    const bool nearLeft   = distLeft   <= snapRange;
    const bool nearRight  = distRight  <= snapRange;
    const bool nearTop    = distTop    <= snapRange;
    const bool nearBottom = distBottom <= snapRange;

    int x = g.left();
    int y = g.top();

    if (nearLeft)        x = avail.left() + kEdgeMargin;
    else if (nearRight)  x = avail.right() - g.width() - kEdgeMargin;

    if (nearTop)         y = avail.top() + kEdgeMargin;
    else if (nearBottom) y = avail.bottom() - g.height() - kEdgeMargin;

    // 四条边都不近（例如从别处甩到中间松手）：保留原来的行为，
    // 贴到最近的那条边，保证球不会悬在屏幕中间。
    if (!nearLeft && !nearRight && !nearTop && !nearBottom) {
        const int minDist = qMin(qMin(distLeft, distRight), qMin(distTop, distBottom));
        if (minDist == distLeft)        x = avail.left() + kEdgeMargin;
        else if (minDist == distRight)  x = avail.right() - g.width() - kEdgeMargin;
        else if (minDist == distTop)    y = avail.top() + kEdgeMargin;
        else                            y = avail.bottom() - g.height() - kEdgeMargin;
    }

    move(x, y);
}

// ============================================================
// 功能菜单：12 种呼吸 + 设置 + 退出
// ============================================================
void DesktopBallWindow::showMenu()
{
    // 每次打开菜单前重新读一次节奏，这样用户在"节奏设置"里改了秒数，
    // 桌面球马上跟着变，不用重启程序
    applyDeskRhythm();

    QMenu menu;
    menu.setStyleSheet(QStringLiteral(
        "QMenu { background:#FBF9F4; border:1px solid #E3DED2; padding:6px; }"
        "QMenu::item { padding:7px 22px; border-radius:5px; color:#33302B; }"
        "QMenu::item:selected { background:#E6F0EA; color:#2E5E4B; }"
        "QMenu::separator { height:1px; background:#EDE8DD; margin:5px 8px; }"));

    QMenu *breathMenu = menu.addMenu(QStringLiteral("开始呼吸引导"));
    const QVector<BreathType> list = BreathLib::all();

    for (const BreathType &t : list) {
        // 菜单项直接显示名称 + 时长，方便快速选
        const QString label = QStringLiteral("%1   （%2 分钟）")
                                  .arg(t.name)
                                  .arg(qMax(1, t.defaultTotalSeconds() / 60));
        QAction *act = breathMenu->addAction(label);
        const QString code = t.code;   // 捕获到 lambda 里
        connect(act, &QAction::triggered, this, [this, code]() {
            emit breathChosen(code);
        });
    }

    // ---------- 桌面球跟着哪种呼吸动 ----------
    // 只影响桌面上这个小球，不会打开引导窗。
    // 选了之后，球的放大/停住/缩小会严格按该呼吸的秒数走。
    QMenu *deskMenu = menu.addMenu(QStringLiteral("桌面呼吸节奏"));
    const QString current = AppSettings::deskBreath();

    QAction *offAct = deskMenu->addAction(QStringLiteral("关闭（轻微起伏）"));
    offAct->setCheckable(true);
    offAct->setChecked(current.isEmpty());
    connect(offAct, &QAction::triggered, this, [this]() {
        AppSettings::setDeskBreath(QString());
        applyDeskRhythm();
    });

    deskMenu->addSeparator();

    for (const BreathType &t : list) {
        QAction *act = deskMenu->addAction(t.name);
        act->setCheckable(true);
        act->setChecked(current == t.code);
        const QString code = t.code;
        connect(act, &QAction::triggered, this, [this, code]() {
            AppSettings::setDeskBreath(code);
            applyDeskRhythm();
        });
    }

    menu.addSeparator();
    menu.addSeparator();

    // 身体放松：菜单里直接列出 6 个动作，点哪个做哪个
    QMenu *stretchMenu = menu.addMenu(QStringLiteral("身体放松"));
    for (const StretchAction &a : StretchLib::all()) {
        QAction *act = stretchMenu->addAction(a.name);
        const QString code = a.code;
        connect(act, &QAction::triggered, this, [this, code]() {
            emit stretchChosen(code);
        });
    }

    QAction *sitAct = menu.addAction(QStringLiteral("久坐提醒…"));
    connect(sitAct, &QAction::triggered, this, [this]() {
        emit sitReminderRequested();
    });

    // 定时放松计划：自己安排"几点做什么动作"，到点弹气泡提醒
    QAction *planAct = menu.addAction(QStringLiteral("定时计划…"));
    connect(planAct, &QAction::triggered, this, [this]() {
        emit planRequested();
    });

    // 我的坚持：今天做了几次、连续几天
    QAction *recordsAct = menu.addAction(QStringLiteral("我的坚持…"));
    connect(recordsAct, &QAction::triggered, this, [this]() {
        emit recordsRequested();
    });

    // 情绪陪伴：选一种此刻的感受，读一段话
    QAction *moodAct = menu.addAction(QStringLiteral("情绪陪伴…"));
    connect(moodAct, &QAction::triggered, this, [this]() {
        emit moodRequested();
    });

    // 氛围放空：看图放空 / 冥想放空
    QAction *ambientAct = menu.addAction(QStringLiteral("氛围放空…"));
    connect(ambientAct, &QAction::triggered, this, [this]() {
        emit ambientRequested();
    });

    QAction *settingsAct = menu.addAction(QStringLiteral("节奏设置…"));
    connect(settingsAct, &QAction::triggered, this, [this]() {
        emit settingsRequested();
    });

    menu.addSeparator();

    // 关于：这里放使用导引和程序简介，不再增加一个独立设置中心。
    QMenu *aboutMenu = menu.addMenu(QStringLiteral("关于"));
    QAction *onboardingAct = aboutMenu->addAction(QStringLiteral("使用导引"));
    connect(onboardingAct, &QAction::triggered, this, [this]() {
        emit onboardingRequested();
    });
    QAction *aboutAct = aboutMenu->addAction(QStringLiteral("关于桌面放松助手…"));
    connect(aboutAct, &QAction::triggered, this, [this]() {
        emit aboutRequested();
    });

    menu.addSeparator();
    QAction *quitAct = menu.addAction(QStringLiteral("退出"));
    connect(quitAct, &QAction::triggered, this, [this]() {
        emit quitRequested();
    });

    // 菜单贴着球的左上角弹出
    menu.exec(mapToGlobal(QPoint(0, 0)));
}
