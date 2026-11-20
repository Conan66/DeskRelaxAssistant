#include "DiagramView.h"

#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPolygonF>
#include <QResizeEvent>
#include <QScrollBar>
#include <QTimer>          // 延迟一帧再适配，等布局完成
#include <QWheelEvent>
#include <QtMath>

namespace {
// 把"归一化坐标"（0~1，相对整张图）换算成图片上的真实坐标。
// 这样换任意尺寸的底图，箭头都还在同一个身体位置上。
inline QPointF norm(const QRectF &r, double nx, double ny)
{
    return QPointF(r.left() + r.width() * nx, r.top() + r.height() * ny);
}

// 画一个实心小箭头（三角形），tip 是箭尖，dir 是"往哪指"
inline void drawArrowHead(QPainter *p, const QPointF &tip, const QPointF &dir, double size)
{
    const double len = std::sqrt(dir.x() * dir.x() + dir.y() * dir.y());
    if (len < 1e-6) return;
    const QPointF u(dir.x() / len, dir.y() / len);   // 单位方向向量
    const QPointF n(-u.y(), u.x());                  // 垂直于方向的法向量
    const QPointF base(tip.x() - u.x() * size, tip.y() - u.y() * size);
    QPolygonF tri;
    tri << tip << base + n * (size * 0.45) << base - n * (size * 0.45);
    p->drawPolygon(tri);
}
}

DiagramView::DiagramView(QWidget *parent)
    : QGraphicsView(parent)
{
    m_scene = new QGraphicsScene(this);
    setScene(m_scene);

    // 去掉边框和滚动条，看着干净
    setFrameShape(QFrame::NoFrame);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setDragMode(QGraphicsView::NoDrag);   // 自己处理拖拽
    setRenderHint(QPainter::SmoothPixmapTransform, true);
    setBackgroundBrush(QColor(QStringLiteral("#FBF9F4")));
}

void DiagramView::setImage(const QString &path)
{
    QPixmap pm(path);
    if (pm.isNull()) return;

    // 记下这张图是正面还是侧面：气流箭头要按朝向选不同的路线
    m_isFront = path.contains(QStringLiteral("front"));
    m_flowActive = false;

    m_scene->clear();
    m_item = m_scene->addPixmap(pm);
    m_item->setTransformationMode(Qt::SmoothTransformation);
    m_scene->setSceneRect(m_item->boundingRect());

    // 等控件真正有了尺寸再适配，否则 viewport 还是 0
    m_fitPending = true;
    m_userZoomed = false;   // 换图后重新回到"自动适配"状态
    resetView();

    // 兜底：此时窗口可能还没完成布局，等第一次事件循环结束再适配一次
    QTimer::singleShot(0, this, [this]() { if (!m_userZoomed) resetView(); });
}

// 让图片完整显示在视口内（等比缩小）
void DiagramView::resetView()
{
    if (!m_item) return;
    if (viewport()->width() < 10 || viewport()->height() < 10) {
        m_fitPending = true;
        return;
    }
    m_fitPending = false;

    fitInView(m_item, Qt::KeepAspectRatio);

    // fitInView 会直接改变换矩阵，这里同步一下自己的缩放系数
    m_zoom = transform().m11();
}

// 滚轮缩放：0.5 倍到 4 倍之间
void DiagramView::wheelEvent(QWheelEvent *event)
{
    if (!m_item) return;

    const double factor = (event->angleDelta().y() > 0) ? 1.15 : (1.0 / 1.15);
    double next = m_zoom * factor;

    if (next < 0.5) next = 0.5;
    if (next > 4.0) next = 4.0;
    if (qFuzzyCompare(next, m_zoom)) return;

    // 以鼠标位置为中心缩放
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    const double real = next / m_zoom;
    scale(real, real);
    m_zoom = next;
    m_userZoomed = true;   // 用户自己调过了，之后窗口变化不再自动适配

    event->accept();
}

// 按住左键拖动平移
void DiagramView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_item) {
        m_dragging = true;
        m_lastPos = event->pos();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }
    QGraphicsView::mousePressEvent(event);
}

void DiagramView::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragging) {
        const QPoint delta = event->pos() - m_lastPos;
        m_lastPos = event->pos();
        // 直接挪滚动条实现平移
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
        verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
        event->accept();
        return;
    }
    QGraphicsView::mouseMoveEvent(event);
}

void DiagramView::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_dragging && event->button() == Qt::LeftButton) {
        m_dragging = false;
        unsetCursor();
        event->accept();
        return;
    }
    QGraphicsView::mouseReleaseEvent(event);
}

// 窗口尺寸变化后，如果之前没适配成功（或用户没手动缩放过）就重新适配
void DiagramView::resizeEvent(QResizeEvent *event)
{
    QGraphicsView::resizeEvent(event);
    if (m_fitPending || !m_userZoomed) resetView();
}

