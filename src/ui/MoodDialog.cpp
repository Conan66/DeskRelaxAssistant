#include "MoodDialog.h"

#include <QDialogButtonBox>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QTextCursor>
#include <QTextEdit>
#include <QVBoxLayout>

#include "ScreenDpiGuard.h"   // 跨屏保护：防止窗口换屏后被系统放大

namespace {
// 类型按钮的统一外观：白底浅绿描边，鼠标移上去变绿
const char *kBtnStyle =
    "QPushButton{background:#FFFFFF; border:1px solid #DCE6DF; border-radius:8px;"
    " padding:14px 8px; font-size:14px; color:#33302B;}"
    "QPushButton:hover{background:#F3F6F4; border-color:#4F8E73; color:#2E5E4B;}";
} // namespace

MoodDialog::MoodDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("情绪陪伴"));
    resize(640, 500);

    // 拖到另一块缩放比例不同的屏幕时，恢复成这里的窗口大小
    new ScreenDpiGuard(this);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);

    m_stack = new QStackedWidget(this);
    root->addWidget(m_stack);

    buildPickerPage();   // 第 0 页：选类型
    buildReadPage();     // 第 1 页：读那段话
    rebuildGrid();       // 按最新数据生成类型按钮
}

// ============================================================
// 第 1 页：选一个此刻的感受
// ============================================================
void MoodDialog::buildPickerPage()
{
    auto *page = new QWidget(this);
    auto *root = new QVBoxLayout(page);
    root->setContentsMargins(28, 24, 28, 18);
    root->setSpacing(6);

    auto *title = new QLabel(QStringLiteral("给自己一段话"), page);
    title->setStyleSheet(QStringLiteral(
        "font-size:20px; font-weight:bold; color:#2E5E4B;"));
    root->addWidget(title);

    auto *sub = new QLabel(
        QStringLiteral("选一个此刻最接近的感受，不用想太多，凭直觉就好。"), page);
    sub->setStyleSheet(QStringLiteral("font-size:12px; color:#8A857C;"));
    root->addWidget(sub);

    root->addSpacing(14);

    // 类型按钮放在网格里，3 列
    auto *gridWrap = new QWidget(page);
    m_grid = new QGridLayout(gridWrap);
    m_grid->setContentsMargins(0, 0, 0, 0);
    m_grid->setSpacing(10);
    root->addWidget(gridWrap);

    root->addStretch();

    // 添加自定义类型（会一直保存，下次打开还在）
    auto *addBtn = new QPushButton(QStringLiteral("＋ 添加我自己的"), page);
    addBtn->setCursor(Qt::PointingHandCursor);
    addBtn->setStyleSheet(QStringLiteral(
        "QPushButton{background:transparent; border:1px dashed #C9D8CF;"
        " border-radius:8px; padding:10px 18px; font-size:13px; color:#4F8E73;}"
        "QPushButton:hover{background:#F3F6F4; border-color:#4F8E73;}"));
    connect(addBtn, &QPushButton::clicked, this, &MoodDialog::addCustomEntry);
    root->addWidget(addBtn, 0, Qt::AlignLeft);

    m_stack->addWidget(page);
}

// ============================================================
// 第 2 页：把那段话展示出来
// ============================================================
void MoodDialog::buildReadPage()
{
    auto *page = new QWidget(this);
    auto *root = new QVBoxLayout(page);
    root->setContentsMargins(28, 22, 28, 18);
    root->setSpacing(10);

    m_readTitle = new QLabel(page);
    m_readTitle->setStyleSheet(QStringLiteral(
        "font-size:15px; font-weight:bold; color:#2E5E4B;"));
    root->addWidget(m_readTitle);

    // 白卡片装着文字；文字太长时出现滚动条，不会把窗口撑爆
    auto *card = new QFrame(page);
    card->setStyleSheet(QStringLiteral(
        "QFrame{background:#FFFFFF; border:1px solid #E8E4DC; border-radius:10px;}"));
    auto *cardLay = new QVBoxLayout(card);
    cardLay->setContentsMargins(24, 20, 24, 20);

    auto *scroll = new QScrollArea(card);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet(QStringLiteral("QScrollArea{background:transparent;}"));

    m_readText = new QLabel(scroll);
    m_readText->setWordWrap(true);
    m_readText->setTextFormat(Qt::PlainText);
    m_readText->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_readText->setStyleSheet(QStringLiteral("font-size:15px; color:#3B3730;"));
    scroll->setWidget(m_readText);

    cardLay->addWidget(scroll);
    root->addWidget(card, 1);

    // 底部：换一个 / 读完了
    auto *bottom = new QHBoxLayout;
    bottom->addStretch();

    auto *backBtn = new QPushButton(QStringLiteral("换一个"), page);
    backBtn->setCursor(Qt::PointingHandCursor);
    backBtn->setStyleSheet(QStringLiteral(
        "QPushButton{background:#FBF9F4; border:1px solid #DCE6DF;"
        " border-radius:6px; padding:8px 20px; font-size:13px; color:#6B655C;}"
        "QPushButton:hover{background:#F3F6F4;}"));
    connect(backBtn, &QPushButton::clicked, this, [this]() {
        m_stack->setCurrentIndex(0);   // 回到类型选择
    });

    auto *doneBtn = new QPushButton(QStringLiteral("读完了"), page);
    doneBtn->setCursor(Qt::PointingHandCursor);
    doneBtn->setDefault(true);
    doneBtn->setStyleSheet(QStringLiteral(
        "QPushButton{background:#4F8E73; border:none; border-radius:6px;"
        " padding:8px 24px; font-size:13px; color:#FFFFFF;}"
        "QPushButton:hover{background:#437A63;}"));
    // 读完就直接关掉窗口，安静地结束
    connect(doneBtn, &QPushButton::clicked, this, &QDialog::accept);

    bottom->addWidget(backBtn);
    bottom->addWidget(doneBtn);
    root->addLayout(bottom);

    m_stack->addWidget(page);
}

