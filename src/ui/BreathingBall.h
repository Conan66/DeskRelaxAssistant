#ifndef BREATHINGBALL_H
#define BREATHINGBALL_H

#include <QColor>
#include <QString>
#include <QWidget>

// ============================================================
// 呼吸球：产品唯一的视觉主体
// ------------------------------------------------------------
// 完全用 QPainter 画，不依赖任何图片资源，
// 所以任意尺寸都清晰（高 DPI 屏也不会糊）。
//
// 组成：外层柔光晕 + 渐变球体 + 球上方倒计时文字
// ============================================================
class BreathingBall : public QWidget
{
    Q_OBJECT
public:
    explicit BreathingBall(QWidget *parent = nullptr);

    // 每帧由外部（引导窗）推进来的视觉参数
    void setVisual(double ballScale, double haloScale, const QColor &color);

    // 球上方的倒计时文字；传空字符串就不显示
    void setCountdownText(const QString &text);

    // 是否显示倒计时文字（桌面小球的尺寸太小，不适合显示）
    void setTextVisible(bool on);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    double m_ballScale = 1.0;
    double m_haloScale = 1.0;
    QColor m_color;

    QString m_text;
    bool m_textVisible = true;
};

#endif // BREATHINGBALL_H
