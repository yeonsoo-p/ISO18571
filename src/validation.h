#pragma once

#include <string_view>
#include <vector>

#include "engine.h"

namespace validation {

int  score_exponent_from_double (double value, std::string_view name);
int  positive_integer_from_double (double value, std::string_view name);
void validate_score_params (const engine::ScoreParams& params);
void append_warning (std::vector<engine::Diagnostic>& diagnostics, engine::DiagnosticComponent component,
                     engine::DiagnosticCode code);

} // namespace validation
