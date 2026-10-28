#pragma once

#include <cctype>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <vector>

namespace fractl {

enum class Tok {
    KwFractal, KwAxiom, KwRules, KwAngle, KwIterations, KwStep, KwStart, KwHeading,
    KwSeed, KwDimensions, KwWidth, KwTaper, KwColors,
    KwScene, KwPlace, KwScatter, KwAt, KwWithin, KwYaw, KwScale, KwClear, KwClumps, KwVary,
    Ident, Number, String,
    LBrace, RBrace, LParen, RParen, LBracket, RBracket,
    Colon, Semi, Comma, Arrow, Pipe,
    End
};

inline const char* to_string(Tok t) {
    switch (t) {
        case Tok::KwFractal:    return "'fractal'";
        case Tok::KwAxiom:      return "'axiom'";
        case Tok::KwRules:      return "'rules'";
        case Tok::KwAngle:      return "'angle'";
        case Tok::KwIterations: return "'iterations'";
        case Tok::KwStep:       return "'step'";
        case Tok::KwStart:      return "'start'";
        case Tok::KwHeading:    return "'heading'";
        case Tok::KwSeed:       return "'seed'";
        case Tok::KwDimensions: return "'dimensions'";
        case Tok::KwWidth:      return "'width'";
        case Tok::KwTaper:      return "'taper'";
        case Tok::KwColors:     return "'colors'";
        case Tok::KwScene:      return "'scene'";
        case Tok::KwPlace:      return "'place'";
        case Tok::KwScatter:    return "'scatter'";
        case Tok::KwAt:         return "'at'";
        case Tok::KwWithin:     return "'within'";
        case Tok::KwYaw:        return "'yaw'";
        case Tok::KwScale:      return "'scale'";
        case Tok::KwClear:      return "'clear'";
        case Tok::KwClumps:     return "'clumps'";
        case Tok::KwVary:       return "'vary'";
        case Tok::Ident:        return "identifier";
        case Tok::Number:       return "number";
        case Tok::String:       return "string";
        case Tok::LBrace:       return "'{'";
        case Tok::RBrace:       return "'}'";
        case Tok::LParen:       return "'('";
        case Tok::RParen:       return "')'";
        case Tok::LBracket:     return "'['";
        case Tok::RBracket:     return "']'";
        case Tok::Colon:        return "':'";
        case Tok::Semi:         return "';'";
        case Tok::Comma:        return "','";
        case Tok::Arrow:        return "'->'";
        case Tok::Pipe:         return "'|'";
        case Tok::End:          return "end of file";
    }
    return "?";
}

struct Token {
    Tok         kind = Tok::End;
    std::string text;
    double      num  = 0.0;
    int         line = 0;
};

struct LexError : std::runtime_error {
    int line;
    LexError(int ln, const std::string& msg)
        : std::runtime_error("line " + std::to_string(ln) + ": " + msg), line(ln) {}
};

class Lexer {
public:
    explicit Lexer(std::string src) : src_(std::move(src)) {
        if (src_.compare(0, 3, "\xEF\xBB\xBF") == 0) i_ = 3;
    }

    std::vector<Token> tokenize() {
        std::vector<Token> out;
        for (;;) {
            skipSpaceAndComments();
            if (atEnd()) { out.push_back(tok(Tok::End, "", line_)); return out; }
            out.push_back(scanToken());
        }
    }

private:
    std::string src_;
    size_t      i_    = 0;
    int         line_ = 1;

    bool atEnd() const { return i_ >= src_.size(); }
    char peek(size_t k = 0) const { return i_ + k < src_.size() ? src_[i_ + k] : '\0'; }
    char advance() { char c = src_[i_++]; if (c == '\n') ++line_; return c; }

    static Token tok(Tok k, std::string text, int ln, double n = 0.0) {
        Token t;
        t.kind = k;
        t.text = std::move(text);
        t.num  = n;
        t.line = ln;
        return t;
    }

    void skipSpaceAndComments() {
        for (;;) {
            while (!atEnd() && std::isspace(static_cast<unsigned char>(peek()))) advance();
            if ((peek() == '/' && peek(1) == '/') || peek() == '#') {
                while (!atEnd() && peek() != '\n') advance();
                continue;
            }
            if (peek() == '/' && peek(1) == '*') {
                int start = line_;
                advance(); advance();
                while (!(peek() == '*' && peek(1) == '/')) {
                    if (atEnd()) throw LexError(start, "unterminated /* comment");
                    advance();
                }
                advance(); advance();
                continue;
            }
            return;
        }
    }

