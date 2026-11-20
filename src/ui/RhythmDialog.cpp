#include "RhythmDialog.h"

#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include "../core/AppSettings.h"
#include "../core/BreathTypes.h"

RhythmDialog::RhythmDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("呼吸节奏设置"));
    setMinimumWidth(400);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(20, 18, 20, 16);
    root->setSpacing(12);

    // ---------- 选择呼吸 ----------
    m_typeBox = new QComboBox(this);
    const QVector<BreathType> list = BreathLib::all();
    for (const BreathType &t : list) {
        m_typeBox->addItem(t.name, t.code);
    }
    root->addWidget(new QLabel(QStringLiteral("选择呼吸方式"), this));
    root->addWidget(m_typeBox);

    // ---------- 三个秒数 ----------
    auto *form = new QFormLayout();
    form->setSpacing(10);

    m_inhale = new QSpinBox(this);
    m_inhale->setRange(2, 8);
    m_inhale->setSuffix(QStringLiteral(" 秒"));

    m_hold = new QSpinBox(this);
    m_hold->setRange(0, 15);
    m_hold->setSuffix(QStringLiteral(" 秒"));
    m_hold->setSpecialValueText(QStringLiteral("0 秒（不屏息）"));

    // 第二次屏息：箱式呼吸一轮有两次（吸-屏-呼-屏），第二次单独调
    m_hold2Label = new QLabel(QStringLiteral("第二次屏息"), this);
    m_hold2 = new QSpinBox(this);
    m_hold2->setRange(0, 15);
    m_hold2->setSuffix(QStringLiteral(" 秒"));
    m_hold2->setSpecialValueText(QStringLiteral("0 秒（不屏息）"));

    m_exhale = new QSpinBox(this);
    m_exhale->setRange(3, 12);
    m_exhale->setSuffix(QStringLiteral(" 秒"));

    form->addRow(QStringLiteral("吸气时长"), m_inhale);
    form->addRow(QStringLiteral("屏息时长"), m_hold);
    form->addRow(m_hold2Label, m_hold2);
    form->addRow(QStringLiteral("呼气时长"), m_exhale);
    root->addLayout(form);

    // ---------- 预览 ----------
    m_preview = new QLabel(this);
    m_preview->setStyleSheet(QStringLiteral("color:#8A8578; font-size:12px;"));
    root->addWidget(m_preview);

    // ---------- 按钮 ----------
    auto *btnRow = new QHBoxLayout();
    auto *resetBtn = new QPushButton(QStringLiteral("恢复默认"), this);
    auto *saveBtn = new QPushButton(QStringLiteral("保存"), this);
    auto *closeBtn = new QPushButton(QStringLiteral("关闭"), this);
    saveBtn->setDefault(true);

    btnRow->addWidget(resetBtn);
    btnRow->addStretch();
    btnRow->addWidget(closeBtn);
    btnRow->addWidget(saveBtn);
    root->addLayout(btnRow);

    connect(m_typeBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &RhythmDialog::onTypeChanged);
    connect(m_inhale, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &RhythmDialog::onValueChanged);
    connect(m_hold, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &RhythmDialog::onValueChanged);
    connect(m_exhale, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &RhythmDialog::onValueChanged);
    connect(m_hold2, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &RhythmDialog::onValueChanged);
    connect(saveBtn, &QPushButton::clicked, this, &RhythmDialog::onSave);
    connect(resetBtn, &QPushButton::clicked, this, &RhythmDialog::onReset);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);

    loadCurrent();
}

// 把当前选中呼吸的参数填进控件
void RhythmDialog::loadCurrent()
{
    const QString code = m_typeBox->currentData().toString();
    const BreathType type = BreathLib::find(code);
    const AppSettings::Rhythm r = AppSettings::rhythmFor(type);

    // blockSignals 避免 setValue 触发 onValueChanged 把预览刷乱
    m_inhale->blockSignals(true);
    m_hold->blockSignals(true);
    m_exhale->blockSignals(true);
    m_hold2->blockSignals(true);
    m_inhale->setValue(r.inhale);
    m_hold->setValue(r.hold);
    m_exhale->setValue(r.exhale);
    m_hold2->setValue(r.hold2);
    m_inhale->blockSignals(false);
    m_hold->blockSignals(false);
    m_exhale->blockSignals(false);
    m_hold2->blockSignals(false);

    // 只有一轮里出现两次屏息的呼吸才显示"第二次屏息"
    int holdCount = 0;
    for (const BreathPhase &p : type.phases) {
        if (p.type == PhaseType::Hold) ++holdCount;
    }
    const bool twoHolds = (holdCount >= 2);
    m_hold2Label->setVisible(twoHolds);
    m_hold2->setVisible(twoHolds);

    // 固定配方的呼吸（4-7-8）：比例本身就是它有效的原因，
    // 所以三个秒数框灰掉，只显示一句说明，不让改。
    m_locked = type.fixedRhythm;
    m_inhale->setEnabled(!m_locked);
    m_hold->setEnabled(!m_locked);
    m_exhale->setEnabled(!m_locked);
    m_hold2->setEnabled(!m_locked);

    if (m_locked) {
        // 固定配方不显示"一轮多少秒"的预览，改显示不能改的原因
        m_preview->setText(QStringLiteral(
            "%1 的秒数是固定配方，不能修改：改了比例就不是这个练习了。")
                               .arg(type.name));
        return;
    }

    onValueChanged();
}

void RhythmDialog::onTypeChanged()
{
    loadCurrent();
}

// 实时更新"一轮多少秒、总共几分钟"
void RhythmDialog::onValueChanged()
{
    // 固定配方（4-7-8）不跟着控件值变化，避免显示一个"假的可调时长"
    if (m_locked) return;

    // 一轮总秒数：两次屏息的呼吸要把第二次也算进去
    int cycle = m_inhale->value() + m_hold->value() + m_exhale->value();
    if (!m_hold2->isHidden()) cycle += m_hold2->value();
    const QString code = m_typeBox->currentData().toString();
    const BreathType type = BreathLib::find(code);
    const int rounds = type.defaultRounds;
    const int total = cycle * rounds;

    m_preview->setText(
        QStringLiteral("一轮 %1 秒 × %2 轮 ≈ %3 分 %4 秒")
            .arg(cycle)
            .arg(rounds)
            .arg(total / 60)
            .arg(total % 60));
}

void RhythmDialog::onSave()
{
    // 固定配方（4-7-8）没有可保存的东西，直接提示一下
    if (m_locked) {
        m_preview->setText(QStringLiteral("这一种是固定配方，不需要保存。"));
        return;
    }

    AppSettings::Rhythm r;
    r.inhale = m_inhale->value();
    r.hold = m_hold->value();
    r.exhale = m_exhale->value();
    r.hold2 = m_hold2->value();

    AppSettings::saveRhythm(m_typeBox->currentData().toString(), r);

    m_preview->setText(m_preview->text() + QStringLiteral("   ✓ 已保存"));
}

void RhythmDialog::onReset()
{
    if (m_locked) return;   // 固定配方没有"默认值"可恢复

    AppSettings::resetRhythm(m_typeBox->currentData().toString());
    loadCurrent();
    m_preview->setText(m_preview->text() + QStringLiteral("   已恢复默认"));
}
