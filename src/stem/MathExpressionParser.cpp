#include "MathExpressionParser.hpp"
#include <cctype>
#include <sstream>
#include <algorithm>

namespace OmniNote {

namespace {

struct NumberNode : public MathExpressionParser::ASTNode {
    double val;
    explicit NumberNode(double v) : val(v) {}
    double evaluate(double /*x*/) const override { return val; }
};

struct VariableNode : public MathExpressionParser::ASTNode {
    double evaluate(double x) const override { return x; }
};

struct UnaryOpNode : public MathExpressionParser::ASTNode {
    MathExpressionParser::TokenType op;
    MathExpressionParser::NodePtr child;

    UnaryOpNode(MathExpressionParser::TokenType o, MathExpressionParser::NodePtr c)
        : op(o), child(std::move(c)) {}

    double evaluate(double x) const override {
        double v = child ? child->evaluate(x) : 0.0;
        if (op == MathExpressionParser::TokenType::Minus) return -v;
        return v;
    }
};

struct BinaryOpNode : public MathExpressionParser::ASTNode {
    MathExpressionParser::TokenType op;
    MathExpressionParser::NodePtr left;
    MathExpressionParser::NodePtr right;

    BinaryOpNode(MathExpressionParser::TokenType o, MathExpressionParser::NodePtr l, MathExpressionParser::NodePtr r)
        : op(o), left(std::move(l)), right(std::move(r)) {}

    double evaluate(double x) const override {
        if (!left || !right) return std::numeric_limits<double>::quiet_NaN();
        double a = left->evaluate(x);
        double b = right->evaluate(x);

        if (std::isnan(a) || std::isnan(b)) return std::numeric_limits<double>::quiet_NaN();

        switch (op) {
        case MathExpressionParser::TokenType::Plus: return a + b;
        case MathExpressionParser::TokenType::Minus: return a - b;
        case MathExpressionParser::TokenType::Multiply: return a * b;
        case MathExpressionParser::TokenType::Divide: {
            if (std::abs(b) < 1e-15) {
                // Return NaN for removable discontinuities or undefined division
                return std::numeric_limits<double>::quiet_NaN();
            }
            return a / b;
        }
        case MathExpressionParser::TokenType::Power: {
            if (a < 0.0 && std::floor(b) != b) {
                return std::numeric_limits<double>::quiet_NaN();
            }
            return std::pow(a, b);
        }
        default:
            return std::numeric_limits<double>::quiet_NaN();
        }
    }
};

struct FunctionNode : public MathExpressionParser::ASTNode {
    std::string name;
    std::vector<MathExpressionParser::NodePtr> args;

    FunctionNode(std::string n, std::vector<MathExpressionParser::NodePtr> a)
        : name(std::move(n)), args(std::move(a)) {}

