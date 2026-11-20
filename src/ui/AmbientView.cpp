#include "AmbientView.h"

#include <QComboBox>
#include <QCoreApplication>
#include <QCursor>
#include <QDir>
#include <QEvent>
#include <QFileInfo>
#include <QFont>
#include <QGridLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QImage>
#include <QImageReader>
#include <QKeyEvent>
#include <QLabel>
#include <QListWidget>
#include <QMediaPlayer>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QResizeEvent>
#include <QScreen>
#include <QShowEvent>
#include <QSlider>
#include <QSoundEffect>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>
#include <QWindow>
#include <QUrl>

#include "../core/NoiseGen.h"
#include "../core/AudioManager.h"

namespace {
// 治愈绿 + 米白配色，和呼吸引导窗/拉伸窗统一
const char *kStyleSheet = R"(
QWidget#ambientRoot { background: #FBF9F4; }
QTabWidget::pane { border: none; }
QTabBar::tab {
    background: #F3F6F4; border: 1px solid #DCE6DF; border-bottom: none;
    border-top-left-radius: 8px; border-top-right-radius: 8px;
    /* min-width 保证"看图放空 / 冥想放空"四个字能完整显示，不被省略号截断 */
    min-width: 92px;
    padding: 8px 24px; font-size: 14px; color: #6B655C;
}
QTabBar::tab:selected {
    background: #FFFFFF; color: #2E5E4B; font-weight: 600;
    border-color: #DCE6DF #DCE6DF #FFFFFF;
}
QLabel#hintTitle { font-size: 15px; font-weight: bold; color: #2E5E4B; }
QLabel#hint { font-size: 13px; color: #8A857C; line-height: 160%; }
QLabel#empty { font-size: 13px; color: #9C9689; padding: 40px; }
QPushButton {
    border: 1px solid #DCE6DF; border-radius: 8px;
    padding: 7px 20px; background: #FFFFFF; color: #2E5E4B; font-size: 13px;
}
QPushButton:hover { background: #F3F6F4; }
QPushButton:disabled { color: #BEB8AE; background: #F7F4EE; }
QPushButton#primary { background: #4F8E73; color: #FFFFFF; border: none; }
QPushButton#primary:hover { background: #437A63; }
QPushButton#primary:disabled { background: #B0C8BD; }
QPushButton#fs {
    background: transparent; border: 1px dashed #B9D3C5; color: #4F8E73;
    padding: 5px 14px; font-size: 12px;
}
QPushButton#fs:hover { background: #EFF5F0; border-color: #4F8E73; }
QListWidget {
    border: 1px solid #E3DED2; border-radius: 7px; background: #FFFFFF;
    font-size: 12px; color: #33302B; padding: 4px;
}
QListWidget::item { padding: 4px 8px; border-radius: 5px; }
QListWidget::item:selected { background: #EFF5F0; color: #2E5E4B; }
/* 下拉框：和整体风格一致（音乐 / 白噪声 用这个） */
QComboBox {
    border: 1px solid #E3DED2; border-radius: 7px;
    padding: 4px 8px; background: #FFFFFF; color: #33302B; font-size: 12px;
    min-height: 20px;
}
QComboBox:hover { border-color: #B9D3C5; }
QComboBox::drop-down { border: none; width: 18px; }
QComboBox QAbstractItemView {
    background: #FFFFFF; color: #33302B;
    selection-background-color: #EFF5F0; selection-color: #2E5E4B;
    border: 1px solid #E3DED2; outline: none;
}
QSlider::groove:horizontal {
    height: 4px; background: #DCE6DF; border-radius: 2px;
}
QSlider::sub-page:horizontal {
    background: #4F8E73; border-radius: 2px;
}
QSlider::handle:horizontal {
    width: 14px; height: 14px; margin: -5px 0;
    background: #FFFFFF; border: 2px solid #4F8E73; border-radius: 7px;
}
)";
} // namespace

// ============================================================
// 主面板
// ============================================================
AmbientView::AmbientView(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("ambientRoot"));
    setStyleSheet(QString::fromUtf8(kStyleSheet));
    setWindowTitle(QStringLiteral("氛围放空"));
    resize(860, 620);

    // 第一次打开时合成白噪声（如果缓存还没有的话）。
    // 放在这里会让构造函数卡几秒，改成等一下再后台做，窗口先出来。
    // TODO：真正用到白噪声时再调，这里先不阻塞构造
    // QTimer::singleShot(500, this, []() { NoiseGen::ensureBuiltin(); });

    buildUi();
}

// ============================================================
// 跨屏尺寸 / 位置处理
// ------------------------------------------------------------
// 现象：窗口在 200% 缩放的副屏上大小正常，拖到 100% 的主屏后
// 一下子变得很大，再拖回副屏更大；关掉重开仍然很大。
//
// 原因：两块屏幕缩放比例不同时，Windows 会按新比例改写窗口的物理大小，
// Qt 5.14 有时会把改写后的尺寸当成"用户设置的大小"再叠加一次缩放，
// 于是窗口越拖越大。而氛围放空窗口是常驻单例，关掉只是隐藏，
// 所以下次打开仍沿用被放大后的尺寸和位置。
//
// 处理：
//   ① resizeEvent 里记住用户真正想要的逻辑尺寸；
//      但换屏引起的 resize（缩放比变了）不算用户操作，不记。
//   ② 换屏后把窗口改回这个逻辑尺寸。
//   ③ 每次打开前再校验一次大小和位置。
// ============================================================
void AmbientView::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);

    // 自己校正尺寸时触发的 resize，不能当成用户拉伸
    if (m_adjustingSize) return;

    // 只有缩放比没变时，才认为这是用户手动拉伸窗口。
    // 换屏时缩放比会变，这种 resize 是系统引起的，要忽略，
    // 否则错误的大尺寸就被记住、再也回不去了。
    const qreal dpr = windowHandle() ? windowHandle()->devicePixelRatio() : 1.0;
    if (m_logicalDpr <= 0.0 || qFuzzyCompare(dpr, m_logicalDpr)) {
        m_logicalSize = event->size();
    }
}

