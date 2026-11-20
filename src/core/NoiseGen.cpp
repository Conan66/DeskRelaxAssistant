#include "NoiseGen.h"

// MSVC 默认不定义 M_PI（C++ 标准没规定），手动补一下
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QtMath>
#include <QRandomGenerator>
#include <QStandardPaths>

namespace {

// ---------- 通用常量 ----------
const int kSampleRate = 44100;     // CD 音质采样率
const int kChannels = 1;           // 单声道（白噪声不需要立体声）
const int kBitsPerSample = 16;     // 16bit PCM
const int kSeconds = 60;           // 每段合成 60 秒，够长了
const int kTotalSamples = kSampleRate * kSeconds;

// ---------- wav 写文件：标准的 RIFF + fmt + data 三段头 ----------
// 小白友好版：照着 wav 规范一个个字段写，数据是小端序。
// 整个文件 = 44 字节头 + PCM 原始数据
void writeWav(const QString &path, const QVector<qint16> &samples)
{
    const int dataBytes = samples.size() * 2;   // 16bit = 2 字节/样本
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) return;

    auto writeLE = [&](quint32 v, int bytes) {
        for (int i = 0; i < bytes; ++i)
            f.write((char *)&v, 1), v >>= 8;   // 小端：最低字节先写
    };

    auto writeStr = [&](const char *s) {
        f.write(s, static_cast<qint64>(qstrlen(s)));
    };

    // RIFF 段（12 字节）
    writeStr("RIFF");
    writeLE(36 + dataBytes, 4);   // 文件总大小 - 8
    writeStr("WAVE");

    // fmt 段（24 字节）
    writeStr("fmt ");
    writeLE(16, 4);               // 段大小（不含自身 8 字节）
    writeLE(1, 2);                // 1 = PCM
    writeLE(kChannels, 2);
    writeLE(kSampleRate, 4);
    writeLE(kSampleRate * kChannels * (kBitsPerSample / 8), 4); // byteRate
    writeLE(kChannels * (kBitsPerSample / 8), 2);               // blockAlign
    writeLE(kBitsPerSample, 2);

    // data 段
    writeStr("data");
    writeLE(dataBytes, 4);
    f.write(reinterpret_cast<const char *>(samples.constData()), dataBytes);
    f.close();
}

// ---------- 白噪声：每个 sample 取 -1.0 ~ +1.0 的随机值 ----------
// 均匀分布，听起来像收音机没信号的沙沙声
QVector<float> generateWhiteNoise(int n)
{
    QVector<float> out(n);
    auto *r = QRandomGenerator::global();
    for (int i = 0; i < n; ++i)
        out[i] = r->generateDouble() * 2.0f - 1.0f;
    return out;
}

// ---------- 低通滤波：把尖锐的高频削掉，让声音变柔和 ----------
// a 越接近 1.0 越"闷"，0.85 是雨声常用的参数
QVector<float> lowPass(const QVector<float> &in, float a)
{
    QVector<float> out(in.size());
    out[0] = in[0];
    for (int i = 1; i < in.size(); ++i)
        out[i] = a * out[i-1] + (1.0f - a) * in[i];
    return out;
}

// ---------- 高通滤波：把低频去掉，让声音更"清脆" ----------
// 溪流声就是白噪声加一点高通
QVector<float> highPass(const QVector<float> &in, float a)
{
    QVector<float> out(in.size());
    out[0] = 0.0f;
    for (int i = 1; i < in.size(); ++i)
        out[i] = a * (out[i-1] + in[i] - in[i-1]);
    return out;
}

// ---------- 棕噪声：把白噪声积分 ----------
// 听起来像远处的风或者瀑布，低频为主、很"沉"
QVector<float> brownNoise(int n)
{
    QVector<float> out(n);
    float running = 0.0f;
    auto *r = QRandomGenerator::global();
    // 积分：每一步加一点随机增量
    for (int i = 0; i < n; ++i) {
        running += (r->generateDouble() * 2.0 - 1.0) * 0.02f;
        // 积分会越来越大，定期拉回来（模拟布朗运动的有界性）
        if (running > 1.0f) running -= 2.0f;
        if (running < -1.0f) running += 2.0f;
        out[i] = running;
    }
    return out;
}

// ---------- 把 float(-1~+1) 压到 qint16(-32768~32767) ----------
// 还有一个整体音量缩放，避免炸耳朵
QVector<qint16> toPCM(const QVector<float> &in, float volume = 0.35f)
{
    QVector<qint16> out(in.size());
    for (int i = 0; i < in.size(); ++i) {
        float v = in[i] * volume;
        // 削波：超过 1.0 就截断到最大 qint16，避免失真
        if (v > 1.0f) v = 1.0f;
        if (v < -1.0f) v = -1.0f;
        out[i] = static_cast<qint16>(v * 32767.0f);
    }
    return out;
}

// ---------- 三种白噪声各有自己的味道 ----------

