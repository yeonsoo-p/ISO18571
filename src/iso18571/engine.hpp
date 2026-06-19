#pragma once

#include <cstddef>
#include <span>
#include <vector>

namespace iso18571 {

using Index = std::ptrdiff_t;

enum class CurveDType {
    Float32,
    Float64,
};

struct ArrayView {
    std::span<const double> values;
    Index                   n = 0;

    double value (Index index) const { return values[static_cast<std::size_t>(index)]; }
};

struct CurveView {
    const char* data          = nullptr;
    Index       row_stride    = 0;
    Index       column_stride = 0;
    Index       n             = 0;
    CurveDType  dtype         = CurveDType::Float64;

    double value (Index index) const {
        const char* ptr = data + index * row_stride + column_stride;
        if (dtype == CurveDType::Float32) {
            return static_cast<double>(*reinterpret_cast<const float*>(ptr));
        }
        return *reinterpret_cast<const double*>(ptr);
    }

    double time (Index index) const {
        const char* ptr = data + index * row_stride;
        if (dtype == CurveDType::Float32) {
            return static_cast<double>(*reinterpret_cast<const float*>(ptr));
        }
        return *reinterpret_cast<const double*>(ptr);
    }
};

struct SignalView {
    std::span<const double> signal_values;
    Index                   n = 0;

    double value (Index index) const { return signal_values[static_cast<std::size_t>(index)]; }

    ArrayView value_slice (Index start, Index length) const {
        return {
            signal_values.subspan(static_cast<std::size_t>(start), static_cast<std::size_t>(length)),
            length,
        };
    }
};

struct ScoreParams {
    int    k_z;
    int    k_p;
    int    k_m;
    double eps_m;
    double e_s;
    double init_min;
    double a_0;
    double b_0;
    double w_z;
    double w_p;
    double w_m;
    double w_s;
};

struct PhaseAlignment {
    Index  reference_start  = 0;
    Index  comparison_start = 0;
    Index  length           = 0;
    Index  n_eps            = 0;
    double max_shift        = 0.2;
};

struct PhaseCorrelation {
    double rho_e = 0.0;
};

enum class DiagnosticSeverity {
    Warning,
};

enum class DiagnosticComponent {
    Corridor,
    Phase,
    Magnitude,
    Slope,
};

enum class DiagnosticCode {
    PhaseUndefinedCorrelation,
    PhaseShiftClampedToUnshifted,
    MagnitudeZeroReferenceDenominator,
    SlopeZeroReferenceDenominator,
};

struct Diagnostic {
    DiagnosticSeverity  severity;
    DiagnosticComponent component;
    DiagnosticCode      code;
};

struct CorridorResult {
    double                  score = 0.0;
    std::vector<Diagnostic> diagnostics;
};

struct PhaseResult {
    double                  score = 0.0;
    PhaseAlignment          alignment;
    PhaseCorrelation        correlation;
    std::vector<Diagnostic> diagnostics;
};

struct MagnitudeResult {
    double                  score = 0.0;
    std::vector<Diagnostic> diagnostics;
};

struct SlopeResult {
    double                  score = 0.0;
    std::vector<Diagnostic> diagnostics;
};

struct ScoreResult {
    CorridorResult  corridor;
    PhaseResult     phase;
    MagnitudeResult magnitude;
    SlopeResult     slope;
    double          overall = 0.0;
};

using ScoreComponentsFn = ScoreResult (*)(const SignalView&, const SignalView&, const ScoreParams&, double);

struct DispatchTable {
    ScoreComponentsFn score_components = nullptr;
    const char*       level            = "x86-64-v1";
};

ScoreResult score_components_v1 (const SignalView& reference, const SignalView& comparison, const ScoreParams& params,
                                 double dt);

#if defined(ISO18571_COMPILED_X86_64_V2)
ScoreResult score_components_v2 (const SignalView& reference, const SignalView& comparison, const ScoreParams& params,
                                 double dt);
#endif

#if defined(ISO18571_COMPILED_X86_64_V3)
ScoreResult score_components_v3 (const SignalView& reference, const SignalView& comparison, const ScoreParams& params,
                                 double dt);
#endif

#if defined(ISO18571_COMPILED_X86_64_V4)
ScoreResult score_components_v4 (const SignalView& reference, const SignalView& comparison, const ScoreParams& params,
                                 double dt);
#endif

const DispatchTable& dispatch_table ();
const char*          compiled_x86_64_levels ();

} // namespace iso18571
