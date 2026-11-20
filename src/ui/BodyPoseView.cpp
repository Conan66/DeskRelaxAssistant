#include "BodyPoseView.h"

#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>
#include <QtMath>

#include <cmath>

namespace {
// 线条颜色：和呼吸示意图一致的近黑色
const QColor kInk(QStringLiteral("#1E1E1E"));
// 箭头颜色：暖赭色——和黑色线稿明显区分，一眼能看出"这是动作指引"
const QColor kArrowInk(QStringLiteral("#C0723A"));
// 背景色：和呼吸示意图一致的米白
const QColor kPaper(QStringLiteral("#FBF9F4"));
}

BodyPoseView::BodyPoseView(QWidget *parent)
    : QWidget(parent)
{
    // 给一个最小尺寸，避免布局压缩时把人画成一条线
    setMinimumSize(300, 380);

    // 两张图交替的定时器：周期由 setImage2 指定（配合动作节奏）
    m_swap = new QTimer(this);
    m_swap->setInterval(2000);
    connect(m_swap, &QTimer::timeout, this, [this]() {
        m_showAlt = !m_showAlt;
        update();
    });

    // 圆周箭头的"流动"动画：25fps 就够顺滑。
    // 注意：QWidget::update 有好几个重载，直接取函数指针会编译不过，
    // 这里用一个小 lambda 包一层
    m_anim = new QTimer(this);
    m_anim->setInterval(40);
    connect(m_anim, &QTimer::timeout, this, [this]() { update(); });
}

void BodyPoseView::setPose(const Pose &pose)
{
    m_pose = pose;
    update();
}

void BodyPoseView::setViewMode(const QString &view)
{
    if (m_view == view) return;
    m_view = view;
    update();
}

void BodyPoseView::setArrow(const QString &arrow)
{
    if (m_arrow == arrow) return;
    m_arrow = arrow;
    // 圆周类箭头（画圈、转体）做成"流动"的，需要动画驱动；
    // 线性箭头（倒头、开肘、抬膝）是静态的，动画没必要开
    if (m_arrow == QLatin1String("circle") || m_arrow == QLatin1String("twist-left")
        || m_arrow == QLatin1String("twist-right")) {
        m_animClock.restart();
        m_anim->start();
    } else {
        m_anim->stop();
    }
    update();
}

void BodyPoseView::setImage(const QString &resPath, bool mirror)
{
    // 路径为空 = 这一步没有专属图，走简笔人兜底
    if (resPath.isEmpty()) {
        m_image = QImage();
    } else {
        // qrc 资源用 ":/" 前缀加载
        m_image = QImage(QStringLiteral(":/") + resPath);
    }
    m_mirror = mirror;
    m_showAlt = false;
    update();
}

void BodyPoseView::setImage2(const QString &resPath, int swapMs,
                             const QString &phaseA, const QString &phaseB)
{
    m_image2 = resPath.isEmpty() ? QImage()
                                 : QImage(QStringLiteral(":/") + resPath);
    m_phaseA = phaseA;
    m_phaseB = phaseB;
    m_swap->setInterval(qMax(1000, swapMs));   // 最少 1 秒，防止配错参数闪屏

    // 只有主图和第二张图都在，交替才有意义
    if (!m_image2.isNull() && !m_image.isNull())
        m_swap->start();
    else
        m_swap->stop();
    update();
}

void BodyPoseView::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.fillRect(rect(), kPaper);

    const QRectF r = rect();

    // 优先用图片：图比简笔人直观得多
    if (!m_image.isNull()) {
        // 留出边距，把图放进白色圆角卡片
        const double pad = qMin(r.width(), r.height()) * 0.03;
        const QRectF box = r.adjusted(pad, pad, -pad, -pad);
        paintImage(p, box);
        paintImageArrow(p, m_lastImgRect);
        return;
    }

    // 没图时的兜底：程序画的简笔人
    // 统一缩放单位：取宽、高里更"紧"的那个，
    // 这样窗口怎么拉，人都能完整待在画面里
    const double u = qMin(r.width() / 11.0, r.height() / 9.6);

    // 主线：粗细随缩放走，细但不虚
    p.setPen(QPen(kInk, qMax(1.5, u * 0.072), Qt::SolidLine,
                  Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);

    if (m_view == QLatin1String("side"))
        paintSide(p, r, u);
    else
        paintFront(p, r, u);
}

