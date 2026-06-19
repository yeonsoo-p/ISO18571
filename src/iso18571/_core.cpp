#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>

#include "engine.hpp"
#include "validation.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace py = pybind11;

namespace {

using iso18571::CurveDType;
using iso18571::CurveView;
using iso18571::Diagnostic;
using iso18571::DiagnosticCode;
using iso18571::DiagnosticSeverity;
using iso18571::Index;
using iso18571::ScoreParams;
using iso18571::ScoreResult;
using iso18571::SignalView;

struct NativeCurve {
    py::array array;
    CurveView view;
};

struct ValidatedCurves {
    NativeCurve reference;
    NativeCurve comparison;
    double      dt = 0.0;
};

struct OwnedCurve {
    std::vector<double> value;

    SignalView view () const {
        return {
            std::span<const double>(value.data(), value.size()),
            static_cast<Index>(value.size()),
        };
    }
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

bool dtype_from_buffer (const py::buffer_info& info, CurveDType& dtype) {
    if (buffer_is_float64(info)) {
        dtype = CurveDType::Float64;
        return true;
    }
    if (buffer_is_float32(info)) {
        dtype = CurveDType::Float32;
        return true;
    }
    return false;
}

py::array cast_array_to_float64 (const py::array& array, const char* name) {
    py::array_t<double, py::array::forcecast> casted = py::array_t<double, py::array::forcecast>::ensure(array);
    if (!casted) {
        throw std::invalid_argument(std::string(name) + " must be convertible to float64");
    }
    return py::array(casted);
}

NativeCurve native_curve_from_array (py::array array, const char* name) {
    const py::buffer_info info  = array.request();
    CurveDType            dtype = CurveDType::Float64;
    if (!dtype_from_buffer(info, dtype)) {
        array = cast_array_to_float64(array, name);
    }
    const py::buffer_info native_info = array.request();
    if (!dtype_from_buffer(native_info, dtype)) {
        throw std::runtime_error(std::string(name) + " did not cast to a supported native dtype");
    }

    if (native_info.ndim != 2) {
        throw std::invalid_argument(std::string(name) + " must be a 2D array");
    }
    if (native_info.shape[0] <= 0) {
        throw std::invalid_argument(std::string(name) + " must not be empty");
    }
    if (native_info.shape[1] != 2) {
        throw std::invalid_argument(std::string(name) + " must have shape (n, 2)");
    }

    return {
        array,
        {
          static_cast<const char*>(native_info.ptr),
          static_cast<Index>(native_info.strides[0]),
          static_cast<Index>(native_info.strides[1]),
          static_cast<Index>(native_info.shape[0]),
          dtype, }
    };
}

double time_grid_tolerance (const CurveView& curve) { return curve.dtype == CurveDType::Float32 ? 1.0e-9 : 1.0e-12; }

double derive_uniform_dt (const CurveView& curve, const char* name) {
    if (curve.n < 2) {
        throw std::invalid_argument(std::string(name) + " must have at least 2 samples");
    }

    double previous_time = curve.time(0);
    if (!std::isfinite(previous_time)) {
        throw std::invalid_argument(std::string(name) + " time values must be finite");
    }

    const double second_time = curve.time(1);
    if (!std::isfinite(second_time)) {
        throw std::invalid_argument(std::string(name) + " time values must be finite");
    }

    const double dt = second_time - previous_time;
    if (!std::isfinite(dt) || dt <= 0.0) {
        throw std::invalid_argument(std::string(name) + " time values must be strictly increasing");
    }

    const double tolerance = time_grid_tolerance(curve);
    previous_time          = second_time;
    for (Index idx = 2; idx < curve.n; ++idx) {
        const double current_time = curve.time(idx);
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

double validate_time_grids (const CurveView& reference, const CurveView& comparison) {
    const double reference_dt  = derive_uniform_dt(reference, "reference_curve");
    const double comparison_dt = derive_uniform_dt(comparison, "comparison_curve");
    const double tolerance     = std::max(time_grid_tolerance(reference), time_grid_tolerance(comparison));
    if (std::abs(comparison_dt - reference_dt) > tolerance) {
        throw std::invalid_argument("Curve time intervals are not equal");
    }

    for (Index idx = 0; idx < reference.n; ++idx) {
        if (std::abs(reference.time(idx) - comparison.time(idx)) > tolerance) {
            throw std::invalid_argument("Curve time values are not equal");
        }
    }

    return reference_dt;
}

void validate_signal_values (const CurveView& curve, const char* name) {
    for (Index idx = 0; idx < curve.n; ++idx) {
        if (!std::isfinite(curve.value(idx))) {
            throw std::invalid_argument(std::string(name) + " signal values must be finite");
        }
    }
}

ValidatedCurves validate_curves (py::array reference_curve, py::array comparison_curve) {
    NativeCurve reference  = native_curve_from_array(reference_curve, "reference_curve");
    NativeCurve comparison = native_curve_from_array(comparison_curve, "comparison_curve");
    if (reference.view.n != comparison.view.n) {
        throw std::invalid_argument("Curves are not equal in size/dimension");
    }
    const double dt = validate_time_grids(reference.view, comparison.view);
    validate_signal_values(reference.view, "reference_curve");
    validate_signal_values(comparison.view, "comparison_curve");
    return {reference, comparison, dt};
}

OwnedCurve copy_curve (const CurveView& curve) {
    OwnedCurve out;
    out.value.resize(static_cast<std::size_t>(curve.n));
    for (Index idx = 0; idx < curve.n; ++idx) {
        const std::size_t offset = static_cast<std::size_t>(idx);
        out.value[offset]       = curve.value(idx);
    }
    return out;
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
    const OwnedCurve reference_snapshot  = copy_curve(curves.reference.view);
    const OwnedCurve comparison_snapshot = copy_curve(curves.comparison.view);
    const SignalView reference_view      = reference_snapshot.view();
    const SignalView comparison_view     = comparison_snapshot.view();

    ScoreResult result;
    {
        py::gil_scoped_release release;
        result = iso18571::dispatch_table().score_components(reference_view, comparison_view, score_params, curves.dt);
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
