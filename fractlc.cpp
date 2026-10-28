
#include <chrono>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "codegen.hpp"
#include "frontend.hpp"

namespace {

void usage() {
    std::cerr << "usage: fractlc <source.fr> --svg <out.svg> | --out <out.f3d>\n"
                 "               [-o <out.o>] [--jit] [--emit-ir <file.ll>] [--unroll] [--opt] [--timings]\n"
                 "  -o <out.o>          native object file to write (link it with runtime.o)\n"
                 "  --jit               compile in memory with LLVM ORC and run immediately (no -o, no linker)\n"
                 "  --svg / --out <f>   path the compiled program writes its drawing to\n"
                 "                      (.svg for 2D fractals, .f3d segment stream for 3D)\n"
                 "  --emit-ir <file>    also dump the LLVM IR (after --opt, if given)\n"
                 "  --unroll            Stage 2 code generation: one call per turtle command\n"
                 "                      (default: recursive, one function per rule symbol)\n"
                 "  --opt               fold adjacent constant rotations\n"
                 "  --timings           print one line of per-phase timings as JSON\n"
                 "  --ir-only           stop after code generation (use with --emit-ir)\n";
}

std::string readFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("cannot open '" + path + "'");
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

std::string stem(const std::string& path) {
    size_t slash = path.find_last_of("/\\");
    std::string base = slash == std::string::npos ? path : path.substr(slash + 1);
    size_t dot = base.find_last_of('.');
    return dot == std::string::npos ? base : base.substr(0, dot);
}

using Clock = std::chrono::steady_clock;
double msSince(Clock::time_point t) {
    return std::chrono::duration<double, std::milli>(Clock::now() - t).count();
}

}

int main(int argc, char** argv) {
    std::string src, objPath, outPath, irPath;
    bool unroll = false, opt = false, jit = false, timings = false, irOnly = false;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto value = [&](std::string& out) {
            if (i + 1 >= argc) { std::cerr << a << " needs a value\n"; return false; }
            out = argv[++i];
            return true;
        };
        if      (a == "-o")                          { if (!value(objPath)) { usage(); return 2; } }
        else if (a == "--svg" || a == "--out")       { if (!value(outPath)) { usage(); return 2; } }
        else if (a == "--emit-ir")                   { if (!value(irPath))  { usage(); return 2; } }
        else if (a == "--unroll")                    unroll = true;
        else if (a == "--opt")                       opt = true;
        else if (a == "--jit")                       jit = true;
        else if (a == "--timings")                   timings = true;
        else if (a == "--ir-only")                   irOnly = true;
        else if (a == "-h" || a == "--help")         { usage(); return 0; }
        else if (!a.empty() && a[0] == '-')          { std::cerr << "unknown option " << a << "\n"; usage(); return 2; }
        else if (src.empty())                        src = a;
        else                                         { usage(); return 2; }
    }
    if (irOnly && outPath.empty()) outPath = "out";
    if (src.empty() || outPath.empty() || (objPath.empty() && !jit && !irOnly)) { usage(); return 2; }

    auto t = Clock::now();
    fractl::SourceFile file;
    std::vector<fractl::Instance> instances;
    double parseMs = 0;
    try {
        file      = fractl::parseFile(readFile(src));
        instances = fractl::placeInstances(file);
    } catch (const std::exception& e) {
        std::cerr << src << ": error: " << e.what() << "\n";
        return 1;
    }
    parseMs = msSince(t);

    size_t commands = 0;
    if (unroll && file.dimensions() == 2) {
        try {
            const auto& d = file.fractals.front();
            commands = fractl::lower(d, fractl::expand(d)).size();
        } catch (const std::exception& e) {
            std::cerr << src << ": error: " << e.what() << "\n";
            return 1;
        }
    }

    fractl::Backend be(stem(src));
    std::string err;
    fractl::CodegenOptions co;
    co.recursive = !unroll;
    co.outPath   = outPath;
    t = Clock::now();
    try {
        if (!be.codegen(file, instances, co, err)) {
            std::cerr << "fractlc: codegen: " << err << "\n";
            return 1;
        }
    } catch (const std::exception& e) {
        std::cerr << src << ": error: " << e.what() << "\n";
        return 1;
    }
    double codegenMs = msSince(t);

    t = Clock::now();
    unsigned folds = opt ? be.foldTurns() : 0;
    double optMs = msSince(t);
    fractl::IRStats st = be.stats();

    if (!irPath.empty() && !be.emitIR(irPath, err)) {
        std::cerr << "fractlc: " << err << "\n";
        return 1;
    }

    std::string what = unroll
        ? (commands ? std::to_string(commands) + " turtle commands" : std::string("unrolled"))
        : "recursive: " + std::to_string(st.functions) + " functions, " +
          std::to_string(st.instructions) + " IR instructions";
    if (opt) what += ", --opt folded " + std::to_string(folds) + " turns";
    if (!file.fractals.empty() && file.dimensions() == 3)
        what += ", " + std::to_string(instances.size()) + " instance" + (instances.size() == 1 ? "" : "s");

    double backendMs = 0, runMs = 0;
    if (irOnly) {
        std::cout << "fractlc: " << src << " -> IR only (" << what << ")\n";
    } else if (jit) {
        std::cout << "fractlc: " << src << " -> JIT (" << what << ")" << std::endl;
        if (!be.runJIT(err, backendMs, runMs)) {
            std::cerr << "fractlc: " << err << "\n";
            return 1;
        }
    } else {
        t = Clock::now();
        if (!be.emitObject(objPath, err)) {
            std::cerr << "fractlc: " << err << "\n";
            return 1;
        }
        backendMs = msSince(t);
        std::cout << "fractlc: " << src << " -> " << objPath << " (" << what << ")\n";
    }

    if (timings) {
        std::printf("fractlc-timings: {\"parse_ms\": %.3f, \"codegen_ms\": %.3f, \"opt_ms\": %.3f, "
                    "\"%s_ms\": %.3f, \"run_ms\": %.3f, \"functions\": %u, \"instructions\": %u, "
                    "\"calls\": %u, \"folds\": %u, \"instances\": %zu, \"mode\": \"%s\"}\n",
                    parseMs, codegenMs, optMs, jit ? "jit" : "emit", backendMs, runMs,
                    st.functions, st.instructions, st.calls, folds, instances.size(),
                    unroll ? "unroll" : "recursive");
    }
    std::fflush(stdout);
    return 0;
}