// ============================================================
// 图片模式
// ============================================================
void BodyPoseView::paintImage(QPainter &p, const QRectF &box)
{
    // 循环动作（如肩绕环"再画两圈"）在两张图之间交替
    const QImage &src = (!m_showAlt && !m_image.isNull()) ? m_image : m_image2;
    // 需要镜像就翻一张（水平翻转，线条画翻过来毫无违和）
    QImage img = m_mirror ? src.mirrored(true, false) : src;

    // 等比缩放到盒子大小
    const QSizeF isz(img.width(), img.height());
    double scale = qMin(box.width() / isz.width(), box.height() / isz.height());
    const QSizeF drawSz(isz.width() * scale, isz.height() * scale);
    // 居中
    const QPointF topLeft(box.center().x() - drawSz.width() / 2.0,
                          box.center().y() - drawSz.height() / 2.0);
    m_lastImgRect = QRectF(topLeft, drawSz);

    // 白色圆角卡片打底：图片本身是纯白底，加个描边看起来更整齐
    QPainterPath card;
    card.addRoundedRect(m_lastImgRect.adjusted(-6, -6, 6, 6), 10, 10);
    p.setPen(QPen(QColor(QStringLiteral("#E8E4DC")), 1));
    p.setBrush(QColor(255, 255, 255));
    p.drawPath(card);

    // 画图（平滑缩放，避免线稿出锯齿）
    p.setPen(Qt::NoPen);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    p.drawImage(m_lastImgRect, img);
    p.setRenderHint(QPainter::SmoothPixmapTransform, false);

    // ---------- 相位标签 ----------
    // 两张图交替的动作（如胸廓打开"打开·吸气 / 收回·呼气"），
    // 在图上方挂一个小圆角标签，显示当前这一拍的相位名。
    // 这样"哪张图 + 吸气还是呼气"一眼就能对上，跟练不用数拍子
    if (!m_image2.isNull() && !m_phaseA.isEmpty()) {
        const QString text = (m_showAlt && !m_phaseB.isEmpty())
                                 ? m_phaseB : m_phaseA;

        // 字号跟着图片大小走，窗口拉大缩小都合适
        QFont f = p.font();
        f.setPointSizeF(qMax(10.0, m_lastImgRect.width() * 0.026));
        f.setBold(true);
        p.setFont(f);
        const QFontMetrics fm(f);
        const int tw = fm.horizontalAdvance(text);
        const int th = fm.height();
        QRectF pill(0, 0, tw + th * 1.4, th * 1.7);
        pill.moveTopLeft(QPointF(m_lastImgRect.center().x() - pill.width() / 2.0,
                                 m_lastImgRect.top() + 10));

        // 白底胶囊 + 暖赭色文字：和箭头一个色系，视觉上是"一家人"
        p.setPen(QPen(QColor(QStringLiteral("#E8E4DC")), 1));
        p.setBrush(QColor(255, 255, 255));
        p.drawRoundedRect(pill, pill.height() / 2.0, pill.height() / 2.0);
        p.setPen(kArrowInk);
        p.drawText(pill, Qt::AlignCenter, text);
    }
}

