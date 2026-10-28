
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <set>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "frontend.hpp"
#include "json.hpp"

using namespace fractl;

namespace fractl {
inline std::ostream& operator<<(std::ostream& os, Tok t) { return os << to_string(t); }
inline std::ostream& operator<<(std::ostream& os, Op o)  { return os << to_string(o); }
}

static int g_failed = 0, g_ran = 0;

#define CHECK(cond)                                                              \
    do {                                                                         \
        ++g_ran;                                                                 \
        if (!(cond)) {                                                           \
            ++g_failed;                                                          \
            std::printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);        \
        }                                                                        \
    } while (0)

#define CHECK_EQ(got, want)                                                      \
    do {                                                                         \
        ++g_ran;                                                                 \
        auto g_ = (got);                                                         \
        auto w_ = (want);                                                        \
        if (!(g_ == w_)) {                                                       \
            ++g_failed;                                                          \
            std::ostringstream os_;                                              \
            os_ << "  FAIL " << __FILE__ << ":" << __LINE__ << "  " << #got      \
                << "\n       got : " << g_ << "\n       want: " << w_ << "\n";   \
            std::fputs(os_.str().c_str(), stdout);                               \
        }                                                                        \
    } while (0)

template <typename F>
static void checkThrows(F fn, const std::string& needle, int line) {
    ++g_ran;
    try {
        fn();
    } catch (const std::exception& e) {
        if (std::string(e.what()).find(needle) == std::string::npos) {
            ++g_failed;
            std::printf("  FAIL %s:%d  message \"%s\" does not contain \"%s\"\n",
                        __FILE__, line, e.what(), needle.c_str());
        }
        return;
    }
    ++g_failed;
    std::printf("  FAIL %s:%d  expected an error containing \"%s\", none thrown\n",
                __FILE__, line, needle.c_str());
}
#define CHECK_THROWS(expr, needle) checkThrows([&] { expr; }, needle, __LINE__)

static const char* kKoch = R"FR(
fractal Koch {
    axiom: "F--F--F";
    rules { F -> "F+F--F+F"; }
    angle: 60;
    iterations: 2;
    step: 4;
    start: (150, 350);
    heading: 0;
}
)FR";


static void testLexer() {
    std::printf("lexer\n");

    auto toks = Lexer(kKoch).tokenize();
    CHECK_EQ(toks.front().kind, Tok::KwFractal);
    CHECK_EQ(toks.back().kind, Tok::End);
    CHECK_EQ(toks[1].kind, Tok::Ident);
    CHECK_EQ(toks[1].text, std::string("Koch"));
    CHECK_EQ(toks[1].line, 2);

    auto t2 = Lexer("-> | [ ] , ( ) : ; { } 1e-3 -90 .5 \"a\\\"b\"").tokenize();
    CHECK_EQ(t2[0].kind, Tok::Arrow);
    CHECK_EQ(t2[1].kind, Tok::Pipe);
    CHECK_EQ(t2[2].kind, Tok::LBracket);
    CHECK_EQ(t2[3].kind, Tok::RBracket);
    CHECK_EQ(t2[11].num, 0.001);
    CHECK_EQ(t2[12].num, -90.0);
    CHECK_EQ(t2[13].num, 0.5);
    CHECK_EQ(t2[14].text, std::string("a\"b"));

    auto t3 = Lexer("// one\n# two\n/* three\n   four */\nangle").tokenize();
    CHECK_EQ(t3[0].kind, Tok::KwAngle);
    CHECK_EQ(t3[0].line, 5);

    CHECK_THROWS(Lexer("\"oops").tokenize(),      "line 1: unterminated string");
    CHECK_THROWS(Lexer("\n\n/* x").tokenize(),    "line 3: unterminated /* comment");
    CHECK_THROWS(Lexer("\n$").tokenize(),         "line 2: unexpected character '$'");
    CHECK_THROWS(Lexer("a - b").tokenize(),       "line 1: unexpected '-'");

    auto bom = Lexer("\xEF\xBB\xBF" "fractal").tokenize();
    CHECK_EQ(bom[0].kind, Tok::KwFractal);
    CHECK_EQ(bom[0].line, 1);
    CHECK_EQ(parseSource(std::string("\xEF\xBB\xBF") + kKoch).name, std::string("Koch"));
}


