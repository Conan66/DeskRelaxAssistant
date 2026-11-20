#ifndef DESKTOPBALLWINDOW_H
#define DESKTOPBALLWINDOW_H

#include <QWidget>

#include "../core/BreathTypes.h"

class BreathingBall;
class QTimer;
class QElapsedTimer;

// ============================================================
// 桌面常驻球
// ------------------------------------------------------------
// · 无边框、透明背景、始终置顶
// · 可拖拽，松手后自动吸附到最近的屏幕边缘
// · 待机时缓慢"呼吸"一下（1.00 ↔ 1.04），
//   让人一眼看出它还在跑，而不是卡死了
// · 左键点击弹出功能菜单（12 种呼吸 + 设置 + 退出）
// ============================================================
class DesktopBallWindow : public QWidget
{
    Q_OBJECT
public:
    explicit DesktopBallWindow(QWidget *parent = nullptr);

    // 引导窗打开时把桌面球藏起来（同屏只留一个球）
    void setSuppressed(bool on);

    // 托盘菜单"隐藏小球"用：只是不显示，和引导窗的抑制互不干扰
    void setUserHidden(bool on);

    // 把球搬回主屏右下角（托盘双击时用，解决"球跑到屏幕外找不回来"）
    void resetToCorner();

    // 按用户选的呼吸，重新计算桌面球要用的节奏。
    // 放在 public 里：用户在"节奏设置"里改完秒数并保存后，
    // main.cpp 会马上调用它，让桌面球立刻跟着新节奏动，不用等下次点菜单。
    void applyDeskRhythm();

signals:
    // 用户从菜单里选了某种呼吸
    void breathChosen(const QString &code);
    // 用户点了"节奏设置"
    void settingsRequested();
    // 用户点了"久坐提醒…"
    void sitReminderRequested();
    // 用户点了"定时计划…"：增加/编辑"几点做什么动作"
    void planRequested();
    // 用户点了"我的坚持…"：看今天做了几次、连续几天
    void recordsRequested();
    // 用户点了"情绪陪伴…"：选一种感受，读一段话
    void moodRequested();
    // 用户点了"氛围放空…"：看图放空 / 冥想放空
    void ambientRequested();
    // 用户从"身体放松"子菜单里选了某个动作（代号见 core/StretchLib.cpp）
    void stretchChosen(const QString &code);
    // 用户右键点了一下球：想知道下一次久坐提醒是什么时候
    void sitInfoRequested();
    // 用户点了「关于 → 使用导引」
    void onboardingRequested();
    // 用户点了「关于 → 关于桌面放松助手…」
    void aboutRequested();
    // 用户点了"退出"
    void quitRequested();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void showEvent(QShowEvent *event) override;

private slots:
    void onIdleFrame();
    void showMenu();

private:
    void snapToNearestEdge();

    BreathingBall *m_ball = nullptr;
    QTimer *m_idleTimer = nullptr;
    QElapsedTimer *m_idleClock = nullptr;

    BreathType m_deskBreath;         // 桌面球当前跟着动的呼吸
    bool m_hasDeskRhythm = false;    // false = 走原来的轻微起伏

    bool m_pressed = false;
    bool m_dragging = false;
    QPoint m_pressGlobal;
    QPoint m_pressOffset;

    bool m_suppressed = false;
    bool m_userHidden = false;   // 用户在托盘里点了"隐藏小球"
};

#endif // DESKTOPBALLWINDOW_H
