#include "dispatch.h"
#include "engine.h"

namespace engine {

namespace {

DispatchTable make_dispatch_table () {
    switch (dispatch::best_x86_64_level(dispatch::compiled_levels())) {
    case dispatch::X86_64Level::V4:
#if defined(ISO18571_COMPILED_X86_64_V4)
        return {score_components_v4, dispatch::level_name(dispatch::X86_64Level::V4)};
#else
        break;
#endif
    case dispatch::X86_64Level::V3:
#if defined(ISO18571_COMPILED_X86_64_V3)
        return {score_components_v3, dispatch::level_name(dispatch::X86_64Level::V3)};
#else
        break;
#endif
    case dispatch::X86_64Level::V2:
#if defined(ISO18571_COMPILED_X86_64_V2)
        return {score_components_v2, dispatch::level_name(dispatch::X86_64Level::V2)};
#else
        break;
#endif
    case dispatch::X86_64Level::V1:
        return {score_components_v1, dispatch::level_name(dispatch::X86_64Level::V1)};
    }
    return {score_components_v1, dispatch::level_name(dispatch::X86_64Level::V1)};
}

} // namespace

const DispatchTable& dispatch_table () {
    static const DispatchTable table = make_dispatch_table();
    return table;
}

} // namespace engine