static void testParser() {
    std::printf("parser\n");

    FractalDecl d = parseSource(kKoch);
    CHECK_EQ(d.name, std::string("Koch"));
    CHECK_EQ(d.axiom, std::string("F--F--F"));
    CHECK_EQ(d.rules.size(), size_t(1));
    CHECK_EQ(d.rules.at('F').size(), size_t(1));
    CHECK_EQ(d.rules.at('F')[0].text, std::string("F+F--F+F"));
    CHECK_EQ(d.rules.at('F')[0].weight, 1.0);
    CHECK_EQ(d.angle, 60.0);
    CHECK_EQ(d.iterations, 2);
    CHECK_EQ(d.step, 4.0);
    CHECK_EQ(d.startX, 150.0);
    CHECK_EQ(d.startY, 350.0);
    CHECK_EQ(d.heading, 0.0);

    FractalDecl d2 = parseSource(
        "fractal R { iterations: 1; angle: 90; step: 2; axiom: \"F\";"
        " rules: { F -> \"F+F\"; G -> \"GG\"; } heading: -90; start: (1, -2); }");
    CHECK_EQ(d2.rules.size(), size_t(2));
    CHECK_EQ(d2.heading, -90.0);
    CHECK_EQ(d2.startY, -2.0);

    CHECK_THROWS(parseSource("fractal K {\n  angle 60;\n}"),
                 "line 2: expected ':' after 'angle'");
    CHECK_THROWS(parseSource("fractal K {\n  axiom: \"F\"\n}"),
                 "line 3: expected ';' after the 'axiom' value");
    CHECK_THROWS(parseSource("fractal K {\n  axiom: \"F\";\n  axiom: \"G\";\n}"),
                 "line 3: duplicate 'axiom' field");
    CHECK_THROWS(parseSource("fractal K {\n  rules { F -> \"F\"; F -> \"G\"; }\n}"),
                 "line 2: duplicate rule for symbol 'F'");
    CHECK_THROWS(parseSource("fractal K {\n  rules { FF -> \"F\"; }\n}"),
                 "rule symbol must be a single character");
    CHECK_THROWS(parseSource("fractal K {\n  colour: 3;\n}"),
                 "line 2: expected a field keyword");
    CHECK_THROWS(parseSource("fractal K {\n  iterations: 1.5;\n}"),
                 "'iterations' must be a whole number");
    CHECK_THROWS(parseSource("fractal K { angle: 60; }"),
                 "has no axiom");
    CHECK_THROWS(parseSource("fractal K { axiom: \"F\"; step: 0; }"),
                 "step must be > 0");
    CHECK_THROWS(parseSource("koch { }"),
                 "line 1: expected 'fractal' at the start of the file");
    CHECK_THROWS(parseSource("fractal K { axiom: \"F\"; } fractal L { }"),
                 "expected end of file");
}


static void testBrackets() {
    std::printf("brackets\n");

    FractalDecl ok = parseSource(
        "fractal B { axiom: \"[F][[F]F]\"; rules { X -> \"F[+X][-X]FX\"; F -> \"FF\"; }"
        " angle: 25; iterations: 1; step: 1; }");
    CHECK_EQ(ok.axiom, std::string("[F][[F]F]"));
    CHECK_EQ(ok.rules.at('X')[0].text, std::string("F[+X][-X]FX"));

    CHECK_THROWS(parseSource("fractal B {\n  axiom: \"F]\";\n}"),
                 "line 2: unbalanced brackets in the axiom: ']' at position 2 has no matching '['");
    CHECK_THROWS(parseSource("fractal B {\n\n  axiom: \"[F[F]\";\n}"),
                 "line 3: unbalanced brackets in the axiom: '[' at position 1 is never closed");
    CHECK_THROWS(parseSource("fractal B {\n  axiom: \"X\";\n  rules {\n    X -> \"F[+X\";\n  }\n}"),
                 "line 4: unbalanced brackets in the rule for 'X': '[' at position 2 is never closed");
    CHECK_THROWS(parseSource("fractal B {\n  axiom: \"X\";\n  rules { X -> \"][\"; }\n}"),
                 "line 3: unbalanced brackets in the rule for 'X': ']' at position 1");
    CHECK_THROWS(parseSource("fractal B { axiom: \"F\"; rules { F -> \"F]F[\"; } }"),
                 "no matching '['");

    FractalDecl nest = parseSource(
        "fractal N { axiom: \"X\"; rules { X -> \"F[X]\"; } angle: 30; iterations: 5; step: 1; }");
    std::string e = expand(nest);
    CHECK_EQ(e, std::string("F[F[F[F[F[X]]]]]"));
    CHECK_EQ(maxBranchDepth(lower(nest, e)), size_t(5));
}


