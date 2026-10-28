
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "frontend.hpp"
#include "json.hpp"

using namespace fractl;

namespace {

std::string readFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("cannot open '" + path + "'");
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

void printTokens(const std::vector<Token>& toks) {
    std::cout << "--- tokens ---\n";
    for (const auto& t : toks) {
        std::printf("  line %-3d %-12s", t.line, to_string(t.kind));
        if (t.kind == Tok::Number)      std::printf("  %g", t.num);
        else if (t.kind == Tok::String) std::printf("  \"%s\"", t.text.c_str());
        else if (t.kind == Tok::Ident)  std::printf("  %s", t.text.c_str());
        std::printf("\n");
    }
}

void usage() {
    std::cerr << "usage: fr_frontend <file.fr> [--tokens] [--ast] [--expand] [--ir]\n"
                 "                             [--stats] [--all]\n"
                 "                             [--json <path>] [--name <key>]\n"
                 "                             [--iterations <n>] [--fractal <name>]\n";
}

std::string stem(const std::string& path) {
    size_t slash = path.find_last_of("/\\");
    std::string base = slash == std::string::npos ? path : path.substr(slash + 1);
    size_t dot = base.find_last_of('.');
    return dot == std::string::npos ? base : base.substr(0, dot);
}

}

int main(int argc, char** argv) {
    if (argc < 2) { usage(); return 2; }

    std::string path, jsonPath, demoName, fractalName;
    bool wantTokens = false, wantAst = false, wantExpand = false;
    bool wantIr = false, wantStats = false;
    int  iterOverride = -1;

    auto value = [&](int& i, const std::string& flag) -> std::string {
        if (i + 1 >= argc) { std::cerr << flag << " needs a value\n"; std::exit(2); }
        return argv[++i];
    };

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if      (a == "--tokens")     wantTokens = true;
        else if (a == "--ast")        wantAst    = true;
        else if (a == "--expand")     wantExpand = true;
        else if (a == "--ir")         wantIr     = true;
        else if (a == "--stats")      wantStats  = true;
        else if (a == "--all")        wantTokens = wantAst = wantExpand = wantIr = wantStats = true;
        else if (a == "--json")       jsonPath   = value(i, a);
        else if (a == "--name")       demoName   = value(i, a);
        else if (a == "--iterations") iterOverride = std::atoi(value(i, a).c_str());
        else if (a == "--fractal")    fractalName = value(i, a);
        else if (a.rfind("--", 0) == 0) { std::cerr << "unknown flag " << a << "\n"; usage(); return 2; }
        else if (path.empty())        path = a;
        else { usage(); return 2; }
    }
    if (path.empty()) { usage(); return 2; }
    if (iterOverride < -1) { std::cerr << "--iterations must be >= 0\n"; return 2; }
    if (!wantTokens && !wantAst && !wantExpand && !wantIr && !wantStats && jsonPath.empty())
        wantIr = true;

    try {
        std::string src = readFile(path);

        std::vector<Token> toks = Lexer(src).tokenize();
        if (wantTokens) printTokens(toks);

        SourceFile file = Parser(toks).parseFile();
        const FractalDecl* pick = fractalName.empty() ? &file.fractals.front() : file.find(fractalName);
        if (!pick) throw std::runtime_error("no fractal named '" + fractalName + "' in this file");
        FractalDecl decl = *pick;
        if (iterOverride >= 0) decl.iterations = iterOverride;
        if (wantAst) { std::cout << "--- ast ---\n"; dumpAst(std::cout, decl); }

        std::string expanded = expand(decl);
        if (wantExpand) {
            std::cout << "--- expanded (" << expanded.size() << " symbols) ---\n";
            std::cout << expanded << "\n";
        }

        Program prog = lower(decl, expanded);
        if (wantIr) { std::cout << "--- turtle ir (" << prog.size() << " cmds) ---\n";
                      dumpProgram(std::cout, prog); }

        if (wantStats) {
            size_t fwd = 0, mov = 0, left = 0, right = 0, push = 0, pop = 0, other = 0;
            for (const auto& c : prog) switch (c.op) {
                case Op::Forward:   ++fwd;   break;
                case Op::Move:      ++mov;   break;
                case Op::TurnLeft:  ++left;  break;
                case Op::TurnRight: ++right; break;
                case Op::Push:      ++push;  break;
                case Op::Pop:       ++pop;   break;
                default:            ++other; break;
            }
            std::cout << "--- stats ---\n"
                      << "  axiom symbols   : " << decl.axiom.size() << "\n"
                      << "  expanded symbols: " << expanded.size()   << "\n"
                      << "  turtle commands : " << prog.size()       << "\n"
                      << "  forward=" << fwd << " move=" << mov
                      << " left="     << left << " right=" << right
                      << " push="     << push << " pop="   << pop << "\n"
                      << "  max branch depth: " << maxBranchDepth(prog) << "\n";
            if (other) std::cout << "  3d ops (blade/pitch/roll/thin/color): " << other << "\n";
        }

        if (!jsonPath.empty()) {
            std::ofstream out(jsonPath, std::ios::binary);
            if (!out) throw std::runtime_error("cannot write '" + jsonPath + "'");
            writeJson(out, demoName.empty() ? stem(path) : demoName,
                      src, toks, decl, expanded, prog);
            if (!out) throw std::runtime_error("write failed for '" + jsonPath + "'");
            std::cout << "wrote " << jsonPath << "\n";
        }
        return 0;
    } catch (const std::exception& e) {
        std::cerr << path << ": error: " << e.what() << "\n";
        return 1;
    }
}
