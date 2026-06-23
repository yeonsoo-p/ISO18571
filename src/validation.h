#pragma once

#include <string_view>
#include <vector>

#include "engine.h"

namespace validation {

int         score_exponent_from_double (f64 value, std::string_view name);
int         positive_integer_from_double (f64 value, std::string_view name);
void        validate_score_params (engine::ScoreParams& params);
void        append_warning (std::vector<engine::Diagnostic>& diagnostics, engine::DiagnosticComponent component,
                            engine::DiagnosticCode code);
const char* warning_message_for_code (engine::DiagnosticCode code);

} // namespace validation