static void testRewriter() {
    std::printf("rewriter\n");

    FractalDecl d = parseSource(kKoch);

    d.iterations = 0;
    CHECK_EQ(expand(d), std::string("F--F--F"));
    d.iterations = 1;
    CHECK_EQ(expand(d), std::string("F+F--F+F--F+F--F+F--F+F--F+F"));
    d.iterations = 2;
    CHECK_EQ(expand(d).size(), size_t(3 * 16 + 4 * 15 + 4));
    d.iterations = 4;
    CHECK_EQ(expand(d).size(), size_t(1792));

    FractalDecl id = parseSource(
        "fractal I { axiom: \"A+B-A\"; rules { A -> \"AB\"; } angle: 90;"
        " iterations: 1; step: 1; }");
    CHECK_EQ(expand(id), std::string("AB+B-AB"));

    FractalDecl par = parseSource(
        "fractal P { axiom: \"A\"; rules { A -> \"AA\"; } angle: 90;"
        " iterations: 3; step: 1; }");
    CHECK_EQ(expand(par), std::string("AAAAAAAA"));

    FractalDecl mut = parseSource(
        "fractal M { axiom: \"X\"; rules { X -> \"XY\"; Y -> \"X\"; } angle: 90;"
        " iterations: 4; step: 1; }");
    CHECK_EQ(expand(mut), std::string("XYXXYXYX"));

    FractalDecl boom = parseSource(
        "fractal B { axiom: \"F\"; rules { F -> \"FFFF\"; } angle: 90;"
        " iterations: 40; step: 1; }");
    CHECK_THROWS(expand(boom), "expansion exceeded");
}


static void testLowering() {
    std::printf("lowering\n");

    FractalDecl d = parseSource(kKoch);
    d.step  = 7;
    d.angle = 31;

    Program p = lower(d, "FG+-X[]fYA");
    CHECK_EQ(p.size(), size_t(7));
    CHECK_EQ(p[0].op, Op::Forward);
    CHECK_EQ(p[0].arg, 7.0);
    CHECK_EQ(p[1].op, Op::Forward);
    CHECK_EQ(p[2].op, Op::TurnLeft);
    CHECK_EQ(p[2].arg, 31.0);
    CHECK_EQ(p[3].op, Op::TurnRight);
    CHECK_EQ(p[3].arg, 31.0);
    CHECK_EQ(p[4].op, Op::Push);
    CHECK_EQ(p[4].arg, 0.0);
    CHECK_EQ(p[5].op, Op::Pop);
    CHECK_EQ(p[6].op, Op::Move);
    CHECK_EQ(p[6].arg, 7.0);

    CHECK_EQ(lower(d, "").size(), size_t(0));
    CHECK_EQ(lower(d, "XYAB").size(), size_t(0));

    CHECK_EQ(maxBranchDepth(lower(d, "")), size_t(0));
    CHECK_EQ(maxBranchDepth(lower(d, "F[F[F]][F]")), size_t(2));
    CHECK_EQ(maxBranchDepth(lower(d, "[[[F]]][F]")), size_t(3));

    d = parseSource(kKoch);
    Program koch2 = lower(d, expand(d));
    size_t fwd = 0;
    for (const auto& c : koch2) if (c.op == Op::Forward) ++fwd;
    CHECK_EQ(fwd, size_t(3 * 16));
}