// ============================================================
// 气流箭头：每帧由引导窗喂进"阶段 + 进度"
// ============================================================
void DiagramView::setFlow(PhaseType type, double progress, FlowRoute route)
{
    m_flowActive = true;
    m_flowType = type;
    m_route = route;
    m_flowProgress = qBound(0.0, progress, 1.0);
    viewport()->update();   // 只刷新视口，比 update() 省钱
}

void DiagramView::clearFlow()
{
    if (!m_flowActive) return;
    m_flowActive = false;
    viewport()->update();
}

// ============================================================
// 叠加层：在示意图上画会流动的气流箭头
// ------------------------------------------------------------
// 吸气：箭头从鼻子往下流到腹部
// 屏息：箭头停在原地不动
// 呼气：箭头从腹部往回流到鼻子
// ============================================================
void DiagramView::drawForeground(QPainter *painter, const QRectF &rect)
{
    Q_UNUSED(rect)
    if (!m_flowActive || !m_item) return;

    const QRectF r = m_item->boundingRect();
    if (r.width() < 10.0) return;

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    // ---------- 1) 腹部光圈 ----------
    // 吸气逐渐亮起，屏息保持最亮，呼气逐渐暗下去。
    // 画在箭头下面，像一团气在腹部聚起来，不会挡住箭头。
    double glow = 0.0;
    if (m_flowType == PhaseType::Inhale) {
        glow = 0.15 + 0.75 * m_flowProgress;
    } else if (m_flowType == PhaseType::Hold) {
        glow = 0.90;
    } else {
        glow = 0.90 - 0.75 * m_flowProgress;
    }
    const QPointF glowCenter = m_isFront ? norm(r, 0.500, 0.490)
                                         : norm(r, 0.545, 0.490);
    const double glowRadius = r.width() * 0.14;
    QRadialGradient halo(glowCenter, glowRadius);
    halo.setColorAt(0.0, QColor(126, 201, 166, int(105 * glow)));
    halo.setColorAt(0.55, QColor(126, 201, 166, int(55 * glow)));
    halo.setColorAt(1.0, QColor(126, 201, 166, 0));
    painter->setPen(Qt::NoPen);
    painter->setBrush(halo);
    painter->drawEllipse(glowCenter, glowRadius, glowRadius);

    // ---------- 2) 定义"气走的那条路" ----------
    QVector<QPainterPath> paths;
    if (m_route == FlowRoute::FootBelly) {
        // 涌泉 / 扎根：前脚掌 → 脚踝 → 小腿 → 膝盖 → 大腿 → 小腹。
        // 吸气正向（从脚往上），呼气反向（从腹送回脚底）。
        QPainterPath leg;
        leg.moveTo(norm(r, 0.303, 0.805));
        leg.cubicTo(norm(r, 0.320, 0.790), norm(r, 0.335, 0.770), norm(r, 0.340, 0.745));
        leg.cubicTo(norm(r, 0.340, 0.690), norm(r, 0.335, 0.630), norm(r, 0.350, 0.590));
        leg.cubicTo(norm(r, 0.390, 0.555), norm(r, 0.475, 0.540), norm(r, 0.520, 0.515));
        leg.cubicTo(norm(r, 0.540, 0.500), norm(r, 0.545, 0.492), norm(r, 0.550, 0.486));
        paths << leg;
    } else if (m_route == FlowRoute::Shoulder) {
        // 肩颈：左右肩上提（吸气）/ 下沉（呼气）
        QPainterPath left;
        left.moveTo(norm(r, 0.395, 0.345));
        left.lineTo(norm(r, 0.395, 0.285));
        QPainterPath right;
        right.moveTo(norm(r, 0.605, 0.345));
        right.lineTo(norm(r, 0.605, 0.285));
        paths << left << right;
    } else if (m_route == FlowRoute::SideRib) {
        // 侧肋：从肋骨中央向两侧打开，呼气时反向收回
        QPainterPath outward;
        outward.moveTo(norm(r, 0.510, 0.390));
        outward.cubicTo(norm(r, 0.540, 0.385), norm(r, 0.570, 0.380), norm(r, 0.600, 0.365));
        QPainterPath inward;
        inward.moveTo(norm(r, 0.510, 0.415));
        inward.cubicTo(norm(r, 0.480, 0.410), norm(r, 0.455, 0.400), norm(r, 0.435, 0.380));
        paths << outward << inward;
    } else if (m_isFront) {
        // 正面图：从鼻子分左右两条，沿两侧胸腔流到小腹
        QPainterPath left;
        left.moveTo(norm(r, 0.500, 0.190));
        left.cubicTo(norm(r, 0.440, 0.260), norm(r, 0.400, 0.340), norm(r, 0.402, 0.430));
        left.cubicTo(norm(r, 0.412, 0.452), norm(r, 0.425, 0.458), norm(r, 0.440, 0.458));

        QPainterPath right;
        right.moveTo(norm(r, 0.500, 0.190));
        right.cubicTo(norm(r, 0.560, 0.260), norm(r, 0.600, 0.340), norm(r, 0.598, 0.430));
        right.cubicTo(norm(r, 0.588, 0.452), norm(r, 0.575, 0.458), norm(r, 0.560, 0.458));

        paths << left << right;
    } else {
        // 侧面图：鼻尖 → 胸口 → 腹部，一条主线
        QPainterPath main;
        main.moveTo(norm(r, 0.500, 0.215));
        main.cubicTo(norm(r, 0.545, 0.270), norm(r, 0.552, 0.340), norm(r, 0.545, 0.400));
        main.cubicTo(norm(r, 0.540, 0.440), norm(r, 0.548, 0.462), norm(r, 0.550, 0.486));
        paths << main;
    }

    // ---------- 2) 画淡淡的路（表示气走的通道） ----------
    // 注意：这些尺寸是"图片坐标"里的，会跟着缩放一起变，
    // 所以按图片宽度的比例来定，换底图或缩放都不会跑偏。
    const double lineW = r.width() * 0.008;
    const double arrowSize = r.width() * 0.055;

    QPen guide(QColor(63, 143, 111, 80));
    guide.setWidthF(lineW * 0.9);
    guide.setStyle(Qt::CustomDashLine);
    guide.setDashPattern({3.0, 4.0});
    painter->setPen(guide);
    painter->setBrush(Qt::NoBrush);
    for (const QPainterPath &p : paths) painter->drawPath(p);

    // ---------- 3) 画流动的箭头 ----------
    const bool holding = (m_flowType == PhaseType::Hold);
    const bool exhaling = (m_flowType == PhaseType::Exhale);
    // 长路径放 4 个箭头；肩膀、侧肋的路径很短，只放 1 个，避免叠成一团。
    // 这里只改箭头数量，不改变原来的流动速度。
    const bool shortRoute = (m_route == FlowRoute::Shoulder || m_route == FlowRoute::SideRib);
    const int kParticles = shortRoute ? 1 : 4;

    for (int pathIndex = 0; pathIndex < paths.size(); ++pathIndex) {
        const QPainterPath &p = paths.at(pathIndex);
        // 侧肋的两条路挨得很近，第二条错开半个周期，避免箭头撞在一起。
        const double stagger = (m_route == FlowRoute::SideRib && pathIndex == 1) ? 0.5 : 0.0;
        for (int i = 0; i < kParticles; ++i) {
            double t;
            if (holding) {
                // 屏息：箭头原地不动，只是淡淡地标着"气停在这儿"
                t = 0.32 + i * 0.20 + stagger;
            } else {
                // 呼气时反着走，所以用 1 - t
                t = std::fmod(m_flowProgress + stagger + double(i) / kParticles, 1.0);
                if (exhaling) t = 1.0 - t;
            }
            t = qBound(0.0, t, 1.0);

            // 当前位置
            const QPointF pos = p.pointAtPercent(t);
            // 用前后两个点求切线方向，箭头才会顺着曲线拐弯
            const double dt = 0.015;
            const double tNext = qBound(0.0, exhaling ? (t - dt) : (t + dt), 1.0);
            QPointF dir = p.pointAtPercent(tNext) - pos;
            if (std::abs(dir.x()) < 1e-6 && std::abs(dir.y()) < 1e-6) dir = QPointF(0, exhaling ? -1 : 1);

            // 屏息时整体变淡
            QColor c(63, 143, 111, holding ? 150 : 235);

            // 先画一圈浅色"光晕"轮廓，深色底图上也看得清
            painter->setPen(QPen(QColor(255, 255, 255, holding ? 90 : 150), lineW * 0.8));
            painter->setBrush(QColor(255, 255, 255, holding ? 90 : 150));
            drawArrowHead(painter, pos, dir, arrowSize * 1.35);

            // 再画绿色箭头本体（箭头 + 一小段尾巴，方向更明确）
            painter->setPen(QPen(c, lineW, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter->setBrush(c);
            drawArrowHead(painter, pos, dir, arrowSize);

            const double dlen = std::sqrt(dir.x() * dir.x() + dir.y() * dir.y());
            if (dlen > 1e-6) {
                const QPointF tail(pos.x() - dir.x() / dlen * arrowSize * 1.6,
                                   pos.y() - dir.y() / dlen * arrowSize * 1.6);
                painter->drawLine(tail, pos);
            }
        }
    }

    painter->restore();
}
