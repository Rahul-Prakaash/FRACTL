#pragma once

#include <string>

#include "ast.hpp"
#include "ir.hpp"
#include "lexer.hpp"
#include "lower.hpp"
#include "parser.hpp"
#include "rewriter.hpp"
#include "scene.hpp"

namespace fractl {

struct Frontend {
    FractalDecl decl;
    std::string expanded;
    Program     program;
};

inline Frontend compileFrontend(const std::string& source) {
    Frontend fe;
    fe.decl     = parseSource(source);
    fe.expanded = expand(fe.decl);
    fe.program  = lower(fe.decl, fe.expanded);
    return fe;
}

}
