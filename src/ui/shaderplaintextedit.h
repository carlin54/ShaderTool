#ifndef SHADERPLAINTEXTEDIT_H
#define SHADERPLAINTEXTEDIT_H

#include <QPlainTextEdit>

class QPaintEvent;

class ShaderPlainTextEdit : public QPlainTextEdit
{
    Q_OBJECT
public:
    explicit ShaderPlainTextEdit(QWidget *parent = nullptr);

    void lineNumberAreaPaintEvent(QPaintEvent *event);
    int lineNumberAreaWidth() const;

protected:
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void updateLineNumberAreaWidth();
    void updateLineNumberArea(const QRect &rect, int dy);

private:
    QWidget *m_lineNumberArea = nullptr;
};

#endif