    double evaluate(double x) const override {
        if (args.empty()) return std::numeric_limits<double>::quiet_NaN();
        double v = args[0]->evaluate(x);
        if (std::isnan(v)) return std::numeric_limits<double>::quiet_NaN();

        if (name == "sin") return std::sin(v);
        if (name == "cos") return std::cos(v);
        if (name == "tan") {
            double c = std::cos(v);
            if (std::abs(c) < 1e-12) return std::numeric_limits<double>::quiet_NaN();
            return std::tan(v);
        }
        if (name == "asin") {
            if (v < -1.0 || v > 1.0) return std::numeric_limits<double>::quiet_NaN();
            return std::asin(v);
        }
        if (name == "acos") {
            if (v < -1.0 || v > 1.0) return std::numeric_limits<double>::quiet_NaN();
            return std::acos(v);
        }
        if (name == "atan") return std::atan(v);
        if (name == "sinh") return std::sinh(v);
        if (name == "cosh") return std::cosh(v);
        if (name == "tanh") return std::tanh(v);
        if (name == "sqrt") {
            if (v < 0.0) return std::numeric_limits<double>::quiet_NaN();
            return std::sqrt(v);
        }
        if (name == "cbrt") return std::cbrt(v);
        if (name == "exp") return std::exp(v);
        if (name == "ln" || name == "log") {
            if (v <= 0.0) return std::numeric_limits<double>::quiet_NaN();
            return std::log(v);
        }
        if (name == "log10") {
            if (v <= 0.0) return std::numeric_limits<double>::quiet_NaN();
            return std::log10(v);
        }
        if (name == "log2") {
            if (v <= 0.0) return std::numeric_limits<double>::quiet_NaN();
            return std::log2(v);
        }
        if (name == "abs") return std::abs(v);
        if (name == "floor") return std::floor(v);
        if (name == "ceil") return std::ceil(v);
        if (name == "sign" || name == "sgn") {
            return (v > 0.0) ? 1.0 : ((v < 0.0) ? -1.0 : 0.0);
        }

        return std::numeric_limits<double>::quiet_NaN();
    }
};

} // namespace

MathExpressionParser::MathExpressionParser() = default;

MathExpressionParser::MathExpressionParser(const std::string &expression) {
    parse(expression);
}

bool MathExpressionParser::parse(const std::string &expression) {
    m_rawExpression = expression;
    m_errorMessage.clear();
    m_tokens.clear();
    m_cursor = 0;
    m_root.reset();

    if (!tokenize(expression)) {
        return false;
    }

    try {
        m_root = parseExpression();
        if (m_cursor < m_tokens.size() && currentToken().type != TokenType::EndOfInput) {
            m_errorMessage = "Símbolo inesperado al final de la expresión: " + currentToken().text;
            m_root.reset();
            return false;
        }
    } catch (const std::exception &e) {
        m_errorMessage = e.what();
        m_root.reset();
        return false;
    }

    return m_root != nullptr;
}

double MathExpressionParser::evaluate(double x) const {
    if (!m_root) return std::numeric_limits<double>::quiet_NaN();
    return m_root->evaluate(x);
}

bool MathExpressionParser::tokenize(const std::string &expr) {
    size_t i = 0;
    const size_t len = expr.length();

    auto isIdentStart = [](char c) {
        return std::isalpha(static_cast<unsigned char>(c)) || c == '_';
    };
    auto isIdentPart = [](char c) {
        return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
    };

    while (i < len) {
        char c = expr[i];

        if (std::isspace(static_cast<unsigned char>(c))) {
            ++i;
            continue;
        }

        // Numbers: e.g. 12, 3.1415, .5
        if (std::isdigit(static_cast<unsigned char>(c)) || (c == '.' && i + 1 < len && std::isdigit(static_cast<unsigned char>(expr[i + 1])))) {
            size_t start = i;
            bool hasDot = (c == '.');
            ++i;
            while (i < len && (std::isdigit(static_cast<unsigned char>(expr[i])) || (!hasDot && expr[i] == '.'))) {
                if (expr[i] == '.') hasDot = true;
                ++i;
            }
            std::string numStr = expr.substr(start, i - start);
            double val = std::stod(numStr);

            // Check if previous token was identifier or number and insert implicit multiplication
            if (!m_tokens.empty()) {
                TokenType pt = m_tokens.back().type;
                if (pt == TokenType::RParen || pt == TokenType::Identifier) {
                    m_tokens.push_back({TokenType::Multiply, 0.0, "*"});
                }
            }

            m_tokens.push_back({TokenType::Number, val, numStr});
            continue;
        }

        // Identifiers and functions
        if (isIdentStart(c)) {
            size_t start = i;
            while (i < len && isIdentPart(expr[i])) {
                ++i;
            }
            std::string ident = expr.substr(start, i - start);
            std::transform(ident.begin(), ident.end(), ident.begin(), [](unsigned char ch) {
                return std::tolower(ch);
            });

            // Implicit multiplication: e.g. 2x, 2sin(x), )x
            if (!m_tokens.empty()) {
                TokenType pt = m_tokens.back().type;
                if (pt == TokenType::Number || pt == TokenType::RParen || pt == TokenType::Identifier) {
                    m_tokens.push_back({TokenType::Multiply, 0.0, "*"});
                }
            }

            m_tokens.push_back({TokenType::Identifier, 0.0, ident});
            continue;
        }

        // Operators
        switch (c) {
        case '+':
            m_tokens.push_back({TokenType::Plus, 0.0, "+"});
            ++i;
            break;
        case '-':
            m_tokens.push_back({TokenType::Minus, 0.0, "-"});
            ++i;
            break;
        case '*':
            m_tokens.push_back({TokenType::Multiply, 0.0, "*"});
            ++i;
            break;
        case '/':
            m_tokens.push_back({TokenType::Divide, 0.0, "/"});
            ++i;
            break;
        case '^':
            m_tokens.push_back({TokenType::Power, 0.0, "^"});
            ++i;
            break;
        case '(':
            if (!m_tokens.empty()) {
                TokenType pt = m_tokens.back().type;
                if (pt == TokenType::Number || pt == TokenType::RParen) {
                    m_tokens.push_back({TokenType::Multiply, 0.0, "*"});
                }
            }
            m_tokens.push_back({TokenType::LParen, 0.0, "("});
            ++i;
            break;
        case ')':
            m_tokens.push_back({TokenType::RParen, 0.0, ")"});
            ++i;
            break;
        case ',':
            m_tokens.push_back({TokenType::Comma, 0.0, ","});
            ++i;
            break;
        default:
            m_errorMessage = std::string("Carácter no reconocido: ") + c;
            return false;
        }
    }

    m_tokens.push_back({TokenType::EndOfInput, 0.0, ""});
    return true;
}

const MathExpressionParser::Token &MathExpressionParser::currentToken() const {
    if (m_cursor < m_tokens.size()) {
        return m_tokens[m_cursor];
    }
    static const Token eofToken{TokenType::EndOfInput, 0.0, ""};
    return eofToken;
}

const MathExpressionParser::Token &MathExpressionParser::advance() {
    if (m_cursor < m_tokens.size()) {
        return m_tokens[m_cursor++];
    }
    static const Token eofToken{TokenType::EndOfInput, 0.0, ""};
    return eofToken;
}

bool MathExpressionParser::match(TokenType type) {
    if (currentToken().type == type) {
        advance();
        return true;
    }
    return false;
}

MathExpressionParser::NodePtr MathExpressionParser::parseExpression() {
    auto node = parseTerm();

    while (currentToken().type == TokenType::Plus || currentToken().type == TokenType::Minus) {
        TokenType op = advance().type;
        auto right = parseTerm();
        node = std::make_unique<BinaryOpNode>(op, std::move(node), std::move(right));
    }

    return node;
}

MathExpressionParser::NodePtr MathExpressionParser::parseTerm() {
    auto node = parsePower();

    while (currentToken().type == TokenType::Multiply || currentToken().type == TokenType::Divide) {
        TokenType op = advance().type;
        auto right = parsePower();
        node = std::make_unique<BinaryOpNode>(op, std::move(node), std::move(right));
    }

    return node;
}

MathExpressionParser::NodePtr MathExpressionParser::parsePower() {
    auto node = parseUnary();

    if (currentToken().type == TokenType::Power) {
        advance();
        auto right = parsePower(); // Right-associative: a^b^c = a^(b^c)
        node = std::make_unique<BinaryOpNode>(TokenType::Power, std::move(node), std::move(right));
    }

    return node;
}

MathExpressionParser::NodePtr MathExpressionParser::parseUnary() {
    if (currentToken().type == TokenType::Plus) {
        advance();
        return parseUnary();
    }
    if (currentToken().type == TokenType::Minus) {
        advance();
        return std::make_unique<UnaryOpNode>(TokenType::Minus, parseUnary());
    }
    return parsePrimary();
}

MathExpressionParser::NodePtr MathExpressionParser::parsePrimary() {
    const auto &token = currentToken();

    if (token.type == TokenType::Number) {
        double val = token.value;
        advance();
        return std::make_unique<NumberNode>(val);
    }

    if (token.type == TokenType::Identifier) {
        std::string name = token.text;
        advance();

        if (name == "x") {
            return std::make_unique<VariableNode>();
        }
        if (name == "pi") {
            return std::make_unique<NumberNode>(M_PI);
        }
        if (name == "e") {
            return std::make_unique<NumberNode>(M_E);
        }

        // Function call
        if (currentToken().type == TokenType::LParen) {
            advance();
            std::vector<NodePtr> args;
            if (currentToken().type != TokenType::RParen) {
                args.push_back(parseExpression());
                while (match(TokenType::Comma)) {
                    args.push_back(parseExpression());
                }
            }
            if (!match(TokenType::RParen)) {
                throw std::runtime_error("Se esperaba ')' tras los argumentos de la función '" + name + "'");
            }
            return std::make_unique<FunctionNode>(name, std::move(args));
        }

        throw std::runtime_error("Identificador desconocido: '" + name + "'");
    }

    if (token.type == TokenType::LParen) {
        advance();
        auto node = parseExpression();
        if (!match(TokenType::RParen)) {
            throw std::runtime_error("Se esperaba ')' para cerrar la expresión");
        }
        return node;
    }

    throw std::runtime_error("Token inesperado: '" + token.text + "'");
}

} // namespace OmniNote
