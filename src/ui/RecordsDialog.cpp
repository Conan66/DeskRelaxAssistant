#include "RecordsDialog.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QVBoxLayout>

#include "../core/AppSettings.h"

namespace {

// ============================================================
// 最近 14 天的小柱状图（自绘）
// ------------------------------------------------------------
// 每根柱子 = 那天做了几次。今天的柱子用绿色高亮，
// 没做的日子画一根很矮的浅灰条，表示"这天是空的"。
// ============================================================
class BarChart : public QWidget
{
public:
    explicit BarChart(QWidget *parent = nullptr) : QWidget(parent)
    {
        setMinimumHeight(120);
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);

        // 取最近 14 天的数据（索引 0 = 最早，最后 = 今天）
        const QList<int> counts = AppSettings::recentDayCounts(14);
        if (counts.isEmpty()) return;

        // 找最大值，用来做柱子高度比例；全是 0 的时候按 1 处理避免除零
        int maxCount = 1;
        for (int c : counts) maxCount = qMax(maxCount, c);

        const int left = 8;
        const int right = width() - 8;
        // 顶部多留一点空间：柱子上方还要标数字，
        // 留太少的话最高那根柱子（通常是"今天"）的数字会被裁掉一截。
        const int top = 22;
        const int bottom = height() - 24;
        const int n = counts.size();

        // 每根柱子占的宽度（留出间距）
        const double slot = double(right - left) / n;
        const double barW = slot * 0.55;

        QFont f = font();
        f.setPixelSize(10);
        p.setFont(f);

        for (int i = 0; i < n; ++i) {
            const double cx = left + slot * i + slot / 2.0;
            const int cnt = counts.at(i);

            // 柱子高度：有数据按比例，没数据留 3px 的"底座"
            const double h = (cnt > 0)
                ? qMax(6.0, (bottom - top) * (double(cnt) / maxCount))
                : 3.0;
            const QRectF bar(cx - barW / 2.0, bottom - h, barW, h);

            const bool isToday = (i == n - 1);
            p.setPen(Qt::NoPen);
            p.setBrush(isToday ? QColor(QStringLiteral("#4F8E73"))
                               : (cnt > 0 ? QColor(QStringLiteral("#9CC3B0"))
                                          : QColor(QStringLiteral("#E3DED5"))));
            p.drawRoundedRect(bar, 3, 3);

            // 有次数的日子在柱子上方标数字
            if (cnt > 0) {
                p.setPen(QColor(QStringLiteral("#6B655C")));
                p.drawText(QRectF(cx - slot / 2.0, bottom - h - 16, slot, 14),
                           Qt::AlignCenter, QString::number(cnt));
            }
        }

        // 底部标两个日期：最左"14 天前"，最右"今天"
        p.setPen(QColor(QStringLiteral("#9A948A")));
        p.drawText(QRectF(left, bottom + 4, slot * 3, 16),
                   Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("14 天前"));
        p.drawText(QRectF(right - slot * 3, bottom + 4, slot * 3, 16),
                   Qt::AlignRight | Qt::AlignVCenter, QStringLiteral("今天"));
    }
};

} // namespace

RecordsDialog::RecordsDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("我的坚持"));
    resize(460, 330);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(22, 20, 22, 16);
    root->setSpacing(16);

    // ---------- 三个大数字 ----------
    auto *statsRow = new QHBoxLayout;
    statsRow->setSpacing(10);

    // 小工具：做一个"数字 + 说明"的竖排块
    auto makeStat = [this](const QString &value, const QString &label) {
        auto *box = new QVBoxLayout;
        auto *v = new QLabel(value, this);
        v->setAlignment(Qt::AlignCenter);
        v->setStyleSheet(QStringLiteral(
            "font-size:26px; font-weight:bold; color:#2E5E4B;"));
        auto *l = new QLabel(label, this);
        l->setAlignment(Qt::AlignCenter);
        l->setStyleSheet(QStringLiteral("font-size:12px; color:#8A857C;"));
        box->addWidget(v);
        box->addWidget(l);
        return box;
    };

    const int today = AppSettings::todayTotalCount();
    const int streak = AppSettings::streakDays();
    const int total = AppSettings::totalCount();

    statsRow->addLayout(makeStat(QString::number(today), QStringLiteral("今天做了")));
    statsRow->addLayout(makeStat(QString::number(streak), QStringLiteral("连续天数")));
    statsRow->addLayout(makeStat(QString::number(total), QStringLiteral("累计次数")));
    root->addLayout(statsRow);

    // ---------- 14 天柱状图 ----------
    auto *chartTitle = new QLabel(QStringLiteral("最近 14 天"), this);
    chartTitle->setStyleSheet(QStringLiteral("font-size:12px; color:#6B655C;"));
    root->addWidget(chartTitle);
    root->addWidget(new BarChart(this), 1);

    // ---------- 说明 ----------
    auto *hint = new QLabel(
        QStringLiteral("记录按天汇总，永久保存。做完一套动作或一轮呼吸才计一次。"), this);
    hint->setWordWrap(true);
    hint->setStyleSheet(QStringLiteral("font-size:11px; color:#9A948A;"));
    root->addWidget(hint);

    // ---------- 关闭 ----------
    auto *bottom = new QHBoxLayout;
    bottom->addStretch();
    auto *closeBtn = new QPushButton(QStringLiteral("关闭"), this);
    closeBtn->setDefault(true);
    bottom->addWidget(closeBtn);
    root->addLayout(bottom);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
}
