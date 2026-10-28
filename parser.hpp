#pragma once

#include <cctype>
#include <cmath>
#include <ostream>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#include "ast.hpp"
#include "lexer.hpp"

namespace fractl {

struct ParseError : std::runtime_error {
    int line;
    ParseError(int ln, const std::string& msg)
        : std::runtime_error(ln > 0 ? "line " + std::to_string(ln) + ": " + msg : msg),
          line(ln) {}
};

class Parser {
public:
    explicit Parser(std::vector<Token> toks) : t_(std::move(toks)) {}

    FractalDecl parseProgram() {
        FractalDecl d = fractalDecl();
        expect(Tok::End, "end of file after the fractal declaration");
        validate(d);
        return d;
    }

    SourceFile parseFile() {
        SourceFile f;
        do {
            f.fractals.push_back(fractalDecl());
            validate(f.fractals.back());
        } while (kind() == Tok::KwFractal);
        if (kind() == Tok::KwScene) {
            f.hasScene = true;
            f.scene    = sceneDecl();
        }
        expect(Tok::End, f.hasScene ? "end of file after the scene"
                                    : "'fractal', 'scene' or end of file");
        validateFile(f);
        return f;
    }

private:
    std::vector<Token> t_;
    size_t             p_ = 0;

    const Token& cur()  const { return t_[p_]; }
    Tok          kind() const { return t_[p_].kind; }
    int          line() const { return t_[p_].line; }
    const Token& advance() { return t_[p_ < t_.size() - 1 ? p_++ : p_]; }
    bool         match(Tok k) { if (kind() == k) { advance(); return true; } return false; }

    const Token& expect(Tok k, const std::string& what) {
        if (kind() != k)
            throw ParseError(line(), "expected " + what + ", found " + describe(cur()));
        return advance();
    }

    static std::string describe(const Token& t) {
        if (t.kind == Tok::Ident || t.kind == Tok::Number) return "'" + t.text + "'";
        if (t.kind == Tok::String)                         return "string \"" + t.text + "\"";
        return to_string(t.kind);
    }

    std::set<std::string> seen_;
    std::set<char>        ruleSymbols_;

    void once(const std::string& field, int ln) {
        if (!seen_.insert(field).second)
            throw ParseError(ln, "duplicate '" + field + "' field");
    }

    FractalDecl fractalDecl() {
        seen_.clear();
        ruleSymbols_.clear();
        int ln = line();
        expect(Tok::KwFractal, "'fractal' at the start of the file");
        FractalDecl d;
        d.line = ln;
        d.name = expect(Tok::Ident, "a fractal name after 'fractal'").text;
        expect(Tok::LBrace, "'{' after the fractal name");
        while (kind() != Tok::RBrace && kind() != Tok::End) field(d);
        expect(Tok::RBrace, "'}' to close the fractal body");
        return d;
    }

