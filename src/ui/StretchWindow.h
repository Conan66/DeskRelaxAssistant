#ifndef STRETCHWINDOW_H
#define STRETCHWINDOW_H

#include <QTimer>
#include <QWidget>

#include "../core/StretchLib.h"

class BodyPoseView;
class AudioManager;
class QComboBox;
class QLabel;
class QPushButton;
class QToolButton;

// ============================================================
// 身体放松引导窗
// ------------------------------------------------------------
// 左边（其实是上方）是简笔图示，下面是步骤说明 + 倒计时。
// 每一步到时间会自动进入下一步，全部做完可以"再来一次"。
// 和呼吸引导窗一样，打开时桌面球会藏起来，关掉再回来。
// ============================================================
class StretchWindow : public QWidget
{
    Q_OBJECT
public:
    explicit StretchWindow(QWidget *parent = nullptr);

    // 开始/重开某个动作（代号见 core/StretchLib.cpp）
    void startAction(const QString &code);

    // 连续做多个动作（定时计划里同一时间有多个计划时用）。
    // 一项做完会稍作停顿，自动开始下一项。
    void startQueue(const QStringList &codes);

signals:
    void closed();      // 窗口关掉时通知主程序，把桌面球放回来

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void beginAction(const QString &code);  // 真正加载动作（队列内部也用它）
    void loadStep(int index);   // 切到第 index 步
    // 重建背景音乐下拉框：内置五音 + music 文件夹里的本地音乐
    void rebuildMusicBox(const QString &current);
    void onTick();              // 每秒调一次
    void onNext();              // 进入下一步
    void finish();              // 全部做完

    StretchAction m_action;     // 当前动作
    QStringList m_queueAll;     // 队列模式下的全部动作（空 = 单动作）
    int m_queueIndex = 0;       // 队列里当前做到第几个
    int m_step = 0;             // 当前第几步（从 0 开始）
    int m_remain = 0;           // 这一步还剩几秒
    bool m_paused = false;      // 是否暂停
    bool m_done = false;        // 是否已全部完成

    QTimer m_timer;
    AudioManager *m_audio = nullptr;   // 跟练声音：每秒"滴" + 背景音乐
    BodyPoseView *m_view = nullptr;
    QLabel *m_actionLabel = nullptr;
    QLabel *m_stepLabel = nullptr;
    QLabel *m_textLabel = nullptr;
    QLabel *m_countLabel = nullptr;
    QLabel *m_noteLabel = nullptr;
    QToolButton *m_tickBtn = nullptr;
    QToolButton *m_musicBtn = nullptr;
    QComboBox *m_musicBox = nullptr;   // 背景音乐选曲（和呼吸引导窗同一套曲目）
    QPushButton *m_pauseBtn = nullptr;
    QPushButton *m_nextBtn = nullptr;
    QPushButton *m_closeBtn = nullptr;
};

#endif // STRETCHWINDOW_H
