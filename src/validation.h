#pragma once

#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

#include "engine.h"
#include "numeric.h"

namespace validation {

namespace {

inline constexpr int kScoreExponentMinimum = 1;
inline constexpr int kScoreExponentMaximum = 3;

inline constexpr f64 kInitMinMinimum          = 0.0;
inline constexpr f64 kInitMinExclusiveMaximum = 1.0;

inline constexpr f64 kA0Minimum = 0.0;
inline constexpr f64 kA0Maximum = 1.0;

inline constexpr f64 kB0Minimum = 0.0;
inline constexpr f64 kB0Maximum = 1.0;

inline constexpr f64 kWeightMinimum = 0.0;

[[noreturn]] inline void throw_score_exponent_error (std::string_view name) {
    throw std::invalid_argument(std::string(name) + " has to be 1, 2, or 3");
}

[[noreturn]] inline void throw_positive_integer_error (std::string_view name) {
    throw std::invalid_argument(std::string(name) + " must be a positive integer");
}

[[noreturn]] inline void throw_weight_sum_error () {
    throw std::invalid_argument("Sum of weighting factors (w_z, w_m, w_p, w_s) must be within tolerance of 1");
}

inline void require_finite (f64 value, std::string_view name) {
    if (!std::isfinite(value)) {
        throw std::invalid_argument(std::string(name) + " must be finite");
    }
}

inline void require_positive (f64 value, std::string_view name) {
    require_finite(value, name);
    if (value <= 0.0) {
        throw std::invalid_argument(std::string(name) + " must be positive");
    }
}

inline void require_non_negative (f64 value, std::string_view name) {
    require_finite(value, name);
    if (value < kWeightMinimum) {
        throw std::invalid_argument(std::string(name) + " must be non-negative");
    }
}

inline void require_score_exponent (int value, std::string_view name) {
    if (value < kScoreExponentMinimum || value > kScoreExponentMaximum) {
        throw_score_exponent_error(name);
    }
}

inline void require_positive_integer (int value, std::string_view name) {
    if (value < 1) {
        throw_positive_integer_error(name);
    }
}

inline void require_closed_interval (f64 value, std::string_view name, f64 minimum, f64 maximum) {
    require_finite(value, name);
    if (value < minimum || value > maximum) {
        throw std::invalid_argument(std::string(name) + " must satisfy 0 <= " + std::string(name) + " <= 1");
    }
}

} // namespace

inline const char* warning_message_for_code (engine::DiagnosticCode code) {
    switch (code) {
    case engine::DiagnosticCode::ReferenceCurveLayoutCopied:
        return "ISO18571 copied reference_curve to a C-contiguous aligned array because its memory layout is unsafe";
    case engine::DiagnosticCode::ComparisonCurveLayoutCopied:
        return "ISO18571 copied comparison_curve to a C-contiguous aligned array because its memory layout is unsafe";
    case engine::DiagnosticCode::PhaseUndefinedCorrelation:
        return "ISO18571 phase correlation is undefined; using finite fallback rho_e";
    case engine::DiagnosticCode::PhaseShiftClampedToUnshifted:
        return "ISO18571 phase alignment left fewer than 9 samples; using unshifted alignment";
    case engine::DiagnosticCode::MagnitudeZeroReferenceDenominator:
        return "ISO18571 magnitude reference denominator is zero; using fallback magnitude score";
    case engine::DiagnosticCode::SlopeZeroReferenceDenominator:
        return "ISO18571 slope reference denominator is zero; using fallback slope score";
    }
}

inline int score_exponent_from_double (f64 value, std::string_view name) {
    if (!std::isfinite(value)) {
        throw_score_exponent_error(name);
    }
    if (value == 1.0) {
        return 1;
    }
    if (value == 2.0) {
        return 2;
    }
    if (value == 3.0) {
        return 3;
    }
    throw_score_exponent_error(name);
}

inline int positive_integer_from_double (f64 value, std::string_view name) {
    if (!std::isfinite(value) || value < 1.0 || value > static_cast<f64>(std::numeric_limits<int>::max()) ||
        std::floor(value) != value) {
        throw_positive_integer_error(name);
    }
    return static_cast<int>(value);
}

inline void validate_score_params (engine::ScoreParams& params) {
    require_positive_integer(params.k_z, "k_z");
    require_score_exponent(params.k_p, "k_p");
    require_score_exponent(params.k_m, "k_m");
    require_score_exponent(params.k_s, "k_s");

    require_positive(params.eps_m, "eps_m");
    require_positive(params.e_s, "e_s");
    require_finite(params.init_min, "init_min");
    if (params.init_min < kInitMinMinimum || params.init_min >= kInitMinExclusiveMaximum) {
        throw std::invalid_argument("init_min must be finite and satisfy 0 <= init_min < 1");
    }

    require_closed_interval(params.a_0, "a_0", kA0Minimum, kA0Maximum);
    require_closed_interval(params.b_0, "b_0", kB0Minimum, kB0Maximum);
    if (params.b_0 <= params.a_0) {
        throw std::invalid_argument("b_0 must be greater than a_0");
    }

    require_non_negative(params.w_z, "w_z");
    require_non_negative(params.w_p, "w_p");
    require_non_negative(params.w_m, "w_m");
    require_non_negative(params.w_s, "w_s");

    const f64 weight_sum = params.w_z + params.w_p + params.w_m + params.w_s;
    if (!numeric::almost_equal(weight_sum, 1.0)) {
        throw_weight_sum_error();
    }
}

} // namespace validation