    void field(FractalDecl& d) {
        int ln = line();
        switch (kind()) {
            case Tok::KwAxiom:      once("axiom", ln);      advance();
                                    colon("axiom");
                                    d.axiom = expect(Tok::String, "a quoted axiom string").text;
                                    checkBrackets(d.axiom, ln, "the axiom");
                                    semi("axiom"); return;

            case Tok::KwAngle:      once("angle", ln);      advance();
                                    colon("angle");  d.angle = number("angle");
                                    semi("angle"); return;

            case Tok::KwIterations: once("iterations", ln); advance();
                                    colon("iterations");
                                    d.iterations = intNumber("iterations");
                                    semi("iterations"); return;

            case Tok::KwStep:       once("step", ln);       advance();
                                    colon("step");   d.step = number("step");
                                    semi("step"); return;

            case Tok::KwHeading:    once("heading", ln);    advance();
                                    colon("heading"); d.heading = number("heading");
                                    semi("heading"); return;

            case Tok::KwStart:      once("start", ln);      advance();
                                    colon("start");
                                    expect(Tok::LParen, "'(' after 'start:'");
                                    d.startX = number("start x");
                                    expect(Tok::Comma, "',' between the start coordinates");
                                    d.startY = number("start y");
                                    expect(Tok::RParen, "')' to close 'start'");
                                    semi("start"); return;

            case Tok::KwRules:      once("rules", ln);      advance();
                                    rulesBlock(d); return;

            case Tok::KwSeed:       once("seed", ln);       advance();
                                    colon("seed");
                                    d.seed = seedNumber("seed");
                                    semi("seed"); return;

            case Tok::KwDimensions: once("dimensions", ln); advance();
                                    colon("dimensions");
                                    d.dimensions = intNumber("dimensions");
                                    if (d.dimensions != 2 && d.dimensions != 3)
                                        throw ParseError(ln, "'dimensions' must be 2 or 3");
                                    semi("dimensions"); return;

            case Tok::KwWidth:      once("width", ln);      advance();
                                    colon("width");  d.width = number("width");
                                    semi("width"); return;

            case Tok::KwTaper:      once("taper", ln);      advance();
                                    colon("taper");  d.taper = number("taper");
                                    semi("taper"); return;

            case Tok::KwColors:     once("colors", ln);     advance();
                                    colon("colors");
                                    expect(Tok::LParen, "'(' after 'colors:'");
                                    do {
                                        int cl = line();
                                        std::string c = expect(Tok::String, "a quoted colour like \"#4f8a2b\"").text;
                                        if (!isHexColor(c))
                                            throw ParseError(cl, "colour \"" + c + "\" must look like \"#4f8a2b\"");
                                        d.colors.push_back(c);
                                    } while (match(Tok::Comma));
                                    expect(Tok::RParen, "')' to close 'colors'");
                                    semi("colors"); return;

            default:
                throw ParseError(ln, "expected a field keyword (axiom, rules, angle, "
                                     "iterations, step, start, heading, seed, dimensions, "
                                     "width, taper, colors) or '}', found " +
                                     describe(cur()));
        }
    }

    void rulesBlock(FractalDecl& d) {
        match(Tok::Colon);
        expect(Tok::LBrace, "'{' to open the rules block");
        while (kind() != Tok::RBrace && kind() != Tok::End) ruleDecl(d);
        expect(Tok::RBrace, "'}' to close the rules block");
        match(Tok::Semi);
    }

    void ruleDecl(FractalDecl& d) {
        int ln = line();
        if (kind() != Tok::Ident)
            throw ParseError(ln, "expected a rule symbol, found " + describe(cur()));
        std::string sym = advance().text;
        if (sym.size() != 1)
            throw ParseError(ln, "rule symbol must be a single character, found '" + sym + "'");
        char c = sym[0];
        if (!ruleSymbols_.insert(c).second)
            throw ParseError(ln, std::string("duplicate rule for symbol '") + c + "'");

        expect(Tok::Arrow, std::string("'->' after rule symbol '") + c + "'");

        bool weighted = false;
        do {
            int pl = line();
            std::string rhs = expect(Tok::String,
                                     std::string("a quoted replacement string for '") + c + "'").text;
            checkBrackets(rhs, pl, std::string("the rule for '") + c + "'");
            double w = 1.0;
            if (match(Tok::LBracket)) {
                int wl = line();
                w = number("the weight");
                if (!(w > 0.0))
                    throw ParseError(wl, std::string("weights in the rule for '") + c + "' must be > 0");
                expect(Tok::RBracket, "']' to close the weight");
                weighted = true;
            }
            d.rules[c].push_back(Production{w, rhs});
        } while (match(Tok::Pipe));

        if (d.rules[c].size() > 1 && !weighted) {
            for (auto& p : d.rules[c]) p.weight = 1.0;
        }
        expect(Tok::Semi, std::string("';' after the rule for '") + c + "'");
    }

    SceneDecl sceneDecl() {
        SceneDecl s;
        s.line = line();
        expect(Tok::KwScene, "'scene'");
        s.name = expect(Tok::Ident, "a scene name after 'scene'").text;
        expect(Tok::LBrace, "'{' after the scene name");
        while (kind() != Tok::RBrace && kind() != Tok::End) s.items.push_back(placement());
        expect(Tok::RBrace, "'}' to close the scene");
        if (s.items.empty()) throw ParseError(s.line, "scene '" + s.name + "' places nothing");
        return s;
    }

