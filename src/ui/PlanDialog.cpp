#include "PlanDialog.h"

#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QDateTime>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QTimeEdit>
#include <QVBoxLayout>

#include "../core/BreathTypes.h"
#include "../core/StretchLib.h"
#include "ScreenDpiGuard.h"   // 跨屏保护：防止窗口换屏后被系统放大

namespace {
// 把星期列表变成人能读的文字。
// repeat 为空 = 每天都做；周一到周五齐了就叫"工作日"
QString repeatText(const QStringList &r)
{
    if (r.isEmpty() || r.size() >= 7) return QStringLiteral("每天");

    if (r.size() == 5 && r.contains(QStringLiteral("1"))
        && r.contains(QStringLiteral("2")) && r.contains(QStringLiteral("3"))
        && r.contains(QStringLiteral("4")) && r.contains(QStringLiteral("5")))
        return QStringLiteral("工作日");

    const char *names[] = {"", "周一", "周二", "周三", "周四", "周五", "周六", "周日"};
    QStringList parts;
    for (int d = 1; d <= 7; ++d) {
        if (r.contains(QString::number(d)))
            parts << QString::fromUtf8(names[d]);
    }
    return parts.join(QStringLiteral("、"));
}

// 计划项 → 中文名。
// kind=breath 是呼吸练习，kind=stretch 是身体放松动作
QString actionName(const QString &kind, const QString &code)
{
    if (kind == QStringLiteral("breath")) {
        const BreathType b = BreathLib::find(code);
        return QStringLiteral("呼吸 · %1").arg(b.name.isEmpty() ? code : b.name);
    }
    const StretchAction a = StretchLib::find(code);
    return a.name.isEmpty() ? code : a.name;
}

// 编辑一条计划的小对话框。点确定返回 true，并把结果写回 plan。
bool editPlan(QWidget *parent, AppSettings::RelaxPlan &plan)
{
    QDialog dlg(parent);
    dlg.setWindowTitle(QStringLiteral("编辑计划"));
    dlg.setMinimumWidth(380);

    auto *root = new QVBoxLayout(&dlg);
    root->setContentsMargins(18, 16, 18, 14);
    root->setSpacing(12);

    // ---------- 时间和动作 ----------
    auto *form = new QFormLayout;
    form->setSpacing(10);

    auto *timeEdit = new QTimeEdit(&dlg);
    timeEdit->setDisplayFormat(QStringLiteral("HH:mm"));
    timeEdit->setTime(QTime::fromString(plan.time, QStringLiteral("HH:mm")));
    form->addRow(QStringLiteral("时间"), timeEdit);

    auto *actBox = new QComboBox(&dlg);
    // 下拉里放两类：身体放松动作 + 呼吸练习。
    // data 存 "kind:code"，保存时拆开分别写进计划。
    for (const StretchAction &a : StretchLib::all())
        actBox->addItem(QStringLiteral("【放松】%1").arg(a.name),
                        QStringLiteral("stretch:%1").arg(a.code));
    for (const BreathType &b : BreathLib::all())
        actBox->addItem(QStringLiteral("【呼吸】%1").arg(b.name),
                        QStringLiteral("breath:%1").arg(b.code));
    const QString curKey = plan.kind + QLatin1Char(':') + plan.action;
    const int actIdx = actBox->findData(curKey);
    if (actIdx >= 0) actBox->setCurrentIndex(actIdx);
    form->addRow(QStringLiteral("内容"), actBox);

    root->addLayout(form);

    // ---------- 星期几 ----------
    root->addWidget(new QLabel(QStringLiteral("哪几天做（一个都不选 = 每天）"), &dlg));
    auto *daysRow = new QHBoxLayout;
    daysRow->setSpacing(6);
    QList<QCheckBox *> dayBoxes;
    const char *dayNames[] = {"一", "二", "三", "四", "五", "六", "日"};
    for (int d = 1; d <= 7; ++d) {
        auto *cb = new QCheckBox(QString::fromUtf8(dayNames[d - 1]), &dlg);
        cb->setChecked(plan.repeat.contains(QString::number(d)));
        dayBoxes << cb;
        daysRow->addWidget(cb);
    }
    root->addLayout(daysRow);

    // ---------- 启用 ----------
    auto *enabledBox = new QCheckBox(QStringLiteral("启用这条计划"), &dlg);
    enabledBox->setChecked(plan.enabled);
    root->addWidget(enabledBox);

    // ---------- 确定 / 取消 ----------
    auto *btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    root->addWidget(btns);
    QObject::connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted) return false;

    plan.time = timeEdit->time().toString(QStringLiteral("HH:mm"));
    // 把 "kind:code" 拆成类型和代号
    const QString key = actBox->currentData().toString();
    const int sep = key.indexOf(QLatin1Char(':'));
    if (sep > 0) {
        plan.kind   = key.left(sep);
        plan.action = key.mid(sep + 1);
    }
    plan.enabled = enabledBox->isChecked();
    plan.repeat.clear();
    for (int d = 1; d <= 7; ++d) {
        if (dayBoxes.at(d - 1)->isChecked())
            plan.repeat << QString::number(d);
    }
    return true;
}
} // namespace

