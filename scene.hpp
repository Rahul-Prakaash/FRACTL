#pragma once

#include <cmath>
#include <algorithm>
#include <cstdint>
#include <vector>

#include "ast.hpp"
#include "rng.hpp"

namespace fractl {

struct Instance {
    size_t        fractal = 0;
    double        x = 0.0, y = 0.0, z = 0.0;
    double        yaw   = 0.0;
    double        scale = 1.0;
    std::uint64_t seed  = 0;
};

inline std::vector<Instance> placeInstances(const SourceFile& f) {
    std::vector<Instance> out;
    auto indexOf = [&](const std::string& name) {
        for (size_t i = 0; i < f.fractals.size(); ++i) if (f.fractals[i].name == name) return i;
        return size_t(0);
    };
    if (!f.hasScene) {
        if (f.fractals.empty() || f.fractals[0].dimensions != 3) return out;
        const FractalDecl& d = f.fractals[0];
        out.push_back({0, d.startX, 0.0, d.startY, d.heading, 1.0, d.seed});
        return out;
    }
    const double kTau = 6.283185307179586;
    for (const auto& p : f.scene.items) {
        size_t fi = indexOf(p.name);
        const FractalDecl& d = f.fractals[fi];
        if (p.kind == Placement::Place) {
            out.push_back({fi, p.x, p.y, p.z, p.yaw, p.scale, d.seed});
            continue;
        }
        const double r0 = p.clear * p.clear, r1 = p.radius * p.radius;
        const double lo = 1.0 - p.vary, span = 2.0 * p.vary;
        std::vector<double> cx, cz;
        for (int j = 0; j < p.clumps; ++j) {
            const std::uint64_t k = static_cast<std::uint64_t>(j);
            double r  = std::sqrt(r0 + unitRandom(p.seed, 0xC1A0, k) * (r1 - r0));
            double th = kTau * unitRandom(p.seed, 0xC1A1, k);
            cx.push_back(r * std::cos(th));
            cz.push_back(r * std::sin(th));
        }
        const double sigma = p.clumps ? p.radius / std::sqrt(static_cast<double>(p.clumps)) * 0.42 : 0.0;
        for (int i = 0; i < p.count; ++i) {
            const std::uint64_t k = static_cast<std::uint64_t>(i);
            double u0 = unitRandom(p.seed, 0xA11CE, k);
            double u1 = unitRandom(p.seed, 0xA11CF, k);
            double u2 = unitRandom(p.seed, 0xA11D0, k);
            double u3 = unitRandom(p.seed, 0xA11D1, k);
            double size = p.scale * (lo + span * u3);
            double x, z;
            if (p.clumps) {
                size_t j = std::min(static_cast<size_t>(u0 * p.clumps), cx.size() - 1);
                double u4 = unitRandom(p.seed, 0xA11D2, k);
                double g  = std::sqrt(-2.0 * std::log(1.0 - u4)) * sigma;
                x = cx[j] + g * std::cos(kTau * u1);
                z = cz[j] + g * std::sin(kTau * u1);
                size *= 0.7 + 0.55 * std::exp(-0.5 * (g / sigma) * (g / sigma));
            } else {
                double r  = std::sqrt(r0 + u0 * (r1 - r0));
                double th = kTau * u1;
                x = r * std::cos(th);
                z = r * std::sin(th);
            }
            out.push_back({fi, x, 0.0, z, 360.0 * u2, size, instanceSeed(d.seed, p.seed, k)});
        }
    }
    return out;
}

}