void BodyPoseView::paintImageArrow(QPainter &p, const QRectF &ir)
{
    if (m_arrow.isEmpty() || m_arrow == QLatin1String("none")) return;

    const QPen saved = p.pen();
    p.setPen(QPen(kArrowInk, qMax(2.0, ir.width() * 0.008), Qt::SolidLine,
                  Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);

    // 图片里人物位置都差不多（全部 1024x1024、居中构图），
    // 按"图的百分比"定位箭头就够了：
    //   头心 ≈ (中心, 20% 高)   肩 ≈ 35% 高   髋 ≈ 62% 高
    const double cx = ir.center().x();
    const double headY = ir.top() + ir.height() * 0.20;
    const double shoulderY = ir.top() + ir.height() * 0.35;
    const double hipY = ir.top() + ir.height() * 0.62;
    const double headR = ir.width() * 0.16;

    // 画一段椭圆弧 + 末端箭头
    auto arcArrow = [&](const QPointF &c, double radius,
                        double startDeg, double spanDeg) {
        const QRectF box(c.x() - radius, c.y() - radius * 0.86,
                         radius * 2.0, radius * 1.72);
        p.drawArc(box, int(startDeg * 16), int(spanDeg * 16));

        auto pointAt = [&](double deg) {
            const double a = qDegreesToRadians(deg);
            return QPointF(c.x() + radius * std::cos(a),
                           c.y() - radius * 0.86 * std::sin(a));
        };
        const double endDeg = startDeg + spanDeg;
        const QPointF tip = pointAt(endDeg);
        const QPointF prev = pointAt(endDeg - (spanDeg > 0 ? 2.0 : -2.0));
        const double ang = std::atan2(tip.y() - prev.y(), tip.x() - prev.x());
        paintArrowHead(p, tip, ang, ir.width() * 0.045);
    };

    // 流动弧：一条带透视的"轨道椭圆" + 几个沿轨道循环移动的实心箭头。
    // ============================================================
    //   ① 压扁椭圆形成"斜挂在身体上"的感觉，像一个环在肩膀上绕
    //   ② 上下两段分别画：上半部分实（肩膀往前绕，靠近观察者），
    //                      下半部分淡（肩膀往后沉，远离观察者）
    //                      ——这就是"近大远小、近实远虚"的透视
    //   ③ 箭头沿轨道循环流动，每个箭头的透明度和尺寸也跟着它
    //      跑到椭圆的哪个位置动态变化，强化"前后环绕"的方向感
    auto flowArc = [&](const QPointF &c, double radius,
                       double startDeg, double spanDeg, int periodMs) {
        // 把椭圆压得扁扁的——太圆就像画了个平面的圈，
        // 压扁一点就像一个斜挂的环，有前后环绕的暗示
        const double squash = 0.58;

        auto pointAt = [&](double deg) {
            const double a = qDegreesToRadians(deg);
            return QPointF(c.x() + radius * std::cos(a),
                           c.y() - radius * squash * std::sin(a));
        };

        // 整个椭圆的外接盒子（y 轴压扁 squash 倍）
        const QRectF box(c.x() - radius, c.y() - radius * squash,
                         radius * 2.0, radius * 2.0 * squash);
        const int fullSpan = int(spanDeg * 16);

        // ① 先画"远半周"（下半部分，淡）——画在底下，让近半周叠在上面
        //    远半周 = 椭圆下半部分（屏幕 y 更大），代表肩膀往后沉远离观察者
        {
            QPen track(QColor(kArrowInk.red(), kArrowInk.green(),
                              kArrowInk.blue(), 55));
            track.setWidthF(qMax(2.0, radius * 0.12));
            track.setCapStyle(Qt::RoundCap);
            p.setPen(track);
            p.setBrush(Qt::NoBrush);
            p.drawArc(box, int(startDeg * 16), fullSpan);
        }

        // ② 再画"近半周"（上半部分，实一点）——叠在远半周上面
        //    近半周 = sin(a) < 0 → 椭圆的上半部分（屏幕 y 更小，靠近观察者）
        //    只画上半圈的 180°：从 -90° 到 90°（Qt 角度的"上半"）
        {
            QPen nearTrack(QColor(kArrowInk.red(), kArrowInk.green(),
                                  kArrowInk.blue(), 140));
            nearTrack.setWidthF(qMax(2.5, radius * 0.15));
            nearTrack.setCapStyle(Qt::RoundCap);
            p.setPen(nearTrack);
            p.setBrush(Qt::NoBrush);

            // 近半周的角度范围：以 c.y 为界，椭圆上的 y < c.y 就是近半周
            // 等价于 Qt 角度的 sin(a) < 0，即 a ∈ [0°, 180°]（Qt 是逆时针，上半圈是 0→180→360 的中间）
            // 但我们的 startDeg 和 spanDeg 可能只覆盖其中一部分，所以先算交集
            double segStart = qMax(startDeg, 0.0);      // Qt 0° = 右，逆时针画到 180°（上方+左侧）
            double segEnd   = qMin(startDeg + spanDeg, 180.0);
            if (segEnd > segStart) {
                p.drawArc(box, int(segStart * 16), int((segEnd - segStart) * 16));
            }
            // 如果 spanDeg 跨过 180°，前半段画了还要画 [360°, 180°) 那段——不，Qt 的上半圈就是 0→180
            // 完整一圈的上半部分就是 0° 到 180°，已经覆盖了"椭圆上方"
        }

        // ③ 三个实心箭头沿轨道循环流动：
        //    每个箭头的 alpha 和 size 还要根据"跑到椭圆上半还是下半"做透视
        const double base = std::fmod(m_animClock.elapsed() / double(periodMs), 1.0);
        for (int i = 0; i < 3; ++i) {
            const double prog = std::fmod(base + i / 3.0, 1.0);
            const double deg = startDeg + spanDeg * prog;
            const QPointF tip = pointAt(deg);
            const QPointF prev = pointAt(deg - (spanDeg > 0 ? 3.0 : -3.0));
            const double ang = std::atan2(tip.y() - prev.y(), tip.x() - prev.x());

            // 透视因子：椭圆上半部分（tip.y < c.y）→ 靠近观察者 → 1.0
            //          椭圆下半部分（tip.y > c.y）→ 远离        → 0.4
            // 用 tip.y 到 c.y 的距离做平滑过渡（越远离越淡越小）
            const double yRatio = (tip.y() - c.y()) / (radius * squash);   // -1 ~ +1
            const double depth = 0.5 + 0.5 * yRatio;   // 近=0 → depth=0.5; 远=+1 → depth=1.0
            const double perspAlpha = 1.0 - 0.6 * depth;           // 近=1.0, 远=0.4
            const double perspSize  = 1.0 - 0.30 * depth;          // 近=1.0, 远=0.7

            const double flowAlpha = std::sin(M_PI * prog);   // 0→1→0 淡入淡出包络
            const double alpha = 0.9 * perspAlpha * flowAlpha;
            const double size = ir.width() * 0.052 * perspSize;

            const QPointF u(std::cos(ang), std::sin(ang));   // 前进方向
            const QPointF n(-u.y(), u.x());                  // 垂直方向
            QPolygonF tri;
            tri << tip
                << tip - u * size + n * (size * 0.45)
                << tip - u * size - n * (size * 0.45);
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(kArrowInk.red(), kArrowInk.green(),
                              kArrowInk.blue(), int(255 * alpha)));
            p.drawPolygon(tri);
        }
    };

    if (m_arrow == QLatin1String("tilt-right")) {
        // 用户向右倒 = 图的左边：弧从头顶扫向左下
        arcArrow(QPointF(cx, headY), headR, 95, 55);
    } else if (m_arrow == QLatin1String("tilt-left")) {
        // 用户向左倒 = 图的右边
        arcArrow(QPointF(cx, headY), headR, 85, -55);
    } else if (m_arrow == QLatin1String("circle")) {
        // 肩膀画圈：左右肩各一条流动弧。
        // 淡轨道提示路径，三个实心箭头沿轨道循环流动；
        // 4 秒扫半圈，刚好配合"吸气抬起 4 拍 + 呼气沉下 4 拍"的节奏
        const double rr = ir.width() * 0.11;
        flowArc(QPointF(cx - ir.width() * 0.22, shoulderY), rr, 200, -180, 4000);
        flowArc(QPointF(cx + ir.width() * 0.22, shoulderY), rr, 340, 180, 4000);
    } else if (m_arrow == QLatin1String("open")) {
        // 两肘向外打开：左右各一个横箭头
        const double y = shoulderY;
        p.drawLine(QPointF(cx - ir.width() * 0.16, y),
                   QPointF(cx - ir.width() * 0.30, y));
        paintArrowHead(p, QPointF(cx - ir.width() * 0.30, y), M_PI, ir.width() * 0.04);
        p.drawLine(QPointF(cx + ir.width() * 0.16, y),
                   QPointF(cx + ir.width() * 0.30, y));
        paintArrowHead(p, QPointF(cx + ir.width() * 0.30, y), 0.0, ir.width() * 0.04);
    } else if (m_arrow == QLatin1String("twist-right")) {
        // 用户向右转 = 图里脸转向左边：流动箭头从右往左扫过胸口，
        // 提示"呼气时慢慢往这边转"
        flowArc(QPointF(cx, ir.top() + ir.height() * 0.44),
                ir.width() * 0.24, 20, 140, 4000);
    } else if (m_arrow == QLatin1String("twist-left")) {
        flowArc(QPointF(cx, ir.top() + ir.height() * 0.44),
                ir.width() * 0.24, 160, -140, 4000);
    } else if (m_arrow == QLatin1String("bend-right")) {
        // 用户向右弯 = 图的左边：弧沿身体左侧往下扫
        arcArrow(QPointF(cx, hipY), ir.width() * 0.26, 100, 60);
    } else if (m_arrow == QLatin1String("bend-left")) {
        arcArrow(QPointF(cx, hipY), ir.width() * 0.26, 80, -60);
    } else if (m_arrow == QLatin1String("lift")) {
        // 抬膝：膝盖右侧画一个向上的竖箭头
        const double ax = cx + ir.width() * 0.13;
        const QPointF from(ax, ir.top() + ir.height() * 0.62);
        const QPointF to(ax, ir.top() + ir.height() * 0.42);
        p.drawLine(from, to);
        paintArrowHead(p, to, -M_PI / 2.0, ir.width() * 0.04);
    }

    p.setPen(saved);
}

