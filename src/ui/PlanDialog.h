#ifndef PLANDIALOG_H
#define PLANDIALOG_H

#include <QDialog>
#include <QList>

#include "../core/AppSettings.h"

class QTableWidget;

// ============================================================
// 定时放松计划对话框
// ------------------------------------------------------------
// 让用户自己安排"几点做什么动作、哪几天做"。
// 列表里每条计划一行，可以添加 / 编辑 / 删除，点"保存"写入设置。
//
// 数据直接复用 AppSettings::RelaxPlan，
// 到点提醒由 main.cpp 里的定时扫描负责，这里只管编辑。
// ============================================================
class PlanDialog : public QDialog
{
    Q_OBJECT
public:
    explicit PlanDialog(QWidget *parent = nullptr);

private slots:
    void onAdd();
    void onEdit();
    void onDelete();
    void onSave();

private:
    void reloadTable();          // 把 m_plans 重新填进表格
    int currentRow() const;      // 当前选中的行，没选中返回 -1

    QTableWidget *m_table = nullptr;
    QList<AppSettings::RelaxPlan> m_plans;   // 编辑中的副本，保存时才写回
};

#endif // PLANDIALOG_H
