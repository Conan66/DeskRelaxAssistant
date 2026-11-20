#ifndef BREATHINGENGINE_H
#define BREATHINGENGINE_H

#include <QColor>
#include <QElapsedTimer>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QVector>

#include "BreathTypes.h"

// ============================================================
// 呼吸引擎：整个应用唯一的时间源
// ------------------------------------------------------------
// 设计要点（PRD 反复强调的「唯一计时源」）：
//   球的大小、颜色、光晕、倒计时文字、节拍音、语音切换
//   全部从这里读取，不允许各自跑独立动画曲线，
//   否则长时间引导一定会越走越偏。
//
// 用法：
//   engine.setBreath(腹式呼吸, 15);
//   engine.start();
//   然后连 frameChanged 信号去刷新 UI
// ============================================================
class BreathingEngine : public QObject
{
    Q_OBJECT
public:
    explicit BreathingEngine(QObject *parent = nullptr);

    // 设置要做哪种呼吸、做几轮
    void setBreath(const BreathType &type, int rounds);

    void start();     // 开始（会重置计时）
    void pause();     // 暂停
    void resume();    // 继续
    void stop();      // 停止并回到待机

    bool isRunning() const { return m_running; }
    bool isPaused() const { return m_paused; }

    // ---- 供 UI 每帧读取的视觉参数 ----
    double ballScale() const;        // 球直径倍率：1.00 ~ 1.18
    double haloScale() const;        // 光晕倍率：1.0 ~ 1.6
    QColor ballColor() const;        // 当前球体颜色

    // 当前这个"屏息"是不是接在呼气后面。
    // 箱式呼吸是 吸-屏-呼-屏，第二个屏息前面是呼气，
    // 这时球应该停在"小"的状态，而不是又鼓大。
    bool holdAfterExhale() const;

    // ---- 当前阶段信息 ----
    PhaseType phaseType() const;
    QString phaseText() const;       // "吸气" / "屏息" / "呼气"
    int secondsLeft() const;         // 当前阶段剩余秒（整数）
    QString countdownText() const;   // "吸气 3" 这种完整文案
    QString currentAudioKey() const; // 当前阶段该播哪句语音
    QString currentCue() const;      // 当前阶段的引导语（给界面显示）
    int phaseIndex() const;          // 当前阶段在一轮里的序号（0 开始）
    double phaseProgressValue() const; // 当前阶段走到哪了：0.0 ~ 1.0

    // ---- 进度信息 ----
    int currentRound() const;        // 第几轮（从 1 开始）
    int totalRounds() const { return m_rounds; }
    double totalProgress() const;    // 0.0 ~ 1.0

signals:
    void frameChanged();     // 每帧（约 60fps），UI 刷新
    void phaseChanged();     // 阶段切换（吸气→屏息等）
    void tick();             // 整秒（节拍音触发点）
    void finished();         // 全部轮次做完

private slots:
    void onFrame();

private:
    void rebuildTimeline();      // 把 phases 展开成时间轴
    qint64 effectiveMs() const;  // 扣掉暂停时间后的真实运行毫秒

    QTimer m_frameTimer;
    QElapsedTimer m_clock;

    QVector<BreathPhase> m_phases;   // 过滤掉 0 秒阶段后的阶段序列
    QVector<qint64> m_phaseStartMs;  // 每个阶段在一轮中的起始毫秒
    qint64 m_cycleMs = 0;            // 一轮总毫秒

    int m_rounds = 12;
    bool m_running = false;
    bool m_paused = false;

    qint64 m_pausedTotal = 0;    // 累计暂停时长
    qint64 m_pauseStartedAt = 0; // 本次暂停开始时刻

    // 当前帧计算结果（每帧算一次，UI 直接读）
    int m_curPhaseIndex = 0;
    PhaseType m_curPhaseType = PhaseType::Inhale;
    double m_playheadMs = 0;     // 当前在一轮内的位置
    int m_curRound = 1;
    int m_lastTickSecond = -1;   // 上一次触发节拍音的剩余秒数
};

#endif // BREATHINGENGINE_H
