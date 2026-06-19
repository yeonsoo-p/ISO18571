#pragma once

#include <string>

namespace dispatch {

enum class X86_64Level {
    V1,
    V2,
    V3,
    V4,
};

struct CompiledX86_64Levels {
    bool v2 = false;
    bool v3 = false;
    bool v4 = false;
};

const char* level_name (X86_64Level level);
X86_64Level best_x86_64_level (CompiledX86_64Levels compiled);
std::string compiled_x86_64_levels (CompiledX86_64Levels compiled);

} // namespace dispatch