void AmbientView::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);

    // 第一次显示时，监听"窗口换屏"信号。
    // windowHandle() 要等窗口真正创建后才有，所以放在这里挂。
    if (!m_screenHooked && windowHandle()) {
        m_screenHooked = true;
        connect(windowHandle(), &QWindow::screenChanged, this, [this](QScreen *) {
            // 换屏后 Windows 还会继续发 DPI 变化消息，
            // 等这一轮处理完再改尺寸，否则刚改完就被系统覆盖。
            QTimer::singleShot(0, this, [this]() {
                if (!isVisible()) return;
                restoreLogicalSize();
                if (windowHandle())
                    m_logicalDpr = windowHandle()->devicePixelRatio();
            });
        });
    }

    // 记下当前屏幕的缩放比，作为"是不是用户拉伸"的判断基准
    if (windowHandle()) m_logicalDpr = windowHandle()->devicePixelRatio();

    // 显示后再校验一次尺寸：窗口隐藏期间被系统改过大小也能救回来
    QTimer::singleShot(0, this, [this]() {
        if (!isVisible()) return;
        restoreLogicalSize();
        if (windowHandle())
            m_logicalDpr = windowHandle()->devicePixelRatio();
    });
}

// 把窗口改回用户期望的逻辑尺寸（已经一样就什么都不做）
void AmbientView::restoreLogicalSize()
{
    if (size() == m_logicalSize) return;

    m_adjustingSize = true;      // 这次 resize 是校正，不是用户操作
    resize(m_logicalSize);
    m_adjustingSize = false;
}

// 打开窗口前调用：校正尺寸 + 放到鼠标所在屏幕
void AmbientView::prepareToShow()
{
    // ① 跨屏后被放大的窗口，先恢复成正常大小
    restoreLogicalSize();

    // ② 让窗口跟鼠标走：在哪个屏幕点开，就在哪个屏幕出现。
    //    这样关掉后再打开，不会一直留在之前那块副屏上。
    QScreen *target = QGuiApplication::screenAt(QCursor::pos());
    if (!target) target = QGuiApplication::primaryScreen();
    if (!target) return;

    const QRect avail = target->availableGeometry();

    // 窗口中心已经在这块屏幕里，说明上次就放在这里，保持原位
    if (!avail.contains(frameGeometry().center())) {
        move(avail.left() + (avail.width() - width()) / 2,
             avail.top() + (avail.height() - height()) / 2);
    }
}

// ============================================================
// 搭 UI
// ============================================================
void AmbientView::buildUi()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(20, 16, 20, 14);
    root->setSpacing(10);

    // ---------- 顶栏 Tab ----------
    m_tabs = new QTabWidget(this);
    m_tabs->addTab(buildImagePage(),   QStringLiteral("看图放空"));
    m_tabs->addTab(buildMeditatePage(), QStringLiteral("冥想放空"));
    connect(m_tabs, &QTabWidget::currentChanged, this, &AmbientView::onTabChanged);
    root->addWidget(m_tabs);

    // ---------- 底部控制条 ----------
    root->addLayout(buildControlBar());
}

