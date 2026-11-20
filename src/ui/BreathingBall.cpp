#include "BreathingBall.h"

#include <QFontMetricsF>
#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>
#include <QtMath>

BreathingBall::BreathingBall(QWidget *parent)
    : QWidget(parent)
    , m_color(QStringLiteral("#5F9E83"))
{
    // 背景交给父窗口，自己不画背景（透明）
    setAttribute(Qt::WA_TranslucentBackground);
}

void BreathingBall::setVisual(double ballScale, double haloScale, const QColor &color)
{
    m_ballScale = ballScale;
    m_haloScale = haloScale;
    m_color = color;
    update();   // 请求重绘
}

void BreathingBall::setCountdownText(const QString &text)
{
    if (m_text == text) return;
    m_text = text;
    update();
}

void BreathingBall::setTextVisible(bool on)
{
    if (m_textVisible == on) return;
    m_textVisible = on;
    update();
}

void BreathingBall::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QRectF r = rect();

    // ---- 布局：球心略微偏下，上方留出倒计时文字的位置 ----
    const QPointF center(r.center().x(), r.top() + r.height() * 0.60);

    // 基准半径（球在 1.00 倍时的半径）
    // 要求：放大到最大 1.18 倍时，"球 + 光晕"仍要完整装在窗口里，
    //       并且球上方还要放得下倒计时文字，否则会被窗口边缘裁掉。
    const double kMaxScale = 1.18;   // 吸气/屏息时的最大放大倍数（PRD 4.0.2）
    const double kHaloPad  = 26.0;   // 光晕向外扩出去的近似像素
    // ① 球心下方只有 40% 的高度可用：球下半 + 光晕都得塞进去
    const double rLimitByBottom = (r.height() * 0.40 - kHaloPad) / kMaxScale;
    // ② 左右方向：球的直径 + 两侧光晕不能超过控件宽度
    const double rLimitByWidth = (r.width() * 0.50 - kHaloPad) / kMaxScale;
    // ③ 球心上方 60% 的高度要同时容纳"球上半 + 文字"，按 1.48 倍球半径估算
    const double rLimitByText = (r.height() * 0.60 - 10.0) / (kMaxScale * 1.48);

    double baseR = qMin(qMin(rLimitByBottom, rLimitByWidth), rLimitByText);
    if (baseR < 24.0) baseR = 24.0;   // 窗口极小时也别画成负半径

    const double ballR = baseR * m_ballScale;

    // ---- 1) 外层柔光晕：深色壁纸上靠它保证可见 ----
    // 光晕宽度从 6px 到约 16px 随阶段变化（PRD 4.0.1）
    const double haloW = 10.0 * m_haloScale;
    const double haloOuter = ballR + haloW * 1.8;

    QRadialGradient halo(center, haloOuter);
    QColor soft(QStringLiteral("#FFFDF8"));   // 米白
    soft.setAlphaF(0.55);
    halo.setColorAt(0.00, soft);
    soft.setAlphaF(0.42);
    halo.setColorAt(ballR / haloOuter, soft);
    soft.setAlphaF(0.00);
    halo.setColorAt(1.00, soft);

    p.setPen(Qt::NoPen);
    p.setBrush(halo);
    p.drawEllipse(center, haloOuter, haloOuter);

    // ---- 2) 球体：径向渐变做出立体感 ----
    // 高光点放在左上方，符合常见的受光方向
    const QPointF lightPos(center.x() - ballR * 0.30, center.y() - ballR * 0.34);
    QRadialGradient body(lightPos, ballR * 1.85);

    QColor cLight = m_color.lighter(122);   // 受光处更亮
    QColor cMid   = m_color;
    QColor cDeep  = m_color.darker(112);    // 边缘略深

    body.setColorAt(0.00, cLight);
    body.setColorAt(0.55, cMid);
    body.setColorAt(1.00, cDeep);

    p.setBrush(body);
    p.drawEllipse(center, ballR, ballR);

    // ---- 3) 球上方的倒计时文字 ----
    // 规格：黑体、白色字 + 深绿描边（深色壁纸也能看清）
    if (m_textVisible && !m_text.isEmpty()) {
        QFont f = font();
        f.setFamily(QStringLiteral("Microsoft YaHei"));
        f.setBold(true);
        // 240px 球对应约 28px 字，这里按球半径等比换算
        f.setPixelSize(qMax(12, int(ballR * 0.24)));
        p.setFont(f);

        const double textY = center.y() - ballR - ballR * 0.20;

        QPainterPath path;
        QFontMetricsF fm(f);
        const double tw = fm.horizontalAdvance(m_text);
        path.addText(center.x() - tw / 2.0, textY, f, m_text);

        // 先用深绿描边，再用白色填充
        // 描边粗细跟球大小走，但限制在 1.2 ~ 2.2px（太粗会把字糊掉）
        QPen outline(QColor(QStringLiteral("#4F8E73")));
        outline.setWidthF(qBound(1.2, ballR * 0.014, 2.2));
        p.setPen(outline);
        p.setBrush(Qt::NoBrush);
        p.drawPath(path);

        p.setPen(Qt::NoPen);
        p.setBrush(QColor(QStringLiteral("#FFFFFF")));
        p.drawPath(path);
    }
}
