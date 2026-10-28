#pragma once

#include <ostream>
#include <string>

#include "ast.hpp"
#include "ir.hpp"

namespace fractl {

inline bool terminalCmd(const FractalDecl& d, char c, TurtleCmd& out) {
    const bool is3d = d.dimensions == 3;
    switch (c) {
        case 'F': out = {Op::Forward,   d.step};  return true;
        case 'G': out = {is3d ? Op::Blade : Op::Forward, d.step}; return true;
        case 'f': out = {Op::Move,      d.step};  return true;
        case '+': out = {Op::TurnLeft,  d.angle}; return true;
        case '-': out = {Op::TurnRight, d.angle}; return true;
        case '|': out = {Op::TurnLeft,  180.0};   return true;
        case '[': out = {Op::Push,      0.0};     return true;
        case ']': out = {Op::Pop,       0.0};     return true;
        default:  break;
    }
    if (!is3d) return false;
    switch (c) {
        case '&':  out = {Op::PitchDown, d.angle}; return true;
        case '^':  out = {Op::PitchUp,   d.angle}; return true;
        case '\\': out = {Op::RollLeft,  d.angle}; return true;
        case '/':  out = {Op::RollRight, d.angle}; return true;
        case '!':  out = {Op::Thin,      d.taper}; return true;
        case '\'': out = {Op::NextColor, 1.0};     return true;
        default:   return false;
    }
}

inline Program lower(const FractalDecl& d, const std::string& expanded) {
    Program prog;
    prog.reserve(expanded.size());
    TurtleCmd cmd{Op::Forward, 0.0};
    for (char c : expanded)
        if (terminalCmd(d, c, cmd)) prog.push_back(cmd);
    return prog;
}

inline size_t maxBranchDepth(const Program& prog) {
    size_t depth = 0, deepest = 0;
    for (const TurtleCmd& c : prog) {
        if (c.op == Op::Push && ++depth > deepest) deepest = depth;
        if (c.op == Op::Pop && depth > 0) --depth;
    }
    return deepest;
}

inline void dumpProgram(std::ostream& os, const Program& prog) {
    for (size_t i = 0; i < prog.size(); ++i)
        os << i << ": " << to_string(prog[i].op) << " " << prog[i].arg << "\n";
}

}
