#include "hlslhighlighter.h"


HlslHighlighter::HlslHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent)
{
    setupHighlightingRules();
}

void HlslHighlighter::setupHighlightingRules()
{
    HighlightingRule rule;

    // Keyword formatting
    keywordFormat.setForeground(Qt::blue);
    QStringList keywordPatterns = {
        "\\bfloat\\b", "\\bint\\b", "\\bbool\\b", "\\bvoid\\b", "\\bif\\b", "\\belse\\b",
        "\\bwhile\\b", "\\bfor\\b", "\\breturn\\b", "\\btrue\\b", "\\bfalse\\b",
        "\\bstruct\\b", "\\bcbuffer\\b", "\\bregister\\b"
    };
    for (const QString &pattern : keywordPatterns) {
        rule.pattern = QRegularExpression(pattern);
        rule.format = keywordFormat;
        highlightingRules.append(rule);
    }

    // Function formatting
    functionFormat.setForeground(Qt::darkCyan);
    rule.pattern = QRegularExpression("\\b[A-Za-z0-9_]+(?=\\()");
    rule.format = functionFormat;
    highlightingRules.append(rule);

    // Single-line comment formatting
    singleLineCommentFormat.setForeground(Qt::darkGreen);
    rule.pattern = QRegularExpression("//[^\n]*");
    rule.format = singleLineCommentFormat;
    highlightingRules.append(rule);

    // Multi-line comment formatting
    multiLineCommentFormat.setForeground(Qt::darkGreen);
    rule.pattern = QRegularExpression("/\\*[^*]*\\*+(?:[^/*][^*]*\\*+)*/");
    rule.format = multiLineCommentFormat;
    highlightingRules.append(rule);

    // Quotation formatting
    quotationFormat.setForeground(Qt::darkRed);
    rule.pattern = QRegularExpression("\".*\"");
    rule.format = quotationFormat;
    highlightingRules.append(rule);

    // Preprocessor directive formatting
    preprocessorFormat.setForeground(Qt::darkMagenta);
    rule.pattern = QRegularExpression("#[^\n]*");
    rule.format = preprocessorFormat;
    highlightingRules.append(rule);
}

void HlslHighlighter::highlightBlock(const QString &text)
{
    for (const HighlightingRule &rule : qAsConst(highlightingRules)) {
        QRegularExpressionMatchIterator matchIterator = rule.pattern.globalMatch(text);
        while (matchIterator.hasNext()) {
            QRegularExpressionMatch match = matchIterator.next();
            setFormat(match.capturedStart(), match.capturedLength(), rule.format);
        }
    }
}
