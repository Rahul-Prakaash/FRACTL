#pragma once

#include <cstdint>
#include <string>
#include <map>
#include <vector>

namespace fractl {

struct Production {
    double      weight;
    std::string text;
};

using RuleSet = std::vector<Production>;

struct FractalDecl {
    std::string name;
    std::string axiom;
    std::map<char, RuleSet> rules;

    double angle      = 0.0;
    int    iterations = 0;
    double step       = 1.0;
    double startX     = 0.0;
    double startY     = 0.0;
    double heading     = 0.0;

    int           dimensions = 2;
    std::uint64_t seed       = 0;
    double        width      = 1.0;
    double        taper      = 0.7;
    std::vector<std::string> colors;
    int           line       = 0;
};

struct Placement {
    enum Kind { Place, Scatter };
    Kind          kind = Place;
    std::string   name;
    double        x = 0.0, y = 0.0, z = 0.0;
    double        yaw    = 0.0;
    double        scale  = 1.0;
    int           count  = 1;
    double        radius = 0.0;
    double        clear  = 0.0;
    int           clumps = 0;
    double        vary   = 0.2;
    std::uint64_t seed   = 0;
    int           line   = 0;
};

struct SceneDecl {
    std::string            name;
    std::vector<Placement> items;
    int                    line = 0;
};

struct SourceFile {
    std::vector<FractalDecl> fractals;
    bool                     hasScene = false;
    SceneDecl                scene;

    const FractalDecl* find(const std::string& name) const {
        for (const auto& f : fractals) if (f.name == name) return &f;
        return nullptr;
    }
    int dimensions() const { return fractals.empty() ? 2 : fractals.front().dimensions; }
};

}