// ============================================================
// 正面视角
// ============================================================
void BodyPoseView::paintFront(QPainter &p, const QRectF &r, double u)
{
    const double cx = r.center().x();
    const double seatY = r.top() + r.height() * 0.66;

    // 关键高度：髋 → 肩 → 脖子
    const double hipY = seatY - 0.25 * u;
    const double shoulderY = hipY - 3.0 * u;
    const double neckY = shoulderY - 0.55 * u;

    // 侧屈：上身整体横移（弯得越多，上半身偏得越远）
    const double bendRad = qDegreesToRadians(m_pose.torsoBend);
    const double shift = std::sin(bendRad) * (hipY - shoulderY);

    const QPointF neck(cx + shift, neckY);
    const double headR = 0.8 * u;

    // 头：绕着脖子转 headTilt 度（正数 = 向画面右侧倒）
    const double tiltRad = qDegreesToRadians(m_pose.headTilt + m_pose.torsoBend * 0.4);
    const QPointF headC(neck.x() + std::sin(tiltRad) * (headR + 0.32 * u),
                        neck.y() - std::cos(tiltRad) * (headR + 0.32 * u));

    // 肩膀：上提（画圈到最高点）/ 下沉（后收）
    const double shoulderLift = (-0.30 * m_pose.shoulderUp + 0.22 * m_pose.shoulderBack) * u;
    const QPointF shoulderMid(cx + shift, shoulderY + shoulderLift);
    const QPointF sL(shoulderMid.x() - 1.35 * u, shoulderMid.y());
    const QPointF sR(shoulderMid.x() + 1.35 * u, shoulderMid.y());

    const QPointF hipMid(cx, hipY);
    const QPointF hipL(cx - 1.00 * u, hipY);
    const QPointF hipR(cx + 1.00 * u, hipY);

    // ① 椅子先画，人在椅子前面
    paintChairFront(p, cx, seatY, u);

    // ② 腿（膝盖朝前会缩短，但脚必须踩到地面 = 椅腿底端）
    const double floorY = seatY + 2.58 * u;
    const QPointF kneeL(cx - 1.12 * u, seatY + 1.15 * u);
    const QPointF kneeR(cx + 1.12 * u, seatY + 1.15 * u);
    const QPointF footL(cx - 1.20 * u, floorY);
    const QPointF footR(cx + 1.20 * u, floorY);
    p.drawLine(hipL, kneeL);
    p.drawLine(kneeL, footL);
    p.drawLine(hipR, kneeR);
    p.drawLine(kneeR, footR);
    p.drawLine(footL, footL + QPointF(0.50 * u, 0));   // 左脚掌朝外
    p.drawLine(footR, footR - QPointF(0.50 * u, 0));   // 右脚掌朝外

    // ③ 躯干（去掉多余的横线，只留两侧轮廓 + 肩线，看起来更像人）
    p.drawLine(hipL, sL);
    p.drawLine(hipR, sR);
    p.drawLine(sL, sR);

    // ④ 脖子 + 头（脖子只画到头的边上，不钻进去）
    const QPointF headDir = headC - neck;
    const double headLen = std::hypot(headDir.x(), headDir.y());
    if (headLen > 1.0) {
        p.drawLine(neck, headC - headDir / headLen * headR * 0.92);
    }
    // 头：整颗头（连同两边的耳朵）一起转。
    // 纯圆转了也看不出来，加两个小耳朵才能一眼看出"头歪向哪边"
    p.save();
    p.translate(headC);
    p.rotate(qRadiansToDegrees(tiltRad));
    p.drawEllipse(QPointF(0, 0), headR, headR);
    p.drawEllipse(QPointF(-headR * 0.97, headR * 0.06), headR * 0.17, headR * 0.17);
    p.drawEllipse(QPointF(headR * 0.97, headR * 0.06), headR * 0.17, headR * 0.17);
    p.restore();

    // ⑤ 手臂
    auto drawArm = [&](const QPointF &s, const QPointF &e, const QPointF &h) {
        p.drawLine(s, e);
        p.drawLine(e, h);
        p.drawEllipse(h, 0.14 * u, 0.14 * u);   // 手掌画个小圆
    };

    QPointF eL, hL, eR, hR;
    switch (m_pose.arms) {
    case 1:   // 双手抱脑后（胸廓打开）
        eL = QPointF(sL.x() - 0.85 * u, sL.y() + 0.10 * u);
        hL = QPointF(headC.x() - 0.70 * u, headC.y() - 0.10 * u);
        eR = QPointF(sR.x() + 0.85 * u, sR.y() + 0.10 * u);
        hR = QPointF(headC.x() + 0.70 * u, headC.y() - 0.10 * u);
        break;
    case 2:   // 右臂举过头顶向左侧伸（向右侧弯时用）
        eL = QPointF(sL.x() - 0.32 * u, sL.y() + 1.55 * u);
        hL = QPointF(eL.x() - 0.08 * u, eL.y() + 1.45 * u);
        eR = QPointF(headC.x() + 0.85 * u, headC.y() - 0.45 * u);
        hR = QPointF(headC.x() - 0.55 * u, headC.y() - 1.30 * u);
        break;
    case 5:   // case 2 的镜像：左臂举过头顶
        eR = QPointF(sR.x() + 0.32 * u, sR.y() + 1.55 * u);
        hR = QPointF(eR.x() + 0.08 * u, eR.y() + 1.45 * u);
        eL = QPointF(headC.x() - 0.85 * u, headC.y() - 0.45 * u);
        hL = QPointF(headC.x() + 0.55 * u, headC.y() - 1.30 * u);
        break;
    case 3:   // 向右转：右手放左膝，左手扶右椅背
        eR = QPointF(cx - 0.30 * u, shoulderY + 1.50 * u);
        hR = QPointF(cx - 0.95 * u, seatY + 0.55 * u);
        eL = QPointF(cx + 1.55 * u, shoulderY + 1.20 * u);
        hL = QPointF(cx + 2.00 * u, seatY - 1.10 * u);
        break;
    case 4:   // case 3 的镜像：向左转
        eL = QPointF(cx + 0.30 * u, shoulderY + 1.50 * u);
        hL = QPointF(cx + 0.95 * u, seatY + 0.55 * u);
        eR = QPointF(cx - 1.55 * u, shoulderY + 1.20 * u);
        hR = QPointF(cx - 2.00 * u, seatY - 1.10 * u);
        break;
    default:  // 自然下垂，手掌落在自己大腿上
        eL = QPointF(sL.x() - 0.12 * u, sL.y() + 1.45 * u);
        hL = QPointF(cx - 0.95 * u, seatY - 0.05 * u);
        eR = QPointF(sR.x() + 0.12 * u, sR.y() + 1.45 * u);
        hR = QPointF(cx + 0.95 * u, seatY - 0.05 * u);
        break;
    }
    drawArm(sL, eL, hL);
    drawArm(sR, eR, hR);

    // ⑥ 方向箭头（叠加在最上面）
    paintArrow(p, headC, shoulderMid, hipMid, u, headR, kneeR);
}

