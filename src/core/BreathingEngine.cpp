#include "BreathingEngine.h"

#include <QtMath>

namespace {
// 颜色常量（PRD 4.0.1 三色精确值）
const QColor kColorBright("#6FAF93"); // 吸气最亮
const QColor kColorBase("#5F9E83");   // 基准色
const QColor kColorDark("#4F8E73");   // 呼气最暗
const QColor kColorIdle("#5F9E83");   // 待机用基准色

const double kScaleMin = 1.00;
const double kScaleMax = 1.18;

// 缓入缓出（smoothstep），让缩放不生硬
inline double easeInOut(double p)
{
    if (p < 0.0) p = 0.0;
    if (p > 1.0) p = 1.0;
    return p * p * (3.0 - 2.0 * p);
}

// 两色线性插值
inline QColor lerpColor(const QColor &a, const QColor &b, double t)
{
    return QColor::fromRgbF(
        a.redF()   + (b.redF()   - a.redF())   * t,
        a.greenF() + (b.greenF() - a.greenF()) * t,
        a.blueF()  + (b.blueF()  - a.blueF())  * t);
}
}

BreathingEngine::BreathingEngine(QObject *parent)
    : QObject(parent)
{
    // 约 60fps 刷新，肉眼看着是连续的
    m_frameTimer.setInterval(16);
    m_frameTimer.setTimerType(Qt::PreciseTimer);
    connect(&m_frameTimer, &QTimer::timeout, this, &BreathingEngine::onFrame);
}

// ============================================================
// 设置呼吸类型：把阶段序列展开成时间轴
// ============================================================
void BreathingEngine::setBreath(const BreathType &type, int rounds)
{
    stop();

    m_rounds = qMax(1, rounds);

    // 丢掉 0 秒的阶段（比如"屏息 0 秒"表示不屏息）
    m_phases.clear();
    for (const BreathPhase &p : type.phases) {
        if (p.seconds > 0) m_phases << p;
    }
    // 极端兜底：万一全被过滤掉，塞一个吸气阶段，避免除零
    if (m_phases.isEmpty()) {
        BreathPhase p;
        p.type = PhaseType::Inhale;
        p.seconds = 4;
        m_phases << p;
    }

    rebuildTimeline();
    m_curPhaseIndex = 0;
    m_curPhaseType = m_phases.first().type;
    m_playheadMs = 0;
    m_curRound = 1;
}

// 计算每个阶段的起始位置和一轮总长
void BreathingEngine::rebuildTimeline()
{
    m_phaseStartMs.clear();
    qint64 acc = 0;
    for (const BreathPhase &p : m_phases) {
        m_phaseStartMs << acc;
        acc += qint64(p.seconds) * 1000;
    }
    m_cycleMs = acc;
}

void BreathingEngine::start()
{
    if (m_cycleMs <= 0) return;

    m_running = true;
    m_paused = false;
    m_pausedTotal = 0;
    m_pauseStartedAt = 0;
    m_curPhaseIndex = 0;
    m_curPhaseType = m_phases.first().type;
    m_playheadMs = 0;
    m_curRound = 1;
    m_lastTickSecond = -1;

    m_clock.start();
    m_frameTimer.start();

    emit phaseChanged();
    emit frameChanged();
}

void BreathingEngine::pause()
{
    if (!m_running || m_paused) return;
    m_paused = true;
    m_pauseStartedAt = m_clock.elapsed();
}

void BreathingEngine::resume()
{
    if (!m_running || !m_paused) return;
    // 把这次暂停的时长累加进去，之后的时间都减掉它
    m_pausedTotal += m_clock.elapsed() - m_pauseStartedAt;
    m_paused = false;
}

void BreathingEngine::stop()
{
    m_frameTimer.stop();
    m_running = false;
    m_paused = false;
    m_pausedTotal = 0;
    m_pauseStartedAt = 0;
    m_playheadMs = 0;
    m_curPhaseIndex = 0;
    m_curRound = 1;
    m_lastTickSecond = -1;
}

