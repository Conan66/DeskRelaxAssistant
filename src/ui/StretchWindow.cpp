#include "StretchWindow.h"

#include <QCloseEvent>
#include <QComboBox>
#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScreen>
#include <QToolButton>
#include <QVBoxLayout>

#include "BodyPoseView.h"
#include "ScreenDpiGuard.h"   // 跨屏保护：防止窗口换屏后被系统放大
#include "../core/AppSettings.h"
#include "../core/AudioManager.h"

StretchWindow::StretchWindow(QWidget *parent)
    : QWidget(parent)
{
    setWindowTitle(QStringLiteral("身体放松"));
    setMinimumSize(520, 720);
    resize(560, 800);
    setStyleSheet(QStringLiteral("background:#FFFFFF;"));

    // 和呼吸引导一样：换到另一块缩放比例不同的屏幕时，
    // 自动恢复成这里的窗口大小，避免被系统越放越大。
    new ScreenDpiGuard(this);

    // 跟练时的声音都在这里：每秒的"滴"声 + 背景音乐（不开 AI 语音）
    m_audio = new AudioManager(this);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(24, 20, 24, 20);
    root->setSpacing(10);

    // ① 动作名 + 声音开关（两个小 toggle，和呼吸引导窗风格一致）
    auto *titleRow = new QHBoxLayout;
    titleRow->setSpacing(8);
    m_actionLabel = new QLabel(this);
    m_actionLabel->setStyleSheet(QStringLiteral(
        "font-size:20px; font-weight:bold; color:#2E5E4B;"));
    titleRow->addWidget(m_actionLabel);
    titleRow->addStretch();

    m_tickBtn = new QToolButton(this);
    m_tickBtn->setText(QStringLiteral("滴声"));
    m_tickBtn->setCheckable(true);
    m_tickBtn->setCursor(Qt::PointingHandCursor);
    m_tickBtn->setToolTip(QStringLiteral("每秒一声「滴」：告诉你时间在走"));
    m_tickBtn->setChecked(AppSettings::tickEnabled());
    m_musicBtn = new QToolButton(this);
    m_musicBtn->setText(QStringLiteral("音乐"));
    m_musicBtn->setCheckable(true);
    m_musicBtn->setCursor(Qt::PointingHandCursor);
    m_musicBtn->setToolTip(QStringLiteral("低音量背景音乐"));
    m_musicBtn->setChecked(AppSettings::musicEnabled());
    // toggle 颜色区分开/关：绿色 = 开；灰 = 关
    for (auto *b : {m_tickBtn, m_musicBtn}) {
        b->setStyleSheet(QStringLiteral(
            "QToolButton{border:1px solid #DCE6DF; border-radius:4px;"
            " padding:3px 10px; background:#FBF9F4; color:#33302B; font-size:12px;}"
            "QToolButton:checked{background:#F3F6F4; color:#2E5E4B; border-color:#4F8E73;}"
            "QToolButton:hover{background:#F3F6F4;}"));
    }
    titleRow->addWidget(m_tickBtn);
    titleRow->addWidget(m_musicBtn);

    // 选曲下拉框：和呼吸引导窗用同一批曲目（内置五音 + music 文件夹里的本地音乐）。
    // 放在「音乐」开关右边，开关负责开/关，下拉框负责听哪首。
    m_musicBox = new QComboBox(this);
    m_musicBox->setToolTip(QStringLiteral("内置五音 + 你自己放进 music 文件夹的音乐"));
    m_musicBox->setMinimumWidth(104);
    m_musicBox->setStyleSheet(QStringLiteral(
        "QComboBox{border:1px solid #DCE6DF; border-radius:4px; padding:2px 6px;"
        " background:#FBF9F4; color:#33302B; font-size:12px;}"
        "QComboBox:hover{background:#F3F6F4;}"
        "QComboBox QAbstractItemView{background:#FFFFFF; border:1px solid #DCE6DF;"
        " selection-background-color:#E7EFEA; selection-color:#2E5E4B;}"));
    titleRow->addSpacing(4);
    titleRow->addWidget(m_musicBox);

    root->addLayout(titleRow);

    // 点一下声音开关：立刻改状态，同时写回 AppSettings（下次默认沿用）
    QObject::connect(m_tickBtn, &QToolButton::toggled, this,
                     [this](bool on) {
        m_audio->setTickEnabled(on);
        AppSettings::setTickEnabled(on);
    });
    QObject::connect(m_musicBtn, &QToolButton::toggled, this,
                     [this](bool on) {
        m_audio->setMusicEnabled(on);
        AppSettings::setMusicEnabled(on);
    });

    // 换曲目：立刻切歌，同时写回设置，下次打开沿用同一首
    QObject::connect(m_musicBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
                     this, [this](int index) {
        const QString track = m_musicBox->itemData(index).toString();
        if (track.isEmpty()) return;
        AppSettings::setMusicTrack(track);
        m_audio->setMusicTrack(track);
    });

    // 曲目列表：内置五音 + 用户自己放进 music 文件夹的音乐
    rebuildMusicBox(AppSettings::musicTrack());

    // ② 图示（占最大空间）
    m_view = new BodyPoseView(this);
    root->addWidget(m_view, 1);

    // ③ 步骤标题
    m_stepLabel = new QLabel(this);
    m_stepLabel->setStyleSheet(QStringLiteral(
        "font-size:15px; font-weight:bold; color:#4F8E73;"));
    root->addWidget(m_stepLabel);

    // ④ 步骤说明（自动换行）
    m_textLabel = new QLabel(this);
    m_textLabel->setWordWrap(true);
    m_textLabel->setStyleSheet(QStringLiteral(
        "font-size:14px; color:#33302B; line-height:150%;"));
    root->addWidget(m_textLabel);

    // ⑤ 倒计时
    m_countLabel = new QLabel(this);
    m_countLabel->setAlignment(Qt::AlignCenter);
    m_countLabel->setStyleSheet(QStringLiteral(
        "font-size:44px; font-weight:bold; color:#4F8E73;"));
    root->addWidget(m_countLabel);

    // ⑥ 注意事项（全程提示）
    m_noteLabel = new QLabel(this);
    m_noteLabel->setWordWrap(true);
    m_noteLabel->setStyleSheet(QStringLiteral(
        "font-size:12px; color:#8A8578;"));
    root->addWidget(m_noteLabel);

    // ⑦ 按钮行
    auto *btnRow = new QHBoxLayout;
    btnRow->setSpacing(10);

    m_pauseBtn = new QPushButton(QStringLiteral("暂停"), this);
    m_nextBtn = new QPushButton(QStringLiteral("下一步"), this);
    m_closeBtn = new QPushButton(QStringLiteral("结束"), this);
    for (QPushButton *b : {m_pauseBtn, m_nextBtn, m_closeBtn}) {
        b->setMinimumHeight(36);
        b->setCursor(Qt::PointingHandCursor);
        b->setStyleSheet(QStringLiteral(
            "QPushButton{background:#F3F6F4; border:1px solid #DCE6DF;"
            " border-radius:8px; color:#2E5E4B; font-size:13px; padding:0 18px;}"
            "QPushButton:hover{background:#E7EFEA;}"));
        btnRow->addWidget(b);
    }
    root->addLayout(btnRow);

    // 每秒一跳
    m_timer.setInterval(1000);
    connect(&m_timer, &QTimer::timeout, this, &StretchWindow::onTick);

    connect(m_pauseBtn, &QPushButton::clicked, this, [this]() {
        m_paused = !m_paused;
        m_pauseBtn->setText(m_paused ? QStringLiteral("继续")
                                     : QStringLiteral("暂停"));
    });

    connect(m_nextBtn, &QPushButton::clicked, this, [this]() {
        // 全部做完时，这个按钮变成"再来一次"
        if (m_done) {
            // 队列模式：整套重新来一遍；单动作：只重做这一个
            if (!m_queueAll.isEmpty())
                startQueue(m_queueAll);
            else
                startAction(m_action.code);
            return;
        }
        onNext();
    });

    connect(m_closeBtn, &QPushButton::clicked, this, &QWidget::close);
}

