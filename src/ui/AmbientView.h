#ifndef AMBIENTVIEW_H
#define AMBIENTVIEW_H

#include <QWidget>

// ============================================================
// 氛围放空主面板
// ------------------------------------------------------------
// 两个 Tab：
//   看图放空 —— 看自然风景图，点一下或按 → 切下一张，可全屏
//   冥想放空 —— 放空引导词 + 背景音乐 + 可选白噪声，到点前倒计时
//
// 底部控制条：
//   音乐选择 / 白噪声选择 / 音量 / 时长 / 全屏按钮 / 开始-暂停
//
// 退出全屏按任意键。
// ============================================================
class QComboBox;
class QLabel;
class QPushButton;
class QSlider;
class QStackedWidget;
class QTabWidget;
class QTimer;
class QResizeEvent;
class QShowEvent;

class AmbientView : public QWidget
{
    Q_OBJECT
public:
    explicit AmbientView(QWidget *parent = nullptr);

    // 每次"打开"这个窗口之前调用：
    //   1. 把窗口恢复成正常大小（跨屏后可能被系统放大过）
    //   2. 保证窗口出现在鼠标所在的那块屏幕上
    //      （在主屏点菜单，窗口就去主屏；在副屏点，就留在副屏）
    void prepareToShow();

protected:
    // 记下用户真正想要的窗口大小，避免跨屏时把系统放大后的尺寸当成用户尺寸
    void resizeEvent(QResizeEvent *event) override;
    // 第一次显示时监听"窗口换屏"，用来处理两块屏幕缩放比例不同的问题
    void showEvent(QShowEvent *event) override;

private slots:
    void onTabChanged(int idx);
    void onMusicChanged();
    void onNoiseChanged();
    void onVolumeChanged(int v);
    void onDurationChanged(int v);
    void onStart();
    void onTick();

    // 看图专用
    void nextImage();
    void prevImage();
    void enterFullscreen();
    void exitFullscreen();

    // 全屏任意键退出
    bool eventFilter(QObject *watched, QEvent *event) override;

    // 看图/冥想内部切换
    void openPickerPage();
    void openRunningPage();

private:
    void buildUi();
    QWidget *buildImagePage();     // 看图 Tab 的 UI
    QWidget *buildMeditatePage();  // 冥想 Tab 的 UI
    QLayout *buildControlBar();    // 底部控制条（音乐/白噪声/音量/时长/开始）
    void loadWallpapers();
    void enterImageSlide();       // 显示当前这张图（含"还没图片"提示）
    void fitPixmapToLabel();      // 按标签当前尺寸缩放当前图（窗口拉伸时也调用）
    void showFullscreenImage();   // 全屏时把当前这张图铺到全屏容器里
    void playEndingCountdown();
    void restoreLogicalSize();    // 跨屏后把窗口恢复到用户期望的逻辑尺寸

    // ---------- UI ----------
    QTabWidget   *m_tabs = nullptr;

    // 看图 Tab
    QWidget      *m_imagePage = nullptr;
    QLabel       *m_imageLabel = nullptr;
    QPushButton  *m_fullscreenBtn = nullptr;
    QTimer       *m_autoNextTimer = nullptr;   // 自动轮播

    // 冥想 Tab
    QWidget      *m_meditatePage = nullptr;
    QStackedWidget *m_meditateStack = nullptr; // picker / running 两页
    QLabel       *m_meditateGuide = nullptr;   // 放空引导词
    QLabel       *m_meditateTimer = nullptr;   // 倒计时显示

    // 底部控制条
    // 音乐和白噪声用下拉框（原来用横向 QListWidget，
    // 项一多就出现水平滚动条挤压内容，箭头还显示不全，所以换成下拉框）
    QComboBox    *m_musicBox = nullptr;
    QComboBox    *m_noiseBox = nullptr;
    QSlider      *m_volumeSlider = nullptr;
    QSlider      *m_durSlider = nullptr;     // 时长滑块
    QLabel       *m_durLabel = nullptr;      // 时长数值标签（"15 分钟"）
    QPushButton  *m_startBtn = nullptr;
    QTimer       *m_durationTimer = nullptr;
    int           m_remainingSec = 0;   // 当前还剩多少秒

    // 全屏相关
    bool          m_fullscreen = false;
    QWidget      *m_fsContainer = nullptr;
    QLabel       *m_fsHint = nullptr;      // 全屏底部的操作提示（几秒后自动隐藏）

    // ---------- 数据 ----------
    QStringList   m_wallpapers;     // 所有图片路径
    int           m_wallpaperIdx = 0;
    int           m_durationMin = 15;   // 默认 15 分钟
    int           m_volume = 18;        // 默认 18%

    // 播放状态
    bool          m_running = false;
    QString       m_currentMusic;      // 当前选中的音乐 id
    QString       m_currentNoise;      // 当前选中的白噪声 kind

    // ---------- 跨屏尺寸记忆 ----------
    // 两块显示器缩放比例不同时（比如主屏 100%、副屏 200%），
    // Windows 会在窗口换屏时按新比例改写窗口大小。
    // 这里记住"用户想要的逻辑尺寸"，换屏后如果被放大了就改回来。
    QSize         m_logicalSize = QSize(860, 620);  // 期望的窗口大小（Qt 逻辑像素）
    qreal         m_logicalDpr = 0.0;   // 记录这个尺寸时，窗口所在屏幕的缩放比
    bool          m_adjustingSize = false;  // 正在校正尺寸，忽略这次 resize
    bool          m_screenHooked = false;   // 是否已经监听"窗口换屏"
};

#endif // AMBIENTVIEW_H