// ============================================================
// 侧面视角（髋屈伸）
// ============================================================
void BodyPoseView::paintSide(QPainter &p, const QRectF &r, double u)
{
    const double seatY = r.top() + r.height() * 0.68;
    const double cx = r.left() + r.width() * 0.36;   // 人偏左，给前面的腿留位置

    const double hipY = seatY - 0.25 * u;
    const QPointF hip(cx, hipY);
    const double shoulderY = hipY - 2.90 * u;
    const QPointF neck(cx + 0.06 * u, shoulderY - 0.15 * u);
    const double headR = 0.78 * u;
    // 头要抬得离肩膀远一点，脖子那一段才看得见
    const QPointF headC(neck.x() + 0.05 * u, neck.y() - headR - 0.30 * u);

    // ① 椅子
    paintChairSide(p, cx, seatY, u);

    // ② 两条腿：一条平放的（画在后面），一条抬起来（画在前面）
    const double floorY = seatY + 2.35 * u;

    // 平放的腿（画得靠前一点，和抬起的腿分开，避免线条糊在一起）
    const QPointF kneeFlat(cx + 2.00 * u, seatY + 0.10 * u);
    const QPointF footFlat(cx + 2.10 * u, floorY);
    p.drawLine(hip, kneeFlat);
    p.drawLine(kneeFlat, footFlat);
    p.drawLine(footFlat, footFlat + QPointF(0.62 * u, 0));

    // 抬起的腿：大腿朝前上方（膝盖顶到胸口前方），小腿往下垂
    QPointF knee(cx + 1.75 * u, seatY + 0.10 * u);
    if (m_pose.kneeLift > 0.5) {
        // kneeLift 是"抬腿强度"，换算成大腿与水平面的夹角（0.72 倍）
        const double a = qDegreesToRadians(m_pose.kneeLift * 0.72);
        knee = QPointF(hip.x() + std::cos(a) * 1.75 * u,
                       hip.y() - std::sin(a) * 1.75 * u);
        // 小腿基本垂直往下垂
        const double a2 = qDegreesToRadians(m_pose.kneeLift * 0.12);
        const QPointF foot(knee.x() + std::sin(a2) * 1.25 * u,
                           knee.y() + std::cos(a2) * 1.25 * u);
        p.drawLine(hip, knee);
        p.drawLine(knee, foot);
        p.drawLine(foot, foot + QPointF(0.50 * u, 0));
    }

    // ③ 躯干：后背一条线 + 前胸一条线，做出厚度
    p.drawLine(hip, QPointF(cx - 0.14 * u, shoulderY));
    p.drawLine(QPointF(cx + 0.78 * u, hipY - 0.05 * u),
               QPointF(cx + 0.52 * u, shoulderY + 0.15 * u));
    p.drawLine(hip, QPointF(cx + 0.82 * u, hipY));
    p.drawLine(QPointF(cx - 0.14 * u, shoulderY),
               QPointF(cx + 0.52 * u, shoulderY + 0.15 * u));

    // ④ 脖子 + 头（侧面加个小鼻子，才看得出朝哪边）
    p.drawLine(QPointF(cx + 0.05 * u, shoulderY + 0.10 * u),
               QPointF(headC.x() - 0.02 * u, headC.y() + headR * 0.90));
    p.drawEllipse(headC, headR, headR);
    QPainterPath nose;
    nose.moveTo(headC.x() + headR * 0.95, headC.y() - headR * 0.10);
    nose.lineTo(headC.x() + headR * 1.18, headC.y() + headR * 0.12);
    nose.lineTo(headC.x() + headR * 0.93, headC.y() + headR * 0.28);
    p.drawPath(nose);

    // ⑤ 手臂：抱小腿 / 自然放手
    const QPointF shoulderPt(cx + 0.30 * u, shoulderY + 0.25 * u);
    QPointF elbow, hand;
    if (m_pose.holdShin) {
        elbow = QPointF(cx + 0.30 * u, shoulderY + 1.55 * u);
        hand = QPointF(knee.x() + 0.10 * u, knee.y() + 0.40 * u);
    } else {
        elbow = QPointF(cx + 0.45 * u, shoulderY + 1.55 * u);
        hand = QPointF(cx + 1.30 * u, seatY - 0.05 * u);
    }
    p.drawLine(shoulderPt, elbow);
    p.drawLine(elbow, hand);
    p.drawEllipse(hand, 0.14 * u, 0.14 * u);

    // ⑥ 箭头
    paintArrow(p, headC, QPointF(cx, shoulderY), hip, u, headR, knee);
}

