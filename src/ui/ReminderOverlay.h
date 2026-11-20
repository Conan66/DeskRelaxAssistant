#ifndef REMINDEROVERLAY_H
#define REMINDEROVERLAY_H

#include <QColor>
#include <QString>
#include <QWidget>

class QElapsedTimer;
class QTimer;
class QScreen;

// ============================================================
// 久坐提醒 · 全屏遮罩（强提醒，默认）
// ------------------------------------------------------------
// 一层半透明白色盖住整个屏幕，中间画一个大呼吸球 + 一行大字，
// 点鼠标任意位置就消失。目的就是"抓眼球"。
//
// 注意：这里刻意不用子控件，所有东西都在 paintEvent 里直接画。
// 全屏置顶的半透明窗口在部分机器上会出现"子控件不显示"的问题，
// 自己画最稳，也少一层依赖。
//
// 每块屏幕各建一个 ReminderOverlay 实例（而不是用一个窗口横跨多屏）：
// 两块屏幕缩放比例不同时，一个跨屏窗口只有一个 DPI，无法同时正确
// 覆盖不同缩放的两块屏，会导致副屏只盖住一半。一屏一个就各管各的。
// ============================================================
class ReminderOverlay : public QWidget
{
    Q_OBJECT
public:
    explicit ReminderOverlay(QWidget *parent = nullptr);

    // 在指定屏幕上显示；sitMinutes 用来写"已经坐了多久"
    // screen 传空则用主屏
    void popup(int sitMinutes, QScreen *screen = nullptr);

signals:
    void dismissed();   // 用户点了（或按了 Esc）

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void onAnim();

private:
    QString m_subText;      // "已经连续坐了 45 分钟…"
    QString m_tipText;      // 随机换的一句提示

    QTimer *m_anim = nullptr;
    QElapsedTimer *m_clock = nullptr;
    double m_wave = 0.0;    // 0~1 的呼吸波形，用来让球轻轻起伏
};

// ============================================================
// 久坐提醒 · 小气泡（温和提醒，也可用来显示站立倒计时）
// ------------------------------------------------------------
// 贴在桌面球旁边，不遮屏幕。同样全部自己画，不用子控件。
// ============================================================
class ReminderBubble : public QWidget
{
    Q_OBJECT
public:
    explicit ReminderBubble(QWidget *parent = nullptr);

    void showNotice(const QString &title, const QString &body,
                    const QRect &anchor, int autoCloseMs = 8000);
    void showCountdown(int seconds, const QRect &anchor);

    // 带「开始」「跳过」两个按钮的提醒（定时放松计划用）。
    // 点「开始」发 actionRequested()，点「跳过」发 dismissed()。
    void showNoticeWithAction(const QString &title, const QString &body,
                              const QRect &anchor, int autoCloseMs = 20000);

    // 新手引导专用：和上面一样是两个按钮，但右边按钮的文字可以自定义
    // （"下一步" / "开始放松"），并且不会自动关闭——
    // 引导要等用户看完点按钮，不能像提醒一样几秒后自己消失。
    void showGuideStep(const QString &title, const QString &body,
                       const QRect &anchor, const QString &actionText);

    // 右键点球的「重新计时」气泡：两个按钮分别是「关闭」「重新计时」。
    // 点「重新计时」发 actionRequested()，点「关闭」发 dismissed()。
    void showRestartAction(const QString &title, const QString &body,
                           const QRect &anchor);

    void hideBubble();

signals:
    void dismissed();        // 用户点掉了气泡
    void actionRequested();  // 用户点了「开始」

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    void updateSize();
    void placeNear(const QRect &anchor);
    // 两个按钮的位置（在气泡内部坐标里），只有 action 模式才算得出来
    QRect startButtonRect() const;
    QRect skipButtonRect() const;

    QString m_titleText;
    QString m_bodyText;
    QString m_leftText = QStringLiteral("跳过");   // 左侧按钮文字（重新计时气泡时变成"关闭"）
    QString m_actionText = QStringLiteral("开始");   // 右侧按钮文字（引导时变成"下一步"）
    QTimer *m_autoClose = nullptr;
    bool m_countdownMode = false;
    bool m_actionMode = false;   // true = 显示「开始 / 跳过」按钮
};

#endif // REMINDEROVERLAY_H