// 扣掉暂停时间后的真实运行毫秒（唯一的"时间真值"）
qint64 BreathingEngine::effectiveMs() const
{
    if (m_paused) {
        return m_pauseStartedAt - m_pausedTotal;
    }
    return m_clock.elapsed() - m_pausedTotal;
}

// ============================================================
// 每帧计算：定位当前阶段、进度、剩余秒
// ============================================================
void BreathingEngine::onFrame()
{
    if (!m_running || m_paused) return;

    const qint64 total = effectiveMs();
    const int roundIndex = int(total / m_cycleMs);

    // 轮次做完了
    if (roundIndex >= m_rounds) {
        stop();
        emit finished();
        emit frameChanged();
        return;
    }

    m_curRound = roundIndex + 1;
    const qint64 within = total % m_cycleMs;
    m_playheadMs = double(within);

    // 找当前落在哪个阶段
    int idx = m_phases.size() - 1;
    for (int i = 0; i < m_phases.size(); ++i) {
        const qint64 start = m_phaseStartMs[i];
        const qint64 end = start + qint64(m_phases[i].seconds) * 1000;
        if (within < end) { idx = i; break; }
    }

    // 阶段变了 → 通知外面（换语音、换文字）
    if (idx != m_curPhaseIndex) {
        m_curPhaseIndex = idx;
        m_curPhaseType = m_phases[idx].type;
        m_lastTickSecond = -1;
        emit phaseChanged();
    }

    // 整秒触发节拍音
    const int left = secondsLeft();
    if (left != m_lastTickSecond) {
        m_lastTickSecond = left;
        emit tick();
    }

    emit frameChanged();
}

// ============================================================
// 当前阶段在一轮内的进度 0.0 ~ 1.0
// ============================================================
static double phaseProgress(const QVector<qint64> &starts,
                            const QVector<BreathPhase> &phases,
                            int idx, double playheadMs)
{
    if (idx < 0 || idx >= phases.size()) return 0.0;
    const double start = double(starts[idx]);
    const double dur = double(phases[idx].seconds) * 1000.0;
    if (dur <= 0.0) return 1.0;
    double p = (playheadMs - start) / dur;
    if (p < 0.0) p = 0.0;
    if (p > 1.0) p = 1.0;
    return p;
}

// ============================================================
// 这个屏息前面是吸气还是呼气？
// ------------------------------------------------------------
// 大多数呼吸只有一次屏息（吸-屏-呼），前面当然是吸气。
// 但箱式呼吸是 吸-屏-呼-屏，第二个屏息前面是呼气：
// 这时球应该保持"小"的状态，否则会出现"呼完反而鼓大"的怪现象。
// ============================================================
bool BreathingEngine::holdAfterExhale() const
{
    // 从当前阶段往前找最近的一个非屏息阶段
    for (int i = m_curPhaseIndex - 1; i >= 0; --i) {
        if (m_phases.at(i).type != PhaseType::Hold) {
            return m_phases.at(i).type == PhaseType::Exhale;
        }
    }
    // 一轮的开头就是屏息（很少见）：保守地当作刚吸完
    return false;
}

// ============================================================
// 球体倍率
//   吸气：1.00 → 1.18
//   屏息：停住。吸完停 = 1.18；呼完停 = 1.00
//   呼气：1.18 → 1.00
// ============================================================
double BreathingEngine::ballScale() const
{
    if (!m_running) return 1.0;

    const double p = easeInOut(phaseProgress(m_phaseStartMs, m_phases,
                                             m_curPhaseIndex, m_playheadMs));
    switch (m_curPhaseType) {
    case PhaseType::Inhale:
        return kScaleMin + (kScaleMax - kScaleMin) * p;
    case PhaseType::Hold:
        // 纹丝不动，但"停在哪"要看前面是吸还是呼
        return holdAfterExhale() ? kScaleMin : kScaleMax;
    case PhaseType::Exhale:
        return kScaleMax - (kScaleMax - kScaleMin) * p;
    }
    return kScaleMin;
}