PlanDialog::PlanDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("定时放松计划"));
    resize(580, 400);
    // 定时计划里有带 Stretch 列的表格，跨屏（不同缩放比例）时
    // Qt 5.14 会把窗口逻辑尺寸错误地缩小一半。
    // 这里锁住最小尺寸，防止它被缩得离谱。
    setMinimumSize(560, 380);

    // 拖到另一块缩放比例不同的屏幕时，恢复成这里的窗口大小；
    // 万一被摆到所有屏幕之外，也会自动拉回来
    new ScreenDpiGuard(this);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(18, 16, 18, 14);
    root->setSpacing(10);

    auto *hint = new QLabel(
        QStringLiteral("到点会弹出提醒，点「开始」直接进入跟练；不想练点「跳过」就行。"), this);
    hint->setWordWrap(true);
    hint->setStyleSheet(QStringLiteral("color:#6B655C; font-size:12px;"));
    root->addWidget(hint);

    // 拿一份当前计划的副本，编辑期间只改副本，点"保存"才真正写回
    m_plans = AppSettings::relaxPlans();

    // ---------- 计划列表 ----------
    m_table = new QTableWidget(this);
    m_table->setColumnCount(4);
    m_table->setHorizontalHeaderLabels({QStringLiteral("时间"), QStringLiteral("内容"),
                                        QStringLiteral("重复"), QStringLiteral("状态")});
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_table->verticalHeader()->setVisible(false);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    root->addWidget(m_table, 1);

    // ---------- 添加 / 编辑 / 删除 ----------
    auto *ops = new QHBoxLayout;
    auto *addBtn = new QPushButton(QStringLiteral("添加计划"), this);
    auto *editBtn = new QPushButton(QStringLiteral("编辑"), this);
    auto *delBtn = new QPushButton(QStringLiteral("删除"), this);
    ops->addWidget(addBtn);
    ops->addWidget(editBtn);
    ops->addWidget(delBtn);
    ops->addStretch();
    root->addLayout(ops);

    // ---------- 保存 / 取消 ----------
    auto *bottom = new QHBoxLayout;
    bottom->addStretch();
    auto *cancelBtn = new QPushButton(QStringLiteral("取消"), this);
    auto *saveBtn = new QPushButton(QStringLiteral("保存"), this);
    saveBtn->setDefault(true);
    bottom->addWidget(cancelBtn);
    bottom->addWidget(saveBtn);
    root->addLayout(bottom);

    connect(addBtn, &QPushButton::clicked, this, &PlanDialog::onAdd);
    connect(editBtn, &QPushButton::clicked, this, &PlanDialog::onEdit);
    connect(delBtn, &QPushButton::clicked, this, &PlanDialog::onDelete);
    connect(m_table, &QTableWidget::doubleClicked, this, &PlanDialog::onEdit);
    connect(saveBtn, &QPushButton::clicked, this, &PlanDialog::onSave);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

    reloadTable();
}

// 把 m_plans 重新填进表格
void PlanDialog::reloadTable()
{
    m_table->setRowCount(m_plans.size());
    for (int i = 0; i < m_plans.size(); ++i) {
        const AppSettings::RelaxPlan &p = m_plans.at(i);
        m_table->setItem(i, 0, new QTableWidgetItem(p.time));
        m_table->setItem(i, 1, new QTableWidgetItem(actionName(p.kind, p.action)));
        m_table->setItem(i, 2, new QTableWidgetItem(repeatText(p.repeat)));
        auto *st = new QTableWidgetItem(p.enabled ? QStringLiteral("启用")
                                                  : QStringLiteral("停用"));
        // 停用的行用浅灰字，一眼能区分
        if (!p.enabled) st->setForeground(QColor(QStringLiteral("#9A948A")));
        m_table->setItem(i, 3, st);
    }
}

int PlanDialog::currentRow() const
{
    return m_table->currentRow();
}

void PlanDialog::onAdd()
{
    AppSettings::RelaxPlan p;
    // id 用时间戳保证唯一，删了再加也不会撞
    p.id = QStringLiteral("plan_%1").arg(QDateTime::currentMSecsSinceEpoch());
    p.time = QStringLiteral("10:00");
    p.kind = QStringLiteral("stretch");          // 默认先选身体放松
    p.action = QStringLiteral("shoulder_circle");
    p.enabled = true;

    if (!editPlan(this, p)) return;
    m_plans << p;
    reloadTable();
    m_table->selectRow(m_plans.size() - 1);
}

void PlanDialog::onEdit()
{
    const int row = currentRow();
    if (row < 0) {
        QMessageBox::information(this, windowTitle(),
                                 QStringLiteral("先选中一条计划，再点编辑。"));
        return;
    }
    if (!editPlan(this, m_plans[row])) return;
    reloadTable();
    m_table->selectRow(row);
}

void PlanDialog::onDelete()
{
    const int row = currentRow();
    if (row < 0) {
        QMessageBox::information(this, windowTitle(),
                                 QStringLiteral("先选中一条计划，再点删除。"));
        return;
    }
    m_plans.removeAt(row);
    reloadTable();
}

void PlanDialog::onSave()
{
    AppSettings::setRelaxPlans(m_plans);
    accept();
}