// 雨声 = 低通后的白噪声 + 每隔一段叠加一个"水滴"脉冲
QVector<float> makeRain(int n)
{
    QVector<float> noise = generateWhiteNoise(n);
    QVector<float> low = lowPass(noise, 0.85f);

    // 每半秒左右加一个"滴落"声（高频短脉冲）
    QVector<float> drips(n, 0.0f);
    auto *r = QRandomGenerator::global();
    int i = r->bounded(4410, 22050);   // 起始随机
    while (i < n - 2205) {
        // 一个 0.05 秒的正弦脉冲 + 衰减，模拟水滴声音
        for (int j = 0; j < 2205 && i + j < n; ++j) {
            float t = j / 44100.0f;
            drips[i + j] = qSin(2.0f * M_PI * 1200.0f * t)   // 1200Hz 高频
                         * qExp(-t * 40.0f)                  // 快速衰减
                         * 0.15f;                             // 音量别太大
        }
        i += r->bounded(11025, 33075); // 250ms ~ 750ms 随机间隔
    }

    for (int k = 0; k < n; ++k)
        low[k] = low[k] + drips[k];
    return low;
}

// 风声 = 棕噪声 + 低频起伏（模拟风速忽大忽小）
QVector<float> makeWind(int n)
{
    QVector<float> brown = brownNoise(n);

    // 再叠一个非常慢的正弦波（0.2Hz，5秒一个周期）做起伏
    QVector<float> out(n);
    for (int i = 0; i < n; ++i) {
        float t = i / 44100.0f;
        float envelope = 0.6f + 0.4f * qSin(2.0f * M_PI * 0.2f * t);
        out[i] = brown[i] * envelope;
    }
    return out;
}

// 溪流 = 高通后的白噪声（清脆的潺潺声）
QVector<float> makeStream(int n)
{
    QVector<float> noise = generateWhiteNoise(n);
    QVector<float> hp = highPass(noise, 0.95f);

    // 加一点点低通让它不那么刺耳
    QVector<float> lp = lowPass(hp, 0.3f);
    return lp;
}

// ---------- 缓存目录 ----------
QString cacheDir()
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    if (dir.isEmpty()) dir = QCoreApplication::applicationDirPath() + "/cache";
    dir += "/noise";
    QDir().mkpath(dir);
    return dir;
}

QString cachePath(const QString &kind)
{
    return cacheDir() + "/" + kind + ".wav";
}

// ---------- ambient/ 文件夹 ----------
// 搜索优先级：
//   1) exeDir/ambient/ —— release 部署
//   2) 向上追溯 source/音效-白噪声/ —— 开发时 VS 调试的兜底
QStringList findAmbientRoots()
{
    QStringList roots;
    const QString appDir = QCoreApplication::applicationDirPath();

    QString p1 = appDir + "/ambient";
    if (QDir(p1).exists()) roots << p1;

    QDir cur(appDir);
    for (int i = 0; i < 8 && cur.cdUp(); ++i) {
        // Windows 上中文路径可能有问题，同时试英文名
        QString cnPath = cur.path() + "/source/音效-白噪声";
        if (QDir(cnPath).exists()) { roots << cnPath; break; }
    }
    return roots;
}

const QStringList kAudioExts = {
    QStringLiteral("*.mp3"), QStringLiteral("*.wav"), QStringLiteral("*.ogg"),
    QStringLiteral("*.m4a"), QStringLiteral("*.flac"), QStringLiteral("*.aac")
};

} // namespace

// ============================================================
// 下面是对外的公开接口
// ============================================================

namespace NoiseGen {

bool cacheExists()
{
    return QFile::exists(cachePath("rain"))
        && QFile::exists(cachePath("wind"))
        && QFile::exists(cachePath("stream"));
}

void ensureBuiltin()
{
    if (cacheExists()) return;

    QVector<qint16> rainPC   = toPCM(makeRain(kTotalSamples),   0.45f);
    QVector<qint16> windPC  = toPCM(makeWind(kTotalSamples),  0.35f);
    QVector<qint16> streamPC = toPCM(makeStream(kTotalSamples), 0.4f);

    writeWav(cachePath("rain"),   rainPC);
    writeWav(cachePath("wind"),  windPC);
    writeWav(cachePath("stream"), streamPC);
}

// 扫 ambient/ 文件夹 + 合成缓存，返回所有可用路径
QStringList allPaths()
{
    ensureBuiltin();

    QStringList out;

    // 1) ambient/ 文件夹（release 和开发兜底两个位置都扫）
    for (const QString &root : findAmbientRoots()) {
        QDir dir(root);
        for (const QString &e : kAudioExts) {
            dir.setNameFilters(QStringList(e));
            const QFileInfoList list = dir.entryInfoList(QDir::Files, QDir::Name);
            for (const QFileInfo &fi : list)
                out << fi.absoluteFilePath();
        }
    }

    // 2) 合成缓存三段（兜底）
    for (const QString &k : { QStringLiteral("rain"), QStringLiteral("wind"), QStringLiteral("stream") }) {
        const QString p = cachePath(k);
        if (QFile::exists(p)) out << p;
    }

    out.removeDuplicates();
    return out;
}

// 把合成缓存里的文件名翻译成中文显示名，其他就用原文件名
QString displayNameOf(const QString &path)
{
    const QString base = QFileInfo(path).completeBaseName();

    static const QHash<QString, QString> builtinName = {
        { cachePath("rain"),   QStringLiteral("合成·雨声") },
        { cachePath("wind"),  QStringLiteral("合成·风声") },
        { cachePath("stream"), QStringLiteral("合成·溪流") },
    };
    auto it = builtinName.find(path);
    if (it != builtinName.end()) return it.value();

    return base;   // 用户文件直接用原文件名（去掉扩展名）
}

} // namespace NoiseGen