// ---------- 看图 Tab ----------
QWidget *AmbientView::buildImagePage()
{
    auto *page = new QWidget(this);
    auto *lay = new QVBoxLayout(page);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(8);

    m_imagePage = page;

    auto *subRow = new QHBoxLayout();
    auto *hint = new QLabel(
        QStringLiteral("把风景图放到程序目录下的 wallpapers/ 文件夹（可以再建子文件夹分类），点图片或按 → 键切换。"),
        page);
    hint->setObjectName(QStringLiteral("hint"));
    hint->setWordWrap(true);
    subRow->addWidget(hint);
    subRow->addStretch();

    m_fullscreenBtn = new QPushButton(QStringLiteral("全屏 □"), page);
    m_fullscreenBtn->setObjectName(QStringLiteral("fs"));
    m_fullscreenBtn->setCursor(Qt::PointingHandCursor);
    connect(m_fullscreenBtn, &QPushButton::clicked, this, &AmbientView::enterFullscreen);
    subRow->addWidget(m_fullscreenBtn);

    lay->addLayout(subRow);

    // 图片展示区
    m_imageLabel = new QLabel(page);
    m_imageLabel->setAlignment(Qt::AlignCenter);
    m_imageLabel->setStyleSheet(
        QStringLiteral("QLabel{background:#FFFFFF; border:1px solid #E3DED2; border-radius:10px;}"));
    m_imageLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_imageLabel->installEventFilter(this);

    lay->addWidget(m_imageLabel, 1);

    // 注意：这里必须延迟到"布局算完之后"再加载图片。
    // 构造界面时 m_imageLabel 的尺寸还是初始值（很小），
    // 立刻缩放会把图缩成一小块；等事件循环跑一圈后尺寸才是真实大小。
    QTimer::singleShot(0, this, [this]() {
        loadWallpapers();
        enterImageSlide();
    });
    return page;
}

// ---------- 冥想 Tab ----------
QWidget *AmbientView::buildMeditatePage()
{
    auto *page = new QWidget(this);
    auto *lay = new QVBoxLayout(page);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(10);

    m_meditatePage = page;

    auto *sub = new QLabel(
        QStringLiteral("放空一下，什么都不用想。音乐 + 白噪声作伴，到点前最后几秒会轻轻提醒你。"),
        page);
    sub->setObjectName(QStringLiteral("hint"));
    sub->setWordWrap(true);
    lay->addWidget(sub);

    // 两页：选配置 / 正在冥想
    m_meditateStack = new QStackedWidget(page);

    auto *picker = new QWidget(m_meditateStack);
    auto *pickLay = new QVBoxLayout(picker);
    pickLay->setContentsMargins(30, 40, 30, 30);
    pickLay->setSpacing(14);

    auto *title = new QLabel(QStringLiteral("准备好了吗？"), picker);
    title->setObjectName(QStringLiteral("hintTitle"));
    title->setStyleSheet(QStringLiteral("font-size:22px;"));
    pickLay->addWidget(title);

    auto *guide = new QLabel(
        QStringLiteral("闭上眼睛，慢慢呼吸。不用刻意去想什么，也不用努力让自己"
                       "'完全放空'——让念头自然地来，也自然地去。\n\n"
                       "这就是冥想。就这一件事，就够了。"),
        picker);
    guide->setObjectName(QStringLiteral("hint"));
    guide->setStyleSheet(QStringLiteral("font-size:14px; color:#3B3730; line-height:180%; padding:16px; background:#FFFFFF;"
                                        " border:1px solid #E3DED2; border-radius:10px;"));
    guide->setWordWrap(true);
    pickLay->addWidget(guide);
    pickLay->addStretch();

    m_meditateStack->addWidget(picker);

    // 运行页
    auto *run = new QWidget(m_meditateStack);
    run->setStyleSheet(QStringLiteral("background:#1D1D1D;"));
    auto *runLay = new QVBoxLayout(run);
    runLay->setContentsMargins(40, 60, 40, 40);

    m_meditateTimer = new QLabel(QString(), run);
    m_meditateTimer->setAlignment(Qt::AlignCenter);
    m_meditateTimer->setStyleSheet(
        QStringLiteral("QLabel{color:#9CC3B0; font-size:48px; font-weight:300; letter-spacing:4px;}"));
    runLay->addStretch();
    runLay->addWidget(m_meditateTimer);
    runLay->addStretch();

    auto *backBtn = new QPushButton(QStringLiteral("返回"), run);
    backBtn->setCursor(Qt::PointingHandCursor);
    backBtn->setStyleSheet(
        QStringLiteral("QPushButton{background:transparent; border:1px solid #4F8E73; color:#9CC3B0;"
                       " border-radius:8px; padding:7px 24px;} QPushButton:hover{background:#2A3F36;}"));
    connect(backBtn, &QPushButton::clicked, this, &AmbientView::openPickerPage);
    runLay->addWidget(backBtn, 0, Qt::AlignCenter);

    m_meditateStack->addWidget(run);

    lay->addWidget(m_meditateStack, 1);
    return page;
}

