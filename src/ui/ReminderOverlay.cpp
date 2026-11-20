#include "ReminderOverlay.h"

#include <QElapsedTimer>
#include <QFont>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>
#include <QRandomGenerator>
#include <QRegion>
#include <QScreen>
#include <QTimer>
#include <QtMath>

namespace {
// 提醒时轮换的说法，每次随机挑一句，避免每次都是同一句话让人麻木。
// 条数多一点（18 条），连续几次提醒都不会撞上同一句。
const char *kTips[] = {
    "起来倒杯水吧，顺便看看远处。",
    "肩颈松一下，脖子慢慢转两圈。",
    "站起来走两步，比坐着伸懒腰管用。",
    "眼睛看看窗外，让睫状肌歇一会儿。",
    "腰往后靠一靠，深呼吸三次。",
    "去接杯水，回来脑子会更清楚。",
    "站直，双手往上举一举，把胸口打开。",
    "去窗边站一分钟，让眼睛看看远处。",
    "站起来掂掂脚尖，让小腿帮忙把血往回送。",
    "双手搓热，捂一会儿眼睛，很解乏。",
    "慢慢转两圈肩膀，前后各来一次。",
    "站起来扶着桌子，做几个扩胸。",
    "走几步，顺便把膝盖轻轻弯曲几下。",
    "抬头看看天花板，把脖子后面拉开。",
    "深呼吸，吸气四拍、呼气六拍，来三轮。",
    "去拉伸一下腰，双手叉腰往后仰一点。",
    "让手腕转几圈，鼠标手也能缓一缓。",
    "站起来闭眼十秒，再睁眼看看远处。",
};
const int kTipCount = int(sizeof(kTips) / sizeof(kTips[0]));

// 画一个和桌面球同款的渐变球（含外层柔光）
void drawBall(QPainter &p, const QPointF &center, double radius, double wave)
{
    // 外圈柔光：随呼吸轻轻外扩，让遮罩“活”起来
    const double haloOuter = radius + 26.0 + 14.0 * wave;
    QRadialGradient halo(center, haloOuter);
    halo.setColorAt(0.55, QColor(111, 175, 147, 80));
    halo.setColorAt(1.00, QColor(111, 175, 147, 0));
    p.setPen(Qt::NoPen);
    p.setBrush(halo);
    p.drawEllipse(center, haloOuter, haloOuter);

    // 球体：左上偏亮、右下偏暗，看起来是个立体的圆
    QRadialGradient g(center - QPointF(radius * 0.25, radius * 0.30), radius * 1.35);
    g.setColorAt(0.00, QColor(QStringLiteral("#7FBF9F")));
    g.setColorAt(0.55, QColor(QStringLiteral("#6FAF93")));
    g.setColorAt(1.00, QColor(QStringLiteral("#4F8E73")));
    p.setBrush(g);
    p.drawEllipse(center, radius, radius);
}
}

// ============================================================
// ReminderOverlay（全屏遮罩）
// ============================================================
ReminderOverlay::ReminderOverlay(QWidget *parent)
    : QWidget(parent)
{
    // 无边框 + 置顶 + 不进任务栏：像一块盖在屏幕上的板子
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setCursor(Qt::PointingHandCursor);

    // 窗口整体半透明（配合下面的白色底，就是"半透明白遮罩"）
    setWindowOpacity(0.9);

    // 让球轻轻起伏，吸引注意
    m_clock = new QElapsedTimer();
    m_clock->start();
    m_anim = new QTimer(this);
    m_anim->setInterval(40);   // 25fps 足够
    connect(m_anim, &QTimer::timeout, this, &ReminderOverlay::onAnim);
}