static bool readFile(const std::string& path, std::string* out) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    std::ostringstream ss;
    ss << in.rdbuf();
    *out = ss.str();
    return true;
}

static void testDemoFiles(const std::string& dir) {
    std::printf("demo .fr files (%s)\n", dir.c_str());

    struct Expect {
        const char* file; const char* name;
        size_t cmds, forwards, moves, pushes, depth;
        bool stage1;
    };
    const Expect expects[] = {
        {"triangle.fr", "Triangle",     7,    3,   0,    0,  0, true},
        {"koch.fr",     "Koch",      1792,  768,   0,    0,  0, true},
        {"dragon.fr",   "Dragon",   12286, 4096,   0,    0,  0, false},
        {"plant.fr",    "Plant",    21063, 6048,   0, 4095, 12, false},
        {"tree.fr",     "Tree",     11769, 4118,   0, 2186,  7, false},
        {"carpet.fr",   "Carpet",    9579, 4096, 803,    0,  0, false},
    };

    for (const auto& e : expects) {
        std::string src;
        if (!readFile(dir + "/" + e.file, &src)) {
            ++g_ran; ++g_failed;
            std::printf("  FAIL cannot open %s/%s\n", dir.c_str(), e.file);
            continue;
        }
        Frontend fe = compileFrontend(src);
        CHECK_EQ(fe.decl.name, std::string(e.name));
        CHECK_EQ(fe.program.size(), e.cmds);
        size_t fwd = 0, mov = 0, push = 0, pop = 0;
        for (const auto& c : fe.program) {
            if (c.op == Op::Forward) ++fwd;
            if (c.op == Op::Move)    ++mov;
            if (c.op == Op::Push)    ++push;
            if (c.op == Op::Pop)     ++pop;
        }
        CHECK_EQ(fwd, e.forwards);
        CHECK_EQ(mov, e.moves);
        CHECK_EQ(push, e.pushes);
        CHECK_EQ(pop, e.pushes);
        CHECK_EQ(maxBranchDepth(fe.program), e.depth);

        long depth = 0;
        for (const auto& c : fe.program) {
            if (c.op == Op::Push) ++depth;
            if (c.op == Op::Pop)  --depth;
            CHECK(depth >= 0);
            if (e.stage1)
                CHECK(c.op == Op::Forward || c.op == Op::TurnLeft || c.op == Op::TurnRight);
        }
        CHECK_EQ(depth, 0L);
    }
}


static bool has(const std::string& hay, const std::string& needle) {
    return hay.find(needle) != std::string::npos;
}

