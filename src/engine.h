#pragma once

#include <cstddef>
#include <span>
#include <vector>

#include "types.h"

namespace engine {

struct ScoreParams {
    int k_z;
    int k_p;
    int k_m;
    int k_s;
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
    // Score
    f64 score = 0.0;

    // Validation
    f64 t_norm           = 0.0;
    f64 inner_half_width = 0.0;
    f64 outer_half_width = 0.0;
};

struct PhaseResult {
    // Score
    f64 score = 0.0;

    // Validation
    std::ptrdiff_t reference_start  = 0;
    std::ptrdiff_t comparison_start = 0;
    std::ptrdiff_t length           = 0;
    std::ptrdiff_t n_eps            = 0;
    f64            max_shift        = 0.2;
    f64            rho_e            = 0.0;
};

struct MagnitudeResult {
    // Score
    f64 score = 0.0;

    // Validation
    f64 numerator   = 0.0;
    f64 denominator = 0.0;
    f64 error       = 0.0;
};

struct SlopeResult {
    // Score
    f64 score = 0.0;

    // Validation
    f64 numerator   = 0.0;
    f64 denominator = 0.0;
    f64 error       = 0.0;
};

struct ScoreTimings {
    f64 corridor_ms  = 0.0;
    f64 phase_ms     = 0.0;
    f64 magnitude_ms = 0.0;
    f64 slope_ms     = 0.0;
    f64 total_ms     = 0.0;
};

struct ScoreResult {
    CorridorResult  corridor;
    PhaseResult     phase;
    MagnitudeResult magnitude;
    SlopeResult     slope;
    ScoreTimings    timings;
    f64             overall = 0.0;
};

using ScoreComponentsFn = ScoreResult (*)(std::span<const f64>, std::span<const f64>, const ScoreParams&, f64,
                                          std::vector<Diagnostic>&);

struct DispatchTable {
    ScoreComponentsFn score_components = nullptr;
    const char*       level            = "x86-64-v1";
};

ScoreResult score_components_v1 (std::span<const f64> reference, std::span<const f64> comparison,
                                 const ScoreParams& params, f64 dt, std::vector<Diagnostic>& diagnostics);

#if defined(ISO18571_COMPILED_X86_64_V2)
ScoreResult score_components_v2 (std::span<const f64> reference, std::span<const f64> comparison,
                                 const ScoreParams& params, f64 dt, std::vector<Diagnostic>& diagnostics);
#endif

#if defined(ISO18571_COMPILED_X86_64_V3)
ScoreResult score_components_v3 (std::span<const f64> reference, std::span<const f64> comparison,
                                 const ScoreParams& params, f64 dt, std::vector<Diagnostic>& diagnostics);
#endif

#if defined(ISO18571_COMPILED_X86_64_V4)
ScoreResult score_components_v4 (std::span<const f64> reference, std::span<const f64> comparison,
                                 const ScoreParams& params, f64 dt, std::vector<Diagnostic>& diagnostics);
#endif

const DispatchTable& dispatch_table ();

} // namespace engine

namespace fft {

inline constexpr bool kForward  = true;
inline constexpr bool kBackward = false;

} // namespace fft