    static Tok keywordOrIdent(const std::string& s) {
        if (s == "fractal")    return Tok::KwFractal;
        if (s == "axiom")      return Tok::KwAxiom;
        if (s == "rules")      return Tok::KwRules;
        if (s == "angle")      return Tok::KwAngle;
        if (s == "iterations") return Tok::KwIterations;
        if (s == "step")       return Tok::KwStep;
        if (s == "start")      return Tok::KwStart;
        if (s == "heading")    return Tok::KwHeading;
        if (s == "seed")       return Tok::KwSeed;
        if (s == "dimensions") return Tok::KwDimensions;
        if (s == "width")      return Tok::KwWidth;
        if (s == "taper")      return Tok::KwTaper;
        if (s == "colors")     return Tok::KwColors;
        if (s == "scene")      return Tok::KwScene;
        if (s == "place")      return Tok::KwPlace;
        if (s == "scatter")    return Tok::KwScatter;
        if (s == "at")         return Tok::KwAt;
        if (s == "within")     return Tok::KwWithin;
        if (s == "yaw")        return Tok::KwYaw;
        if (s == "scale")      return Tok::KwScale;
        if (s == "clear")      return Tok::KwClear;
        if (s == "clumps")     return Tok::KwClumps;
        if (s == "vary")       return Tok::KwVary;
        return Tok::Ident;
    }

    Token scanToken() {
        int  startLine = line_;
        char c         = peek();

        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') return scanIdent(startLine);
        if (std::isdigit(static_cast<unsigned char>(c)) || c == '.') return scanNumber(startLine);
        if (c == '-' && peek(1) != '>' &&
            (std::isdigit(static_cast<unsigned char>(peek(1))) || peek(1) == '.'))
            return scanNumber(startLine);
        if (c == '"') return scanString(startLine);

        advance();
        switch (c) {
            case '{': return tok(Tok::LBrace,   "{", startLine);
            case '}': return tok(Tok::RBrace,   "}", startLine);
            case '(': return tok(Tok::LParen,   "(", startLine);
            case ')': return tok(Tok::RParen,   ")", startLine);
            case '[': return tok(Tok::LBracket, "[", startLine);
            case ']': return tok(Tok::RBracket, "]", startLine);
            case ':': return tok(Tok::Colon,    ":", startLine);
            case ';': return tok(Tok::Semi,     ";", startLine);
            case ',': return tok(Tok::Comma,    ",", startLine);
            case '|': return tok(Tok::Pipe,     "|", startLine);
            case '-':
                if (peek() == '>') { advance(); return tok(Tok::Arrow, "->", startLine); }
                throw LexError(startLine, "unexpected '-' (did you mean '->'?)");
            default:
                throw LexError(startLine, std::string("unexpected character '") + c + "'");
        }
    }

    Token scanIdent(int startLine) {
        std::string s;
        while (!atEnd() && (std::isalnum(static_cast<unsigned char>(peek())) || peek() == '_'))
            s += advance();
        return tok(keywordOrIdent(s), s, startLine);
    }

    Token scanNumber(int startLine) {
        std::string s;
        if (peek() == '-' || peek() == '+') s += advance();
        while (!atEnd() && (std::isdigit(static_cast<unsigned char>(peek())) || peek() == '.'))
            s += advance();
        if (peek() == 'e' || peek() == 'E') {
            s += advance();
            if (peek() == '-' || peek() == '+') s += advance();
            while (!atEnd() && std::isdigit(static_cast<unsigned char>(peek()))) s += advance();
        }
        const char* begin = s.c_str();
        char*       end   = nullptr;
        double      v     = std::strtod(begin, &end);
        if (end == begin || *end != '\0') throw LexError(startLine, "malformed number '" + s + "'");
        return tok(Tok::Number, s, startLine, v);
    }

    Token scanString(int startLine) {
        advance();
        std::string s;
        while (peek() != '"') {
            if (atEnd() || peek() == '\n') throw LexError(startLine, "unterminated string literal");
            char c = advance();
            if (c == '\\' && (peek() == '"' || peek() == '\\')) c = advance();
            s += c;
        }
        advance();
        return tok(Tok::String, s, startLine);
    }
};

}