static void testJson() {
    std::printf("json export\n");

    CHECK_EQ(json::escape("a\"b\\c\nd\te"), std::string("a\\\"b\\\\c\\nd\\te"));
    CHECK_EQ(json::escape(std::string(1, '\x01')), std::string("\\u0001"));
    CHECK_EQ(json::num(60.0), std::string("60"));
    CHECK_EQ(json::num(-90.5), std::string("-90.5"));

    CHECK_EQ(tokKindLabel(Tok::KwFractal), std::string("fractal"));
    CHECK_EQ(tokKindLabel(Tok::LBrace), std::string("{"));
    CHECK_EQ(tokKindLabel(Tok::Ident), std::string("identifier"));

    Frontend fe = compileFrontend(kKoch);
    std::ostringstream os;
    writeJson(os, "koch", kKoch, Lexer(kKoch).tokenize(), fe.decl, fe.expanded, fe.program);
    const std::string j = os.str();

    for (const char* key : {"\"name\"", "\"source\"", "\"tokens\"", "\"ast\"", "\"rules\"",
                            "\"expanded\"", "\"preview\"", "\"fullLength\"", "\"full\"",
                            "\"program\"", "\"programLength\"", "\"programTruncated\"",
                            "\"stats\"", "\"symbolCounts\"", "\"opCounts\""})
        CHECK(has(j, key));

    CHECK(has(j, "\"fullLength\": 112"));
    CHECK(has(j, "\"programLength\": 112"));
    CHECK(has(j, "\"programTruncated\": false"));
    CHECK(has(j, "\"FORWARD\": 48"));
    CHECK(has(j, "\"iterations\": 2"));
    CHECK(has(j, "\"angle\": 60"));
    CHECK(has(j, "\"weight\": 1"));
    for (const char* key : {"\"dimensions\": 2", "\"seed\": 0", "\"width\": 1", "\"taper\": 0.7", "\"colors\": []"})
        CHECK(has(j, key));
    for (const char* op : {"\"MOVE\": 0", "\"PUSH\": 0", "\"POP\": 0"}) CHECK(has(j, op));
    CHECK(!has(j, "\"source\": \"\nfractal"));
    CHECK(has(j, "\\\"F--F--F\\\""));

    FractalDecl big = parseSource(kKoch);
    big.iterations = 4;
    std::string bigExp = expand(big);
    std::ostringstream os2;
    writeJson(os2, "koch", "", Lexer(kKoch).tokenize(), big, bigExp, lower(big, bigExp));
    CHECK(has(os2.str(), "\"truncated\": true"));
    CHECK(has(os2.str(), "\"fullLength\": 1792"));
    CHECK(has(j, "\"maxDepth\": 0"));

    const char* branchy = "fractal P { axiom: \"F[+F][-F[f]]\"; angle: 30; step: 2; }";
    Frontend bp = compileFrontend(branchy);
    std::ostringstream os3;
    writeJson(os3, "p", branchy, Lexer(branchy).tokenize(), bp.decl, bp.expanded, bp.program);
    const std::string j3 = os3.str();
    for (const char* op : {"\"PUSH\": 3", "\"POP\": 3", "\"MOVE\": 1", "\"FORWARD\": 3",
                           "\"maxDepth\": 2", "{\"op\": \"PUSH\", \"arg\": 0}"})
        CHECK(has(j3, op));
}

static void testStochastic() {
    std::printf("stochastic rules\n");

    FractalDecl d = parseSource(
        "fractal S {\n seed: 9;\n axiom: \"F\";\n"
        " rules { F -> \"F+F\" [0.2] | \"F-F\" [0.8]; X -> \"A\" | \"B\" | \"C\"; }\n"
        " angle: 30; iterations: 3; step: 1; }");
    CHECK_EQ(d.seed, std::uint64_t(9));
    CHECK_EQ(d.rules.at('F').size(), size_t(2));
    CHECK_EQ(d.rules.at('F')[0].weight, 0.2);
    CHECK_EQ(d.rules.at('F')[1].text, std::string("F-F"));
    CHECK_EQ(d.rules.at('X').size(), size_t(3));
    CHECK_EQ(d.rules.at('X')[2].weight, 1.0);
    CHECK(isStochastic(d));
    CHECK(!isStochastic(parseSource(kKoch)));

    CHECK_EQ(expand(d), expand(d));
    CHECK_EQ(expand(d, 9), expand(d));
    bool differs = false;
    for (std::uint64_t s = 1; s < 20 && !differs; ++s) differs = expand(d, s) != expand(d);
    CHECK(differs);

    RuleSet rs{{0.2, "a"}, {0.8, "b"}};
    CHECK_EQ(chooseProduction(rs, 0.0), size_t(0));
    CHECK_EQ(chooseProduction(rs, 0.1999), size_t(0));
    CHECK_EQ(chooseProduction(rs, 0.2001), size_t(1));
    CHECK_EQ(chooseProduction(rs, 0.9999999), size_t(1));

    int firsts = 0;
    for (std::uint64_t i = 0; i < 20000; ++i) {
        double r = unitRandom(7, 0, i);
        CHECK(r >= 0.0 && r < 1.0);
        if (chooseProduction(rs, r) == 0) ++firsts;
    }
    CHECK(firsts > 3700 && firsts < 4300);
    CHECK(unitRandom(1, 0, 0) != unitRandom(1, 0, 1));
    CHECK(unitRandom(1, 0, 0) != unitRandom(1, 1, 0));
    CHECK(unitRandom(1, 0, 0) != unitRandom(2, 0, 0));
    CHECK_EQ(mix64(0), std::uint64_t(0xE220A8397B1DCDAFULL));

    CHECK_THROWS(parseSource("fractal S {\n axiom: \"F\";\n rules { F -> \"F\" [0]; }\n}"),
                 "line 3: weights in the rule for 'F' must be > 0");
    CHECK_THROWS(parseSource("fractal S { axiom: \"F\"; rules { F -> \"F\" [1 | \"G\"; } }"),
                 "expected ']' to close the weight");
    CHECK_THROWS(parseSource("fractal S { axiom: \"F\"; rules { F -> \"F\" | ; } }"),
                 "a quoted replacement string for 'F'");
    CHECK_THROWS(parseSource("fractal S {\n axiom: \"F\";\n rules { F -> \"F\" | \"[F\"; }\n}"),
                 "line 3: unbalanced brackets in the rule for 'F'");
    CHECK_THROWS(parseSource("fractal S { seed: -1; axiom: \"F\"; }"), "'seed' must be a whole number >= 0");
    CHECK_THROWS(parseSource("fractal S { seed: 1.5; axiom: \"F\"; }"), "'seed' must be a whole number >= 0");
}

