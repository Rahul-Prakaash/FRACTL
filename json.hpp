#pragma once

#include <cstdio>
#include <map>
#include <ostream>
#include <string>
#include <vector>

#include "ast.hpp"
#include "ir.hpp"
#include "lexer.hpp"

namespace fractl {

constexpr size_t kPreviewChars = 200;
constexpr size_t kMaxEmbeddedExpansion = 200000;
constexpr size_t kMaxEmbeddedProgram = 20000;

namespace json {

inline std::string escape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (unsigned char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            case '\b': out += "\\b";  break;
            case '\f': out += "\\f";  break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof buf, "\\u%04x", c);
                    out += buf;
                } else {
                    out += static_cast<char>(c);
                }
        }
    }
    return out;
}

inline std::string str(const std::string& s) { return "\"" + escape(s) + "\""; }

inline std::string num(double v) {
    char buf[40];
    std::snprintf(buf, sizeof buf, "%.10g", v);
    return buf;
}

}

inline std::string tokKindLabel(Tok k) {
    std::string s = to_string(k);
    if (s.size() >= 2 && s.front() == '\'' && s.back() == '\'') s = s.substr(1, s.size() - 2);
    return s;
}

struct Stats {
    std::map<char, size_t> symbolCounts;
    std::map<int, size_t>  opCounts;
    size_t                 maxDepth = 0;
};

inline Stats computeStats(const std::string& expanded, const Program& prog) {
    Stats st;
    for (char c : expanded) ++st.symbolCounts[c];
    for (int i = 0; i < kOpCount; ++i) st.opCounts[i] = 0;
    for (const auto& cmd : prog) ++st.opCounts[static_cast<int>(cmd.op)];
    st.maxDepth = maxBranchDepth(prog);
    return st;
}

inline void writeJson(std::ostream& os,
                      const std::string& name,
                      const std::string& source,
                      const std::vector<Token>& tokens,
                      const FractalDecl& decl,
                      const std::string& expanded,
                      const Program& prog) {
    using namespace json;
    const Stats st = computeStats(expanded, prog);

    os << "{\n";
    os << "  \"name\": "   << str(name)   << ",\n";
    os << "  \"source\": " << str(source) << ",\n";

    os << "  \"tokens\": [\n";
    for (size_t i = 0; i < tokens.size(); ++i) {
        const Token& t = tokens[i];
        os << "    {\"line\": " << t.line
           << ", \"kind\": " << str(tokKindLabel(t.kind))
           << ", \"text\": " << str(t.text);
        if (t.kind == Tok::Number) os << ", \"num\": " << num(t.num);
        os << "}" << (i + 1 < tokens.size() ? "," : "") << "\n";
    }
    os << "  ],\n";

    os << "  \"ast\": {\n";
    os << "    \"name\": "       << str(decl.name)  << ",\n";
    os << "    \"axiom\": "      << str(decl.axiom) << ",\n";
    os << "    \"rules\": [\n";
    {
        size_t r = 0;
        for (const auto& kv : decl.rules) {
            os << "      {\"symbol\": " << str(std::string(1, kv.first)) << ", \"productions\": [";
            for (size_t p = 0; p < kv.second.size(); ++p) {
                os << "{\"weight\": " << num(kv.second[p].weight)
                   << ", \"text\": "  << str(kv.second[p].text) << "}"
                   << (p + 1 < kv.second.size() ? ", " : "");
            }
            os << "]}" << (++r < decl.rules.size() ? "," : "") << "\n";
        }
    }
    os << "    ],\n";
    os << "    \"angle\": "      << num(decl.angle)   << ",\n";
    os << "    \"iterations\": " << decl.iterations   << ",\n";
    os << "    \"step\": "       << num(decl.step)    << ",\n";
    os << "    \"startX\": "     << num(decl.startX)  << ",\n";
    os << "    \"startY\": "     << num(decl.startY)  << ",\n";
    os << "    \"heading\": "    << num(decl.heading) << ",\n";
    os << "    \"dimensions\": " << decl.dimensions   << ",\n";
    os << "    \"seed\": "       << decl.seed         << ",\n";
    os << "    \"width\": "      << num(decl.width)   << ",\n";
    os << "    \"taper\": "      << num(decl.taper)   << ",\n";
    os << "    \"colors\": [";
    for (size_t c = 0; c < decl.colors.size(); ++c) os << (c ? ", " : "") << str(decl.colors[c]);
    os << "]\n";
    os << "  },\n";

    const bool embedFull = expanded.size() <= kMaxEmbeddedExpansion;
    os << "  \"expanded\": {\n";
    os << "    \"preview\": "    << str(expanded.substr(0, kPreviewChars)) << ",\n";
    os << "    \"fullLength\": " << expanded.size() << ",\n";
    os << "    \"truncated\": "  << (expanded.size() > kPreviewChars ? "true" : "false") << ",\n";
    os << "    \"full\": "       << (embedFull ? str(expanded) : "null") << "\n";
    os << "  },\n";

    const size_t emitted = prog.size() < kMaxEmbeddedProgram ? prog.size() : kMaxEmbeddedProgram;
    os << "  \"program\": [\n";
    for (size_t i = 0; i < emitted; ++i) {
        os << "    {\"op\": " << str(to_string(prog[i].op))
           << ", \"arg\": "   << num(prog[i].arg) << "}"
           << (i + 1 < emitted ? "," : "") << "\n";
    }
    os << "  ],\n";
    os << "  \"programLength\": "    << prog.size() << ",\n";
    os << "  \"programTruncated\": " << (emitted < prog.size() ? "true" : "false") << ",\n";

    os << "  \"stats\": {\n";
    os << "    \"symbolCounts\": {";
    {
        size_t i = 0;
        for (const auto& kv : st.symbolCounts)
            os << (i++ ? ", " : "") << str(std::string(1, kv.first)) << ": " << kv.second;
    }
    os << "},\n";
    os << "    \"opCounts\": {";
    {
        size_t i = 0;
        for (const auto& kv : st.opCounts)
            os << (i++ ? ", " : "")
               << str(to_string(static_cast<Op>(kv.first))) << ": " << kv.second;
    }
    os << "},\n";
    os << "    \"maxDepth\": " << st.maxDepth << "\n";
    os << "  }\n";
    os << "}\n";
}

}
