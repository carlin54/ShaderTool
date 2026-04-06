#include "stagepage.h"

#include "hlslhighlighter.h"
#include "shaderplaintextedit.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QVBoxLayout>

StagePage::StagePage(const QString &kind, const QString &entry, const QString &source, QWidget *parent)
    : QWidget(parent)
    , m_kind(kind)
{
    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(4, 4, 4, 4);

    auto *top = new QHBoxLayout;
    top->addWidget(new QLabel(QStringLiteral("Stage:")));
    auto *kl = new QLabel(kind);
    kl->setStyleSheet(QStringLiteral("font-weight: bold;"));
    top->addWidget(kl);
    top->addWidget(new QLabel(QStringLiteral("Entry:")));
    m_entry = new QLineEdit(entry);
    m_entry->setPlaceholderText(QStringLiteral("main"));
    top->addWidget(m_entry, 1);
    lay->addLayout(top);

    m_source = new ShaderPlainTextEdit;
    m_source->setPlainText(source);
    m_source->setTabStopDistance(40);
    new HlslHighlighter(m_source->document());
    lay->addWidget(m_source, 1);

    connect(m_entry, &QLineEdit::textChanged, this, &StagePage::contentChanged);
    connect(m_source, &QPlainTextEdit::textChanged, this, &StagePage::contentChanged);
}

QString StagePage::entry() const
{
    return m_entry ? m_entry->text().trimmed() : QString();
}

QString StagePage::source() const
{
    return m_source ? m_source->toPlainText() : QString();
}

void StagePage::setEntry(const QString &e)
{
    if (m_entry)
        m_entry->setText(e);
}

void StagePage::setSource(const QString &s)
{
    if (m_source)
        m_source->setPlainText(s);
}