static void testThreeD() {
    std::printf("3d turtle\n");

    FractalDecl d = parseSource(
        "fractal T { dimensions: 3; axiom: \"FG&^\\\\/|!'f[]X\"; angle: 30; step: 2;"
        " width: 0.5; taper: 0.6; colors: (\"#112233\", \"#AABBCC\"); }");
    CHECK_EQ(d.dimensions, 3);
    CHECK_EQ(d.width, 0.5);
    CHECK_EQ(d.taper, 0.6);
    CHECK_EQ(d.colors.size(), size_t(2));
    CHECK_EQ(d.colors[1], std::string("#AABBCC"));
    Program p = lower(d, d.axiom);
    const Op want[] = {Op::Forward, Op::Blade, Op::PitchDown, Op::PitchUp, Op::RollLeft, Op::RollRight,
                       Op::TurnLeft, Op::Thin, Op::NextColor, Op::Move, Op::Push, Op::Pop};
    CHECK_EQ(p.size(), sizeof want / sizeof want[0]);
    for (size_t i = 0; i < p.size() && i < sizeof want / sizeof want[0]; ++i) CHECK_EQ(p[i].op, want[i]);
    if (p.size() == 12) {
        CHECK_EQ(p[1].arg, 2.0);
        CHECK_EQ(p[2].arg, 30.0);
        CHECK_EQ(p[6].arg, 180.0);
        CHECK_EQ(p[7].arg, 0.6);
    }

    FractalDecl flat = parseSource("fractal P { axiom: \"FG&^\\\\/!'|\"; angle: 30; step: 2; }");
    Program q = lower(flat, flat.axiom);
    CHECK_EQ(q.size(), size_t(3));
    if (q.size() == 3) {
        CHECK_EQ(q[0].op, Op::Forward);
        CHECK_EQ(q[1].op, Op::Forward);
        CHECK_EQ(q[2].op, Op::TurnLeft);
        CHECK_EQ(q[2].arg, 180.0);
    }
    CHECK_EQ(std::string(to_string(Op::Blade)), std::string("BLADE"));
    CHECK_EQ(std::string(to_string(Op::NextColor)), std::string("NEXT_COLOR"));

    CHECK_THROWS(parseSource("fractal T {\n dimensions: 4;\n axiom: \"F\";\n}"),
                 "line 2: 'dimensions' must be 2 or 3");
    CHECK_THROWS(parseSource("fractal T { axiom: \"F\"; taper: 0; }"), "taper must be in (0, 1]");
    CHECK_THROWS(parseSource("fractal T { axiom: \"F\"; taper: 1.5; }"), "taper must be in (0, 1]");
    CHECK_THROWS(parseSource("fractal T { axiom: \"F\"; width: 0; }"), "width must be > 0");
    CHECK_THROWS(parseSource("fractal T {\n axiom: \"F\";\n colors: (\"green\");\n}"),
                 "line 3: colour \"green\" must look like \"#4f8a2b\"");
    CHECK_THROWS(parseSource("fractal T { axiom: \"F\"; iterations: 61; }"), "iterations must be <= 60");
}