    Placement placement() {
        Placement p;
        p.line = line();
        if (match(Tok::KwPlace)) {
            p.kind = Placement::Place;
            p.name = expect(Tok::Ident, "a fractal name after 'place'").text;
            expect(Tok::KwAt, "'at' after the fractal name");
            expect(Tok::LParen, "'(' after 'at'");
            p.x = number("x");
            expect(Tok::Comma, "',' between coordinates");
            p.y = number("y");
            expect(Tok::Comma, "',' between coordinates");
            p.z = number("z");
            expect(Tok::RParen, "')' to close the position");
            for (;;) {
                if (match(Tok::KwYaw))        p.yaw = number("yaw");
                else if (match(Tok::KwScale)) p.scale = positive("scale");
                else break;
            }
        } else if (match(Tok::KwScatter)) {
            p.kind  = Placement::Scatter;
            p.name  = expect(Tok::Ident, "a fractal name after 'scatter'").text;
            int cl  = line();
            p.count = intNumber("the instance count");
            if (p.count < 1 || p.count > 100000)
                throw ParseError(cl, "scatter count must be between 1 and 100000");
            expect(Tok::KwWithin, "'within' after the instance count");
            p.radius = positive("radius");
            for (;;) {
                if (match(Tok::KwClear))       p.clear = number("clear");
                else if (match(Tok::KwSeed))   p.seed  = seedNumber("seed");
                else if (match(Tok::KwScale))  p.scale = positive("scale");
                else if (match(Tok::KwClumps)) p.clumps = intNumber("clumps");
                else if (match(Tok::KwVary))   p.vary  = number("vary");
                else break;
            }
            if (p.clear < 0.0 || p.clear >= p.radius)
                throw ParseError(p.line, "'clear' must be between 0 and the radius");
            if (p.clumps < 0 || p.clumps > 10000)
                throw ParseError(p.line, "'clumps' must be between 0 and 10000");
            if (p.vary < 0.0 || p.vary >= 1.0)
                throw ParseError(p.line, "'vary' must be at least 0 and below 1");
        } else {
            throw ParseError(line(), "expected 'place' or 'scatter' in the scene, found " +
                                     describe(cur()));
        }
        expect(Tok::Semi, "';' after the placement");
        return p;
    }

    static bool isHexColor(const std::string& c) {
        if (c.size() != 7 || c[0] != '#') return false;
        for (size_t i = 1; i < 7; ++i)
            if (!std::isxdigit(static_cast<unsigned char>(c[i]))) return false;
        return true;
    }

    static void checkBrackets(const std::string& s, int ln, const std::string& where) {
        std::vector<size_t> open;
        for (size_t i = 0; i < s.size(); ++i) {
            if (s[i] == '[') {
                open.push_back(i);
            } else if (s[i] == ']') {
                if (open.empty())
                    throw ParseError(ln, "unbalanced brackets in " + where + ": ']' at position " +
                                         std::to_string(i + 1) + " has no matching '['");
                open.pop_back();
            }
        }
        if (!open.empty())
            throw ParseError(ln, "unbalanced brackets in " + where + ": '[' at position " +
                                 std::to_string(open.back() + 1) + " is never closed");
    }

    void colon(const std::string& field) { expect(Tok::Colon, "':' after '" + field + "'"); }
    void semi (const std::string& field) { expect(Tok::Semi,  "';' after the '" + field + "' value"); }

    double number(const std::string& field) {
        return expect(Tok::Number, "a number for '" + field + "'").num;
    }

    double positive(const std::string& field) {
        int ln = line();
        double v = number(field);
        if (!(v > 0.0)) throw ParseError(ln, "'" + field + "' must be > 0");
        return v;
    }