// ============================================================
// 光晕倍率：吸气外扩、屏息保持、呼气收回
// 屏息保持的"大小"同样取决于前面是吸气还是呼气
// ============================================================
double BreathingEngine::haloScale() const
{
    if (!m_running) return 1.0;

    const double p = easeInOut(phaseProgress(m_phaseStartMs, m_phases,
                                             m_curPhaseIndex, m_playheadMs));
    switch (m_curPhaseType) {
    case PhaseType::Inhale:
        return 1.0 + 0.6 * p;
    case PhaseType::Hold:
        return holdAfterExhale() ? 1.0 : 1.6;
    case PhaseType::Exhale:
        return 1.6 - 0.6 * p;
    }
    return 1.0;
}

// ============================================================
// 球体颜色：暗 → 亮（吸气）→ 保持（屏息）→ 暗（呼气）
// 这样首尾相接，不会在阶段切换时突然跳色
// ============================================================
QColor BreathingEngine::ballColor() const
{
    if (!m_running) return kColorIdle;

    const double p = easeInOut(phaseProgress(m_phaseStartMs, m_phases,
                                             m_curPhaseIndex, m_playheadMs));
    switch (m_curPhaseType) {
    case PhaseType::Inhale:
        return lerpColor(kColorDark, kColorBright, p);
    case PhaseType::Hold:
        // 呼完再屏息时，球是小的，颜色也该是暗的
        return holdAfterExhale() ? kColorDark : kColorBright;
    case PhaseType::Exhale:
        return lerpColor(kColorBright, kColorDark, p);
    }
    return kColorBase;
}

PhaseType BreathingEngine::phaseType() const
{
    return m_curPhaseType;
}

QString BreathingEngine::phaseText() const
{
    switch (m_curPhaseType) {
    case PhaseType::Inhale: return QStringLiteral("吸气");
    case PhaseType::Hold:   return QStringLiteral("屏息");
    case PhaseType::Exhale: return QStringLiteral("呼气");
    }
    return QString();
}

// 剩余秒数：向上取整。刚开始时 = 阶段总秒数，最后一秒内显示 1
int BreathingEngine::secondsLeft() const
{
    if (!m_running) return 0;
    if (m_curPhaseIndex < 0 || m_curPhaseIndex >= m_phases.size()) return 0;

    const double start = double(m_phaseStartMs[m_curPhaseIndex]);
    const double dur = double(m_phases[m_curPhaseIndex].seconds) * 1000.0;
    const double elapsed = m_playheadMs - start;
    const double remain = (dur - elapsed) / 1000.0;
    int s = int(qCeil(remain));
    return s < 0 ? 0 : s;
}

QString BreathingEngine::countdownText() const
{
    if (!m_running) return QString();
    return QStringLiteral("%1 %2").arg(phaseText()).arg(secondsLeft());
}

QString BreathingEngine::currentAudioKey() const
{
    if (m_curPhaseIndex < 0 || m_curPhaseIndex >= m_phases.size()) return QString();
    return m_phases[m_curPhaseIndex].audioKey;
}

// 当前阶段的引导语（就是 BreathPhase 里的 cue）
QString BreathingEngine::currentCue() const
{
    if (m_curPhaseIndex < 0 || m_curPhaseIndex >= m_phases.size()) return QString();
    return m_phases[m_curPhaseIndex].cue;
}

int BreathingEngine::phaseIndex() const
{
    return m_curPhaseIndex;
}

// 当前阶段走了多少：0.0 刚开始，1.0 走完（示意图的气流箭头用它推进）
double BreathingEngine::phaseProgressValue() const
{
    if (!m_running) return 0.0;
    return phaseProgress(m_phaseStartMs, m_phases, m_curPhaseIndex, m_playheadMs);
}

int BreathingEngine::currentRound() const
{
    return m_curRound;
}

double BreathingEngine::totalProgress() const
{
    if (!m_running || m_rounds <= 0 || m_cycleMs <= 0) return 0.0;
    const double total = double(effectiveMs());
    const double all = double(m_cycleMs) * double(m_rounds);
    if (all <= 0.0) return 0.0;
    double p = total / all;
    if (p < 0.0) p = 0.0;
    if (p > 1.0) p = 1.0;
    return p;
}
