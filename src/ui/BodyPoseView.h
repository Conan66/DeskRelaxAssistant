#ifndef BODYPOSEVIEW_H
#define BODYPOSEVIEW_H

#include <QWidget>

#include <QElapsedTimer>
#include <QTimer>

#include "../core/StretchLib.h"

// ============================================================
// 动作图示（程序画的简笔线稿）
// ------------------------------------------------------------
// 为什么不用图片：动作的关键是"往哪个方向、转到哪、手放哪"，
// 用代码画可以精确控制角度，还能随窗口大小无级缩放不糊。
//
// 画的东西和呼吸示意图统一：黑描边、米白底、无脸光头人偶 + 简笔木椅。
// 上面还会叠加"动作方向箭头"，颜色用墨棕 #4A423C（PRD 4.9 约定）。
// ============================================================
class BodyPoseView : public QWidget
{
    Q_OBJECT
public:
    explicit BodyPoseView(QWidget *parent = nullptr);

    // 换姿势（数值见 core/StretchLib.h 里的 Pose）
    void setPose(const Pose &pose);
    // "front" 正面 / "side" 侧面
    void setViewMode(const QString &view);
    // 叠加箭头：none / tilt-right / tilt-left / circle / open /
    //           twist-right / twist-left / bend-right / bend-left / lift
    void setArrow(const QString &arrow);

    // 本步的图示（qrc 路径）。传空字符串就回退到程序画的简笔人。
    // mirror = true 时水平镜像（图只画了单侧，另一侧复用同一张图）。
    void setImage(const QString &resPath, bool mirror);

    // 可选的第二张图：和主图按 swapMs 周期交替，表现循环动作；
    // phaseA/phaseB 显示当前相位的名称（如"打开·吸气"/"收回·呼气"）
    void setImage2(const QString &resPath, int swapMs = 2000,
                   const QString &phaseA = QString(),
                   const QString &phaseB = QString());

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    // ---- 有图时的绘制 ----
    // 把图居中放进白色圆角卡片里画出来
    void paintImage(QPainter &p, const QRectF &box);
    // 有图时的箭头：按图里"头/肩/髋"的大致位置叠加
    void paintImageArrow(QPainter &p, const QRectF &imgRect);
    // 在圆弧末端画一个箭头尖（图片模式复用）
    void paintArrowHead(QPainter &p, const QPointF &tip, double angleRad, double size);

    // ---- 没图时的兜底：程序画的简笔人 ----
    // 正面（肩颈、转体、侧屈）
    void paintFront(QPainter &p, const QRectF &r, double u);
    // 侧面（髋屈伸）
    void paintSide(QPainter &p, const QRectF &r, double u);
    // 画一把正面看的木椅（在身体后面）
    void paintChairFront(QPainter &p, double cx, double seatY, double u);
    // 画一把侧面看的木椅
    void paintChairSide(QPainter &p, double cx, double seatY, double u);
    // 画方向箭头（简笔人模式）
    void paintArrow(QPainter &p, const QPointF &headC, const QPointF &shoulderMid,
                    const QPointF &hipMid, double u, double headR,
                    const QPointF &knee);

    Pose m_pose;
    QString m_view = QStringLiteral("front");
    QString m_arrow = QStringLiteral("none");

    QImage m_image;          // 当前步骤的图（未镜像的原图）
    QImage m_image2;         // 可选的第二张图（循环动作交替用）
    bool m_mirror = false;   // 是否水平镜像
    QRectF m_lastImgRect;    // 上次画图的位置（箭头要叠在图上）
    QTimer *m_swap = nullptr;    // 两张图交替的定时器
    bool m_showAlt = false;      // 当前是否显示第二张图
    QString m_phaseA;            // 主图相位的名字
    QString m_phaseB;            // 第二张图相位的名字
    QTimer *m_anim = nullptr;    // 圆周箭头的"流动"动画驱动
    QElapsedTimer m_animClock;   // 动画时钟（计算流动位置用）
};

#endif // BODYPOSEVIEW_H