static const char* kScene = R"FR(
fractal Tree {
    dimensions: 3;
    seed: 4;
    axiom: "FA";
    rules { A -> "[&FA]/////[&FA]" | "[&FA]"; }
    angle: 20;
    iterations: 3;
}
fractal Tuft {
    dimensions: 3;
    axiom: "[&G][^G]";
}
scene Garden {
    place Tree at (1, 0, -2) yaw 45 scale 2;
    scatter Tuft 50 within 10 clear 3 seed 8;
}
)FR";

static void testScene() {
    std::printf("scenes\n");

    SourceFile f = parseFile(kScene);
    CHECK_EQ(f.fractals.size(), size_t(2));
    CHECK(f.hasScene);
    CHECK_EQ(f.scene.name, std::string("Garden"));
    CHECK_EQ(f.scene.items.size(), size_t(2));
    CHECK_EQ(f.dimensions(), 3);
    if (f.scene.items.size() == 2) {
        const Placement& a = f.scene.items[0];
        CHECK(a.kind == Placement::Place);
        CHECK_EQ(a.x, 1.0);
        CHECK_EQ(a.z, -2.0);
        CHECK_EQ(a.yaw, 45.0);
        CHECK_EQ(a.scale, 2.0);
        const Placement& b = f.scene.items[1];
        CHECK(b.kind == Placement::Scatter);
        CHECK_EQ(b.count, 50);
        CHECK_EQ(b.radius, 10.0);
        CHECK_EQ(b.clear, 3.0);
        CHECK_EQ(b.seed, std::uint64_t(8));
    }

    std::vector<Instance> in = placeInstances(f);
    CHECK_EQ(in.size(), size_t(51));
    if (in.size() == 51) {
        CHECK_EQ(in[0].fractal, size_t(0));
        CHECK_EQ(in[0].seed, std::uint64_t(4));
        CHECK_EQ(in[0].x, 1.0);
        std::set<std::uint64_t> seeds;
        for (size_t i = 1; i < in.size(); ++i) {
            double r = std::sqrt(in[i].x * in[i].x + in[i].z * in[i].z);
            CHECK(r >= 3.0 - 1e-9 && r <= 10.0 + 1e-9);
            CHECK(in[i].yaw >= 0.0 && in[i].yaw < 360.0);
            CHECK(in[i].scale >= 0.8 && in[i].scale <= 1.2);
            CHECK_EQ(in[i].fractal, size_t(1));
            seeds.insert(in[i].seed);
        }
        CHECK_EQ(seeds.size(), size_t(50));
        std::vector<Instance> again = placeInstances(parseFile(kScene));
        CHECK_EQ(again[17].x, in[17].x);
        CHECK_EQ(again[17].seed, in[17].seed);
    }

    SourceFile single = parseFile("fractal T { dimensions: 3; axiom: \"F\"; start: (2, 3); heading: 30; }");
    std::vector<Instance> one = placeInstances(single);
    CHECK_EQ(one.size(), size_t(1));
    if (one.size() == 1) { CHECK_EQ(one[0].x, 2.0); CHECK_EQ(one[0].z, 3.0); CHECK_EQ(one[0].yaw, 30.0); }
    CHECK_EQ(placeInstances(parseFile(kKoch)).size(), size_t(0));
    CHECK_EQ(parseFile(kKoch).fractals[0].name, std::string("Koch"));

    CHECK_THROWS(parseFile("fractal A { axiom: \"F\"; }\nfractal B { axiom: \"F\"; }"),
                 "line 2: a file with several fractals needs a 'scene'");
    CHECK_THROWS(parseFile("fractal A { dimensions: 3; axiom: \"F\"; }\nscene S {\n place B at (0,0,0);\n}"),
                 "line 3: scene places unknown fractal 'B'");
    CHECK_THROWS(parseFile("fractal A {\n axiom: \"F\";\n}\nscene S { place A at (0,0,0); }"),
                 "line 1: fractal 'A' is used in a scene, so it needs 'dimensions: 3;'");
    CHECK_THROWS(parseFile("fractal A { dimensions: 3; axiom: \"F\"; }\nfractal A { dimensions: 3; axiom: \"F\"; }\n"
                           "scene S { place A at (0,0,0); }"),
                 "line 2: duplicate fractal name 'A'");
    CHECK_THROWS(parseFile("fractal A { dimensions: 3; axiom: \"F\"; }\nscene S {\n scatter A 5 within 4 clear 4;\n}"),
                 "line 3: 'clear' must be between 0 and the radius");
    CHECK_THROWS(parseFile("fractal A { dimensions: 3; axiom: \"F\"; }\nscene S { }"), "scene 'S' places nothing");
    CHECK_THROWS(parseFile("fractal A { dimensions: 3; axiom: \"F\"; }\nscene S { grow A; }"),
                 "expected 'place' or 'scatter' in the scene");
    CHECK_THROWS(parseFile("fractal A { dimensions: 3; axiom: \"F\"; }\nscene S { scatter A 0 within 4; }"),
                 "scatter count must be between 1 and 100000");

    const char* patchy = "fractal A { dimensions: 3; axiom: \"F\"; }\n"
                         "scene S { scatter A 400 within 20 clumps 5 vary 0.5 seed 3; }";
    SourceFile pf = parseFile(patchy);
    CHECK_EQ(pf.scene.items[0].clumps, 5);
    CHECK_EQ(pf.scene.items[0].vary, 0.5);
    std::vector<Instance> pi = placeInstances(pf), pj = placeInstances(parseFile(patchy));
    CHECK_EQ(pi.size(), size_t(400));
    double minS = 1e9, maxS = 0, near = 0;
    for (size_t i = 0; i < pi.size(); ++i) {
        minS = std::min(minS, pi[i].scale);
        maxS = std::max(maxS, pi[i].scale);
        CHECK_EQ(pi[i].x, pj[i].x);
        CHECK(std::sqrt(pi[i].x * pi[i].x + pi[i].z * pi[i].z) < 40.0);
    }
    CHECK(minS >= 0.5 * 0.7 - 1e-9 && maxS <= 1.5 * 1.25 + 1e-9);
    CHECK(maxS / minS > 2.0);
    for (size_t i = 0; i < pi.size(); ++i)
        for (size_t j = i + 1; j < pi.size(); ++j)
            if (std::hypot(pi[i].x - pi[j].x, pi[i].z - pi[j].z) < 1.5) ++near;
    SourceFile uf = parseFile("fractal A { dimensions: 3; axiom: \"F\"; }\nscene S { scatter A 400 within 20 seed 3; }");
    std::vector<Instance> ui = placeInstances(uf);
    double nearU = 0;
    for (size_t i = 0; i < ui.size(); ++i)
        for (size_t j = i + 1; j < ui.size(); ++j)
            if (std::hypot(ui[i].x - ui[j].x, ui[i].z - ui[j].z) < 1.5) ++nearU;
    CHECK(near > nearU * 2.0);
    CHECK_THROWS(parseFile("fractal A { dimensions: 3; axiom: \"F\"; }\nscene S { scatter A 5 within 4 clumps -1; }"),
                 "'clumps' must be between 0 and 10000");
    CHECK_THROWS(parseFile("fractal A { dimensions: 3; axiom: \"F\"; }\nscene S { scatter A 5 within 4 vary 1; }"),
                 "'vary' must be at least 0 and below 1");
}

int main(int argc, char** argv) {
    std::string dir = argc > 1 ? argv[1] : "..";

    testLexer();
    testParser();
    testBrackets();
    testRewriter();
    testLowering();
    testJson();
    testStochastic();
    testThreeD();
    testScene();
    testDemoFiles(dir);

    std::printf("\n%d checks, %d failed\n", g_ran, g_failed);
    return g_failed == 0 ? 0 : 1;
}
