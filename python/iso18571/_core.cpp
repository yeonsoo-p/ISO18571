#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>

#include "engine.h"
#include "validation.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace py = pybind11;

namespace {

using engine::Diagnostic;
using engine::DiagnosticCode;
using engine::DiagnosticSeverity;
using engine::DoubleSpan;
using engine::Index;
using engine::ScoreParams;
using engine::ScoreResult;

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
        return validation::positive_integer_from_double(std::numeric_limits<double>::quiet_NaN(), name);
    }

    py::object number = py::reinterpret_steal<py::object>(PyNumber_Float(value.ptr()));
    if (!number) {
        PyErr_Clear();
        return validation::positive_integer_from_double(std::numeric_limits<double>::quiet_NaN(), name);
    }

    return validation::positive_integer_from_double(py::cast<double>(number), name);
}

int get_required_score_exponent (const py::dict& params, const char* name) {
    const py::handle value = require_param(params, name);
    if (PyNumber_Check(value.ptr()) == 0) {
        return validation::score_exponent_from_double(std::numeric_limits<double>::quiet_NaN(), name);
    }

    py::object number = py::reinterpret_steal<py::object>(PyNumber_Float(value.ptr()));
    if (!number) {
        PyErr_Clear();
        return validation::score_exponent_from_double(std::numeric_limits<double>::quiet_NaN(), name);
    }

    return validation::score_exponent_from_double(py::cast<double>(number), name);
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

double time_tolerance_for_dtype (const py::dtype& dtype) {
    switch (dtype.kind()) {
    case 'i':
        if (dtype.equal(py::dtype::of<std::int8_t>()) || dtype.equal(py::dtype::of<std::int16_t>()) ||
            dtype.equal(py::dtype::of<std::int32_t>()) || dtype.equal(py::dtype::of<std::int64_t>())) {
            return 1.0e-12;
        }
        return 1.0e-12;
    case 'u':
        if (dtype.equal(py::dtype::of<std::uint8_t>()) || dtype.equal(py::dtype::of<std::uint16_t>()) ||
            dtype.equal(py::dtype::of<std::uint32_t>()) || dtype.equal(py::dtype::of<std::uint64_t>())) {
            return 1.0e-12;
        }
        return 1.0e-12;
    case 'f':
        if (dtype.equal(py::dtype::of<float>())) {
            return 1.0e-7;
        }
        if (dtype.equal(py::dtype::of<double>()) || dtype.equal(py::dtype::of<long double>())) {
            return 1.0e-12;
        }
        return 1.0e-12;
    case 'c':
        if (dtype.equal(py::dtype::of<std::complex<float>>())) {
            return 1.0e-7;
        }
        if (dtype.equal(py::dtype::of<std::complex<double>>()) ||
            dtype.equal(py::dtype::of<std::complex<long double>>())) {
            return 1.0e-12;
        }
        return 1.0e-12;
    default:
        return 1.0e-12;
    }
}

ValidatedCurves validate_curves (py::array reference_curve, py::array comparison_curve) {
    py::array reference  = reference_curve;
    py::array comparison = comparison_curve;

    const py::buffer_info initial_reference_info = reference.request();
    if (initial_reference_info.ndim != 2) {
        throw std::invalid_argument("reference_curve must be a 2D array");
    }
    if (initial_reference_info.shape[0] <= 0) {
        throw std::invalid_argument("reference_curve must not be empty");
    }
    if (initial_reference_info.shape[1] != 2) {
        throw std::invalid_argument("reference_curve must have shape (n, 2)");
    }

    const py::buffer_info initial_comparison_info = comparison.request();
    if (initial_comparison_info.ndim != 2) {
        throw std::invalid_argument("comparison_curve must be a 2D array");
    }
    if (initial_comparison_info.shape[0] <= 0) {
        throw std::invalid_argument("comparison_curve must not be empty");
    }
    if (initial_comparison_info.shape[1] != 2) {
        throw std::invalid_argument("comparison_curve must have shape (n, 2)");
    }

    const Index n = static_cast<Index>(initial_reference_info.shape[0]);
    if (n != static_cast<Index>(initial_comparison_info.shape[0])) {
        throw std::invalid_argument("Curves are not equal in size/dimension");
    }
    if (n < 2) {
        throw std::invalid_argument("reference_curve must have at least 2 samples");
    }

    const py::dtype initial_reference_dtype = reference.dtype();
    const char      initial_reference_kind  = initial_reference_dtype.kind();
    if (initial_reference_kind != 'i' && initial_reference_kind != 'u' && initial_reference_kind != 'f' &&
        initial_reference_kind != 'c') {
        throw std::invalid_argument("reference_curve must be numeric");
    }

    const py::dtype initial_comparison_dtype = comparison.dtype();
    const char      initial_comparison_kind  = initial_comparison_dtype.kind();
    if (initial_comparison_kind != 'i' && initial_comparison_kind != 'u' && initial_comparison_kind != 'f' &&
        initial_comparison_kind != 'c') {
        throw std::invalid_argument("comparison_curve must be numeric");
    }

    bool            reference_is_float32  = false;
    py::buffer_info reference_native_info = reference.request();
    if (!((reference_native_info.itemsize == static_cast<py::ssize_t>(sizeof(double)) &&
           reference_native_info.format == py::format_descriptor<double>::format()) ||
          (reference_native_info.itemsize == static_cast<py::ssize_t>(sizeof(float)) &&
           reference_native_info.format == py::format_descriptor<float>::format()))) {
        py::array_t<double, py::array::forcecast> casted = py::array_t<double, py::array::forcecast>::ensure(reference);
        if (!casted) {
            throw std::invalid_argument("reference_curve must be convertible to float64");
        }
        reference = py::array(casted);
    }
    reference_native_info = reference.request();
    reference_is_float32  = reference_native_info.itemsize == static_cast<py::ssize_t>(sizeof(float)) &&
                            reference_native_info.format == py::format_descriptor<float>::format();
    if (!reference_is_float32 && !(reference_native_info.itemsize == static_cast<py::ssize_t>(sizeof(double)) &&
                                   reference_native_info.format == py::format_descriptor<double>::format())) {
        throw std::runtime_error("reference_curve did not cast to a supported native dtype");
    }

    bool            comparison_is_float32  = false;
    py::buffer_info comparison_native_info = comparison.request();
    if (!((comparison_native_info.itemsize == static_cast<py::ssize_t>(sizeof(double)) &&
           comparison_native_info.format == py::format_descriptor<double>::format()) ||
          (comparison_native_info.itemsize == static_cast<py::ssize_t>(sizeof(float)) &&
           comparison_native_info.format == py::format_descriptor<float>::format()))) {
        py::array_t<double, py::array::forcecast> casted =
            py::array_t<double, py::array::forcecast>::ensure(comparison);
        if (!casted) {
            throw std::invalid_argument("comparison_curve must be convertible to float64");
        }
        comparison = py::array(casted);
    }
    comparison_native_info = comparison.request();
    comparison_is_float32  = comparison_native_info.itemsize == static_cast<py::ssize_t>(sizeof(float)) &&
                             comparison_native_info.format == py::format_descriptor<float>::format();
    if (!comparison_is_float32 && !(comparison_native_info.itemsize == static_cast<py::ssize_t>(sizeof(double)) &&
                                    comparison_native_info.format == py::format_descriptor<double>::format())) {
        throw std::runtime_error("comparison_curve did not cast to a supported native dtype");
    }

    const py::buffer_info reference_info  = reference.request();
    const py::buffer_info comparison_info = comparison.request();

    const char* reference_data          = static_cast<const char*>(reference_info.ptr);
    const Index reference_row_stride    = static_cast<Index>(reference_info.strides[0]);
    const Index reference_column_stride = static_cast<Index>(reference_info.strides[1]);

    const char* comparison_data          = static_cast<const char*>(comparison_info.ptr);
    const Index comparison_row_stride    = static_cast<Index>(comparison_info.strides[0]);
    const Index comparison_column_stride = static_cast<Index>(comparison_info.strides[1]);

    const char* reference_first_time_ptr = reference_data;
    double      reference_previous_time =
        reference_is_float32 ? static_cast<double>(*reinterpret_cast<const float*>(reference_first_time_ptr))
                             : static_cast<double>(*reinterpret_cast<const double*>(reference_first_time_ptr));
    if (!std::isfinite(reference_previous_time)) {
        throw std::invalid_argument("reference_curve time values must be finite");
    }

    const char*  reference_second_time_ptr = reference_data + reference_row_stride;
    const double reference_second_time =
        reference_is_float32 ? static_cast<double>(*reinterpret_cast<const float*>(reference_second_time_ptr))
                             : static_cast<double>(*reinterpret_cast<const double*>(reference_second_time_ptr));
    if (!std::isfinite(reference_second_time)) {
        throw std::invalid_argument("reference_curve time values must be finite");
    }

    const double reference_dt = reference_second_time - reference_previous_time;
    if (!std::isfinite(reference_dt) || reference_dt <= 0.0) {
        throw std::invalid_argument("reference_curve time values must be strictly increasing");
    }

    const double reference_time_tolerance = time_tolerance_for_dtype(initial_reference_dtype);
    reference_previous_time               = reference_second_time;
    for (Index idx = 2; idx < n; ++idx) {
        const char*  reference_time_ptr = reference_data + idx * reference_row_stride;
        const double current_time       = reference_is_float32
                                            ? static_cast<double>(*reinterpret_cast<const float*>(reference_time_ptr))
                                            : static_cast<double>(*reinterpret_cast<const double*>(reference_time_ptr));
        if (!std::isfinite(current_time)) {
            throw std::invalid_argument("reference_curve time values must be finite");
        }
        const double step = current_time - reference_previous_time;
        if (step <= 0.0) {
            throw std::invalid_argument("reference_curve time values must be strictly increasing");
        }
        if (std::abs(step - reference_dt) > reference_time_tolerance) {
            throw std::invalid_argument("reference_curve time values must have a constant interval");
        }
        reference_previous_time = current_time;
    }

    const char* comparison_first_time_ptr = comparison_data;
    double      comparison_previous_time =
        comparison_is_float32 ? static_cast<double>(*reinterpret_cast<const float*>(comparison_first_time_ptr))
                              : static_cast<double>(*reinterpret_cast<const double*>(comparison_first_time_ptr));
    if (!std::isfinite(comparison_previous_time)) {
        throw std::invalid_argument("comparison_curve time values must be finite");
    }

    const char*  comparison_second_time_ptr = comparison_data + comparison_row_stride;
    const double comparison_second_time =
        comparison_is_float32 ? static_cast<double>(*reinterpret_cast<const float*>(comparison_second_time_ptr))
                              : static_cast<double>(*reinterpret_cast<const double*>(comparison_second_time_ptr));
    if (!std::isfinite(comparison_second_time)) {
        throw std::invalid_argument("comparison_curve time values must be finite");
    }

    const double comparison_dt = comparison_second_time - comparison_previous_time;
    if (!std::isfinite(comparison_dt) || comparison_dt <= 0.0) {
        throw std::invalid_argument("comparison_curve time values must be strictly increasing");
    }

    const double comparison_time_tolerance = time_tolerance_for_dtype(initial_comparison_dtype);
    comparison_previous_time               = comparison_second_time;
    for (Index idx = 2; idx < n; ++idx) {
        const char*  comparison_time_ptr = comparison_data + idx * comparison_row_stride;
        const double current_time = comparison_is_float32
                                      ? static_cast<double>(*reinterpret_cast<const float*>(comparison_time_ptr))
                                      : static_cast<double>(*reinterpret_cast<const double*>(comparison_time_ptr));
        if (!std::isfinite(current_time)) {
            throw std::invalid_argument("comparison_curve time values must be finite");
        }
        const double step = current_time - comparison_previous_time;
        if (step <= 0.0) {
            throw std::invalid_argument("comparison_curve time values must be strictly increasing");
        }
        if (std::abs(step - comparison_dt) > comparison_time_tolerance) {
            throw std::invalid_argument("comparison_curve time values must have a constant interval");
        }
        comparison_previous_time = current_time;
    }

    const double time_tolerance = std::max(reference_time_tolerance, comparison_time_tolerance);
    if (std::abs(comparison_dt - reference_dt) > time_tolerance) {
        throw std::invalid_argument("Curve time intervals are not equal");
    }

    for (Index idx = 0; idx < n; ++idx) {
        const char*  reference_time_ptr  = reference_data + idx * reference_row_stride;
        const char*  comparison_time_ptr = comparison_data + idx * comparison_row_stride;
        const double reference_time  = reference_is_float32
                                         ? static_cast<double>(*reinterpret_cast<const float*>(reference_time_ptr))
                                         : static_cast<double>(*reinterpret_cast<const double*>(reference_time_ptr));
        const double comparison_time = comparison_is_float32
                                         ? static_cast<double>(*reinterpret_cast<const float*>(comparison_time_ptr))
                                         : static_cast<double>(*reinterpret_cast<const double*>(comparison_time_ptr));
        if (std::abs(reference_time - comparison_time) > time_tolerance) {
            throw std::invalid_argument("Curve time values are not equal");
        }
    }

    for (Index idx = 0; idx < n; ++idx) {
        const char*  reference_value_ptr = reference_data + idx * reference_row_stride + reference_column_stride;
        const double reference_value = reference_is_float32
                                         ? static_cast<double>(*reinterpret_cast<const float*>(reference_value_ptr))
                                         : static_cast<double>(*reinterpret_cast<const double*>(reference_value_ptr));
        if (!std::isfinite(reference_value)) {
            throw std::invalid_argument("reference_curve signal values must be finite");
        }
    }

    for (Index idx = 0; idx < n; ++idx) {
        const char*  comparison_value_ptr = comparison_data + idx * comparison_row_stride + comparison_column_stride;
        const double comparison_value = comparison_is_float32
                                          ? static_cast<double>(*reinterpret_cast<const float*>(comparison_value_ptr))
                                          : static_cast<double>(*reinterpret_cast<const double*>(comparison_value_ptr));
        if (!std::isfinite(comparison_value)) {
            throw std::invalid_argument("comparison_curve signal values must be finite");
        }
    }

    std::vector<double> reference_values(static_cast<std::size_t>(n));
    for (Index idx = 0; idx < n; ++idx) {
        const std::size_t offset              = static_cast<std::size_t>(idx);
        const char*       reference_value_ptr = reference_data + idx * reference_row_stride + reference_column_stride;
        reference_values[offset] = reference_is_float32
                                     ? static_cast<double>(*reinterpret_cast<const float*>(reference_value_ptr))
                                     : static_cast<double>(*reinterpret_cast<const double*>(reference_value_ptr));
    }

    std::vector<double> comparison_values(static_cast<std::size_t>(n));
    for (Index idx = 0; idx < n; ++idx) {
        const std::size_t offset         = static_cast<std::size_t>(idx);
        const char* comparison_value_ptr = comparison_data + idx * comparison_row_stride + comparison_column_stride;
        comparison_values[offset] = comparison_is_float32
                                      ? static_cast<double>(*reinterpret_cast<const float*>(comparison_value_ptr))
                                      : static_cast<double>(*reinterpret_cast<const double*>(comparison_value_ptr));
    }

    return {
        std::move(reference_values),
        std::move(comparison_values),
        reference_dt,
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
    validation::validate_score_params(score_params);
    const DoubleSpan reference_values(curves.reference_values.data(), curves.reference_values.size());
    const DoubleSpan comparison_values(curves.comparison_values.data(), curves.comparison_values.size());

    ScoreResult result;
    {
        py::gil_scoped_release release;
        result =
            engine::dispatch_table().score_components(reference_values, comparison_values, score_params, curves.dt);
    }

    emit_score_warnings(result);

    py::dict out;
    add_score_fields(out, result);
    return out;
}

py::dict backend_info () {
    py::dict info;
    info["implementation"] = "C++23";
    info["optimization"]   = engine::dispatch_table().level;
    return info;
}

} // namespace

PYBIND11_MODULE (_core, m) {
    m.doc() = "Clean-room native ISO/TS 18571 engine";
    m.def("backend_info", &backend_info);
    m.def("_score_components", &score_components, py::arg("reference_curve"), py::arg("comparison_curve"),
          py::arg("params"));
}
