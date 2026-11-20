#include "GuideWindow.h"

#include <QCloseEvent>
#include <QComboBox>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScreen>
#include <QToolButton>
#include <QVBoxLayout>

#include "../core/AppSettings.h"
#include "../core/AudioManager.h"
#include "../core/BreathingEngine.h"
#include "BreathingBall.h"
#include "DiagramView.h"
#include "ScreenDpiGuard.h"   // 跨屏保护：防止窗口换屏后被系统放大

namespace {
// 统一配色，和 PRD 的治愈绿 / 米白一致
const char *kStyleSheet = R"(
QWidget#guideRoot { background: #FBF9F4; }
QLabel#title { font-size: 19px; font-weight: 600; color: #2E5E4B; }
QLabel#round { font-size: 12px; color: #8A8578; }
QLabel#phase { font-size: 15px; color: #3A5647; }
QLabel#intro { font-size: 12px; color: #7D8A81; }
QLabel#posture {
    font-size: 12px; color: #3F6B57;
    background: #EFF5F0; border-radius: 7px; padding: 5px 9px;
}
QLabel#cue   { font-size: 14px; color: #2E5E4B; }
QLabel#tip   { font-size: 11px; color: #9C9689; }
QToolButton {
    border: 1px solid #E3DED2; border-radius: 12px;
    padding: 4px 12px; background: #FFFFFF; color: #8A8578; font-size: 12px;
}
QToolButton:checked { background: #E6F0EA; color: #2E5E4B; border-color: #B9D3C5; }
QPushButton {
    border: none; border-radius: 14px; padding: 7px 20px;
    background: #5F9E83; color: #FFFFFF; font-size: 13px;
}
QPushButton:hover { background: #529074; }
QPushButton#stop { background: #EFEAE0; color: #6E6A5E; }
QPushButton#stop:hover { background: #E6E0D3; }
QComboBox {
    border: 1px solid #E3DED2; border-radius: 8px;
    padding: 3px 7px; background: #FFFFFF; color: #6E7D74; font-size: 11px;
}
QComboBox QAbstractItemView { background: #FFFFFF; color: #3A5647; selection-background-color: #E6F0EA; }
)";
}

GuideWindow::GuideWindow(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("guideRoot"));
    setStyleSheet(QString::fromUtf8(kStyleSheet));
    setWindowTitle(QStringLiteral("呼吸引导"));
    setMinimumSize(660, 470);

    // 给一个舒服的初始尺寸，并摆到屏幕正中间（避开任务栏）
    // 说明：不指定位置时系统会把窗口放到奇怪的地方，甚至被屏幕边缘切掉
    resize(820, 590);
    const QRect avail = QGuiApplication::primaryScreen()->availableGeometry();
    move(avail.center() - rect().center());

    // 两块显示器缩放比例不同时，窗口拖到另一块屏会被系统放大/缩小。
    // 装上这个保护后，换屏会自动恢复成现在这个大小。
    new ScreenDpiGuard(this);

    // ---------- 引擎与音频 ----------
    m_engine = new BreathingEngine(this);
    m_audio = new AudioManager(this);

    connect(m_engine, &BreathingEngine::frameChanged, this, &GuideWindow::onFrame);
    connect(m_engine, &BreathingEngine::phaseChanged, this, &GuideWindow::onPhaseChanged);
    connect(m_engine, &BreathingEngine::tick, this, &GuideWindow::onTick);
    connect(m_engine, &BreathingEngine::finished, this, &GuideWindow::onFinished);

    buildUi();
}

