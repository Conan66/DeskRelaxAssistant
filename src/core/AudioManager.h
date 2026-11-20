#ifndef AUDIOMANAGER_H
#define AUDIOMANAGER_H

#include <QObject>
#include <QString>
#include <QStringList>

class QSoundEffect;
class QMediaPlayer;

// ============================================================
// 音频管理：节拍音 + 语音旁白
// ------------------------------------------------------------
//   节拍音用 QSoundEffect：延迟低，适合每秒一次的"滴"。
//   语音用 QMediaPlayer：能播 mp3，且不阻塞界面。
//
// 语音文件按 PRD A.2.2 的规则存放：
//   :/assets/voice/{音色}/{呼吸代码}_{阶段}.mp3
// 例如 :/assets/voice/yunxi/fushi_inhale.mp3
// ============================================================
class AudioManager : public QObject
{
    Q_OBJECT
public:
    explicit AudioManager(QObject *parent = nullptr);

    void setVoiceEnabled(bool on);
    void setTickEnabled(bool on);
    void setVoiceId(const QString &id);   // "yunxi" / "xiaoxiao"
    QString voiceId() const { return m_voiceId; }

    // 播一声节拍（每秒调用一次）
    void playTick();

    // 播当前阶段的语音；audioKey 为空则静默跳过
    void playPhase(const QString &breathCode, const QString &audioKey);

    void stopVoice();

    // ---- 背景音乐（和语音独立，互不影响开关） ----
    void setMusicEnabled(bool on);
    // 曲目 id：内置五音用 gong/shang/jue/zhi/yu；
    // 用户自己的音乐用 "file:绝对路径"，程序会自动识别
    void setMusicTrack(const QString &id);
    void stopMusic();                       // 关窗时停止，但不改用户的开关设置

    // 扫描 exe 同目录下的 music 文件夹，返回可用的音乐文件绝对路径。
    // 这样用户想换成自己下载的音乐，只要把文件丢进去就行，不用改程序。
    static QStringList localMusicFiles();

private:
    // 把一首曲目真正交给播放器（内置资源或外部文件）
    void loadMusic(const QString &id);

private:
    QSoundEffect *m_tick = nullptr;
    QMediaPlayer *m_voice = nullptr;
    QMediaPlayer *m_music = nullptr;

    bool m_voiceEnabled = true;
    bool m_tickEnabled = true;
    bool m_musicEnabled = false;
    QString m_voiceId = QStringLiteral("yunxi");
    QString m_musicTrack = QStringLiteral("yu");
};

#endif // AUDIOMANAGER_H
