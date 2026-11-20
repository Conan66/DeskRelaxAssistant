#ifndef NOISEGEN_H
#define NOISEGEN_H

#include <QString>
#include <QStringList>

// ============================================================
// 白噪声合成 + 用户自定义白噪声
// ------------------------------------------------------------
// 程序自带三段兜底白噪声（雨声/风声/溪流），是纯代码合成的，
// 效果够用但不惊艳。如果用户把真实录音放进 exe 同目录的
// ambient/ 文件夹（文件名 rain.wind.stream 任取一种，
// 扩展名可以是 mp3/wav/ogg/m4a），程序会优先用录音。
//
// 合成结果会缓存到 QStandardPaths::CacheLocation() 下，
// 下次启动就直接读缓存，不会再重新算一遍。
// ============================================================
namespace NoiseGen {

// 合成缓存是否已经存在（用于判断要不要重新生成）
bool cacheExists();

// 确保三段合成白噪声都已经写到缓存目录；
// 如果缓存已存在就直接返回，不重复计算。
void ensureBuiltin();

// 列出 ambient/ 文件夹里所有可用的白噪声文件路径（mp3/wav/ogg/m4a/flac/aac）
// 以及三个兜底的合成文件（文件名带 "合成·" 前缀）
// 前者用户可以换成自己的录音，后者是程序自动生成的
QStringList allPaths();

// 每个路径对应的"显示名"（去掉路径和扩展名）
// 合成的三段会显示成 "合成·雨声" / "合成·风声" / "合成·溪流"
QString displayNameOf(const QString &path);

} // namespace NoiseGen

#endif // NOISEGEN_H
