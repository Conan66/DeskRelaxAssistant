#ifndef RECORDSDIALOG_H
#define RECORDSDIALOG_H

#include <QDialog>

// ============================================================
// 「我的坚持」面板
// ------------------------------------------------------------
// 显示：今天做了几次、连续坚持几天、累计几次，
// 外加最近 14 天的小柱状图，让用户看到自己的坚持。
// 数据来自 AppSettings 的完成记录（按天汇总）。
// ============================================================
class RecordsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit RecordsDialog(QWidget *parent = nullptr);
};

#endif // RECORDSDIALOG_H
