#include "SitReminderDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSpinBox>
#include <QTimeEdit>
#include <QVBoxLayout>

#include "../core/AppSettings.h"

SitReminderDialog::SitReminderDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("久坐提醒设置"));
    setMinimumWidth(440);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(20, 18, 20, 16);
    root->setSpacing(12);

    // ---------- 总开关 ----------
    m_enabled = new QCheckBox(QStringLiteral("开启久坐提醒"), this);
    root->addWidget(m_enabled);

    // ---------- 开机自启 ----------
    // 和久坐总开关分开：就算关掉提醒，也可能希望球开机就在
    m_autoStart = new QCheckBox(QStringLiteral("开机时自动启动"), this);
    root->addWidget(m_autoStart);

    // ---------- 时长和方式 ----------
    auto *form = new QFormLayout();
    form->setSpacing(10);

    m_sitMinutes = new QSpinBox(this);
    m_sitMinutes->setRange(20, 120);
    m_sitMinutes->setSuffix(QStringLiteral(" 分钟"));

    m_standMinutes = new QSpinBox(this);
    m_standMinutes->setRange(1, 30);
    m_standMinutes->setSuffix(QStringLiteral(" 分钟"));

    m_style = new QComboBox(this);
    m_style->addItem(QStringLiteral("全屏遮罩（醒目，推荐）"), 0);
    m_style->addItem(QStringLiteral("球旁小气泡（温和，不打断）"), 1);

    form->addRow(QStringLiteral("坐着多久提醒一次"), m_sitMinutes);
    form->addRow(QStringLiteral("提醒后活动多久"), m_standMinutes);
    form->addRow(QStringLiteral("提醒方式"), m_style);
    root->addLayout(form);

    // ---------- 离开检测的说明 ----------
    auto *detectHint = new QLabel(QStringLiteral(
        "离开检测：锁屏、电脑休眠或合盖后，会自动重新计时，不需要手动点「我离开了」。\n"
        "看视频、等程序跑完、低头看手机时鼠标不动，不会被误判成离开。"), this);
    detectHint->setWordWrap(true);
    detectHint->setStyleSheet(QStringLiteral("color:#8A8578; font-size:12px;"));
    root->addWidget(detectHint);

    // ---------- 免打扰时段 ----------
    auto *group = new QGroupBox(QStringLiteral("免打扰时段（可添加多段）"), this);
    auto *gv = new QVBoxLayout(group);
    gv->setSpacing(8);

    m_periodList = new QListWidget(group);
    m_periodList->setMinimumHeight(90);
    gv->addWidget(m_periodList);

    auto *editRow = new QHBoxLayout();
    editRow->addWidget(new QLabel(QStringLiteral("从"), group));
    m_from = new QTimeEdit(QTime(12, 0), group);
    m_from->setDisplayFormat(QStringLiteral("HH:mm"));
    editRow->addWidget(m_from);
    editRow->addWidget(new QLabel(QStringLiteral("到"), group));
    m_to = new QTimeEdit(QTime(13, 30), group);
    m_to->setDisplayFormat(QStringLiteral("HH:mm"));
    editRow->addWidget(m_to);

    auto *addBtn = new QPushButton(QStringLiteral("添加"), group);
    auto *delBtn = new QPushButton(QStringLiteral("删除选中"), group);
    editRow->addWidget(addBtn);
    editRow->addWidget(delBtn);
    editRow->addStretch();
    gv->addLayout(editRow);

    auto *periodHint = new QLabel(QStringLiteral(
        "时段内不会提醒；开始时间比结束时间晚表示跨夜（比如 18:30-09:00）。"
        "时段结束后会重新计满。"), group);
    periodHint->setWordWrap(true);
    periodHint->setStyleSheet(QStringLiteral("color:#8A8578; font-size:12px;"));
    gv->addWidget(periodHint);

    root->addWidget(group);

    // ---------- 状态提示 ----------
    m_status = new QLabel(this);
    m_status->setStyleSheet(QStringLiteral("color:#5F9E83; font-size:12px;"));
    root->addWidget(m_status);

    // ---------- 底部按钮 ----------
    auto *btnRow = new QHBoxLayout();
    auto *closeBtn = new QPushButton(QStringLiteral("关闭"), this);
    auto *saveBtn = new QPushButton(QStringLiteral("保存"), this);
    saveBtn->setDefault(true);
    btnRow->addStretch();
    btnRow->addWidget(closeBtn);
    btnRow->addWidget(saveBtn);
    root->addLayout(btnRow);

    connect(addBtn, &QPushButton::clicked, this, &SitReminderDialog::onAddPeriod);
    connect(delBtn, &QPushButton::clicked, this, &SitReminderDialog::onRemovePeriod);
    connect(saveBtn, &QPushButton::clicked, this, &SitReminderDialog::onSave);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);

    // 关掉总开关时，其他选项变灰，一眼知道"现在不生效"
    connect(m_enabled, &QCheckBox::toggled, this, [this, group](bool on) {
        m_sitMinutes->setEnabled(on);
        m_standMinutes->setEnabled(on);
        m_style->setEnabled(on);
        group->setEnabled(on);
    });

    loadCurrent();
}

