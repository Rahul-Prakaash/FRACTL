#pragma once

#include <cstdint>

namespace fractl {

inline std::uint64_t mix64(std::uint64_t x) {
    x += 0x9E3779B97F4A7C15ULL;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ULL;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBULL;
    return x ^ (x >> 31);
}

inline double unitRandom(std::uint64_t seed, std::uint64_t level, std::uint64_t counter) {
    std::uint64_t h = mix64(seed ^ mix64(level * 0x100000001B3ULL ^ mix64(counter)));
    return static_cast<double>(h >> 11) * (1.0 / 9007199254740992.0);
}

inline std::uint64_t instanceSeed(std::uint64_t fractalSeed, std::uint64_t placementSeed,
                                  std::uint64_t index) {
    return mix64(fractalSeed ^ mix64(placementSeed + 0x51ED27ULL * (index + 1)));
}

}
