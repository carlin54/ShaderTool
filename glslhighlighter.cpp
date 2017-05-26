#include "glslhighlighter.h"

GLSLHighlighter::GLSLHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent)
{
    HighlightingRule rule;
    keywordFormat.setForeground(Qt::darkBlue);
    keywordFormat.setFontWeight(QFont::Bold);
    QStringList keywordPatterns;
    keywordPatterns << "\\bclass\\b" << "\\bstruct\\b";
    foreach (const QString &pattern, keywordPatterns) {
        rule.pattern = QRegExp(pattern);
        rule.format = keywordFormat;
        highlightingRules.append(rule);
    }

    dataTypesFormat.setForeground(Qt::blue);
    QStringList dataTypePatterns;
    dataTypePatterns
            << "\\bfloat\\b" << "\\bvec2\\b" << "\\bvec3\\b" << "\\bvec4\\b"
            << "\\bint\\b" << "\\bivec2\\b" << "\\bivec3\\b" << "\\bivec4\\b"
            << "\\bbool\\b" << "\\bbvec2\\b" << "\\bbvec3\\b" << "\\bbvec4\\b"
            << "\\bmat2\\b" << "\\bmat3\\b" << "\\bmat4\\b"
            << "\\bvoid\\b";
    foreach (const QString &pattern, dataTypePatterns) {
        rule.pattern = QRegExp(pattern);
        rule.format = dataTypesFormat;
        highlightingRules.append(rule);
    }



    dataTypesQualifiersFormat.setForeground(Qt::magenta);
    QStringList dataTypeQualifiersPatterns;
    dataTypeQualifiersPatterns << "\\bconst\\b" << "\\bin\\b" << "\\bout\\b" << "\\buniform\\b";
    foreach (const QString &pattern, dataTypeQualifiersPatterns) {
        rule.pattern = QRegExp(pattern);
        rule.format = dataTypesQualifiersFormat;
        highlightingRules.append(rule);
    }

    preprocessingFormat.setForeground(Qt::darkGreen);
    QStringList preprocessingPatterns;
    preprocessingPatterns << "#version \\d\\d\\d";
    foreach (const QString &pattern, preprocessingPatterns) {
        rule.pattern = QRegExp(pattern);
        rule.format = preprocessingFormat;
        highlightingRules.append(rule);
    }


    classFormat.setFontWeight(QFont::Bold);
    classFormat.setForeground(Qt::darkMagenta);
    rule.pattern = QRegExp("\\bQ[A-Za-z]+\\b");
    rule.format = classFormat;
    highlightingRules.append(rule);

    quotationFormat.setForeground(Qt::darkGreen);
    rule.pattern = QRegExp("\".*\"");
    rule.format = quotationFormat;
    highlightingRules.append(rule);

    functionFormat.setFontItalic(true);
    functionFormat.setForeground(Qt::blue);
    rule.pattern = QRegExp("\\b[A-Za-z0-9_]+(?=\\()");
    rule.format = functionFormat;
    highlightingRules.append(rule);

    singleLineCommentFormat.setForeground(Qt::red);
    rule.pattern = QRegExp("//[^\n]*");
    rule.format = singleLineCommentFormat;
    highlightingRules.append(rule);

    multiLineCommentFormat.setForeground(Qt::red);

    commentStartExpression = QRegExp("/\\*");
    commentEndExpression = QRegExp("\\*/");
}

void GLSLHighlighter::highlightBlock(const QString &text)
{
    foreach (const HighlightingRule &rule, highlightingRules) {
        QRegExp expression(rule.pattern);
        int index = expression.indexIn(text);
        while (index >= 0) {
            int length = expression.matchedLength();
            setFormat(index, length, rule.format);
            index = expression.indexIn(text, index + length);
        }
    }

    setCurrentBlockState(0);

    int startIndex = 0;
    if (previousBlockState() != 1)
        startIndex = commentStartExpression.indexIn(text);

    while (startIndex >= 0) {
        int endIndex = commentEndExpression.indexIn(text, startIndex);
        int commentLength;
        if (endIndex == -1) {
            setCurrentBlockState(1);
            commentLength = text.length() - startIndex;
        } else {
            commentLength = endIndex - startIndex
                            + commentEndExpression.matchedLength();
        }
        setFormat(startIndex, commentLength, multiLineCommentFormat);
        startIndex = commentStartExpression.indexIn(text, startIndex + commentLength);
    }
}
