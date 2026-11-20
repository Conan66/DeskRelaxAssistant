#include "AudioManager.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QMediaPlayer>
#include <QSoundEffect>
#include <QStringList>
#include <QUrl>

AudioManager::AudioManager(QObject *parent)
    : QObject(parent)
{
    // ---------- 节拍音 ----------
    m_tick = new QSoundEffect(this);
    m_tick->setSource(QUrl(QStringLiteral("qrc:/assets/sfx/tick.wav")));
    // wav 本身振幅约 0.28，这里再压到 0.35，保证不盖住语音
    m_tick->setVolume(0.35);

    // ---------- 语音 ----------
    m_voice = new QMediaPlayer(this);
    m_voice->setVolume(85);

    // ---------- 背景音乐 ----------
    // 单独一个播放器，这样开关音乐不会打断正在播的语音。
    m_music = new QMediaPlayer(this);
    m_music->setVolume(18);   // 只做背景，必须明显低于语音的 85

    // 一首播完就回到开头再播，形成无缝循环。
    // 只在 EndOfMedia 时循环；用户手动停止不会触发它。
    connect(m_music, &QMediaPlayer::mediaStatusChanged, this,
            [this](QMediaPlayer::MediaStatus status) {
                if (status == QMediaPlayer::EndOfMedia && m_musicEnabled) {
                    m_music->setPosition(0);
                    m_music->play();
                }
            });
}

void AudioManager::setVoiceEnabled(bool on)
{
    m_voiceEnabled = on;
    if (!on) stopVoice();
}

void AudioManager::setTickEnabled(bool on)
{
    m_tickEnabled = on;
}

void AudioManager::setVoiceId(const QString &id)
{
    // 只接受两个已生成音频的音色
    m_voiceId = (id == QLatin1String("xiaoxiao")) ? id : QStringLiteral("yunxi");
}

void AudioManager::playTick()
{
    if (!m_tickEnabled) return;
    // 正在播就从头再来一次，避免连点时漏掉节拍
    if (m_tick->status() == QSoundEffect::Error) return;
    m_tick->play();
}

void AudioManager::playPhase(const QString &breathCode, const QString &audioKey)
{
    if (!m_voiceEnabled) return;
    if (audioKey.isEmpty()) return;    // 该阶段设计上就没有语音

    const QString path = QStringLiteral("qrc:/assets/voice/%1/%2_%3.mp3")
                             .arg(m_voiceId, breathCode, audioKey);

    // 切歌前先停，避免上一句还没念完就叠在一起
    m_voice->stop();
    m_voice->setMedia(QUrl(path));
    m_voice->play();
}

void AudioManager::stopVoice()
{
    if (m_voice) m_voice->stop();
}

void AudioManager::setMusicEnabled(bool on)
{
    m_musicEnabled = on;
    if (on) {
        // 如果还没选过曲目，先载入当前曲目
        if (m_music->media().isNull()) setMusicTrack(m_musicTrack);
        m_music->play();
    } else {
        m_music->stop();
    }
}

void AudioManager::setMusicTrack(const QString &id)
{
    // 以 file: 开头的是用户自己放进 music 文件夹的音乐，直接放行
    QString key = id;
    if (!id.startsWith(QStringLiteral("file:"))) {
        static const QStringList valid = {QStringLiteral("gong"), QStringLiteral("shang"),
                                          QStringLiteral("jue"), QStringLiteral("zhi"),
                                          QStringLiteral("yu")};
        if (!valid.contains(id)) key = QStringLiteral("yu");
    }

    m_musicTrack = key;
    loadMusic(key);
}

// 真正把某首曲子交给播放器：内置的五音在 exe 资源里，外部音乐在硬盘上
void AudioManager::loadMusic(const QString &id)
{
    m_music->stop();

    if (id.startsWith(QStringLiteral("file:"))) {
        // file: 后面存的是完整路径，转成 QUrl 才能给 QMediaPlayer
        m_music->setMedia(QUrl::fromLocalFile(id.mid(5)));
    } else {
        m_music->setMedia(QUrl(QStringLiteral("qrc:/assets/music/%1.wav").arg(id)));
    }

    if (m_musicEnabled) m_music->play();
}

// 扫描 exe 同目录下的 music 文件夹。
// 用户想换成自己下载的音乐时，只要把文件放进去，下拉框里就会自动出现。
QStringList AudioManager::localMusicFiles()
{
    QStringList result;

    QDir dir(QCoreApplication::applicationDirPath() + QStringLiteral("/music"));
    if (!dir.exists()) return result;

    // 常见音频格式都收，具体能不能播由系统解码器决定
    const QStringList filters = {QStringLiteral("*.mp3"), QStringLiteral("*.wav"),
                                 QStringLiteral("*.m4a"), QStringLiteral("*.ogg")};
    const QFileInfoList files = dir.entryInfoList(filters, QDir::Files, QDir::Name);
    for (const QFileInfo &fi : files) {
        result << fi.absoluteFilePath();
    }
    return result;
}

void AudioManager::stopMusic()
{
    if (m_music) m_music->stop();
}
