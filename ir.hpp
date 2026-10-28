#pragma once

#include <vector>

namespace fractl {

enum class Op {
    Forward,
    Move,
    TurnLeft,
    TurnRight,
    Push,
    Pop,
    Blade,
    PitchDown,
    PitchUp,
    RollLeft,
    RollRight,
    Thin,
    NextColor
};

constexpr int kOpCount = static_cast<int>(Op::NextColor) + 1;

struct TurtleCmd {
    Op     op;
    double arg = 0.0;
};

using Program = std::vector<TurtleCmd>;

inline const char* to_string(Op op) {
    switch (op) {
        case Op::Forward:   return "FORWARD";
        case Op::Move:      return "MOVE";
        case Op::TurnLeft:  return "TURN_LEFT";
        case Op::TurnRight: return "TURN_RIGHT";
        case Op::Push:      return "PUSH";
        case Op::Pop:       return "POP";
        case Op::Blade:     return "BLADE";
        case Op::PitchDown: return "PITCH_DOWN";
        case Op::PitchUp:   return "PITCH_UP";
        case Op::RollLeft:  return "ROLL_LEFT";
        case Op::RollRight: return "ROLL_RIGHT";
        case Op::Thin:      return "THIN";
        case Op::NextColor: return "NEXT_COLOR";
    }
    return "?";
}

}