void ReminderOverlay::popup(int sitMinutes, QScreen *screen)
{
    // 每个遮罩只盖自己负责的那块屏幕。
    // 之前是一个窗口横跨所有屏幕：主屏 100%、副屏 200% 时，
    // 跨屏窗口只有一个缩放比，物理尺寸算不准，副屏只能盖住一半。
    // 现在一屏一个遮罩，各用各的缩放比，就不会漏。
    if (!screen) screen = QGuiApplication::primaryScreen();
    if (!screen) return;

    setGeometry(screen->geometry());

    m_subText = QStringLiteral("已经连续坐了 %1 分钟，站起来活动一下吧").arg(sitMinutes);
    m_tipText = QString::fromUtf8(kTips[QRandomGenerator::global()->bounded(kTipCount)]);

    m_clock->restart();
    m_anim->start();

    show();
    raise();
    activateWindow();
    setFocus();
}

void ReminderOverlay::onAnim()
{
    // 4 秒一个来回：0 → 1 → 0
    const double t = double(m_clock->elapsed() % 4000) / 4000.0;
    m_wave = 0.5 - 0.5 * qCos(2.0 * M_PI * t);
    update();
}

void ReminderOverlay::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    // 这个遮罩只盖一块屏幕，直接按自己的大小画一份即可
    const QRect r = rect();
    const int W = r.width();
    const int H = r.height();

    // ① 整屏白色底（窗口本身 90% 不透明，合起来就是"半透明白"）
    p.fillRect(r, QColor(255, 255, 255));

    // ② 中央的球，随呼吸轻轻涨缩
    const double radius = 100.0 + 10.0 * m_wave;
    const QPointF center(r.left() + W / 2.0, r.top() + H * 0.38);
    drawBall(p, center, radius, m_wave);

    // ③ 文案（都以这块屏的横向中线对齐）
    QFont titleFont = font();
    titleFont.setPixelSize(30);
    titleFont.setBold(true);
    p.setFont(titleFont);
    p.setPen(QColor(QStringLiteral("#2E5E4B")));
    const QRect titleRect(r.left(), int(center.y() + radius + 34), W, 48);
    p.drawText(titleRect, Qt::AlignHCenter | Qt::AlignTop,
               QStringLiteral("该起来活动一下了"));

    QFont subFont = font();
    subFont.setPixelSize(15);
    p.setFont(subFont);
    p.setPen(QColor(QStringLiteral("#4F6E60")));
    const QRect subRect(r.left(), titleRect.bottom() + 10, W, 28);
    p.drawText(subRect, Qt::AlignHCenter | Qt::AlignTop, m_subText);

    QFont tipFont = font();
    tipFont.setPixelSize(17);
    p.setFont(tipFont);
    p.setPen(QColor(QStringLiteral("#33302B")));
    const QRect tipRect(r.left(), subRect.bottom() + 20, W, 30);
    p.drawText(tipRect, Qt::AlignHCenter | Qt::AlignTop, m_tipText);

    QFont footFont = font();
    footFont.setPixelSize(13);
    p.setFont(footFont);
    p.setPen(QColor(QStringLiteral("#8A8578")));
    const QRect footRect(r.left(), r.bottom() - 96, W, 26);
    p.drawText(footRect, Qt::AlignHCenter | Qt::AlignTop,
               QStringLiteral("点击任意位置继续"));
}

void ReminderOverlay::mousePressEvent(QMouseEvent *event)
{
    event->accept();
    m_anim->stop();
    hide();
    emit dismissed();
}

void ReminderOverlay::keyPressEvent(QKeyEvent *event)
{
    // 按 Esc 也当点过
    if (event->key() == Qt::Key_Escape) {
        m_anim->stop();
        hide();
        emit dismissed();
        return;
    }
    QWidget::keyPressEvent(event);
}

// ============================================================
// ReminderBubble（小气泡）
// ============================================================
ReminderBubble::ReminderBubble(QWidget *parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setCursor(Qt::PointingHandCursor);
    setFixedWidth(330);

    // 8 秒自动收起用的
    m_autoClose = new QTimer(this);
    m_autoClose->setSingleShot(true);
    connect(m_autoClose, &QTimer::timeout, this, [this]() {
        if (m_countdownMode) return;   // 倒计时模式不自动关
        // 只是先藏起来，不当作"用户已经确认"：
        // 久坐提醒的气泡过一会儿还会重弹，直到用户真的点一下
        hideBubble();
    });
}