// ============================================================
// 椅子
// ============================================================
void BodyPoseView::paintChairFront(QPainter &p, double cx, double seatY, double u)
{
    // 椅背两根立柱 + 上横档
    p.drawLine(QPointF(cx - 2.00 * u, seatY), QPointF(cx - 2.00 * u, seatY - 2.80 * u));
    p.drawLine(QPointF(cx + 2.00 * u, seatY), QPointF(cx + 2.00 * u, seatY - 2.80 * u));
    p.drawLine(QPointF(cx - 2.00 * u, seatY - 2.20 * u),
               QPointF(cx + 2.00 * u, seatY - 2.20 * u));

    // 座面
    p.drawRect(QRectF(cx - 2.20 * u, seatY, 4.40 * u, 0.28 * u));

    // 两条前腿
    p.drawLine(QPointF(cx - 2.05 * u, seatY + 0.28 * u),
               QPointF(cx - 2.15 * u, seatY + 2.30 * u));
    p.drawLine(QPointF(cx + 2.05 * u, seatY + 0.28 * u),
               QPointF(cx + 2.15 * u, seatY + 2.30 * u));
}

void BodyPoseView::paintChairSide(QPainter &p, double cx, double seatY, double u)
{
    // 座面
    p.drawLine(QPointF(cx - 0.65 * u, seatY), QPointF(cx + 2.05 * u, seatY));
    p.drawLine(QPointF(cx - 0.65 * u, seatY + 0.22 * u),
               QPointF(cx + 2.05 * u, seatY + 0.22 * u));

    // 靠背
    p.drawLine(QPointF(cx - 0.65 * u, seatY), QPointF(cx - 0.70 * u, seatY - 3.20 * u));
    p.drawLine(QPointF(cx - 0.65 * u, seatY + 0.22 * u),
               QPointF(cx - 0.55 * u, seatY - 3.10 * u));
    p.drawLine(QPointF(cx - 0.70 * u, seatY - 3.20 * u),
               QPointF(cx - 0.55 * u, seatY - 3.10 * u));

    // 前腿 + 后腿
    p.drawLine(QPointF(cx + 1.95 * u, seatY + 0.22 * u),
               QPointF(cx + 2.00 * u, seatY + 2.30 * u));
    p.drawLine(QPointF(cx - 0.62 * u, seatY + 0.22 * u),
               QPointF(cx - 0.68 * u, seatY + 2.30 * u));
}

