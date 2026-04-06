#ifndef HLSLHIGHLIGHTER_H
#define HLSLHIGHLIGHTER_H

#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QRegularExpression>
#include <QTextDocument>

class HlslHighlighter : public QSyntaxHighlighter
{
private:
    struct HighlightingRule
    {
        QRegularExpression pattern;
        QTextCharFormat format;
    };
    QVector<HighlightingRule> highlightingRules;

    QTextCharFormat keywordFormat;
    QTextCharFormat functionFormat;
    QTextCharFormat singleLineCommentFormat;
    QTextCharFormat multiLineCommentFormat;
    QTextCharFormat quotationFormat;
    QTextCharFormat preprocessorFormat;

    void setupHighlightingRules();
protected:
    void highlightBlock(const QString &text) override;
public:
    HlslHighlighter(QTextDocument *parent = nullptr);
};

#endif // HLSLHIGHLIGHTER_H