// ---------- 底部控制条 ----------
// 用 QGridLayout 两行排列：
//   第 0 行：所有标题（音乐 / 白噪声 / 音量 / 时长）
//   第 1 行：所有控件
// 这样标题一定在同一水平线上，控件也一定在同一水平线上，
// 不会再出现"标签忽高忽低"的错位。
QLayout *AmbientView::buildControlBar()
{
    auto *grid = new QGridLayout();
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(10);   // 各组之间的水平间距
    grid->setVerticalSpacing(3);      // 标题和控件之间的垂直间距

    // 往某一列放一组："标题 + 控件"
    // col：第几列；title：标题文字；w：控件；stretch：这一列占的宽度比例
    auto addCol = [grid](int col, const QString &title, QWidget *w, int stretch) {
        auto *t = new QLabel(title);
        t->setStyleSheet(QStringLiteral("font-size:11px; color:#9C9689;"));
        // 标题贴左对齐、贴底，紧挨下面的控件
        grid->addWidget(t, 0, col, Qt::AlignLeft | Qt::AlignBottom);
        // 控件顶部对齐，这样滑块和下拉框的"上边缘"是一条线
        grid->addWidget(w, 1, col, Qt::AlignTop);
        grid->setColumnStretch(col, stretch);
    };

    // ---------- 音乐（下拉框） ----------
    m_musicBox = new QComboBox(this);
    m_musicBox->setMinimumWidth(130);
    m_musicBox->setToolTip(QStringLiteral("内置五音 + 你自己放进 music 文件夹的音乐"));

    // 内置五音：显示名在前，data 存内部 id
    m_musicBox->addItem(QStringLiteral("宫音·安稳"), QStringLiteral("gong"));
    m_musicBox->addItem(QStringLiteral("商音·清肃"), QStringLiteral("shang"));
    m_musicBox->addItem(QStringLiteral("角音·舒展"), QStringLiteral("jue"));
    m_musicBox->addItem(QStringLiteral("徵音·温暖"), QStringLiteral("zhi"));
    m_musicBox->addItem(QStringLiteral("羽音·沉静"), QStringLiteral("yu"));

    // 用户 music/ 文件夹里的
    for (const QString &path : AudioManager::localMusicFiles()) {
        m_musicBox->addItem(QStringLiteral("本地·%1").arg(QFileInfo(path).completeBaseName()),
                            QStringLiteral("file:") + path);
    }
    m_musicBox->setCurrentIndex(4);   // 默认羽音
    connect(m_musicBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) { onMusicChanged(); });
    addCol(0, QStringLiteral("音乐"), m_musicBox, 3);

    // ---------- 白噪声（下拉框） ----------
    m_noiseBox = new QComboBox(this);
    m_noiseBox->setMinimumWidth(150);
    m_noiseBox->setToolTip(QStringLiteral("ambient 文件夹里的白噪声；文件名太长会截断，悬停可看全名"));

    m_noiseBox->addItem(QStringLiteral("不播放"), QString());   // data 空 = 不播放
    // 扫 ambient/ 文件夹 + 合成兜底
    for (const QString &p : NoiseGen::allPaths()) {
        const QString full = NoiseGen::displayNameOf(p);
        // 名字太长就截断显示，但 tooltip 保留完整名字
        QString shown = full;
        if (shown.size() > 16) shown = shown.left(14) + QStringLiteral("…");
        m_noiseBox->addItem(shown, p);
        m_noiseBox->setItemData(m_noiseBox->count() - 1, full, Qt::ToolTipRole);
    }
    m_noiseBox->setCurrentIndex(0);
    connect(m_noiseBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) { onNoiseChanged(); });
    addCol(1, QStringLiteral("白噪声"), m_noiseBox, 3);

    // ---------- 音量 ----------
    m_volumeSlider = new QSlider(Qt::Horizontal);
    m_volumeSlider->setMinimumWidth(90);
    m_volumeSlider->setRange(0, 100);
    m_volumeSlider->setValue(m_volume);
    connect(m_volumeSlider, &QSlider::valueChanged, this, &AmbientView::onVolumeChanged);
    addCol(2, QStringLiteral("音量"), m_volumeSlider, 2);

    // ---------- 时长 ----------
    // 数值直接写在标题里："时长 · 15 分钟"，拖动时实时更新。
    // 这样标题行还是齐的，不会多出一个孤立的小标签。
    m_durSlider = new QSlider(Qt::Horizontal);
    m_durSlider->setMinimumWidth(90);
    m_durSlider->setRange(1, 12);      // 1=5 分钟 ... 12=60 分钟
    m_durSlider->setValue(3);          // 默认 3×5 = 15 分钟
    m_durSlider->setToolTip(QStringLiteral("5 ~ 60 分钟，每格 5 分钟"));
    connect(m_durSlider, &QSlider::valueChanged, this, [this](int v) {
        m_durationMin = v * 5;
        if (m_durLabel)
            m_durLabel->setText(QStringLiteral("时长 · %1 分钟").arg(m_durationMin));
    });
    // 标题用动态 label，所以要自己建，不能走 addCol 的静态标题
    m_durLabel = new QLabel(QStringLiteral("时长 · 15 分钟"));
    m_durLabel->setStyleSheet(QStringLiteral("font-size:11px; color:#9C9689;"));
    grid->addWidget(m_durLabel, 0, 3, Qt::AlignLeft | Qt::AlignBottom);
    grid->addWidget(m_durSlider, 1, 3, Qt::AlignTop);
    grid->setColumnStretch(3, 2);

    // ---------- 开始 / 暂停 ----------
    m_startBtn = new QPushButton(QStringLiteral("▶ 开始"), this);
    m_startBtn->setObjectName(QStringLiteral("primary"));
    m_startBtn->setCursor(Qt::PointingHandCursor);
    m_startBtn->setMinimumHeight(32);
    m_startBtn->setMinimumWidth(86);
    connect(m_startBtn, &QPushButton::clicked, this, &AmbientView::onStart);
    // 按钮单独占第 4 列，和标题行底部对齐（视觉上跟控件在一块）
    grid->addWidget(m_startBtn, 1, 4, Qt::AlignTop);
    grid->setColumnStretch(4, 0);

    return grid;
}

