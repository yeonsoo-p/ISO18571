#include "engine.h"

#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

namespace engine {

namespace {

[[noreturn]] void throw_score_exponent_error (std::string_view name) {
    throw std::invalid_argument(std::string(name) + " has to be 1, 2, or 3");
}

[[noreturn]] void throw_positive_integer_error (std::string_view name) {
    throw std::invalid_argument(std::string(name) + " must be a positive integer");
}

void require_finite (double value, std::string_view name) {
    if (!std::isfinite(value)) {
        throw std::invalid_argument(std::string(name) + " must be finite");
    }
}

void require_positive (double value, std::string_view name) {
    require_finite(value, name);
    if (value <= 0.0) {
        throw std::invalid_argument(std::string(name) + " must be positive");
    }
}

void require_non_negative (double value, std::string_view name) {
    require_finite(value, name);
    if (value < kWeightMinimum) {
        throw std::invalid_argument(std::string(name) + " must be non-negative");
    }
}

void require_score_exponent (int value, std::string_view name) {
    if (value < kScoreExponentMinimum || value > kScoreExponentMaximum) {
        throw_score_exponent_error(name);
    }
}

void require_positive_integer (int value, std::string_view name) {
    if (value < 1) {
        throw_positive_integer_error(name);
    }
}

void require_closed_interval (double value, std::string_view name, double minimum, double maximum) {
    require_finite(value, name);
    if (value < minimum || value > maximum) {
        throw std::invalid_argument(std::string(name) + " must satisfy 0 <= " + std::string(name) + " <= 1");
    }
}

} // namespace

int score_exponent_from_double (double value, std::string_view name) {
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

int positive_integer_from_double (double value, std::string_view name) {
    if (!std::isfinite(value) || value < 1.0 || value > static_cast<double>(std::numeric_limits<int>::max()) ||
        std::floor(value) != value) {
        throw_positive_integer_error(name);
    }
    return static_cast<int>(value);
}

void validate_score_params (const ScoreParams& params) {
    require_positive_integer(params.k_z, "k_z");
    require_score_exponent(params.k_p, "k_p");
    require_score_exponent(params.k_m, "k_m");

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

    const double weights_sum = params.w_z + params.w_m + params.w_p + params.w_s;
    if (std::fabs(weights_sum - kExpectedWeightSum) > kWeightSumAbsoluteTolerance) {
        throw std::invalid_argument("Sum of weighting factors (w_z, w_m, w_p, w_s) must be within tolerance of 1");
    }
}

} // namespace engine
