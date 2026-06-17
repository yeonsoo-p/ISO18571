#pragma once

#include "scorer.hpp"

namespace iso18571 {

inline constexpr int kScoreExponentMinimum = 1;
inline constexpr int kScoreExponentMaximum = 3;

inline constexpr double kInitMinMinimum = 0.0;
inline constexpr double kInitMinExclusiveMaximum = 1.0;

inline constexpr double kA0Minimum = 0.0;
inline constexpr double kA0Maximum = 1.0;
inline constexpr double kA0Default = 0.05;

inline constexpr double kB0Minimum = 0.0;
inline constexpr double kB0Maximum = 1.0;
inline constexpr double kB0Default = 0.5;

inline constexpr double kWeightMinimum = 0.0;
inline constexpr double kExpectedWeightSum = 1.0;
inline constexpr double kWeightSumAbsoluteTolerance = 1.0e-12;

void validate_score_params(const ScoreParams& params);

}  // namespace iso18571
