# 桌面放松助手（DeskRelaxAssistant）

一个常驻 Windows 桌面的放松小工具，以一个呼吸小球为入口，帮你随时开始呼吸练习、身体放松、久坐提醒、情绪陪伴和氛围放空。

## 功能

- **呼吸引导**：12 种呼吸法，桌面球跟随节奏涨缩，带文字提示和倒计时音效
- **身体放松**：肩颈、腰部等动作，配图逐步跟练，可选背景音乐
- **久坐提醒**：到点弹全屏遮罩或球旁小气泡，支持免打扰时段、离开检测
- **情绪陪伴**：按当下的感受读一段有引导性的话
- **氛围放空**：看风景图、听白噪声，或闭眼冥想放空
- **定时计划**：安排固定时间自动开始呼吸或拉伸
- **我的坚持**：按天统计完成次数和连续天数

## 技术栈

- C++ / Qt 5.14.2（Widgets + Multimedia）
- 构建工具：qmake + MSVC（Visual Studio 2022）

## 构建

1. 安装 Qt 5.14.2（`msvc2017_64`）和 Visual Studio 2022
2. 双击 `build.bat`（或运行 `build_rebuild.ps1`），产物在 `build\release\RelaxAssistant.exe`
3. 运行前把 `ambient\`、`wallpapers\` 复制到 exe 同目录（白噪声和风景图在运行时从这两个文件夹读取）

## 资源来源

| 资源 | 目录 | 来源 / 许可 |
| --- | --- | --- |
| 白噪声 | `ambient\` | [Pixabay](https://pixabay.com/)，遵循 Pixabay Content License |
| 风景壁纸 | `wallpapers\` | 豆包 AI 生成 |
| 动作图 / 呼吸图 / 五音 | `assets\` | 项目自带 |
| 语音引导 | （未包含） | 微软 TTS 生成，因版权原因不在本仓库，仅本地使用 |

## License

代码采用 [MIT License](LICENSE)。资源文件各自遵循其来源许可，见上表。
