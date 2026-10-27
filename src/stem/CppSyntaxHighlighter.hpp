#pragma once

#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QRegularExpression>
#include <QVector>

namespace OmniNote {

class CppSyntaxHighlighter : public QSyntaxHighlighter {
    Q_OBJECT

public:
    enum class Language {
        Cpp,
        Python,
        Pseudocode
    };

    explicit CppSyntaxHighlighter(QTextDocument *parent = nullptr, Language lang = Language::Cpp);
    ~CppSyntaxHighlighter() override = default;

    void setLanguage(Language lang);
    Language currentLanguage() const { return m_language; }

protected:
    void highlightBlock(const QString &text) override;

private:
    struct HighlightingRule {
        QRegularExpression pattern;
        QTextCharFormat format;
    };

    void setupFormats();
    void buildRules();

    Language m_language;
    QVector<HighlightingRule> m_highlightingRules;

    QRegularExpression m_commentStartPattern;
    QRegularExpression m_commentEndPattern;

    QTextCharFormat m_keywordFormat;
    QTextCharFormat m_typeFormat;
    QTextCharFormat m_functionFormat;
    QTextCharFormat m_stringFormat;
    QTextCharFormat m_numberFormat;
    QTextCharFormat m_commentFormat;
    QTextCharFormat m_preprocessorFormat;
    QTextCharFormat m_operatorFormat;
};

} // namespace OmniNote