// ============================================================
// 看图：扫描 wallpapers/ 文件夹
// ============================================================
// ---------- 看图：递归扫 wallpapers/ 所有子文件夹 ----------
// 搜索优先级：
//   1) exe 同目录下的 wallpapers/  —— release 部署时的标准位置
//   2) 源码目录下的 source/picture/ —— 开发时从 VS 调试运行的兜底
// 这样无论 release 运行还是 F5 调试都能找到图
static QStringList findWallpapersRoots()
{
    QStringList roots;
    const QString appDir = QCoreApplication::applicationDirPath();

    // 1. 标准位置：exeDir/wallpapers
    QString p1 = appDir + "/wallpapers";
    if (QDir(p1).exists()) roots << p1;

    // 2. 开发兜底：向上追溯找到 source/picture
    //    build/release/../../../source/picture 或
    //    build/debug/../../../source/picture 或
    //    直接双击 exe 后 source 目录不存在（已被上面覆盖）
    QDir cur(appDir);
    for (int i = 0; i < 8 && cur.cdUp(); ++i) {
        QString candidate = cur.path() + "/source/picture";
        if (QDir(candidate).exists()) { roots << candidate; break; }
    }
    // 还往上试试 source/音效-白噪声 同级的 picture
    QDir cur2(appDir);
    for (int i = 0; i < 8 && cur2.cdUp(); ++i) {
        QString candidate = cur2.path() + "/source";
        if (QDir(candidate + "/picture").exists() && !roots.contains(candidate + "/picture"))
            roots << (candidate + "/picture");
    }
    return roots;
}

static void collectImages(QDir &dir, QStringList *out)
{
    const QStringList exts = {
        QStringLiteral("*.jpg"), QStringLiteral("*.jpeg"),
        QStringLiteral("*.png"), QStringLiteral("*.gif"),
        QStringLiteral("*.bmp")
    };
    for (const QString &e : exts) {
        dir.setNameFilters(QStringList(e));
        const QFileInfoList list = dir.entryInfoList(QDir::Files, QDir::Name);
        for (const QFileInfo &fi : list)
            *out << fi.absoluteFilePath();
    }

    // 注意：上面循环结束时 dir 的名字过滤器还停留在 "*.bmp"，
    // 如果不清掉，下一句枚举子目录时子目录也会被这个过滤器挡掉，
    // 结果就是"子文件夹里的图永远搜不到"。
    dir.setNameFilters(QStringList());

    const QFileInfoList subdirs = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo &fi : subdirs) {
        QDir subDir(fi.absoluteFilePath());
        collectImages(subDir, out);
    }
}

void AmbientView::loadWallpapers()
{
    m_wallpapers.clear();

    for (const QString &root : findWallpapersRoots()) {
        QDir dir(root);
        if (!dir.exists()) continue;
        collectImages(dir, &m_wallpapers);
    }
    // 去重（防止两个 root 搜到同一个文件）
    m_wallpapers.removeDuplicates();
    m_wallpaperIdx = 0;
}

void AmbientView::enterImageSlide()
{
    if (m_wallpapers.isEmpty()) {
        m_imageLabel->setText(QStringLiteral("还没有图片\n\n把 jpg / png / gif 放到\n程序目录下的 wallpapers/ 文件夹里"));
        m_imageLabel->setObjectName(QStringLiteral("empty"));
        // 取消之前可能设置过的 pixmap
        m_imageLabel->clear();
        m_imageLabel->setStyleSheet(
            QStringLiteral("QLabel{background:#FFFFFF; border:1px dashed #C9D8CF; border-radius:10px; color:#9C9689;}"));
        return;
    }

    // 有图状态：清掉可能残留的提示文字，换成黑底展示区
    m_imageLabel->setText(QString());
    m_imageLabel->setObjectName(QString());
    m_imageLabel->setStyleSheet(
        QStringLiteral("QLabel{background:#1D1D1D; border:1px solid #E3DED2; border-radius:10px;}"));

    // 按当前标签尺寸缩放显示
    fitPixmapToLabel();
}

