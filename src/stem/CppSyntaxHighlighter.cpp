#include "CppSyntaxHighlighter.hpp"

namespace OmniNote {

CppSyntaxHighlighter::CppSyntaxHighlighter(QTextDocument *parent, Language lang)
    : QSyntaxHighlighter(parent), m_language(lang) {
    setupFormats();
    buildRules();
}

void CppSyntaxHighlighter::setLanguage(Language lang) {
    if (m_language != lang) {
        m_language = lang;
        buildRules();
        rehighlight();
    }
}

void CppSyntaxHighlighter::setupFormats() {
    // Tokyo Night / Catppuccin color scheme
    m_keywordFormat.setForeground(QColor("#bb9af7")); // Magenta / Purple
    m_keywordFormat.setFontWeight(QFont::Bold);

    m_typeFormat.setForeground(QColor("#2ac3de"));    // Cyan
    m_typeFormat.setFontWeight(QFont::DemiBold);

    m_functionFormat.setForeground(QColor("#7aa2f7")); // Blue
    m_functionFormat.setFontItalic(true);

    m_stringFormat.setForeground(QColor("#9ece6a"));   // Green

    m_numberFormat.setForeground(QColor("#ff9e64"));   // Orange

    m_commentFormat.setForeground(QColor("#565f89"));  // Muted blue-gray
    m_commentFormat.setFontItalic(true);

    m_preprocessorFormat.setForeground(QColor("#f7768e")); // Soft red/pink

    m_operatorFormat.setForeground(QColor("#89ddff"));
}

void CppSyntaxHighlighter::buildRules() {
    m_highlightingRules.clear();

    HighlightingRule rule;

    // Number literals (hex, float, int)
    rule.pattern = QRegularExpression(QStringLiteral("\\b(0x[0-9a-fA-F]+|[0-9]+(\\.[0-9]+)?([eE][+-]?[0-9]+)?f?)\\b"));
    rule.format = m_numberFormat;
    m_highlightingRules.append(rule);

    if (m_language == Language::Cpp) {
        // C++ Keywords
        const QStringList keywords = {
            QStringLiteral("\\balignas\\b"), QStringLiteral("\\balignof\\b"), QStringLiteral("\\band\\b"),
            QStringLiteral("\\bauto\\b"), QStringLiteral("\\bbool\\b"), QStringLiteral("\\bbreak\\b"),
            QStringLiteral("\\bcase\\b"), QStringLiteral("\\bcatch\\b"), QStringLiteral("\\bclass\\b"),
            QStringLiteral("\\bconcept\\b"), QStringLiteral("\\bconst\\b"), QStringLiteral("\\bconsteval\\b"),
            QStringLiteral("\\bconstexpr\\b"), QStringLiteral("\\bconstinit\\b"), QStringLiteral("\\bcontinue\\b"),
            QStringLiteral("\\bco_await\\b"), QStringLiteral("\\bco_return\\b"), QStringLiteral("\\bco_yield\\b"),
            QStringLiteral("\\bdecltype\\b"), QStringLiteral("\\bdefault\\b"), QStringLiteral("\\bdelete\\b"),
            QStringLiteral("\\bdo\\b"), QStringLiteral("\\belse\\b"), QStringLiteral("\\benum\\b"),
            QStringLiteral("\\bexplicit\\b"), QStringLiteral("\\bexport\\b"), QStringLiteral("\\bextern\\b"),
            QStringLiteral("\\bfalse\\b"), QStringLiteral("\\bfor\\b"), QStringLiteral("\\bfriend\\b"),
            QStringLiteral("\\bgoto\\b"), QStringLiteral("\\bif\\b"), QStringLiteral("\\binline\\b"),
            QStringLiteral("\\bmutable\\b"), QStringLiteral("\\bnamespace\\b"), QStringLiteral("\\bnew\\b"),
            QStringLiteral("\\bnoexcept\\b"), QStringLiteral("\\bnot\\b"), QStringLiteral("\\bnullptr\\b"),
            QStringLiteral("\\boperator\\b"), QStringLiteral("\\bor\\b"), QStringLiteral("\\bprivate\\b"),
            QStringLiteral("\\bprotected\\b"), QStringLiteral("\\bpublic\\b"), QStringLiteral("\\bregister\\b"),
            QStringLiteral("\\brequires\\b"), QStringLiteral("\\breturn\\b"), QStringLiteral("\\bsigned\\b"),
            QStringLiteral("\\bsizeof\\b"), QStringLiteral("\\bstatic\\b"), QStringLiteral("\\bstatic_assert\\b"),
            QStringLiteral("\\bstruct\\b"), QStringLiteral("\\bswitch\\b"), QStringLiteral("\\btemplate\\b"),
            QStringLiteral("\\bthis\\b"), QStringLiteral("\\bthread_local\\b"), QStringLiteral("\\bthrow\\b"),
            QStringLiteral("\\btrue\\b"), QStringLiteral("\\btry\\b"), QStringLiteral("\\btypedef\\b"),
            QStringLiteral("\\btypeid\\b"), QStringLiteral("\\btypename\\b"), QStringLiteral("\\bunion\\b"),
            QStringLiteral("\\bunsigned\\b"), QStringLiteral("\\busing\\b"), QStringLiteral("\\bvirtual\\b"),
            QStringLiteral("\\bvolatile\\b"), QStringLiteral("\\bwhile\\b")
        };
        for (const auto &kw : keywords) {
            rule.pattern = QRegularExpression(kw);
            rule.format = m_keywordFormat;
            m_highlightingRules.append(rule);
        }

        // C++ Types
        const QStringList types = {
            QStringLiteral("\\bint\\b"), QStringLiteral("\\bfloat\\b"), QStringLiteral("\\bdouble\\b"),
            QStringLiteral("\\bchar\\b"), QStringLiteral("\\bchar8_t\\b"), QStringLiteral("\\bchar16_t\\b"),
            QStringLiteral("\\bchar32_t\\b"), QStringLiteral("\\bwchar_t\\b"), QStringLiteral("\\bvoid\\b"),
            QStringLiteral("\\bint8_t\\b"), QStringLiteral("\\bint16_t\\b"), QStringLiteral("\\bint32_t\\b"),
            QStringLiteral("\\bint64_t\\b"), QStringLiteral("\\buint8_t\\b"), QStringLiteral("\\buint16_t\\b"),
            QStringLiteral("\\buint32_t\\b"), QStringLiteral("\\buint64_t\\b"), QStringLiteral("\\bsize_t\\b"),
            QStringLiteral("\\bstd::vector\\b"), QStringLiteral("\\bstd::string\\b"), QStringLiteral("\\bstd::map\\b"),
            QStringLiteral("\\bstd::unique_ptr\\b"), QStringLiteral("\\bstd::shared_ptr\\b"), QStringLiteral("\\bstd::pair\\b")
        };
        for (const auto &t : types) {
            rule.pattern = QRegularExpression(t);
            rule.format = m_typeFormat;
            m_highlightingRules.append(rule);
        }

        // Preprocessor
        rule.pattern = QRegularExpression(QStringLiteral("#[^\n]*"));
        rule.format = m_preprocessorFormat;
        m_highlightingRules.append(rule);

        // Strings
        rule.pattern = QRegularExpression(QStringLiteral("\".*\""));
        rule.format = m_stringFormat;
        m_highlightingRules.append(rule);

        // Single line comments
        rule.pattern = QRegularExpression(QStringLiteral("//[^\n]*"));
        rule.format = m_commentFormat;
        m_highlightingRules.append(rule);

        // Multi-line comment delimiters
        m_commentStartPattern = QRegularExpression(QStringLiteral("/\\*"));
        m_commentEndPattern = QRegularExpression(QStringLiteral("\\*/"));

    } else if (m_language == Language::Python) {
        // Python Keywords
        const QStringList pyKeywords = {
            QStringLiteral("\\band\\b"), QStringLiteral("\\bas\\b"), QStringLiteral("\\bassert\\b"),
            QStringLiteral("\\basync\\b"), QStringLiteral("\\bawait\\b"), QStringLiteral("\\bbreak\\b"),
            QStringLiteral("\\bclass\\b"), QStringLiteral("\\bcontinue\\b"), QStringLiteral("\\bdef\\b"),
            QStringLiteral("\\bdel\\b"), QStringLiteral("\\belif\\b"), QStringLiteral("\\belse\\b"),
            QStringLiteral("\\bexcept\\b"), QStringLiteral("\\bfinally\\b"), QStringLiteral("\\bfor\\b"),
            QStringLiteral("\\bfrom\\b"), QStringLiteral("\\bglobal\\b"), QStringLiteral("\\bif\\b"),
            QStringLiteral("\\bimport\\b"), QStringLiteral("\\bin\\b"), QStringLiteral("\\bis\\b"),
            QStringLiteral("\\blambda\\b"), QStringLiteral("\\bnonlocal\\b"), QStringLiteral("\\bnot\\b"),
            QStringLiteral("\\bor\\b"), QStringLiteral("\\bpass\\b"), QStringLiteral("\\braise\\b"),
            QStringLiteral("\\breturn\\b"), QStringLiteral("\\btry\\b"), QStringLiteral("\\bwhile\\b"),
            QStringLiteral("\\bwith\\b"), QStringLiteral("\\byield\\b"), QStringLiteral("\\bTrue\\b"),
            QStringLiteral("\\bFalse\\b"), QStringLiteral("\\bNone\\b"), QStringLiteral("\\bself\\b")
        };
        for (const auto &kw : pyKeywords) {
            rule.pattern = QRegularExpression(kw);
            rule.format = m_keywordFormat;
            m_highlightingRules.append(rule);
        }

        // Builtin Types & functions
        const QStringList pyTypes = {
            QStringLiteral("\\bint\\b"), QStringLiteral("\\bfloat\\b"), QStringLiteral("\\bstr\\b"),
            QStringLiteral("\\blist\\b"), QStringLiteral("\\bdict\\b"), QStringLiteral("\\bset\\b"),
            QStringLiteral("\\btuple\\b"), QStringLiteral("\\bbool\\b"), QStringLiteral("\\brange\\b"),
            QStringLiteral("\\blen\\b"), QStringLiteral("\\bprint\\b"), QStringLiteral("\\btype\\b")
        };
        for (const auto &t : pyTypes) {
            rule.pattern = QRegularExpression(t);
            rule.format = m_typeFormat;
            m_highlightingRules.append(rule);
        }

        // Strings
        rule.pattern = QRegularExpression(QStringLiteral("(\"[^\"]*\"|'[^']*')"));
        rule.format = m_stringFormat;
        m_highlightingRules.append(rule);

        // Comments
        rule.pattern = QRegularExpression(QStringLiteral("#[^\n]*"));
        rule.format = m_commentFormat;
        m_highlightingRules.append(rule);

        m_commentStartPattern = QRegularExpression(QStringLiteral("\"\"\""));
        m_commentEndPattern = QRegularExpression(QStringLiteral("\"\"\""));

    } else { // Pseudocode
        const QStringList pseudoKeywords = {
            QStringLiteral("\\bALGORITHM\\b"), QStringLiteral("\\bBEGIN\\b"), QStringLiteral("\\bEND\\b"),
            QStringLiteral("\\bPROCEDURE\\b"), QStringLiteral("\\bFUNCTION\\b"), QStringLiteral("\\bWHILE\\b"),
            QStringLiteral("\\bDO\\b"), QStringLiteral("\\bFOR\\b"), QStringLiteral("\\bTO\\b"),
            QStringLiteral("\\bDOWNTO\\b"), QStringLiteral("\\bSTEP\\b"), QStringLiteral("\\bIF\\b"),
            QStringLiteral("\\bTHEN\\b"), QStringLiteral("\\bELSE\\b"), QStringLiteral("\\bELSEIF\\b"),
            QStringLiteral("\\bRETURN\\b"), QStringLiteral("\\bINPUT\\b"), QStringLiteral("\\bOUTPUT\\b"),
            QStringLiteral("\\bPRINT\\b"), QStringLiteral("\\bREAD\\b"), QStringLiteral("\\bAND\\b"),
            QStringLiteral("\\bOR\\b"), QStringLiteral("\\bNOT\\b"), QStringLiteral("\\bTRUE\\b"),
            QStringLiteral("\\bFALSE\\b"), QStringLiteral("\\bNULL\\b"), QStringLiteral("\\bREPEAT\\b"),
            QStringLiteral("\\bUNTIL\\b"), QStringLiteral("\\bSWITCH\\b"), QStringLiteral("\\bCASE\\b")
        };
        for (const auto &kw : pseudoKeywords) {
            rule.pattern = QRegularExpression(kw, QRegularExpression::CaseInsensitiveOption);
            rule.format = m_keywordFormat;
            m_highlightingRules.append(rule);
        }

        // Strings
        rule.pattern = QRegularExpression(QStringLiteral("(\"[^\"]*\"|'[^']*')"));
        rule.format = m_stringFormat;
        m_highlightingRules.append(rule);

        // Comments
        rule.pattern = QRegularExpression(QStringLiteral("(//|#)[^\n]*"));
        rule.format = m_commentFormat;
        m_highlightingRules.append(rule);

        m_commentStartPattern = QRegularExpression(QStringLiteral("/\\*"));
        m_commentEndPattern = QRegularExpression(QStringLiteral("\\*/"));
    }

    // Function calls across all languages
    rule.pattern = QRegularExpression(QStringLiteral("\\b[A-Za-z_][A-Za-z0-9_]*(?=\\()"));
    rule.format = m_functionFormat;
    m_highlightingRules.append(rule);
}

void CppSyntaxHighlighter::highlightBlock(const QString &text) {
    for (const auto &rule : m_highlightingRules) {
        QRegularExpressionMatchIterator matchIterator = rule.pattern.globalMatch(text);
        while (matchIterator.hasNext()) {
            QRegularExpressionMatch match = matchIterator.next();
            setFormat(match.capturedStart(), match.capturedLength(), rule.format);
        }
    }

    // Multi-line comment handling
    setCurrentBlockState(0);

    int startIndex = 0;
    if (previousBlockState() != 1) {
        startIndex = text.indexOf(m_commentStartPattern);
    }

    while (startIndex >= 0) {
        QRegularExpressionMatch endMatch;
        int endIndex = text.indexOf(m_commentEndPattern, startIndex, &endMatch);
        int commentLength;
        if (endIndex == -1) {
            setCurrentBlockState(1);
            commentLength = text.length() - startIndex;
        } else {
            commentLength = endIndex - startIndex + endMatch.capturedLength();
        }
        setFormat(startIndex, commentLength, m_commentFormat);
        startIndex = text.indexOf(m_commentStartPattern, startIndex + commentLength);
    }
}

} // namespace OmniNote