// ============================================================
// 重新生成类型按钮（新增/删除自定义后调用）
// ============================================================
void MoodDialog::rebuildGrid()
{
    // 先把旧按钮清掉
    while (QLayoutItem *item = m_grid->takeAt(0)) {
        if (QWidget *w = item->widget()) w->deleteLater();
        delete item;
    }

    const QList<MoodEntry> list = MoodLib::all();
    const int kCols = 3;

    for (int i = 0; i < list.size(); ++i) {
        const MoodEntry e = list.at(i);   // 拷贝一份，lambda 里用着安全

        auto *b = new QPushButton(e.name, m_grid->parentWidget());
        b->setCursor(Qt::PointingHandCursor);
        b->setMinimumHeight(52);
        b->setStyleSheet(QLatin1String(kBtnStyle));
        connect(b, &QPushButton::clicked, this, [this, e]() { showEntry(e); });

        // 自定义类型支持右键删除（内置的不能删）
        if (!e.builtin) {
            b->setContextMenuPolicy(Qt::CustomContextMenu);
            connect(b, &QWidget::customContextMenuRequested, this,
                    [this, b, e](const QPoint &pos) {
                QMenu menu(this);
                QAction *del = menu.addAction(QStringLiteral("删除「%1」").arg(e.name));
                if (menu.exec(b->mapToGlobal(pos)) == del) {
                    MoodLib::removeCustom(e.id);
                    rebuildGrid();
                }
            });
        }

        m_grid->addWidget(b, i / kCols, i % kCols);
    }
}

// ============================================================
// 选好类型 → 显示那段话
// ============================================================
void MoodDialog::showEntry(const MoodEntry &e)
{
    m_readTitle->setText(QStringLiteral("此刻的你，辛苦了"));
    m_readText->setText(e.text);
    m_stack->setCurrentIndex(1);
}

// ============================================================
// 添加一条自定义话术
// ------------------------------------------------------------
// 名称最多 12 字、正文最多 500 字，超了直接截断并显示字数。
// 太长的内容用户也读不下去，反而失去"一段话"的意义。
// 这里的 500 字和 MoodLib::kMaxTextLen 保持一致。
// ============================================================
void MoodDialog::addCustomEntry()
{
    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("添加我自己的"));
    dlg.setMinimumWidth(480);

    auto *root = new QVBoxLayout(&dlg);
    root->setContentsMargins(22, 18, 22, 16);
    root->setSpacing(8);

    auto *nameLabel = new QLabel(
        QStringLiteral("给它起个名字（最多 12 字）"), &dlg);
    nameLabel->setStyleSheet(QStringLiteral("font-size:13px; color:#33302B;"));
    auto *nameEdit = new QLineEdit(&dlg);
    nameEdit->setMaxLength(12);
    nameEdit->setPlaceholderText(QStringLiteral("比如：被批评了、睡不着、想家"));
    root->addWidget(nameLabel);
    root->addWidget(nameEdit);

    root->addSpacing(6);

    auto *textLabel = new QLabel(
        QStringLiteral("写下想对那时的自己说的一段话（最多 200 字）"), &dlg);
    textLabel->setStyleSheet(QStringLiteral("font-size:13px; color:#33302B;"));
    auto *textEdit = new QTextEdit(&dlg);
    textEdit->setPlaceholderText(
        QStringLiteral("不要着急，这不是你的错。慢慢来……"));
    textEdit->setMinimumHeight(150);
    root->addWidget(textLabel);
    root->addWidget(textEdit);

    // 右下角的字数提示，超过 200 字直接把多出来的截掉
    auto *counter = new QLabel(QStringLiteral("0 / 500"), &dlg);
    counter->setAlignment(Qt::AlignRight);
    counter->setStyleSheet(QStringLiteral("font-size:11px; color:#9A948A;"));
    root->addWidget(counter);

    QObject::connect(textEdit, &QTextEdit::textChanged, [textEdit, counter]() {
        QString t = textEdit->toPlainText();
        if (t.size() > 500) {
            t.truncate(500);
            // 改文本会再次触发 textChanged，先屏蔽信号避免递归
            QSignalBlocker blocker(textEdit);
            textEdit->setPlainText(t);
            QTextCursor c = textEdit->textCursor();
            c.movePosition(QTextCursor::End);
            textEdit->setTextCursor(c);
        }
        counter->setText(QStringLiteral("%1 / 500").arg(t.size()));
    });

    // 确定 / 取消
    auto *btns = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    btns->button(QDialogButtonBox::Ok)->setText(QStringLiteral("添加"));
    btns->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
    root->addWidget(btns);
    QObject::connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted) return;

    // 内容为空就不添加（静默忽略，不给用户弹错误框）
    MoodLib::addCustom(nameEdit->text(), textEdit->toPlainText());
    rebuildGrid();
}