// ============================================================
// 搭界面
// ============================================================
void GuideWindow::buildUi()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(20, 16, 20, 16);
    root->setSpacing(10);

    // ---------- 顶栏：标题 + 四个开关 ----------
    auto *topRow = new QHBoxLayout();
    m_titleLabel = new QLabel(QStringLiteral("呼吸引导"), this);
    m_titleLabel->setObjectName(QStringLiteral("title"));
    topRow->addWidget(m_titleLabel);
    topRow->addStretch();

    m_voiceBtn = new QToolButton(this);
    m_voiceBtn->setText(QStringLiteral("语音"));
    m_voiceBtn->setCheckable(true);

    m_diagramBtn = new QToolButton(this);
    m_diagramBtn->setText(QStringLiteral("示意图"));
    m_diagramBtn->setCheckable(true);

    m_animBtn = new QToolButton(this);
    m_animBtn->setText(QStringLiteral("动画"));
    m_animBtn->setCheckable(true);

    m_tickBtn = new QToolButton(this);
    m_tickBtn->setText(QStringLiteral("节拍"));
    m_tickBtn->setCheckable(true);

    topRow->addWidget(m_voiceBtn);
    topRow->addSpacing(6);
    topRow->addWidget(m_diagramBtn);
    topRow->addSpacing(6);
    topRow->addWidget(m_animBtn);
    topRow->addSpacing(6);
    topRow->addWidget(m_tickBtn);
    topRow->addSpacing(6);

    // 背景音乐：开关 + 五音选曲。音量固定低档，不盖语音。
    m_musicBtn = new QToolButton(this);
    m_musicBtn->setText(QStringLiteral("音乐"));
    m_musicBtn->setCheckable(true);
    topRow->addWidget(m_musicBtn);
    topRow->addSpacing(4);

    m_musicBox = new QComboBox(this);
    m_musicBox->setToolTip(QStringLiteral("内置五音 + 你自己放进 music 文件夹的音乐"));
    topRow->addWidget(m_musicBox);
    root->addLayout(topRow);

    // ---------- 详细介绍：这个呼吸是干什么的、怎么做 ----------
    m_introLabel = new QLabel(this);
    m_introLabel->setObjectName(QStringLiteral("intro"));
    m_introLabel->setWordWrap(true);      // 长句自动折行
    m_introLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    root->addWidget(m_introLabel);

    // ---------- 坐姿提示：单独的浅色小条，比正文更显眼 ----------
    // 每种呼吸坐姿要求不同（比如肩颈、侧肋必须坐直不靠椅背），所以按呼吸显示
    m_postureLabel = new QLabel(this);
    m_postureLabel->setObjectName(QStringLiteral("posture"));
    m_postureLabel->setWordWrap(true);
    root->addWidget(m_postureLabel);

    // ---------- 中部：左边球，右边示意图 ----------
    auto *body = new QHBoxLayout();
    body->setSpacing(16);

    m_ball = new BreathingBall(this);
    m_ball->setMinimumSize(300, 360);
    m_ball->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    // 球 + 球下方的一句引导语（竖着排，引导语居中对齐球）
    auto *ballCol = new QVBoxLayout();
    ballCol->setSpacing(4);

    m_cueLabel = new QLabel(this);
    m_cueLabel->setObjectName(QStringLiteral("cue"));
    m_cueLabel->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
    m_cueLabel->setWordWrap(true);
    m_cueLabel->setMinimumHeight(40);     // 预留两行高度，避免文字变化时界面跳动

    ballCol->addWidget(m_ball, 1);
    ballCol->addWidget(m_cueLabel, 0);

    m_diagram = new DiagramView(this);
    m_diagram->setMinimumWidth(240);
    m_diagram->setMaximumWidth(320);

    body->addLayout(ballCol, 1);
    body->addWidget(m_diagram, 0);
    root->addLayout(body, 1);

    // ---------- 底部：轮次 / 阶段 / 按钮 ----------
    auto *bottom = new QHBoxLayout();

    auto *leftInfo = new QVBoxLayout();
    leftInfo->setSpacing(2);
    m_roundLabel = new QLabel(QStringLiteral("—"), this);
    m_roundLabel->setObjectName(QStringLiteral("round"));
    m_phaseLabel = new QLabel(QStringLiteral("准备开始"), this);
    m_phaseLabel->setObjectName(QStringLiteral("phase"));
    leftInfo->addWidget(m_roundLabel);
    leftInfo->addWidget(m_phaseLabel);
    bottom->addLayout(leftInfo);

    bottom->addStretch();

    m_tipLabel = new QLabel(QStringLiteral("示意图可滚轮缩放、按住拖动"), this);
    m_tipLabel->setObjectName(QStringLiteral("tip"));
    bottom->addWidget(m_tipLabel);
    bottom->addSpacing(14);

    m_pauseBtn = new QPushButton(QStringLiteral("暂停"), this);
    m_stopBtn = new QPushButton(QStringLiteral("结束"), this);
    m_stopBtn->setObjectName(QStringLiteral("stop"));
    bottom->addWidget(m_pauseBtn);
    bottom->addSpacing(8);
    bottom->addWidget(m_stopBtn);

    root->addLayout(bottom);

    // ---------- 信号连接 ----------
    connect(m_pauseBtn, &QPushButton::clicked, this, &GuideWindow::onPauseClicked);
    connect(m_stopBtn, &QPushButton::clicked, this, [this]() { close(); });

    connect(m_voiceBtn, &QToolButton::toggled, this, &GuideWindow::onVoiceToggle);
    connect(m_diagramBtn, &QToolButton::toggled, this, &GuideWindow::onDiagramToggle);
    connect(m_animBtn, &QToolButton::toggled, this, &GuideWindow::onAnimationToggle);
    connect(m_tickBtn, &QToolButton::toggled, this, &GuideWindow::onTickToggle);
    connect(m_musicBtn, &QToolButton::toggled, this, &GuideWindow::onMusicToggle);
    connect(m_musicBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &GuideWindow::onMusicTrackChanged);

    // 曲目列表：内置五音 + 用户自己放进 music 文件夹的音乐
    rebuildMusicBox(AppSettings::musicTrack());
}

