#ifndef RHYTHMDIALOG_H
#define RHYTHMDIALOG_H

#include <QDialog>

class QComboBox;
class QSpinBox;
class QLabel;

// ============================================================
// 节奏设置对话框
// ------------------------------------------------------------
// PRD FR-22：每种呼吸可以单独调「吸 / 屏 / 呼」秒数。
// 12 种各自记忆，互不影响；也可以一键恢复科学默认值。
// ============================================================
class RhythmDialog : public QDialog
{
    Q_OBJECT
public:
    explicit RhythmDialog(QWidget *parent = nullptr);

private slots:
    void onTypeChanged();
    void onSave();
    void onReset();
    void onValueChanged();

private:
    void loadCurrent();

    QComboBox *m_typeBox = nullptr;
    QSpinBox *m_inhale = nullptr;
    QSpinBox *m_hold = nullptr;
    QSpinBox *m_exhale = nullptr;
    // 第二次屏息：只有箱式这种"吸-屏-呼-屏"的呼吸才显示
    QSpinBox *m_hold2 = nullptr;
    QLabel *m_hold2Label = nullptr;
    QLabel *m_preview = nullptr;

    // true = 当前这种呼吸的秒数是固定配方（4-7-8），不让改
    bool m_locked = false;
};

#endif // RHYTHMDIALOG_H
