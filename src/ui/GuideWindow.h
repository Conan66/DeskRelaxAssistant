#ifndef GUIDEWINDOW_H
#define GUIDEWINDOW_H

#include <QWidget>

#include "../core/BreathTypes.h"

class BreathingBall;
class BreathingEngine;
class AudioManager;
class DiagramView;
class QLabel;
class QToolButton;
class QPushButton;
class QComboBox;

// ============================================================
// 引导窗：用户真正"跟着做"的界面
// ------------------------------------------------------------
// 布局：[呼吸球]  |  [示意图]
//       底部：轮次进度 / 倒计时 / 暂停 / 结束
//       顶部：语音 · 示意图 · 动画 · 节拍 四个独立开关
//
// 四个开关互不干扰（PRD 4.0.6）：
//   关语音 → 只看球和文字
//   关示意图 → 界面只剩球，更清爽
//   关动画 → 球只在阶段切换瞬间跳变（适合易分心的人）
//   关节拍 → 不再每秒"滴"
// ============================================================
class GuideWindow : public QWidget
{
    Q_OBJECT
public:
    explicit GuideWindow(QWidget *parent = nullptr);

    // 开始一种呼吸（code 见 BreathTypes：fushi / 478 / box ...）
    void startBreath(const QString &code);

signals:
    // 窗口关闭后通知桌面球恢复显示
    void closed();

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onFrame();
    void onPhaseChanged();
    void onTick();
    void onFinished();
    void onPauseClicked();
    void onDiagramToggle(bool on);
    void onVoiceToggle(bool on);
    void onAnimationToggle(bool on);
    void onTickToggle(bool on);
    void onMusicToggle(bool on);
    void onMusicTrackChanged(int index);

private:
    void buildUi();
    void applyVisuals(bool snap);
    void rebuildMusicBox(const QString &current);

    BreathingEngine *m_engine = nullptr;
    AudioManager *m_audio = nullptr;
    BreathType m_current;      // 当前正在做的呼吸（含用户自定义节奏）
    bool m_finished = false;   // 一轮引导是否已经走完（走完后按钮变成"再来一次"）

    BreathingBall *m_ball = nullptr;
    DiagramView *m_diagram = nullptr;
    QLabel *m_titleLabel = nullptr;
    QLabel *m_introLabel = nullptr;   // 顶部的详细介绍（这个呼吸是干什么的、怎么做）
    QLabel *m_postureLabel = nullptr; // 坐姿提示（靠不靠椅背、手放哪）
    QLabel *m_cueLabel = nullptr;     // 球下方的一句话引导（"想象气息从脚底升上来"）
    QLabel *m_roundLabel = nullptr;
    QLabel *m_phaseLabel = nullptr;
    QLabel *m_tipLabel = nullptr;
    QToolButton *m_voiceBtn = nullptr;
    QToolButton *m_diagramBtn = nullptr;
    QToolButton *m_animBtn = nullptr;
    QToolButton *m_tickBtn = nullptr;
    QToolButton *m_musicBtn = nullptr;
    QComboBox *m_musicBox = nullptr;
    QPushButton *m_pauseBtn = nullptr;
    QPushButton *m_stopBtn = nullptr;
};

#endif // GUIDEWINDOW_H
