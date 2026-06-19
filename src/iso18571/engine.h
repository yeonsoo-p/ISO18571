#pragma once

#include <cstddef>
#include <span>
#include <vector>

namespace engine {
inline constexpr int kScoreExponentMinimum = 1;
inline constexpr int kScoreExponentMaximum = 3;

inline constexpr double kInitMinMinimum          = 0.0;
inline constexpr double kInitMinExclusiveMaximum = 1.0;

inline constexpr double kA0Minimum = 0.0;
inline constexpr double kA0Maximum = 1.0;

inline constexpr double kB0Minimum = 0.0;
inline constexpr double kB0Maximum = 1.0;

inline constexpr double kWeightMinimum              = 0.0;
inline constexpr double kExpectedWeightSum          = 1.0;
inline constexpr double kWeightSumAbsoluteTolerance = 1.0e-12;

using Index      = std::ptrdiff_t;
using DoubleSpan = std::span<const double>;

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

int  score_exponent_from_double (double value, const char* name);
int  positive_integer_from_double (double value, const char* name);
void validate_score_params (const ScoreParams& params);

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

using ScoreComponentsFn = ScoreResult (*)(DoubleSpan, DoubleSpan, const ScoreParams&, double);

struct DispatchTable {
    ScoreComponentsFn score_components = nullptr;
    const char*       level            = "x86-64-v1";
};

ScoreResult score_components_v1 (DoubleSpan reference, DoubleSpan comparison, const ScoreParams& params, double dt);

#if defined(ISO18571_COMPILED_X86_64_V2)
ScoreResult score_components_v2 (DoubleSpan reference, DoubleSpan comparison, const ScoreParams& params, double dt);
#endif

#if defined(ISO18571_COMPILED_X86_64_V3)
ScoreResult score_components_v3 (DoubleSpan reference, DoubleSpan comparison, const ScoreParams& params, double dt);
#endif

#if defined(ISO18571_COMPILED_X86_64_V4)
ScoreResult score_components_v4 (DoubleSpan reference, DoubleSpan comparison, const ScoreParams& params, double dt);
#endif

const DispatchTable& dispatch_table ();

} // namespace engine
