#include "ScreenDpiGuard.h"

#include <QEvent>
#include <QGuiApplication>
#include <QResizeEvent>
#include <QScreen>
#include <QShowEvent>
#include <QTimer>
#include <QWidget>
#include <QWindow>

ScreenDpiGuard::ScreenDpiGuard(QWidget *window)
    : QObject(window)
    , m_window(window)
{
    // 装一个事件过滤器：窗口显示和改变大小时会先经过这里
    if (m_window) m_window->installEventFilter(this);
}

// 当前窗口的缩放比
// 100% = 1.0，125% = 1.25，200% = 2.0
qreal ScreenDpiGuard::currentDpr() const
{
    if (m_window && m_window->windowHandle())
        return m_window->windowHandle()->devicePixelRatio();
    return 1.0;
}

bool ScreenDpiGuard::eventFilter(QObject *watched, QEvent *event)
{
    if (watched != m_window)
        return QObject::eventFilter(watched, event);

    switch (event->type()) {
    case QEvent::Show:
        onShown();
        break;
    case QEvent::Resize:
        onResized();
        break;
    case QEvent::Move:
        // 窗口被拖到另一块屏幕时，除了系统会发"换屏"信号，
        // 有些窗口（尤其是带表格的对话框）不一定收到，所以这里再兜一层。
        scheduleDpiCheck();
        break;
    default:
        break;
    }
    return QObject::eventFilter(watched, event);
}

// 窗口显示时调用
void ScreenDpiGuard::onShown()
{
    // 等这一轮显示流程走完再处理：
    // 此时布局已经算好，窗口也真正落在某块屏幕上，拿到的尺寸才准。
    QTimer::singleShot(0, this, [this]() {
        if (!m_window) return;

        // 第一次显示：把当前尺寸当成"用户期望的尺寸"
        if (!m_ready) {
            m_ready = true;
            m_logicalSize = m_window->size();
        }
        m_logicalDpr = currentDpr();

        // 第一次显示时挂上"窗口换屏"信号。
        // windowHandle() 要等窗口真正创建后才有，所以不能放在构造函数里。
        if (!m_hooked && m_window->windowHandle()) {
            m_hooked = true;
            connect(m_window->windowHandle(), &QWindow::screenChanged,
                    this, [this](QScreen *) {
                // 换屏后 Windows 还会继续发 DPI 变化消息，
                // 等这一轮处理完再改尺寸，否则刚改完就被系统覆盖。
                scheduleDpiCheck();
            });
        }

        // 每次显示都校验一次：窗口隐藏期间若被系统改过大小也能救回来
        restoreLogicalSize();
        keepInsideScreen();
    });
}

// 窗口大小变化时调用
void ScreenDpiGuard::onResized()
{
    if (!m_ready || m_adjusting || !m_window) return;

    // 最大化 / 全屏不归这里管，交给系统
    if (m_window->isMaximized() || m_window->isFullScreen()) return;

    const QSize now = m_window->size();

    // 尺寸突然变成原来的 1.5 倍以上（或缩到 2/3 以下），
    // 通常是跨屏 DPI 被系统改写，而不是用户拉伸，不能记成新尺寸。
    if (m_logicalSize.isValid() && m_logicalSize.width() > 0) {
        const double ratio = double(now.width()) / double(m_logicalSize.width());
        if (ratio > 1.5 || ratio < 0.67) return;
    }

    // 只有缩放比没变时，才认为这是用户自己拉伸窗口。
    // 换屏时缩放比会变，这种 resize 是系统引起的，必须忽略，
    // 否则错误的大尺寸会被记住、再也回不去。
    if (qFuzzyCompare(currentDpr(), m_logicalDpr))
        m_logicalSize = now;
}

