#ifndef DIAGRAMVIEW_H
#define DIAGRAMVIEW_H

#include <QGraphicsView>
#include <QPainterPath>

#include "../core/BreathTypes.h"

// ============================================================
// 示意图查看器（可缩放、可拖拽、带气流箭头）
// ------------------------------------------------------------
// 用 QGraphicsView 实现，滚轮缩放 0.5x ~ 4x，按住拖动平移。
// 这是 PRD 4.0.3 的要求：示意图是"参考资料"，
// 用户想仔细看时可以放大，不想看可以关掉。
//
// 叠加层：在底图上画"气流箭头"
//   吸气：箭头从鼻子往下流到腹部
//   屏息：箭头静止不动
//   呼气：箭头从腹部往回流到鼻子
// ============================================================
class DiagramView : public QGraphicsView
{
    Q_OBJECT
public:
    explicit DiagramView(QWidget *parent = nullptr);

    // 换一张底图（qrc 路径或本地路径）
    void setImage(const QString &path);

    // 恢复"刚好铺满"的初始视图
    void resetView();

    // 每帧喂给气流箭头：阶段 + 该阶段进度（0.0 ~ 1.0）
    void setFlow(PhaseType type, double progress,
                 FlowRoute route = FlowRoute::NoseBelly);

    // 引导结束/关闭时把箭头收起来
    void clearFlow();

protected:
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    // 叠加层：底图之上画气流箭头（坐标系是"图片坐标"，会跟着缩放）
    void drawForeground(QPainter *painter, const QRectF &rect) override;

private:
    QGraphicsScene *m_scene = nullptr;
    QGraphicsPixmapItem *m_item = nullptr;

    double m_zoom = 1.0;
    bool m_dragging = false;
    QPoint m_lastPos;
    bool m_fitPending = false;
    // 用户是否手动缩放过：没缩放过时，窗口尺寸变化会自动重新适配，
    // 避免"图片在窗口还没定尺寸时就被适配成很小"的问题
    bool m_userZoomed = false;

    // ---- 气流箭头状态 ----
    bool m_flowActive = false;      // 是否正在显示箭头
    bool m_isFront = false;         // 当前底图是正面还是侧面（决定箭头走哪条路）
    PhaseType m_flowType = PhaseType::Inhale;
    FlowRoute m_route = FlowRoute::NoseBelly;
    double m_flowProgress = 0.0;
};

#endif // DIAGRAMVIEW_H
