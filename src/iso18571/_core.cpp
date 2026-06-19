#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>

#include "engine.hpp"
#include "validation.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace py = pybind11;

namespace {

using iso18571::Diagnostic;
using iso18571::DiagnosticCode;
using iso18571::DiagnosticSeverity;
using iso18571::DoubleSpan;
using iso18571::Index;
using iso18571::ScoreParams;
using iso18571::ScoreResult;

struct ValidatedCurves {
    std::vector<double> reference_values;
    std::vector<double> comparison_values;
    double              dt = 0.0;
};

py::handle require_param (const py::dict& params, const char* name) {
    const py::str key(name);
    if (!params.contains(key)) {
        throw std::invalid_argument(std::string("Missing required score parameter: ") + name);
    }
    return params[key];
}

double get_required_double_param (const py::dict& params, const char* name) {
    try {
        return py::cast<double>(require_param(params, name));
    } catch (const py::cast_error&) {
        throw std::invalid_argument(std::string(name) + " must be numeric");
    }
}

int get_required_positive_integer_param (const py::dict& params, const char* name) {
    const py::handle value = require_param(params, name);
    if (PyNumber_Check(value.ptr()) == 0) {
        return iso18571::positive_integer_from_double(std::numeric_limits<double>::quiet_NaN(), name);
    }

    py::object number = py::reinterpret_steal<py::object>(PyNumber_Float(value.ptr()));
    if (!number) {
        PyErr_Clear();
        return iso18571::positive_integer_from_double(std::numeric_limits<double>::quiet_NaN(), name);
    }

    return iso18571::positive_integer_from_double(py::cast<double>(number), name);
}

int get_required_score_exponent (const py::dict& params, const char* name) {
    const py::handle value = require_param(params, name);
    if (PyNumber_Check(value.ptr()) == 0) {
        return iso18571::score_exponent_from_double(std::numeric_limits<double>::quiet_NaN(), name);
    }

    py::object number = py::reinterpret_steal<py::object>(PyNumber_Float(value.ptr()));
    if (!number) {
        PyErr_Clear();
        return iso18571::score_exponent_from_double(std::numeric_limits<double>::quiet_NaN(), name);
    }

    return iso18571::score_exponent_from_double(py::cast<double>(number), name);
}

ScoreParams score_params_from_dict (const py::dict& params) {
    ScoreParams out;
    out.k_z      = get_required_positive_integer_param(params, "k_z");
    out.k_p      = get_required_score_exponent(params, "k_p");
    out.k_m      = get_required_score_exponent(params, "k_m");
    out.eps_m    = get_required_double_param(params, "eps_m");
    out.e_s      = get_required_double_param(params, "e_s");
    out.init_min = get_required_double_param(params, "init_min");
    out.a_0      = get_required_double_param(params, "a_0");
    out.b_0      = get_required_double_param(params, "b_0");
    out.w_z      = get_required_double_param(params, "w_z");
    out.w_p      = get_required_double_param(params, "w_p");
    out.w_m      = get_required_double_param(params, "w_m");
    out.w_s      = get_required_double_param(params, "w_s");
    return out;
}

bool buffer_is_float64 (const py::buffer_info& info) {
    return info.itemsize == static_cast<py::ssize_t>(sizeof(double)) &&
           info.format == py::format_descriptor<double>::format();
}

bool buffer_is_float32 (const py::buffer_info& info) {
    return info.itemsize == static_cast<py::ssize_t>(sizeof(float)) &&
           info.format == py::format_descriptor<float>::format();
}

py::array cast_array_to_float64 (const py::array& array, const char* name) {
    py::array_t<double, py::array::forcecast> casted = py::array_t<double, py::array::forcecast>::ensure(array);
    if (!casted) {
        throw std::invalid_argument(std::string(name) + " must be convertible to float64");
    }
    return py::array(casted);
}

bool buffer_is_native_float (const py::buffer_info& info) { return buffer_is_float64(info) || buffer_is_float32(info); }

void validate_curve_shape (const py::buffer_info& info, const char* name) {
    if (info.ndim != 2) {
        throw std::invalid_argument(std::string(name) + " must be a 2D array");
    }
    if (info.shape[0] <= 0) {
        throw std::invalid_argument(std::string(name) + " must not be empty");
    }
    if (info.shape[1] != 2) {
        throw std::invalid_argument(std::string(name) + " must have shape (n, 2)");
    }
}

py::array native_curve_array (py::array array, const char* name, bool& use_float32_time_tolerance) {
    py::buffer_info info = array.request();
    if (!buffer_is_native_float(info)) {
        array = cast_array_to_float64(array, name);
    }
    info                       = array.request();
    use_float32_time_tolerance = buffer_is_float32(info);
    if (!use_float32_time_tolerance && !buffer_is_float64(info)) {
        throw std::runtime_error(std::string(name) + " did not cast to a supported native dtype");
    }

    validate_curve_shape(info, name);
    return array;
}

template<typename T>
double scalar_at (const py::buffer_info& info, Index row, Index column) {
    const char* ptr = static_cast<const char*>(info.ptr) + row * static_cast<Index>(info.strides[0]) +
                      column * static_cast<Index>(info.strides[1]);
    return static_cast<double>(*reinterpret_cast<const T*>(ptr));
}

double curve_scalar_at (const py::buffer_info& info, bool is_float32, Index row, Index column) {
    if (is_float32) {
        return scalar_at<float>(info, row, column);
    }
    return scalar_at<double>(info, row, column);
}

Index curve_length (const py::buffer_info& info) { return static_cast<Index>(info.shape[0]); }

double time_grid_tolerance (bool is_float32) { return is_float32 ? 1.0e-9 : 1.0e-12; }

double derive_uniform_dt (const py::buffer_info& info, bool is_float32, const char* name) {
    const Index n = curve_length(info);
    if (n < 2) {
        throw std::invalid_argument(std::string(name) + " must have at least 2 samples");
    }

    double previous_time = curve_scalar_at(info, is_float32, 0, 0);
    if (!std::isfinite(previous_time)) {
        throw std::invalid_argument(std::string(name) + " time values must be finite");
    }

    const double second_time = curve_scalar_at(info, is_float32, 1, 0);
    if (!std::isfinite(second_time)) {
        throw std::invalid_argument(std::string(name) + " time values must be finite");
    }

    const double dt = second_time - previous_time;
    if (!std::isfinite(dt) || dt <= 0.0) {
        throw std::invalid_argument(std::string(name) + " time values must be strictly increasing");
    }

    const double tolerance = time_grid_tolerance(is_float32);
    previous_time          = second_time;
    for (Index idx = 2; idx < n; ++idx) {
        const double current_time = curve_scalar_at(info, is_float32, idx, 0);
        if (!std::isfinite(current_time)) {
            throw std::invalid_argument(std::string(name) + " time values must be finite");
        }
        const double step = current_time - previous_time;
        if (step <= 0.0) {
            throw std::invalid_argument(std::string(name) + " time values must be strictly increasing");
        }
        if (std::abs(step - dt) > tolerance) {
            throw std::invalid_argument(std::string(name) + " time values must have a constant interval");
        }
        previous_time = current_time;
    }

    return dt;
}

double validate_time_grids (const py::buffer_info& reference, bool reference_is_float32,
                            const py::buffer_info& comparison, bool comparison_is_float32) {
    const double reference_dt  = derive_uniform_dt(reference, reference_is_float32, "reference_curve");
    const double comparison_dt = derive_uniform_dt(comparison, comparison_is_float32, "comparison_curve");
    const double tolerance =
        std::max(time_grid_tolerance(reference_is_float32), time_grid_tolerance(comparison_is_float32));
    if (std::abs(comparison_dt - reference_dt) > tolerance) {
        throw std::invalid_argument("Curve time intervals are not equal");
    }

    const Index n = curve_length(reference);
    for (Index idx = 0; idx < n; ++idx) {
        const double reference_time  = curve_scalar_at(reference, reference_is_float32, idx, 0);
        const double comparison_time = curve_scalar_at(comparison, comparison_is_float32, idx, 0);
        if (std::abs(reference_time - comparison_time) > tolerance) {
            throw std::invalid_argument("Curve time values are not equal");
        }
    }

    return reference_dt;
}

void validate_signal_values (const py::buffer_info& info, bool is_float32, const char* name) {
    const Index n = curve_length(info);
    for (Index idx = 0; idx < n; ++idx) {
        if (!std::isfinite(curve_scalar_at(info, is_float32, idx, 1))) {
            throw std::invalid_argument(std::string(name) + " signal values must be finite");
        }
    }
}

std::vector<double> copy_signal_values (const py::buffer_info& info, bool is_float32) {
    const Index         n = curve_length(info);
    std::vector<double> out(static_cast<std::size_t>(n));
    for (Index idx = 0; idx < n; ++idx) {
        const std::size_t offset = static_cast<std::size_t>(idx);
        out[offset]              = curve_scalar_at(info, is_float32, idx, 1);
    }
    return out;
}

ValidatedCurves validate_curves (py::array reference_curve, py::array comparison_curve) {
    bool      reference_is_float32  = false;
    bool      comparison_is_float32 = false;
    py::array reference             = native_curve_array(reference_curve, "reference_curve", reference_is_float32);
    py::array comparison            = native_curve_array(comparison_curve, "comparison_curve", comparison_is_float32);

    const py::buffer_info reference_info  = reference.request();
    const py::buffer_info comparison_info = comparison.request();
    if (curve_length(reference_info) != curve_length(comparison_info)) {
        throw std::invalid_argument("Curves are not equal in size/dimension");
    }

    const double dt = validate_time_grids(reference_info, reference_is_float32, comparison_info, comparison_is_float32);
    validate_signal_values(reference_info, reference_is_float32, "reference_curve");
    validate_signal_values(comparison_info, comparison_is_float32, "comparison_curve");
    return {
        copy_signal_values(reference_info, reference_is_float32),
        copy_signal_values(comparison_info, comparison_is_float32),
        dt,
    };
}

void emit_runtime_warning (const char* message) {
    if (PyErr_WarnEx(PyExc_RuntimeWarning, message, 1) != 0) {
        throw py::error_already_set();
    }
}

const char* warning_message_for_code (DiagnosticCode code) {
    switch (code) {
    case DiagnosticCode::PhaseUndefinedCorrelation:
        return "ISO18571 phase correlation is undefined; using finite fallback rho_e";
    case DiagnosticCode::PhaseShiftClampedToUnshifted:
        return "ISO18571 phase alignment left fewer than 9 samples; using unshifted alignment";
    case DiagnosticCode::MagnitudeZeroReferenceDenominator:
        return "ISO18571 magnitude reference denominator is zero; using fallback magnitude score";
    case DiagnosticCode::SlopeZeroReferenceDenominator:
        return "ISO18571 slope reference denominator is zero; using fallback slope score";
    }
    throw std::runtime_error("Unknown ISO18571 native diagnostic code");
}

void emit_component_warnings (const std::vector<Diagnostic>& diagnostics) {
    for (const Diagnostic& diagnostic : diagnostics) {
        if (diagnostic.severity != DiagnosticSeverity::Warning) {
            throw std::runtime_error("Unsupported ISO18571 native diagnostic severity");
        }
        emit_runtime_warning(warning_message_for_code(diagnostic.code));
    }
}

void emit_score_warnings (const ScoreResult& result) {
    emit_component_warnings(result.corridor.diagnostics);
    emit_component_warnings(result.phase.diagnostics);
    emit_component_warnings(result.magnitude.diagnostics);
    emit_component_warnings(result.slope.diagnostics);
}

void add_score_fields (py::dict& out, const ScoreResult& result) {
    out["Z"]                = result.corridor.score;
    out["EP"]               = result.phase.score;
    out["EM"]               = result.magnitude.score;
    out["ES"]               = result.slope.score;
    out["R"]                = result.overall;
    out["n_eps"]            = result.phase.alignment.n_eps;
    out["rho_e"]            = result.phase.correlation.rho_e;
    out["reference_start"]  = result.phase.alignment.reference_start;
    out["comparison_start"] = result.phase.alignment.comparison_start;
    out["shift_length"]     = result.phase.alignment.length;
}

py::dict score_components (py::array reference_curve, py::array comparison_curve, py::dict params) {
    const ValidatedCurves curves       = validate_curves(reference_curve, comparison_curve);
    ScoreParams           score_params = score_params_from_dict(params);
    iso18571::validate_score_params(score_params);
    const DoubleSpan reference_values(curves.reference_values.data(), curves.reference_values.size());
    const DoubleSpan comparison_values(curves.comparison_values.data(), curves.comparison_values.size());

    ScoreResult result;
    {
        py::gil_scoped_release release;
        result =
            iso18571::dispatch_table().score_components(reference_values, comparison_values, score_params, curves.dt);
    }

    emit_score_warnings(result);

    py::dict out;
    add_score_fields(out, result);
    return out;
}

py::dict backend_info () {
    py::dict info;
    info["implementation"] = "C++20";
    info["optimization"]   = iso18571::dispatch_table().level;
    return info;
}

} // namespace

PYBIND11_MODULE (_core, m) {
    m.doc() = "Clean-room native ISO/TS 18571 engine";
    m.def("backend_info", &backend_info);
    m.def("_score_components", &score_components, py::arg("reference_curve"), py::arg("comparison_curve"),
          py::arg("params"));
}