// 重建背景音乐下拉框。
// 内置五音写死在程序里；外部音乐来自 exe 同目录的 music 文件夹，
// 用户随时可以往里丢新歌，下次打开身体放松就会出现在列表里。
void StretchWindow::rebuildMusicBox(const QString &current)
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
    // 本地音乐路径可能因斜杠方向或盘符大小写不同而对不上，这里做一次宽松比较。
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

void StretchWindow::startAction(const QString &code)
{
    // 单动作模式：把队列清掉，避免上一个队列的残留影响
    m_queueAll.clear();
    m_queueIndex = 0;
    beginAction(code);
}

void StretchWindow::startQueue(const QStringList &codes)
{
    m_queueAll = codes;
    m_queueIndex = 0;
    if (!m_queueAll.isEmpty())
        beginAction(m_queueAll.first());
}

// 真正把某个动作加载进来（单动作和队列都走这里）
void StretchWindow::beginAction(const QString &code)
{
    m_action = StretchLib::find(code);
    if (m_action.code.isEmpty()) return;

    // 声音配置沿用设置页里呼吸练习的同一套开关：
    // 先选曲目再开开关，这样每次开始都从曲子开头播
    m_audio->setMusicTrack(AppSettings::musicTrack());
    m_audio->setMusicEnabled(AppSettings::musicEnabled());
    m_audio->setTickEnabled(AppSettings::tickEnabled());

    setWindowTitle(QStringLiteral("身体放松 · %1").arg(m_action.name));
    m_actionLabel->setText(m_action.name);
    m_noteLabel->setText(QStringLiteral("注意：%1").arg(m_action.note));
    m_view->setViewMode(m_action.view);

    // 重置按钮状态
    m_paused = false;
    m_done = false;
    m_pauseBtn->setVisible(true);
    m_pauseBtn->setText(QStringLiteral("暂停"));
    m_nextBtn->setVisible(true);   // 队列切换时上一项可能把它藏起来了
    m_nextBtn->setText(QStringLiteral("下一步"));
    m_closeBtn->setText(QStringLiteral("结束"));

    loadStep(0);

    // 放到屏幕中央再显示
    if (QScreen *scr = screen()) {
        const QRect avail = scr->availableGeometry();
        move(avail.center().x() - width() / 2,
             avail.center().y() - height() / 2);
    }
    show();
    raise();
    activateWindow();
    m_timer.start();
}