// ============================================================
// 方向箭头
// ============================================================
void BodyPoseView::paintArrow(QPainter &p, const QPointF &headC, const QPointF &shoulderMid,
                              const QPointF &hipMid, double u, double headR,
                              const QPointF &knee)
{
    if (m_arrow.isEmpty() || m_arrow == QLatin1String("none")) return;

    const QPen saved = p.pen();
    p.setPen(QPen(kArrowInk, qMax(1.6, u * 0.070), Qt::SolidLine,
                  Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);

    // 画一段椭圆弧 + 末端箭头
    // 说明：Qt 的角度是"逆时针、0° 在 3 点方向"，所以 y 要取反来换算成屏幕坐标
    auto arcArrow = [&](const QPointF &c, double radius, double startDeg, double spanDeg) {
        const QRectF box(c.x() - radius, c.y() - radius * 0.86,
                         radius * 2.0, radius * 1.72);
        p.drawArc(box, int(startDeg * 16), int(spanDeg * 16));

        auto pointAt = [&](double deg) {
            const double a = qDegreesToRadians(deg);
            return QPointF(c.x() + radius * std::cos(a),
                           c.y() - radius * 0.86 * std::sin(a));
        };
        const double endDeg = startDeg + spanDeg;
        const QPointF tip = pointAt(endDeg);
        const QPointF prev = pointAt(endDeg - (spanDeg > 0 ? 2.0 : -2.0));
        const double ang = std::atan2(tip.y() - prev.y(), tip.x() - prev.x());
        paintArrowHead(p, tip, ang, u * 0.42);
    };

    if (m_arrow == QLatin1String("tilt-right")) {
        // 头的上方，从左上往右下画弧：表示头顶向右侧倒
        arcArrow(QPointF(headC.x(), headC.y() + headR * 0.40), headR * 2.30, 155, -85);
    } else if (m_arrow == QLatin1String("tilt-left")) {
        arcArrow(QPointF(headC.x(), headC.y() + headR * 0.40), headR * 2.30, 25, 85);
    } else if (m_arrow == QLatin1String("circle")) {
        // 肩膀画圈：左右肩各一条向上的弧（和图片模式的画法一致）
        arcArrow(QPointF(shoulderMid.x() - 1.70 * u, shoulderMid.y()),
                 0.85 * u, 200, -180);
        arcArrow(QPointF(shoulderMid.x() + 1.70 * u, shoulderMid.y()),
                 0.85 * u, 340, 180);
    } else if (m_arrow == QLatin1String("open")) {
        // 两肘向外打开：左右各一个横箭头
        const double y = shoulderMid.y() + 0.10 * u;
        p.drawLine(QPointF(shoulderMid.x() - 1.35 * u, y),
                   QPointF(shoulderMid.x() - 2.35 * u, y));
        paintArrowHead(p, QPointF(shoulderMid.x() - 2.35 * u, y), M_PI, u * 0.40);
        p.drawLine(QPointF(shoulderMid.x() + 1.35 * u, y),
                   QPointF(shoulderMid.x() + 2.35 * u, y));
        paintArrowHead(p, QPointF(shoulderMid.x() + 2.35 * u, y), 0.0, u * 0.40);
    } else if (m_arrow == QLatin1String("twist-right")) {
        arcArrow(QPointF(shoulderMid.x(), shoulderMid.y() + 0.75 * u), 1.90 * u, 150, -55);
    } else if (m_arrow == QLatin1String("twist-left")) {
        arcArrow(QPointF(shoulderMid.x(), shoulderMid.y() + 0.75 * u), 1.90 * u, 30, 55);
    } else if (m_arrow == QLatin1String("bend-right")) {
        // 腰侧弯：以髋为圆心，沿身体右侧往上画弧
        arcArrow(QPointF(hipMid.x(), hipMid.y()), 2.90 * u, 88, -50);
    } else if (m_arrow == QLatin1String("bend-left")) {
        arcArrow(QPointF(hipMid.x(), hipMid.y()), 2.90 * u, 92, 50);
    } else if (m_arrow == QLatin1String("lift")) {
        // 抬膝：在膝盖旁边画一个向上的竖箭头（不穿过身体）
        const QPointF from(knee.x() + 0.60 * u, knee.y() + 1.05 * u);
        const QPointF to(knee.x() + 0.60 * u, knee.y() - 0.35 * u);
        p.drawLine(from, to);
        paintArrowHead(p, to, -M_PI / 2.0, u * 0.40);
    }

    p.setPen(saved);
}

void BodyPoseView::paintArrowHead(QPainter &p, const QPointF &tip, double angleRad, double size)
{
    // 箭头尖：两条斜线，方向沿 angleRad 的反方向各偏 24°
    const double a1 = angleRad + M_PI - 0.42;
    const double a2 = angleRad + M_PI + 0.42;
    QPainterPath path;
    path.moveTo(tip + QPointF(std::cos(a1), std::sin(a1)) * size);
    path.lineTo(tip);
    path.lineTo(tip + QPointF(std::cos(a2), std::sin(a2)) * size);
    p.drawPath(path);
}
