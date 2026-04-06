#ifndef STAGEPAGE_H
#define STAGEPAGE_H

#include <QWidget>

class QLineEdit;
class QPlainTextEdit;

// One shader stage: fixed kind label, editable entry + HLSL source.
class StagePage : public QWidget
{
    Q_OBJECT
public:
    StagePage(const QString &kind, const QString &entry, const QString &source, QWidget *parent = nullptr);

    QString kind() const { return m_kind; }
    QString entry() const;
    QString source() const;
    void setEntry(const QString &e);
    void setSource(const QString &s);

    QPlainTextEdit *sourceEdit() const { return m_source; }

signals:
    void contentChanged();

private:
    QString m_kind;
    QLineEdit *m_entry = nullptr;
    QPlainTextEdit *m_source = nullptr;
};

#endif