void ReminderBubble::showNotice(const QString &title, const QString &body,
                                const QRect &anchor, int autoCloseMs)
{
    m_countdownMode = false;
    m_actionMode = false;
    m_titleText = title;
    m_bodyText = body;
    updateSize();
    placeNear(anchor);
    m_autoClose->start(autoCloseMs);
    show();
    raise();
}

// 带按钮的提醒：定时放松计划到点时用。
// 比普通提示高一点，底部放「开始」「跳过」两个按钮。
void ReminderBubble::showNoticeWithAction(const QString &title, const QString &body,
                                          const QRect &anchor, int autoCloseMs)
{
    m_countdownMode = false;
    m_actionMode = true;
    m_titleText = title;
    m_bodyText = body;
    m_leftText = QStringLiteral("跳过");
    m_actionText = QStringLiteral("开始");   // 普通计划提醒仍然使用原来的按钮文字
    updateSize();
    placeNear(anchor);
    m_autoClose->start(autoCloseMs);
    show();
    raise();
}

// 新手导引不会自动消失，必须由用户点「下一步」或「跳过」。
void ReminderBubble::showGuideStep(const QString &title, const QString &body,
                                   const QRect &anchor, const QString &actionText)
{
    m_countdownMode = false;
    m_actionMode = true;
    m_autoClose->stop();
    m_titleText = title;
    m_bodyText = body;
    m_leftText = QStringLiteral("跳过");
    m_actionText = actionText;
    updateSize();
    placeNear(anchor);
    show();
    raise();
}

// 右键点球的「重新计时」气泡：左边「关闭」、右边「重新计时」
void ReminderBubble::showRestartAction(const QString &title, const QString &body,
                                       const QRect &anchor)
{
    m_countdownMode = false;
    m_actionMode = true;
    m_autoClose->stop();
    m_titleText = title;
    m_bodyText = body;
    m_leftText = QStringLiteral("关闭");
    m_actionText = QStringLiteral("重新计时");
    updateSize();
    placeNear(anchor);
    show();
    raise();
}

void ReminderBubble::showCountdown(int seconds, const QRect &anchor)
{
    m_countdownMode = true;
    m_actionMode = false;
    m_autoClose->stop();

    if (seconds < 0) seconds = 0;
    const QString timeText = QStringLiteral("%1:%2")
                                 .arg(seconds / 60, 2, 10, QLatin1Char('0'))
                                 .arg(seconds % 60, 2, 10, QLatin1Char('0'));
    m_titleText = QStringLiteral("站起来活动一下");
    m_bodyText = QStringLiteral("还剩 %1，活动完会自动重新计时").arg(timeText);
    updateSize();
    placeNear(anchor);
    show();
    // 文字变了必须重绘，否则气泡已经显示时，倒计时数字不会每秒跳
    update();
}

void ReminderBubble::hideBubble()
{
    m_autoClose->stop();
    hide();
}

// 根据文字算出气泡多高，并把圆角以外的区域裁掉
void ReminderBubble::updateSize()
{
    QFont bodyFont = font();
    bodyFont.setPixelSize(12);
    const QFontMetrics fm(bodyFont);
    const int bodyW = width() - 36;
    const QRect need = fm.boundingRect(QRect(0, 0, bodyW, 1000),
                                       Qt::TextWordWrap, m_bodyText);
    // 按钮模式时底部多留 40px 放两个按钮
    const int h = 14 + 22 + 8 + need.height() + 14 + (m_actionMode ? 40 : 0);
    setFixedHeight(h);

    // 圆角矩形遮罩：不用半透明背景也能有圆角
    QPainterPath path;
    path.addRoundedRect(QRectF(0, 0, width(), height()), 12, 12);
    setMask(QRegion(path.toFillPolygon().toPolygon()));
}