// ============================================================
// 重建背景音乐下拉框
// ------------------------------------------------------------
// 内置五音写死在程序里；外部音乐来自 exe 同目录的 music 文件夹，
// 用户随时可以往里丢新歌，下次打开引导窗就会出现在列表里。
// ============================================================
void GuideWindow::rebuildMusicBox(const QString &current)
{
    m_musicBox->blockSignals(true);   // 重建期间不要触发切歌
    m_musicBox->clear();

    m_musicBox->addItem(QStringLiteral("宫音·安稳"), QStringLiteral("gong"));
    m_musicBox->addItem(QStringLiteral("商音·清肃"), QStringLiteral("shang"));
    m_musicBox->addItem(QStringLiteral("角音·舒展"), QStringLiteral("jue"));
    m_musicBox->addItem(QStringLiteral("徵音·温暖"), QStringLiteral("zhi"));
    m_musicBox->addItem(QStringLiteral("羽音·沉静"), QStringLiteral("yu"));

    const QStringList locals = AudioManager::localMusicFiles();
    for (const QString &path : locals) {
        const QString name = QFileInfo(path).completeBaseName();
        m_musicBox->addItem(QStringLiteral("本地·%1").arg(name),
                            QStringLiteral("file:") + path);
    }

    // 尽量停在用户上次选的那首；找不到就回到第一首。
    // 外部音乐的路径可能因为斜杠方向或盘符大小写不同而对不上（比如 F: 和 f:），
    // 所以这里再做一次宽松比较，避免"明明选了本地歌，却跳回第一首"。
    int idx = m_musicBox->findData(current);
    if (idx < 0 && current.startsWith(QStringLiteral("file:"))) {
        const QString want = QDir::fromNativeSeparators(current.mid(5)).toLower();
        for (int i = 0; i < m_musicBox->count(); ++i) {
            const QString data = m_musicBox->itemData(i).toString();
            if (data.startsWith(QStringLiteral("file:"))
                && QDir::fromNativeSeparators(data.mid(5)).toLower() == want) {
                idx = i;
                break;
            }
        }
    }
    m_musicBox->setCurrentIndex(idx >= 0 ? idx : 0);
    m_musicBox->blockSignals(false);
}