void StretchWindow::loadStep(int index)
{
    if (index < 0 || index >= m_action.steps.size()) return;

    m_step = index;
    const StretchStep &s = m_action.steps.at(index);

    m_stepLabel->setText(QStringLiteral("%1 · 第 %2/%3 步")
                             .arg(s.title)
                             .arg(index + 1)
                             .arg(m_action.steps.size()));
    m_textLabel->setText(s.text);
    m_view->setPose(s.pose);
    m_view->setArrow(s.arrow);
    // 有图用图（image 为空时内部自动回退简笔人）
    m_view->setImage(s.image, s.mirror);
    // 第二张图（循环动作交替用）：带上切换周期和相位名（如"打开·吸气"）
    m_view->setImage2(s.image2, s.swapMs, s.phaseA, s.phaseB);

    m_remain = s.seconds;
    m_countLabel->setText(QString::number(m_remain));
}

void StretchWindow::onTick()
{
    if (m_paused || m_done) return;

    // 每秒一声"滴"：不看屏幕也知道时间在走（暂停时上面就返回了，不会响）
    m_audio->playTick();

    --m_remain;
    if (m_remain <= 0) {
        m_countLabel->setText(QStringLiteral("0"));
        onNext();
        return;
    }
    m_countLabel->setText(QString::number(m_remain));
}

void StretchWindow::onNext()
{
    if (m_step + 1 < m_action.steps.size()) {
        loadStep(m_step + 1);
    } else {
        finish();
    }
}

void StretchWindow::finish()
{
    m_done = true;
    m_timer.stop();

    // ---------- 队列模式：还有下一项 ----------
    // 先停 5 秒喘口气，再自动开始下一项。
    // 这里不显示"全部完成"，免得用户以为已经结束了
    if (m_queueIndex + 1 < m_queueAll.size()) {
        const QString nextCode = m_queueAll.at(m_queueIndex + 1);
        const StretchAction nextAct = StretchLib::find(nextCode);

        m_stepLabel->setText(QStringLiteral("这一项完成"));
        m_textLabel->setText(QStringLiteral("休息一下，5 秒后开始「%1」…")
                                 .arg(nextAct.name));
        m_countLabel->setText(QStringLiteral("5"));
        m_pauseBtn->hide();
        m_nextBtn->hide();       // 等待期间不给点，避免和自动切换打架
        m_closeBtn->setText(QStringLiteral("结束"));

        QTimer::singleShot(5000, this, [this]() {
            // 窗口已经关了就不继续
            if (!isVisible()) return;
            ++m_queueIndex;
            beginAction(m_queueAll.at(m_queueIndex));
            m_nextBtn->show();   // 恢复按钮
        });
        return;
    }

    // 全部做完时音乐继续放着，和呼吸引导的体验一致；
    // 真正停音乐只在关窗时（closeEvent），避免刚做完就突然安静。

    // 记一次完成记录（队列模式下，能走到这里说明整套都做完了）
    AppSettings::recordCompletion(QStringLiteral("stretch"));
    const int todayN = AppSettings::todayTotalCount();
    const int streak = AppSettings::streakDays();

    QString msg = QStringLiteral(
        "整套动作做完了。慢慢转一下脖子、抖抖肩，再回到座位上就好。");
    if (streak > 1) {
        msg += QStringLiteral("\n今天第 %1 次，已连续坚持 %2 天。")
                   .arg(todayN).arg(streak);
    } else {
        msg += QStringLiteral("\n今天第 %1 次。").arg(todayN);
    }

    m_stepLabel->setText(QStringLiteral("全部完成"));
    m_textLabel->setText(msg);
    m_countLabel->setText(QStringLiteral("✓"));
    m_pauseBtn->hide();
    m_nextBtn->setText(QStringLiteral("再来一次"));
    m_nextBtn->show();
    m_closeBtn->setText(QStringLiteral("关闭"));
}

void StretchWindow::closeEvent(QCloseEvent *event)
{
    m_timer.stop();
    m_audio->stopMusic();   // 关窗音乐跟着停（用户设置里的开关不受影响）
    emit closed();
    QWidget::closeEvent(event);
}
