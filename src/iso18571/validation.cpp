#include "validation.hpp"

#include <cmath>
#include <stdexcept>
#include <string>

namespace iso18571 {
namespace {

void require_finite(double value, const char* name) {
    if (!std::isfinite(value)) {
        throw std::invalid_argument(std::string(name) + " must be finite");
    }
}

void require_positive(double value, const char* name) {
    require_finite(value, name);
    if (value <= 0.0) {
        throw std::invalid_argument(std::string(name) + " must be positive");
    }
}

void require_non_negative(double value, const char* name) {
    require_finite(value, name);
    if (value < kWeightMinimum) {
        throw std::invalid_argument(std::string(name) + " must be non-negative");
    }
}

void require_score_exponent(int value, const char* name) {
    if (value < kScoreExponentMinimum || value > kScoreExponentMaximum) {
        throw std::invalid_argument(std::string(name) + " has to be 1, 2, or 3");
    }
}

void require_closed_interval(
    double value,
    const char* name,
    double minimum,
    double maximum
) {
    require_finite(value, name);
    if (value < minimum || value > maximum) {
        throw std::invalid_argument(
            std::string(name) + " must satisfy 0 <= " + name + " <= 1"
        );
    }
}

}  // namespace

void validate_score_params(const ScoreParams& params) {
    require_score_exponent(params.k_z, "k_z");
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
    require_positive(params.dt, "dt");

    const double weights_sum = params.w_z + params.w_m + params.w_p + params.w_s;
    if (
        std::fabs(weights_sum - kExpectedWeightSum)
        > kWeightSumAbsoluteTolerance
    ) {
        throw std::invalid_argument(
            "Sum of weighting factors (w_z, w_m, w_p, w_s) must be within tolerance of 1"
        );
    }
}

}  // namespace iso18571
