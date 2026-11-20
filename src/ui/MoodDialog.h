#ifndef MOODDIALOG_H
#define MOODDIALOG_H

#include <QDialog>

#include "../core/MoodLib.h"

class QGridLayout;
class QLabel;
class QStackedWidget;
class QWidget;

// ============================================================
// 情绪陪伴窗口
// ------------------------------------------------------------
// 两页：
//   第 1 页 —— 选一个此刻的感受（内置 6 类 + 用户自定义）
//   第 2 页 —— 显示那一整段话，让用户自己慢慢读一遍
//
// 只做引导，不记录用户选了哪种情绪（避免反刍）。
// ============================================================
class MoodDialog : public QDialog
{
    Q_OBJECT
public:
    explicit MoodDialog(QWidget *parent = nullptr);

private:
    void buildPickerPage();
    void buildReadPage();
    void rebuildGrid();                 // 按最新数据重排"类型"按钮
    void showEntry(const MoodEntry &e); // 跳到阅读页
    void addCustomEntry();              // 弹出"添加我自己的"小窗

    QStackedWidget *m_stack = nullptr;
    QGridLayout *m_grid = nullptr;
    QLabel *m_readTitle = nullptr;
    QLabel *m_readText = nullptr;
};

#endif // MOODDIALOG_H
