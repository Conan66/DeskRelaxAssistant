#ifndef SITREMINDERDIALOG_H
#define SITREMINDERDIALOG_H

#include <QDialog>

class QCheckBox;
class QComboBox;
class QLabel;
class QListWidget;
class QPushButton;
class QSpinBox;
class QTimeEdit;

// ============================================================
// 久坐提醒设置
// ------------------------------------------------------------
// 坐多久提醒、提醒后活动多久、提醒方式、免打扰时段（可多段）。
// ============================================================
class SitReminderDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SitReminderDialog(QWidget *parent = nullptr);

private slots:
    void onAddPeriod();
    void onRemovePeriod();
    void onSave();

private:
    void loadCurrent();
    // 把界面上的时段列表存进设置
    void savePeriods();

    QCheckBox *m_enabled = nullptr;
    QCheckBox *m_autoStart = nullptr;    // 开机自启
    QSpinBox *m_sitMinutes = nullptr;
    QSpinBox *m_standMinutes = nullptr;
    QComboBox *m_style = nullptr;

    QListWidget *m_periodList = nullptr;
    QTimeEdit *m_from = nullptr;
    QTimeEdit *m_to = nullptr;

    QLabel *m_status = nullptr;
};

#endif // SITREMINDERDIALOG_H