// ============================================================
// 开始引导
// ============================================================
void GuideWindow::startBreath(const QString &code)
{
    // 拿到这种呼吸（已套用用户的自定义节奏）
    m_current = AppSettings::tunedBreath(BreathLib::find(code));
    if (m_current.code.isEmpty()) return;

    m_titleLabel->setText(m_current.name);
    setWindowTitle(QStringLiteral("呼吸引导 · %1").arg(m_current.name));

    // 顶部详细介绍（做法 + 想象画面 + 适用场景）
    m_introLabel->setText(m_current.intro);
    // 坐姿提示（坐着要不要靠椅背、手放哪）
    m_postureLabel->setText(QStringLiteral("坐姿：%1").arg(m_current.posture));

    // ---------- 把开关恢复成用户上次的选择 ----------
    // 用 blockSignals 防止这里触发 toggled 导致连锁反应
    const bool v = AppSettings::voiceEnabled();
    const bool d = AppSettings::diagramEnabled();
    const bool a = AppSettings::animationEnabled();
    const bool t = AppSettings::tickEnabled();

    m_voiceBtn->blockSignals(true);  m_voiceBtn->setChecked(v);  m_voiceBtn->blockSignals(false);
    m_diagramBtn->blockSignals(true); m_diagramBtn->setChecked(d); m_diagramBtn->blockSignals(false);
    m_animBtn->blockSignals(true);   m_animBtn->setChecked(a);   m_animBtn->blockSignals(false);
    m_tickBtn->blockSignals(true);   m_tickBtn->setChecked(t);   m_tickBtn->blockSignals(false);

    m_audio->setVoiceEnabled(v);
    m_audio->setTickEnabled(t);
    m_audio->setVoiceId(AppSettings::voiceId());

    // ---------- 背景音乐：恢复上次的开关与曲目 ----------
    const bool music = AppSettings::musicEnabled();
    const QString track = AppSettings::musicTrack();
    m_musicBtn->blockSignals(true);
    m_musicBtn->setChecked(music);
    m_musicBtn->blockSignals(false);

    // 顺便把用户后来放进 music 文件夹的新歌补进列表
    rebuildMusicBox(track);

    m_audio->setMusicTrack(track);
    m_audio->setMusicEnabled(music);

    // ---------- 示意图：按呼吸类型选侧面 / 正面 ----------
    m_diagram->setVisible(d);
    if (d) {
        m_diagram->setImage(m_current.useFrontDiagram
                                ? QStringLiteral(":/assets/diagram/front.png")
                                : QStringLiteral(":/assets/diagram/side.png"));
    }

    // ---------- 启动引擎 ----------
    m_engine->setBreath(m_current, m_current.defaultRounds);
    m_engine->start();

    m_finished = false;
    m_pauseBtn->setText(QStringLiteral("暂停"));
    m_stopBtn->setText(QStringLiteral("结束"));
    applyVisuals(false);
}

// ============================================================
// 每帧刷新
// ============================================================
void GuideWindow::onFrame()
{
    if (!m_engine->isRunning()) return;

    // 动画开关关闭时，只在阶段切换才重绘（见 onPhaseChanged）
    if (m_animBtn->isChecked()) applyVisuals(false);

    // 示意图上的气流箭头：跟同一个引擎的进度走，不会和球不同步
    if (m_diagram->isVisible()) {
        m_diagram->setFlow(m_engine->phaseType(), m_engine->phaseProgressValue(), m_current.route);
    }

    m_roundLabel->setText(QStringLiteral("第 %1 / %2 轮")
                              .arg(m_engine->currentRound())
                              .arg(m_engine->totalRounds()));
    m_phaseLabel->setText(m_engine->countdownText());
}

// ============================================================
// 阶段切换：换语音 + 立刻跳一次视觉（兜底，防止动画关时不动）
// ============================================================
void GuideWindow::onPhaseChanged()
{
    if (!m_engine->isRunning()) return;

    m_audio->playPhase(m_current.code, m_engine->currentAudioKey());
    applyVisuals(!m_animBtn->isChecked());

    // 换阶段就换引导语（"想象气息从脚底升上来"这种）
    m_cueLabel->setText(m_engine->currentCue());

    // 气流箭头立刻对齐新阶段，避免动画关掉时箭头不动
    if (m_diagram->isVisible()) {
        m_diagram->setFlow(m_engine->phaseType(), m_engine->phaseProgressValue(), m_current.route);
    }
}

void GuideWindow::onTick()
{
    if (!m_engine->isRunning()) return;
    m_audio->playTick();
}