// 把气泡放在锚点（桌面球）上方，并且别跑出屏幕
void ReminderBubble::placeNear(const QRect &anchor)
{
    QScreen *scr = QGuiApplication::screenAt(anchor.center());
    if (!scr) scr = QGuiApplication::primaryScreen();
    const QRect avail = scr ? scr->availableGeometry() : QRect(0, 0, 1280, 800);

    int x = anchor.center().x() - width() / 2;
    int y = anchor.top() - height() - 12;

    if (x < avail.left() + 8) x = avail.left() + 8;
    if (x + width() > avail.right() - 8) x = avail.right() - 8 - width();
    if (y < avail.top() + 8) y = anchor.bottom() + 12;   // 上面放不下就放下面

    move(x, y);
}

// 气泡底部两个按钮的位置（内部坐标）。
// 宽 330、左右各留 18 的边距，两个按钮平分中间宽度。
QRect ReminderBubble::startButtonRect() const
{
    const int y = height() - 10 - 30;
    const int w = (width() - 18 * 2 - 10) / 2;
    return QRect(18 + w + 10, y, w, 30);
}

QRect ReminderBubble::skipButtonRect() const
{
    const int y = height() - 10 - 30;
    const int w = (width() - 18 * 2 - 10) / 2;
    return QRect(18, y, w, 30);
}

void ReminderBubble::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    // 圆角白卡片 + 浅灰绿描边
    const QRectF r = rect().adjusted(1, 1, -1, -1);
    p.setPen(QPen(QColor(QStringLiteral("#DCE6DF")), 1));
    p.setBrush(QColor(255, 255, 255, 246));
    p.drawRoundedRect(r, 12, 12);

    QFont titleFont = font();
    titleFont.setPixelSize(15);
    titleFont.setBold(true);
    p.setFont(titleFont);
    p.setPen(QColor(QStringLiteral("#2E5E4B")));
    p.drawText(QRect(18, 14, width() - 36, 22), Qt::AlignLeft | Qt::AlignVCenter,
               m_titleText);

    QFont bodyFont = font();
    bodyFont.setPixelSize(12);
    p.setFont(bodyFont);
    p.setPen(QColor(QStringLiteral("#4F4A42")));
    // 有按钮时，文字区要让出底部 40px，否则会和按钮叠在一起
    const int bodyH = height() - 58 - (m_actionMode ? 40 : 0);
    p.drawText(QRect(18, 44, width() - 36, bodyH),
               Qt::TextWordWrap | Qt::AlignLeft | Qt::AlignTop, m_bodyText);

    // ---------- 「开始」「跳过」两个按钮 ----------
    if (m_actionMode) {
        // 跳过/关闭：低调的描边按钮
        p.setPen(QPen(QColor(QStringLiteral("#DCE6DF")), 1));
        p.setBrush(QColor(QStringLiteral("#FBF9F4")));
        p.drawRoundedRect(QRectF(skipButtonRect()), 6, 6);
        QFont btnFont = font();
        btnFont.setPixelSize(12);
        p.setFont(btnFont);
        p.setPen(QColor(QStringLiteral("#6B655C")));
        p.drawText(skipButtonRect(), Qt::AlignCenter, m_leftText);

        // 开始：绿色实心，一眼就能看到
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(QStringLiteral("#4F8E73")));
        p.drawRoundedRect(QRectF(startButtonRect()), 6, 6);
        p.setPen(QColor(255, 255, 255));
        p.drawText(startButtonRect(), Qt::AlignCenter, m_actionText);
    }
}

void ReminderBubble::mousePressEvent(QMouseEvent *event)
{
    event->accept();

    // 按钮模式：先判断点的是哪个按钮
    if (m_actionMode) {
        if (startButtonRect().contains(event->pos())) {
            hideBubble();
            emit actionRequested();
            return;
        }
        if (skipButtonRect().contains(event->pos())) {
            hideBubble();
            emit dismissed();
            return;
        }
    }

    hideBubble();
    emit dismissed();
}
