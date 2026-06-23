#pragma once

#include <cstddef>
#include <span>
#include <vector>

#include "types.h"

namespace engine {
using Index      = std::ptrdiff_t;
using DoubleSpan = std::span<const f64>;

struct ScoreParams {
    int k_z;
    int k_p;
    int k_m;
    f64 eps_m;
    f64 e_s;
    f64 init_min;
    f64 a_0;
    f64 b_0;
    f64 w_z;
    f64 w_p;
    f64 w_m;
    f64 w_s;
};

struct PhaseAlignment {
    Index reference_start  = 0;
    Index comparison_start = 0;
    Index length           = 0;
    Index n_eps            = 0;
    f64   max_shift        = 0.2;
};

struct PhaseCorrelation {
    f64 rho_e = 0.0;
};

enum class DiagnosticSeverity {
    Warning,
};

enum class DiagnosticComponent {
    Validation,
    Corridor,
    Phase,
    Magnitude,
    Slope,
};

enum class DiagnosticCode {
    ReferenceCurveLayoutCopied,
    ComparisonCurveLayoutCopied,
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
    f64                     score = 0.0;
    std::vector<Diagnostic> diagnostics;
};

struct PhaseResult {
    f64                     score = 0.0;
    PhaseAlignment          alignment;
    PhaseCorrelation        correlation;
    std::vector<Diagnostic> diagnostics;
};

struct MagnitudeResult {
    f64                     score = 0.0;
    std::vector<Diagnostic> diagnostics;
};

struct SlopeResult {
    f64                     score = 0.0;
    std::vector<Diagnostic> diagnostics;
};

struct ScoreResult {
    CorridorResult  corridor;
    PhaseResult     phase;
    MagnitudeResult magnitude;
    SlopeResult     slope;
    f64             overall = 0.0;
};

using ScoreComponentsFn = ScoreResult (*)(DoubleSpan, DoubleSpan, const ScoreParams&, f64);

struct DispatchTable {
    ScoreComponentsFn score_components = nullptr;
    const char*       level            = "x86-64-v1";
};

ScoreResult score_components_v1 (DoubleSpan reference, DoubleSpan comparison, const ScoreParams& params, f64 dt);

#if defined(ISO18571_COMPILED_X86_64_V2)
ScoreResult score_components_v2 (DoubleSpan reference, DoubleSpan comparison, const ScoreParams& params, f64 dt);
#endif

#if defined(ISO18571_COMPILED_X86_64_V3)
ScoreResult score_components_v3 (DoubleSpan reference, DoubleSpan comparison, const ScoreParams& params, f64 dt);
#endif

#if defined(ISO18571_COMPILED_X86_64_V4)
ScoreResult score_components_v4 (DoubleSpan reference, DoubleSpan comparison, const ScoreParams& params, f64 dt);
#endif

const DispatchTable& dispatch_table ();

} // namespace engine

namespace fft {

inline constexpr bool kForward  = true;
inline constexpr bool kBackward = false;

using Complex = c128;

} // namespace fft
