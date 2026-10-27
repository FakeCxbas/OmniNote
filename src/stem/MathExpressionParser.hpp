#pragma once

#include <string>
#include <vector>
#include <memory>
#include <cmath>
#include <limits>

namespace OmniNote {

class MathExpressionParser {
public:
    enum class TokenType {
        Number,
        Identifier,
        Plus,
        Minus,
        Multiply,
        Divide,
        Power,
        LParen,
        RParen,
        Comma,
        EndOfInput,
        Invalid
    };

    struct Token {
        TokenType type;
        double value{0.0};
        std::string text;
    };

    struct ASTNode {
        virtual ~ASTNode() = default;
        virtual double evaluate(double x) const = 0;
    };

    using NodePtr = std::unique_ptr<ASTNode>;

    MathExpressionParser();
    explicit MathExpressionParser(const std::string &expression);
    ~MathExpressionParser() = default;

    bool parse(const std::string &expression);
    double evaluate(double x) const;
    bool isValid() const { return m_root != nullptr; }
    std::string errorMessage() const { return m_errorMessage; }
    std::string expression() const { return m_rawExpression; }

private:
    std::string m_rawExpression;
    std::string m_errorMessage;
    std::vector<Token> m_tokens;
    size_t m_cursor{0};
    NodePtr m_root;

    // Lexer
    bool tokenize(const std::string &expr);

    // Parser grammar
    const Token &currentToken() const;
    const Token &advance();
    bool match(TokenType type);

    NodePtr parseExpression();
    NodePtr parseTerm();
    NodePtr parsePower();
    NodePtr parseUnary();
    NodePtr parsePrimary();
};

} // namespace OmniNote