// 把已保存的设置填进界面
void SitReminderDialog::loadCurrent()
{
    m_enabled->setChecked(AppSettings::sitReminderEnabled());
    m_autoStart->setChecked(AppSettings::autoStart());
    m_sitMinutes->setValue(AppSettings::sitMinutes());
    m_standMinutes->setValue(AppSettings::standMinutes());

    const int style = AppSettings::sitReminderStyle();
    m_style->setCurrentIndex(m_style->findData(style) >= 0 ? m_style->findData(style) : 0);

    m_periodList->clear();
    m_periodList->addItems(AppSettings::skipPeriods());
}

// 添加一段时段
void SitReminderDialog::onAddPeriod()
{
    const QString from = m_from->time().toString(QStringLiteral("HH:mm"));
    const QString to   = m_to->time().toString(QStringLiteral("HH:mm"));

    if (from == to) {
        m_status->setText(QStringLiteral("开始和结束时间不能一样"));
        return;
    }

    const QString one = QStringLiteral("%1-%2").arg(from, to);

    // 已经有一模一样的就不重复添加
    if (!m_periodList->findItems(one, Qt::MatchExactly).isEmpty()) {
        m_status->setText(QStringLiteral("这一段已经存在了"));
        return;
    }

    m_periodList->addItem(one);
    m_status->setText(QStringLiteral("已添加，记得点「保存」"));
}

// 删除选中的时段
void SitReminderDialog::onRemovePeriod()
{
    QListWidgetItem *item = m_periodList->currentItem();
    if (!item) {
        m_status->setText(QStringLiteral("先在列表里点一段再删"));
        return;
    }
    delete m_periodList->takeItem(m_periodList->row(item));
    m_status->setText(QStringLiteral("已删除，记得点「保存」"));
}

void SitReminderDialog::savePeriods()
{
    QStringList list;
    for (int i = 0; i < m_periodList->count(); ++i) {
        list << m_periodList->item(i)->text();
    }
    AppSettings::setSkipPeriods(list);
}

void SitReminderDialog::onSave()
{
    AppSettings::setSitReminderEnabled(m_enabled->isChecked());
    AppSettings::setSitMinutes(m_sitMinutes->value());
    AppSettings::setStandMinutes(m_standMinutes->value());
    AppSettings::setSitReminderStyle(m_style->currentData().toInt());
    savePeriods();

    // 开机自启是写注册表的，立刻生效（下次开机会按它来）
    AppSettings::setAutoStart(m_autoStart->isChecked());

    m_status->setText(QStringLiteral("已保存，新的计时现在就生效"));
}
