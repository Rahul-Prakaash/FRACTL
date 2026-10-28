#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>

#include "ast.hpp"
#include "rng.hpp"

namespace fractl {

constexpr size_t kMaxExpansion = 1u << 26;

struct RewriteError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

inline double totalWeight(const RuleSet& rs) {
    double t = 0.0;
    for (const auto& p : rs) t += p.weight;
    return t;
}

inline size_t chooseProduction(const RuleSet& rs, double r) {
    double x = r * totalWeight(rs), cum = 0.0;
    for (size_t i = 0; i + 1 < rs.size(); ++i) {
        cum += rs[i].weight;
        if (x < cum) return i;
    }
    return rs.size() - 1;
}

inline std::string expandOnce(const FractalDecl& d, const std::string& s,
                              std::uint64_t seed, int level) {
    std::string   out;
    std::uint64_t counter = 0;
    out.reserve(s.size() * 2);
    for (char c : s) {
        auto it = d.rules.find(c);
        if (it == d.rules.end() || it->second.empty()) {
            out += c;
        } else if (it->second.size() == 1) {
            out += it->second.front().text;
        } else {
            double r = unitRandom(seed, static_cast<std::uint64_t>(level), counter++);
            out += it->second[chooseProduction(it->second, r)].text;
        }
        if (out.size() > kMaxExpansion)
            throw RewriteError("expansion exceeded " + std::to_string(kMaxExpansion) +
                               " symbols — lower 'iterations'");
    }
    return out;
}

inline std::string expandOnce(const FractalDecl& d, const std::string& s) {
    return expandOnce(d, s, d.seed, 0);
}

inline std::string expand(const FractalDecl& d, std::uint64_t seed) {
    std::string s = d.axiom;
    for (int i = 0; i < d.iterations; ++i) s = expandOnce(d, s, seed, i);
    return s;
}

inline std::string expand(const FractalDecl& d) { return expand(d, d.seed); }

inline bool isStochastic(const FractalDecl& d) {
    for (const auto& kv : d.rules) if (kv.second.size() > 1) return true;
    return false;
}

}