void GuideWindow::onFinished()
{
    m_phaseLabel->setText(QStringLiteral("完成了，辛苦啦"));
    m_roundLabel->setText(QStringLiteral("本次引导结束"));

    // 记一次完成记录（完整走完一轮引导才计一次，中途关掉不算）
    AppSettings::recordCompletion(QStringLiteral("breath"));
    const int todayN = AppSettings::todayTotalCount();
    const int streak = AppSettings::streakDays();
    QString cue = QStringLiteral("把这份松弛带回接下来的事情里");
    if (streak > 1)
        cue += QStringLiteral("（今天第 %1 次 · 连续 %2 天）").arg(todayN).arg(streak);
    else
        cue += QStringLiteral("（今天第 %1 次）").arg(todayN);
    m_cueLabel->setText(cue);

    m_audio->stopVoice();
    m_diagram->clearFlow();

    // 引导结束后，"暂停"已经没有意义了，改成"再来一次"和"关闭"
    m_finished = true;
    m_pauseBtn->setText(QStringLiteral("再来一次"));
    m_stopBtn->setText(QStringLiteral("关闭"));

    applyVisuals(false);
}

// ============================================================
// 把引擎算出来的值推给球
// snap = true 表示"极简模式"：不做平滑，直接跳到阶段端点
// ============================================================
void GuideWindow::applyVisuals(bool snap)
{
    double scale = m_engine->ballScale();
    double halo = m_engine->haloScale();

    if (snap) {
        // 极简模式：直接跳到阶段端点
        switch (m_engine->phaseType()) {
        case PhaseType::Inhale:
            scale = 1.18;
            halo = 1.6;
            break;
        case PhaseType::Hold:
            // 吸完再停 → 保持大球；呼完再停 → 保持小球
            // （箱式呼吸的第二个屏息属于后者，不能又鼓起来）
            if (m_engine->holdAfterExhale()) {
                scale = 1.00;
                halo = 1.0;
            } else {
                scale = 1.18;
                halo = 1.6;
            }
            break;
        case PhaseType::Exhale:
            scale = 1.00;
            halo = 1.0;
            break;
        }
    }

    m_ball->setVisual(scale, halo, m_engine->ballColor());
    m_ball->setCountdownText(m_engine->countdownText());
}

// ============================================================
// 按钮 / 开关
// ============================================================
void GuideWindow::onPauseClicked()
{
    // 引导已经结束：这个按钮现在叫"再来一次"，重新从头开始
    if (m_finished) {
        startBreath(m_current.code);
        return;
    }

    if (!m_engine->isRunning()) return;

    if (m_engine->isPaused()) {
        m_engine->resume();
        m_pauseBtn->setText(QStringLiteral("暂停"));
        m_phaseLabel->setText(m_engine->countdownText());
    } else {
        m_engine->pause();
        m_pauseBtn->setText(QStringLiteral("继续"));
        m_phaseLabel->setText(QStringLiteral("已暂停"));
        m_audio->stopVoice();
    }
}

void GuideWindow::onVoiceToggle(bool on)
{
    AppSettings::setVoiceEnabled(on);
    m_audio->setVoiceEnabled(on);
}

void GuideWindow::onDiagramToggle(bool on)
{
    AppSettings::setDiagramEnabled(on);
    m_diagram->setVisible(on);
    if (on && !m_current.code.isEmpty()) {
        m_diagram->setImage(m_current.useFrontDiagram
                                ? QStringLiteral(":/assets/diagram/front.png")
                                : QStringLiteral(":/assets/diagram/side.png"));
        // 重新打开示意图时，箭头立刻接上当前阶段
        if (m_engine->isRunning()) {
            m_diagram->setFlow(m_engine->phaseType(), m_engine->phaseProgressValue(), m_current.route);
        }
    }
}

void GuideWindow::onAnimationToggle(bool on)
{
    AppSettings::setAnimationEnabled(on);
    applyVisuals(!on);
}

void GuideWindow::onTickToggle(bool on)
{
    AppSettings::setTickEnabled(on);
    m_audio->setTickEnabled(on);
}

void GuideWindow::onMusicToggle(bool on)
{
    AppSettings::setMusicEnabled(on);
    m_audio->setMusicEnabled(on);
}

void GuideWindow::onMusicTrackChanged(int index)
{
    const QString track = m_musicBox->itemData(index).toString();
    AppSettings::setMusicTrack(track);
    m_audio->setMusicTrack(track);
}

void GuideWindow::closeEvent(QCloseEvent *event)
{
    m_engine->stop();
    m_audio->stopVoice();
    m_audio->stopMusic();
    emit closed();
    QWidget::closeEvent(event);
}
