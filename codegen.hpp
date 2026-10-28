#pragma once

#include <memory>
#include <string>
#include <vector>

#include "ast.hpp"
#include "ir.hpp"
#include "scene.hpp"

namespace fractl {

struct CodegenOptions {
    bool        recursive = true;
    std::string outPath;
};

struct IRStats {
    unsigned functions    = 0;
    unsigned instructions = 0;
    unsigned calls        = 0;
};

class Backend {
public:
    explicit Backend(const std::string& moduleName);
    ~Backend();
    Backend(const Backend&)            = delete;
    Backend& operator=(const Backend&) = delete;

    bool codegen(const SourceFile& file, const std::vector<Instance>& instances,
                 const CodegenOptions& opts, std::string& err);

    unsigned foldTurns();
    IRStats  stats() const;

    bool emitIR(const std::string& path, std::string& err) const;
    bool emitObject(const std::string& path, std::string& err);
    bool runJIT(std::string& err, double& compileMs, double& runMs);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}