// 窗口移动 / 换屏后，把尺寸恢复成记住的值
void ScreenDpiGuard::scheduleDpiCheck()
{
    if (!m_ready || !m_window || m_checkPending) return;
    m_checkPending = true;

    QTimer::singleShot(0, this, [this]() {
        m_checkPending = false;
        if (!m_window || !m_window->isVisible()) return;

        // 先按记住的尺寸恢复一次。
        restoreLogicalSize();
        keepInsideScreen();
        m_logicalDpr = currentDpr();

        // 带表格/滚动区的窗口，布局会在 DPI 变化后重新算一遍最小尺寸，
        // 可能又把窗口撑大。稍等片刻再检查一次，如果普通 resize 没起作用，
        // 就改用"隐藏→重设→显示"的强制同步（会轻微闪一下，但能救回来）。
        QTimer::singleShot(150, this, [this]() {
            if (!m_window || !m_window->isVisible()) return;

            const QSize now = m_window->size();
            const bool severe = m_logicalSize.isValid()
                && m_logicalSize.width() > 0
                && (double(now.width()) / m_logicalSize.width() > 1.5
                    || double(now.width()) / m_logicalSize.width() < 0.67);

            if (severe) {
                forceRestore();
            } else {
                restoreLogicalSize();
                keepInsideScreen();
            }
            m_logicalDpr = currentDpr();
        });
    });
}

// 普通 resize 对某些窗口（带表格的模态对话框）无效，
// 这里用"先隐藏 → 一次性设置位置和大小 → 再显示"强制同步。
void ScreenDpiGuard::forceRestore()
{
    if (!m_window || !m_logicalSize.isValid()) return;
    if (m_window->isMaximized() || m_window->isFullScreen()) return;

    const bool wasVisible = m_window->isVisible();

    // 选一块屏幕作为落点：优先窗口中心能落进的屏幕，其次当前屏，最后主屏
    QScreen *scr = QGuiApplication::screenAt(m_window->frameGeometry().center());
    if (!scr) scr = m_window->screen();
    if (!scr) scr = QGuiApplication::primaryScreen();
    if (!scr) return;

    // 目标位置：这块屏幕工作区的正中间
    const QRect avail = scr->availableGeometry();
    const QPoint targetPos(
        avail.left() + (avail.width() - m_logicalSize.width()) / 2,
        avail.top() + (avail.height() - m_logicalSize.height()) / 2);

    m_adjusting = true;   // 拦截这次校正触发的 resize/move，别记成用户操作

    if (wasVisible) m_window->hide();

    // 隐藏状态下设置几何最可靠：位置和大小一次到位，
    // 能避开 Qt 5.14 在可见窗口上直接改尺寸时的原生窗口同步问题。
    m_window->setGeometry(QRect(targetPos, m_logicalSize));

    if (wasVisible) {
        m_window->show();
        m_window->raise();
    }

    m_adjusting = false;

    // show 后布局可能又把它撑大，稍等片刻再补一次
    QTimer::singleShot(150, this, [this]() {
        if (!m_window || !m_window->isVisible()) return;
        if (m_window->isMaximized() || m_window->isFullScreen()) return;
        if (m_window->size() != m_logicalSize) {
            m_adjusting = true;
            m_window->setGeometry(QRect(m_window->frameGeometry().topLeft(),
                                        m_logicalSize));
            m_adjusting = false;
        }
    });
}

// 把窗口改回记住的逻辑尺寸（已经一样就什么都不做）
void ScreenDpiGuard::restoreLogicalSize()
{
    if (!m_window || !m_logicalSize.isValid()) return;

    // 最大化 / 全屏时不要动尺寸，否则会把最大化状态弄没
    if (m_window->isMaximized() || m_window->isFullScreen()) return;
    if (m_window->size() == m_logicalSize) return;

    m_adjusting = true;          // 这次 resize 是我们自己校正，不是用户操作
    m_window->resize(m_logicalSize);
    m_adjusting = false;
}

// 窗口中心已经不在任何屏幕上时，拉回所在屏幕居中，避免"找不回来"
void ScreenDpiGuard::keepInsideScreen()
{
    if (!m_window) return;

    const QRect frame = m_window->frameGeometry();

    // 中心点还在某块屏幕上：属于正常的跨屏摆放，不要动
    if (QGuiApplication::screenAt(frame.center())) return;

    QScreen *scr = m_window->screen();
    if (!scr) scr = QGuiApplication::primaryScreen();
    if (!scr) return;

    const QRect avail = scr->availableGeometry();
    m_window->move(avail.center() - m_window->rect().center());
}
