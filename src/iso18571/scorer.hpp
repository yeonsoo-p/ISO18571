#pragma once

#include <cstddef>

namespace iso18571 {

using Index = std::ptrdiff_t;

struct ArrayView {
    const char* data = nullptr;
    Index stride = 0;
    Index n = 0;

    double value(Index index) const {
        return *reinterpret_cast<const double*>(data + index * stride);
    }
};

struct CurveView {
    const char* data = nullptr;
    Index row_stride = 0;
    Index column_stride = 0;
    Index n = 0;

    double value(Index index) const {
        return *reinterpret_cast<const double*>(data + index * row_stride + column_stride);
    }

    double time(Index index) const {
        return *reinterpret_cast<const double*>(data + index * row_stride);
    }
};

struct ScoreParams {
    int k_z = 2;
    int k_p = 1;
    int k_m = 1;
    double eps_m = 0.50;
    double e_s = 2.0;
    double init_min = 0.8;
    double a_0 = 0.05;
    double b_0 = 0.5;
    double w_z = 0.4;
    double w_p = 0.2;
    double w_m = 0.2;
    double w_s = 0.2;
    double dt = 0.0001;
};

struct ShiftResult {
    Index reference_start = 0;
    Index comparison_start = 0;
    Index length = 0;
    Index n_eps = 0;
    double rho_e = 0.0;
    double max_shift = 0.2;
};

struct ScoreResult {
    double z = 0.0;
    double ep = 0.0;
    double em = 0.0;
    double es = 0.0;
    double r = 0.0;
    ShiftResult shift;
};

using ScoreComponentsFn = ScoreResult (*)(const CurveView&, const CurveView&, const ScoreParams&);

struct DispatchTable {
    ScoreComponentsFn score_components = nullptr;
    const char* level = "x86-64-v1";
};

ScoreResult score_components_v1(const CurveView& reference, const CurveView& comparison, const ScoreParams& params);

#if defined(ISO18571_COMPILED_X86_64_V2)
ScoreResult score_components_v2(const CurveView& reference, const CurveView& comparison, const ScoreParams& params);
#endif

#if defined(ISO18571_COMPILED_X86_64_V3)
ScoreResult score_components_v3(const CurveView& reference, const CurveView& comparison, const ScoreParams& params);
#endif

#if defined(ISO18571_COMPILED_X86_64_V4)
ScoreResult score_components_v4(const CurveView& reference, const CurveView& comparison, const ScoreParams& params);
#endif

const DispatchTable& dispatch_table();
const char* compiled_x86_64_levels();

}  // namespace iso18571
