#ifndef SCREENDPIGUARD_H
#define SCREENDPIGUARD_H

#include <QObject>
#include <QSize>

class QWidget;

// ============================================================
// 跨屏尺寸保护
// ------------------------------------------------------------
// 两块显示器缩放比例不同时（比如主屏 100%、副屏 200%），
// 窗口从一块屏拖到另一块屏，Windows 会按新比例改写窗口大小。
// Qt 5.14 有时会把改写后的尺寸当成"用户设定的大小"再乘一次，
// 结果窗口越拖越大（反过来也会越拖越小）。
//
// 用法：窗口构造完最后 new 一个就行：
//     new ScreenDpiGuard(this);
//
// 它会：
//   ① 记住窗口第一次显示时的逻辑尺寸和当时的屏幕缩放比
//   ② 只有缩放比没变（说明是用户自己拉伸）时才更新记忆
//   ③ 换屏后把窗口恢复成记住的尺寸
//   ④ 万一窗口被摆到所有屏幕之外，拉回所在屏幕居中
// ============================================================
class ScreenDpiGuard : public QObject
{
public:
    explicit ScreenDpiGuard(QWidget *window);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void onShown();               // 窗口显示：记录基准尺寸、监听换屏
    void onResized();             // 窗口大小变化：判断是否要记住新尺寸
    void scheduleDpiCheck();      // 窗口移动后检查是否换了缩放比例
    void restoreLogicalSize();    // 把窗口改回记住的逻辑尺寸
    void forceRestore();          // 普通 resize 无效时，隐藏→重设→显示强同步
    void keepInsideScreen();      // 窗口跑到所有屏幕外时拉回来
    qreal currentDpr() const;     // 当前窗口所在屏幕的缩放比

    QWidget *m_window = nullptr;
    QSize    m_logicalSize;       // 用户期望的窗口大小（Qt 逻辑像素）
    qreal    m_logicalDpr = 0.0;  // 记录这个尺寸时屏幕的缩放比
    bool     m_ready = false;     // 是否已经记录过基准尺寸
    bool     m_adjusting = false; // 正在校正，忽略这次 resize
    bool     m_hooked = false;    // 是否已经监听换屏信号
    bool     m_checkPending = false;  // 已经排好一次 DPI 检查，避免重复排
};

#endif // SCREENDPIGUARD_H