// 只负责"把当前这张图按标签大小缩放后显示"
// 单独抽出来，是为了窗口拉伸时能反复调用，不用重复设置样式
void AmbientView::fitPixmapToLabel()
{
    if (m_wallpapers.isEmpty() || !m_imageLabel) return;

    const QString &path = m_wallpapers.at(m_wallpaperIdx);
    QPixmap pm(path);
    // 某些 gif/bmp 如果直接构造失败，可以再试用 QImageReader 加载
    if (pm.isNull()) {
        QImageReader reader(path);
        const QImage img = reader.read();
        if (img.isNull()) return;
        pm = QPixmap::fromImage(img);
    }

    // 按标签大小等比缩放，保持比例，背景黑
    const QSize target = m_imageLabel->size() - QSize(8, 8);
    // 尺寸太小时不缩放（窗口还没布局完），避免把图缩没
    if (target.width() <= 10 || target.height() <= 10) return;
    if (pm.width() > target.width() || pm.height() > target.height()) {
        pm = pm.scaled(target, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    m_imageLabel->setPixmap(pm);
}

// ============================================================
// 看图切换（鼠标点击或 → 键）
// ============================================================
void AmbientView::nextImage()
{
    if (m_wallpapers.isEmpty()) return;
    m_wallpaperIdx = (m_wallpaperIdx + 1) % m_wallpapers.size();
    // 全屏时走全屏刷新，否则走窗口内的刷新
    if (m_fullscreen) showFullscreenImage();
    else              enterImageSlide();
}

void AmbientView::prevImage()
{
    if (m_wallpapers.isEmpty()) return;
    m_wallpaperIdx = (m_wallpaperIdx - 1 + m_wallpapers.size()) % m_wallpapers.size();
    if (m_fullscreen) showFullscreenImage();
    else              enterImageSlide();
}

// ============================================================
// 看图全屏
// ============================================================
void AmbientView::enterFullscreen()
{
    if (m_fullscreen || m_wallpapers.isEmpty()) return;

    // 保存当前状态，恢复时用
    m_fsContainer = new QWidget(this);
    m_fsContainer->setStyleSheet(QStringLiteral("background:#1D1D1D;"));
    m_fsContainer->setParent(this, Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    m_fsContainer->showFullScreen();
    m_fsContainer->installEventFilter(this);

    // 把图片 Label 挪进去
    m_imageLabel->setParent(m_fsContainer);
    m_imageLabel->setStyleSheet(QStringLiteral("QLabel{background:#1D1D1D;}"));
    m_imageLabel->show();

    m_fullscreen = true;
    m_fullscreenBtn->setText(QStringLiteral("退出全屏"));

    // 按全屏尺寸摆放当前这张图
    showFullscreenImage();

    // 底部给一条操作提示，3 秒后自动隐藏：
    // 让用户知道全屏里还能切图，而不是一按就退出
    m_fsHint = new QLabel(QStringLiteral("点击 / ← → 切换图片 · 任意其他键退出"), m_fsContainer);
    m_fsHint->setStyleSheet(QStringLiteral(
        "QLabel{color:#FFFFFF; background:rgba(0,0,0,150);"
        " border-radius:9px; padding:7px 16px; font-size:13px;}"));
    m_fsHint->adjustSize();
    const QSize fs = m_fsContainer->size();
    m_fsHint->move((fs.width() - m_fsHint->width()) / 2,
                   fs.height() - m_fsHint->height() - 40);
    m_fsHint->show();
    m_fsHint->raise();   // 压在图片上面
    // 以 hint 自己作为定时器上下文：容器提前销毁时不会野指针
    QLabel *hint = m_fsHint;
    QTimer::singleShot(3000, hint, [hint]() { hint->hide(); });
}

// 全屏状态下：把当前这张图按屏幕尺寸等比缩放并居中
void AmbientView::showFullscreenImage()
{
    if (!m_fsContainer || m_wallpapers.isEmpty()) return;

    const QSize target = m_fsContainer->size();
    if (target.width() <= 0 || target.height() <= 0) return;

    QPixmap pm(m_wallpapers.at(m_wallpaperIdx));
    if (pm.isNull()) {
        QImageReader reader(m_wallpapers.at(m_wallpaperIdx));
        const QImage img = reader.read();
        if (img.isNull()) return;
        pm = QPixmap::fromImage(img);
    }
    pm = pm.scaled(target, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    m_imageLabel->setPixmap(pm);
    m_imageLabel->setGeometry((target.width() - pm.width()) / 2,
                              (target.height() - pm.height()) / 2,
                              pm.width(), pm.height());
    // 提示条重新压到最上层（切图后 label 可能重绘到上面）
    if (m_fsHint) m_fsHint->raise();
}

void AmbientView::exitFullscreen()
{
    if (!m_fullscreen) return;

    m_fullscreen = false;
    m_fullscreenBtn->setText(QStringLiteral("全屏 □"));

    // 图片 Label 挪回看图页，并且必须重新塞回布局！
    // 只 setParent 不进布局的话，布局里就少了这一项，
    // 结果提示文字和全屏按钮会撑满整页、垂直居中（这就是之前的问题）
    m_imageLabel->setStyleSheet(
        QStringLiteral("QLabel{background:#1D1D1D; border:1px solid #E3DED2; border-radius:10px;}"));
    if (auto *v = qobject_cast<QVBoxLayout *>(m_imagePage->layout()))
        v->addWidget(m_imageLabel, 1);
    else
        m_imageLabel->setParent(m_imagePage);
    m_imageLabel->show();

    if (m_fsContainer) {
        m_fsHint = nullptr;              // 提示条是容器的子控件，会跟着一起删掉
        m_fsContainer->deleteLater();
        m_fsContainer = nullptr;
    }

    // 等布局重新算完再缩放：否则拿到的还是全屏时的旧尺寸
    QTimer::singleShot(0, this, [this]() { enterImageSlide(); });
}

// 全屏容器上装 eventFilter：
//   → / ←  切下一张 / 上一张
//   鼠标左键  切下一张（保留"点一下就换"的直觉）
//   鼠标右键 / 其他任意键  退出全屏
bool AmbientView::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_fsContainer && m_fullscreen) {
        if (event->type() == QEvent::KeyPress) {
            auto *ke = static_cast<QKeyEvent *>(event);
            if (ke->key() == Qt::Key_Right) { nextImage(); return true; }
            if (ke->key() == Qt::Key_Left)  { prevImage(); return true; }
            exitFullscreen();
            return true;
        }
        if (event->type() == QEvent::MouseButtonPress) {
            auto *me = static_cast<QMouseEvent *>(event);
            if (me->button() == Qt::RightButton) { exitFullscreen(); return true; }
            nextImage();
            return true;
        }
        return QWidget::eventFilter(watched, event);
    }
    // 看图 Label 也支持点击切下一张（非全屏时）
    if (watched == m_imageLabel && !m_fullscreen) {
        if (event->type() == QEvent::MouseButtonPress) {
            nextImage();
            return true;
        }
        // 窗口尺寸变了（拉伸/最大化），按新尺寸重新缩放当前图，
        // 否则图片会一直保持旧大小，周围留一大片空白
        if (event->type() == QEvent::Resize
            && !m_wallpapers.isEmpty() && !m_running) {
            QTimer::singleShot(0, this, [this]() { fitPixmapToLabel(); });
        }
    }
    return QWidget::eventFilter(watched, event);
}

// ============================================================
// Tab 切换
// ============================================================
void AmbientView::onTabChanged(int idx)
{
    Q_UNUSED(idx)
    // 切换 Tab 时如果正在播放，先停掉（避免看图和冥想同时响）
    if (m_running) onStart();
}

// ============================================================
// 音乐 / 白噪声 / 音量变化（UI 层的选择变化，还没播）
// ============================================================
void AmbientView::onMusicChanged()
{
    // 下拉框里存的是内部 id（gong/shang/... 或 file:路径）
    if (!m_musicBox) { m_currentMusic = QStringLiteral("yu"); return; }
    const QString v = m_musicBox->currentData().toString();
    m_currentMusic = v.isEmpty() ? QStringLiteral("yu") : v;
}

// 白噪声下拉框里存的是完整路径（空字符串 = 不播放）
void AmbientView::onNoiseChanged()
{
    m_currentNoise = m_noiseBox ? m_noiseBox->currentData().toString() : QString();
}

void AmbientView::onVolumeChanged(int v)
{
    m_volume = v;
}

void AmbientView::onDurationChanged(int v)
{
    m_durationMin = v;
}

// ============================================================
// 开始 / 暂停 / 停止
// ------------------------------------------------------------
// 因为这个控件里同时有"音乐播放器"和"白噪声播放器"两个 QMediaPlayer，
// 所以自己 new 两个，不污染全局的 AudioManager。
// ============================================================
namespace {
// 用两个 QMediaPlayer 同时播音乐和白噪声，互不干扰
QMediaPlayer *g_musicPlayer = nullptr;
QMediaPlayer *g_noisePlayer = nullptr;
}

void AmbientView::onStart()
{
    // 第一次调用时才创建播放器
    if (!g_musicPlayer) {
        g_musicPlayer = new QMediaPlayer(this);
        g_musicPlayer->setVolume(m_volume);
        g_noisePlayer = new QMediaPlayer(this);
        g_noisePlayer->setVolume(m_volume * 70 / 100);   // 白噪声略小声

        // 两首都循环播
        for (auto *p : {g_musicPlayer, g_noisePlayer}) {
            connect(p, &QMediaPlayer::mediaStatusChanged, this,
                [p](QMediaPlayer::MediaStatus s) {
                    if (s == QMediaPlayer::EndOfMedia) {
                        p->setPosition(0);
                        p->play();
                    }
                });
        }
    }

    if (m_running) {
        // 正在跑 → 暂停 / 停止
        if (g_musicPlayer) g_musicPlayer->pause();
        if (g_noisePlayer) g_noisePlayer->pause();
        if (m_durationTimer) m_durationTimer->stop();
        m_startBtn->setText(QStringLiteral("▶ 继续"));
        m_running = false;
        return;
    }

    // 没在跑 → 开始（包括"继续"）
    // 1) 音乐
    onMusicChanged();
    if (m_currentMusic.isEmpty()) m_currentMusic = QStringLiteral("yu");
    g_musicPlayer->setVolume(m_volume);
    if (m_currentMusic.startsWith(QStringLiteral("file:"))) {
        g_musicPlayer->setMedia(QUrl::fromLocalFile(m_currentMusic.mid(5)));
    } else {
        g_musicPlayer->setMedia(QUrl(QStringLiteral("qrc:/assets/music/%1.wav").arg(m_currentMusic)));
    }
    g_musicPlayer->play();

    // 2) 白噪声（m_currentNoise 现在是完整路径，直接用）
    onNoiseChanged();
    g_noisePlayer->stop();
    // 重新开始时把音量恢复到用户设定值：
    // 上次如果是在"结束前淡出"阶段暂停的，音量还停在很低的位置
    g_noisePlayer->setVolume(m_volume * 70 / 100);
    if (!m_currentNoise.isEmpty()) {
        g_noisePlayer->setMedia(QUrl::fromLocalFile(m_currentNoise));
        g_noisePlayer->play();
    }

    // 3) 时长倒计时
    // 从滑块读当前值：滑块 1~12 格，每格 5 分钟
    if (m_durSlider) m_durationMin = m_durSlider->value() * 5;
    m_remainingSec = m_durationMin * 60;
    if (!m_durationTimer) {
        m_durationTimer = new QTimer(this);
        m_durationTimer->setInterval(1000);
        connect(m_durationTimer, &QTimer::timeout, this, &AmbientView::onTick);
    }
    m_durationTimer->start();
    m_startBtn->setText(QStringLiteral("‖ 暂停"));
    m_running = true;

    // 如果是冥想 Tab，切到运行页并显示剩余时间
    if (m_tabs->currentIndex() == 1) {
        m_meditateStack->setCurrentIndex(1);
        m_meditateTimer->setText(QStringLiteral("%1:%2")
            .arg(m_remainingSec / 60, 2, 10, QChar('0'))
            .arg(m_remainingSec % 60, 2, 10, QChar('0')));
    }
}

// ============================================================
// 每秒一次的倒计时 tick
// ============================================================
void AmbientView::onTick()
{
    --m_remainingSec;

    // ---------- 结束前 8 秒：音乐和白噪声一起线性淡出 ----------
    // 不淡出的话，时间一到声音"啪"地断掉，放空到一半会被惊醒。
    // 做法：剩余 8 秒时音量是 100%，之后每秒按比例往下调，
    // 到 0 秒时正好静音，再停止播放，听感是"慢慢远去了"。
    if (m_remainingSec > 0 && m_remainingSec <= 8) {
        const double factor = m_remainingSec / 8.0;      // 1.0 → 0.125
        const int baseMusic = m_volume;                  // 音乐的基础音量
        const int baseNoise = m_volume * 70 / 100;       // 白噪声平时就小一点
        if (g_musicPlayer) g_musicPlayer->setVolume(int(baseMusic * factor));
        if (g_noisePlayer) g_noisePlayer->setVolume(int(baseNoise * factor));
    }

    if (m_tabs->currentIndex() == 1) {
        // 冥想 Tab：显示倒计时
        if (m_remainingSec >= 0) {
            m_meditateTimer->setText(QStringLiteral("%1:%2")
                .arg(m_remainingSec / 60, 2, 10, QChar('0'))
                .arg(m_remainingSec % 60, 2, 10, QChar('0')));
        }

        // 最后 6 秒播放滴答声
        if (m_remainingSec > 0 && m_remainingSec <= 6) {
            playEndingCountdown();
        }
    }

    if (m_remainingSec <= 0) {
        // 时间到了
        m_durationTimer->stop();
        if (g_musicPlayer) g_musicPlayer->stop();
        if (g_noisePlayer) g_noisePlayer->stop();
        m_running = false;
        m_startBtn->setText(QStringLiteral("▶ 开始"));

        if (m_tabs->currentIndex() == 1) {
            openPickerPage();
            // 结束后在 picker 页弹个轻量提示（不用 QMessageBox 打断放空）
            auto *ok = new QTimer(this);
            ok->setSingleShot(true);
            connect(ok, &QTimer::timeout, ok, &QTimer::deleteLater);
            ok->start(1000);
        }
    }
}

void AmbientView::playEndingCountdown()
{
    static QSoundEffect *tick = nullptr;
    if (!tick) {
        tick = new QSoundEffect(this);
        tick->setSource(QUrl(QStringLiteral("qrc:/assets/sfx/tick.wav")));
        tick->setVolume(0.85);   // 倒计时滴答比平时节拍大声一点
    }
    tick->play();
}

void AmbientView::openPickerPage()
{
    m_meditateStack->setCurrentIndex(0);
}

void AmbientView::openRunningPage()
{
    m_meditateStack->setCurrentIndex(1);
}