    int intNumber(const std::string& field) {
        int    ln = line();
        double v  = number(field);
        if (v != static_cast<double>(static_cast<int>(v)))
            throw ParseError(ln, "'" + field + "' must be a whole number");
        return static_cast<int>(v);
    }

    std::uint64_t seedNumber(const std::string& field) {
        int    ln = line();
        double v  = number(field);
        if (v < 0 || v > 9007199254740992.0 || v != std::floor(v))
            throw ParseError(ln, "'" + field + "' must be a whole number >= 0");
        return static_cast<std::uint64_t>(v);
    }

    static void validate(const FractalDecl& d) {
        if (d.axiom.empty())    throw ParseError(0, "fractal '" + d.name + "' has no axiom");
        if (d.iterations < 0)   throw ParseError(0, "iterations must be >= 0");
        if (d.step <= 0.0)      throw ParseError(0, "step must be > 0");
        if (d.iterations > 60)  throw ParseError(0, "iterations must be <= 60");
        if (d.width <= 0.0)     throw ParseError(0, "width must be > 0");
        if (!(d.taper > 0.0 && d.taper <= 1.0))
            throw ParseError(0, "taper must be in (0, 1]");
    }

    static void validateFile(const SourceFile& f) {
        std::set<std::string> names;
        for (const auto& d : f.fractals)
            if (!names.insert(d.name).second)
                throw ParseError(d.line, "duplicate fractal name '" + d.name + "'");
        if (!f.hasScene) {
            if (f.fractals.size() > 1)
                throw ParseError(f.fractals[1].line,
                                 "a file with several fractals needs a 'scene' to place them");
            return;
        }
        for (const auto& d : f.fractals)
            if (d.dimensions != 3)
                throw ParseError(d.line, "fractal '" + d.name +
                                         "' is used in a scene, so it needs 'dimensions: 3;'");
        for (const auto& p : f.scene.items)
            if (!f.find(p.name))
                throw ParseError(p.line, "scene places unknown fractal '" + p.name + "'");
    }
};

inline FractalDecl parseSource(const std::string& src) {
    return Parser(Lexer(src).tokenize()).parseProgram();
}

inline SourceFile parseFile(const std::string& src) {
    return Parser(Lexer(src).tokenize()).parseFile();
}

inline void dumpAst(std::ostream& os, const FractalDecl& d) {
    os << "fractal " << d.name << " {\n";
    os << "  axiom      : \"" << d.axiom << "\"\n";
    os << "  rules      :\n";
    for (const auto& kv : d.rules)
        for (const auto& p : kv.second)
            os << "      " << kv.first << " -> \"" << p.text << "\"  [w=" << p.weight << "]\n";
    os << "  angle      : " << d.angle << "\n";
    os << "  iterations : " << d.iterations << "\n";
    os << "  step       : " << d.step << "\n";
    os << "  start      : (" << d.startX << ", " << d.startY << ")\n";
    os << "  heading    : " << d.heading << "\n";
    if (d.dimensions != 2) os << "  dimensions : " << d.dimensions << "\n";
    if (d.seed)            os << "  seed       : " << d.seed << "\n";
    if (d.dimensions == 3) {
        os << "  width      : " << d.width << "\n";
        os << "  taper      : " << d.taper << "\n";
    }
    if (!d.colors.empty()) {
        os << "  colors     :";
        for (const auto& c : d.colors) os << " " << c;
        os << "\n";
    }
    os << "}\n";
}

inline void dumpScene(std::ostream& os, const SceneDecl& s) {
    os << "scene " << s.name << " {\n";
    for (const auto& p : s.items) {
        if (p.kind == Placement::Place)
            os << "  place " << p.name << " at (" << p.x << ", " << p.y << ", " << p.z
               << ") yaw " << p.yaw << " scale " << p.scale << "\n";
        else
            os << "  scatter " << p.name << " " << p.count << " within " << p.radius
               << " clear " << p.clear << " clumps " << p.clumps << " vary " << p.vary
               << " seed " << p.seed << " scale " << p.scale << "\n";
    }
    os << "}\n";
}

}
