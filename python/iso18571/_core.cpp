#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>

#include "engine.h"
#include "float16.h"
#include "numeric.h"
#include "validation.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstdint>
#include <limits>
#include <ranges>
#include <span>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace py = pybind11;

namespace {

using engine::Diagnostic;
using engine::DiagnosticSeverity;
using engine::ScoreParams;
using engine::ScoreResult;

struct ValidatedCurves {
    std::vector<f64> reference_values;
    std::vector<f64> comparison_values;
    f64              dt = 0.0;
};

py::handle require_param (const py::dict& params, const char* name) {
    const py::str key(name);
    if (!params.contains(key)) {
        throw std::invalid_argument(std::string("Missing required score parameter: ") + name);
    }
    py::handle value      = params[key];
    bool       is_boolean = PyBool_Check(value.ptr()) != 0;
    if (!is_boolean) {
        py::object numpy_bool    = py::module_::import("numpy").attr("bool_");
        const int  is_numpy_bool = PyObject_IsInstance(value.ptr(), numpy_bool.ptr());
        if (is_numpy_bool < 0) {
            throw py::error_already_set();
        }
        is_boolean = is_numpy_bool != 0;
    }
    if (is_boolean) {
        throw std::invalid_argument(std::string(name) + " must not be boolean");
    }
    return value;
}

f64 get_required_double_param (const py::dict& params, const char* name) {
    try {
        return py::cast<f64>(require_param(params, name));
    } catch (const py::cast_error&) {
        throw std::invalid_argument(std::string(name) + " must be numeric");
    }
}

int get_required_positive_integer_param (const py::dict& params, const char* name) {
    const py::handle value = require_param(params, name);
    if (PyNumber_Check(value.ptr()) == 0) {
        return validation::positive_integer_from_double(std::numeric_limits<f64>::quiet_NaN(), name);
    }

    py::object number = py::reinterpret_steal<py::object>(PyNumber_Float(value.ptr()));
    if (!number) {
        PyErr_Clear();
        return validation::positive_integer_from_double(std::numeric_limits<f64>::quiet_NaN(), name);
    }

    return validation::positive_integer_from_double(py::cast<f64>(number), name);
}

int get_required_score_exponent (const py::dict& params, const char* name) {
    const py::handle value = require_param(params, name);
    if (PyNumber_Check(value.ptr()) == 0) {
        return validation::score_exponent_from_double(std::numeric_limits<f64>::quiet_NaN(), name);
    }

    py::object number = py::reinterpret_steal<py::object>(PyNumber_Float(value.ptr()));
    if (!number) {
        PyErr_Clear();
        return validation::score_exponent_from_double(std::numeric_limits<f64>::quiet_NaN(), name);
    }

    return validation::score_exponent_from_double(py::cast<f64>(number), name);
}

ScoreParams score_params_from_dict (const py::dict& params) {
    ScoreParams out;
    out.k_z      = get_required_positive_integer_param(params, "k_z");
    out.k_p      = get_required_score_exponent(params, "k_p");
    out.k_m      = get_required_score_exponent(params, "k_m");
    out.k_s      = get_required_score_exponent(params, "k_s");
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

enum class CurveInputDtype {
    Int8,
    Int16,
    Int32,
    Int64,
    UInt8,
    UInt16,
    UInt32,
    UInt64,
    Float16,
    Float32,
    Float64,
    Float128,
    Complex64,  // real: f32  imag: f32
    Complex128, // real: f64  imag: f64
    Complex256, // real: f128 imag: f128
    Other,
};

template<typename T>
struct TypeTag {
    using type = T;
};

template<typename T>
struct IsStdComplex: std::false_type {};

template<typename T>
struct IsStdComplex<std::complex<T>>: std::true_type {};

template<typename T>
void visit_curve_dtype (CurveInputDtype dtype, T&& visitor) {
    switch (dtype) {
    case CurveInputDtype::Int8:
        std::forward<T>(visitor)(TypeTag<i8> {});
        return;
    case CurveInputDtype::Int16:
        std::forward<T>(visitor)(TypeTag<i16> {});
        return;
    case CurveInputDtype::Int32:
        std::forward<T>(visitor)(TypeTag<i32> {});
        return;
    case CurveInputDtype::Int64:
        std::forward<T>(visitor)(TypeTag<i64> {});
        return;
    case CurveInputDtype::UInt8:
        std::forward<T>(visitor)(TypeTag<u8> {});
        return;
    case CurveInputDtype::UInt16:
        std::forward<T>(visitor)(TypeTag<u16> {});
        return;
    case CurveInputDtype::UInt32:
        std::forward<T>(visitor)(TypeTag<u32> {});
        return;
    case CurveInputDtype::UInt64:
        std::forward<T>(visitor)(TypeTag<u64> {});
        return;
    case CurveInputDtype::Float16:
        std::forward<T>(visitor)(TypeTag<f16> {});
        return;
    case CurveInputDtype::Float32:
        std::forward<T>(visitor)(TypeTag<f32> {});
        return;
    case CurveInputDtype::Float64:
        std::forward<T>(visitor)(TypeTag<f64> {});
        return;
    case CurveInputDtype::Float128:
        std::forward<T>(visitor)(TypeTag<f128> {});
        return;
    case CurveInputDtype::Complex64:
        std::forward<T>(visitor)(TypeTag<c64> {});
        return;
    case CurveInputDtype::Complex128:
        std::forward<T>(visitor)(TypeTag<c128> {});
        return;
    case CurveInputDtype::Complex256:
        std::forward<T>(visitor)(TypeTag<c256> {});
        return;
    case CurveInputDtype::Other:
        return;
    }
    std::unreachable();
}

template<typename T>
auto real_component (const T& value) {
    if constexpr (IsStdComplex<T>::value) {
        return value.real();
    } else {
        return value;
    }
}

template<typename T>
auto materialize_typed_dt (const py::buffer_info& info, const char* curve_name, std::ptrdiff_t n) {
    using Time                      = decltype(real_component(std::declval<T>()));
    const T*          data          = static_cast<const T*>(info.ptr);
    const std::size_t element_count = static_cast<std::size_t>(n) * 2U;
    auto              time_values   = std::span<const T>(data, element_count) | std::views::stride(2);
    auto              time_value    = time_values.begin();

    Time current = real_component(*time_value);
    ++time_value;
    Time next = real_component(*time_value);
    if (next <= current) {
        throw std::invalid_argument(std::string(curve_name) + " time values must be strictly increasing");
    }

    if constexpr (std::is_integral_v<Time>) {
        using Step    = std::make_unsigned_t<Time>;
        const Step dt = static_cast<Step>(next) - static_cast<Step>(current);

        current = next;
        for (std::ptrdiff_t idx = 2; idx < n; ++idx) {
            ++time_value;
            next = real_component(*time_value);

            if (next <= current) {
                throw std::invalid_argument(std::string(curve_name) + " time values must be strictly increasing");
            }

            const Step step = static_cast<Step>(next) - static_cast<Step>(current);
            if (step != dt) {
                throw std::invalid_argument(std::string(curve_name) + " time values must have a constant interval");
            }

            current = next;
        }

        return dt;
    } else {
        const Time dt = next - current;
        if (!std::isfinite(dt) || dt <= Time {0}) {
            throw std::invalid_argument(std::string(curve_name) + " time values must have a constant interval");
        }

        f128 mean = static_cast<f128>(dt);
        current   = next;
        for (std::ptrdiff_t idx = 2; idx < n; ++idx) {
            ++time_value;
            next = real_component(*time_value);

            if (next <= current) {
                throw std::invalid_argument(std::string(curve_name) + " time values must be strictly increasing");
            }

            const Time step = next - current;
            if (!std::isfinite(step) || step <= Time {0} || !numeric::almost_equal(step, dt)) {
                throw std::invalid_argument(std::string(curve_name) + " time values must have a constant interval");
            }

            mean += (static_cast<f128>(step) - mean) / static_cast<f128>(idx);
            current = next;
        }

        return static_cast<Time>(mean);
    }
}

template<typename T>
void require_typed_time (const py::buffer_info& info, const char* curve_name, std::ptrdiff_t n) {
    const T*          data          = static_cast<const T*>(info.ptr);
    const std::size_t element_count = static_cast<std::size_t>(n) * 2U;
    auto              time_values   = std::span<const T>(data, element_count) | std::views::stride(2);

    for (const T& value : time_values) {
        if constexpr (IsStdComplex<T>::value) {
            const auto zero = decltype(value.imag()) {0};
            if (value.imag() != zero) {
                throw std::invalid_argument(std::string(curve_name) + " must have zero imaginary components");
            }
        }
        const auto real = real_component(value);
        if (!std::isfinite(real)) {
            throw std::invalid_argument(std::string(curve_name) + " must be finite");
        }
    }

    materialize_typed_dt<T>(info, curve_name, n);
}

template<typename T>
void require_typed_value (const py::buffer_info& info, const char* curve_name, std::ptrdiff_t n) {
    const T*          data          = static_cast<const T*>(info.ptr);
    const std::size_t element_count = static_cast<std::size_t>(n) * 2U;
    auto              values        = std::span<const T>(data + 1, element_count - 1U) | std::views::stride(2);

    for (const T& value : values) {
        if constexpr (IsStdComplex<T>::value) {
            const auto zero = decltype(value.imag()) {0};
            if (value.imag() != zero) {
                throw std::invalid_argument(std::string(curve_name) + " must have zero imaginary components");
            }
        }
        const auto real = real_component(value);
        if (!std::isfinite(real)) {
            throw std::invalid_argument(std::string(curve_name) + " must be finite");
        }
        if constexpr (std::is_same_v<decltype(real), f128>) {
            constexpr f128 max_double = static_cast<f128>(std::numeric_limits<f64>::max());
            if (real < -max_double || real > max_double) {
                throw std::invalid_argument(std::string(curve_name) + " must be representable as finite float64");
            }
        }
    }
}

CurveInputDtype curve_input_dtype (const py::dtype& dtype, const py::buffer_info& info) {
    switch (dtype.kind()) {
    case 'i':
        if (dtype.equal(py::dtype::of<i8>())) {
            return CurveInputDtype::Int8;
        }
        if (dtype.equal(py::dtype::of<i16>())) {
            return CurveInputDtype::Int16;
        }
        if (dtype.equal(py::dtype::of<i32>())) {
            return CurveInputDtype::Int32;
        }
        if (dtype.equal(py::dtype::of<i64>())) {
            return CurveInputDtype::Int64;
        }
        return CurveInputDtype::Other;
    case 'u':
        if (dtype.equal(py::dtype::of<u8>())) {
            return CurveInputDtype::UInt8;
        }
        if (dtype.equal(py::dtype::of<u16>())) {
            return CurveInputDtype::UInt16;
        }
        if (dtype.equal(py::dtype::of<u32>())) {
            return CurveInputDtype::UInt32;
        }
        if (dtype.equal(py::dtype::of<u64>())) {
            return CurveInputDtype::UInt64;
        }
        return CurveInputDtype::Other;
    case 'f':
        if (info.itemsize == static_cast<py::ssize_t>(sizeof(f16))) {
            return CurveInputDtype::Float16;
        }
        if (dtype.equal(py::dtype::of<f32>())) {
            return CurveInputDtype::Float32;
        }
        if (dtype.equal(py::dtype::of<f64>())) {
            return CurveInputDtype::Float64;
        }
        if (dtype.equal(py::dtype::of<f128>())) {
            return CurveInputDtype::Float128;
        }
        return CurveInputDtype::Other;
    case 'c':
        if (dtype.equal(py::dtype::of<c64>())) {
            return CurveInputDtype::Complex64;
        }
        if (dtype.equal(py::dtype::of<c128>())) {
            return CurveInputDtype::Complex128;
        }
        if (dtype.equal(py::dtype::of<c256>())) {
            return CurveInputDtype::Complex256;
        }
        return CurveInputDtype::Other;
    default:
        return CurveInputDtype::Other;
    }
}

void require_curve_input (CurveInputDtype dtype, const py::buffer_info& info, const char* curve_name,
                          std::ptrdiff_t n) {
    visit_curve_dtype(dtype, [&] (auto tag) {
        using T = typename decltype(tag)::type;
        require_typed_time<T>(info, curve_name, n);
        require_typed_value<T>(info, curve_name, n);
    });
}

void require_materialized_dt (f64 dt, const char* curve_name) {
    if (!std::isfinite(dt) || dt <= 0.0 || dt < std::numeric_limits<f64>::min()) {
        throw std::invalid_argument(std::string(curve_name) + " time interval is too small");
    }
}

template<typename T>
f64 materialize_dt (const py::buffer_info& info, const char* curve_name, std::ptrdiff_t n) {
    const f64 dt = static_cast<f64>(materialize_typed_dt<T>(info, curve_name, n));
    require_materialized_dt(dt, curve_name);
    return dt;
}

f64 materialize_matching_dt (CurveInputDtype reference_dtype, const py::buffer_info& reference_info,
                             CurveInputDtype comparison_dtype, const py::buffer_info& comparison_info, std::ptrdiff_t n,
                             std::vector<Diagnostic>& diagnostics) {
    constexpr f64 kRecommendedMinimumDt              = 0.01;
    f64           reference_dt                       = 0.0;
    bool          interval_below_recommended_minimum = false;
    bool          time_start_not_zero                = false;
    visit_curve_dtype(reference_dtype, [&] (auto reference_tag) {
        using ReferenceT                 = typename decltype(reference_tag)::type;
        const ReferenceT* reference_data = static_cast<const ReferenceT*>(reference_info.ptr);
        if (!numeric::almost_equal(real_component(reference_data[0]), 0)) {
            time_start_not_zero = true;
        }
        const auto typed_reference_dt = materialize_typed_dt<ReferenceT>(reference_info, "reference_curve", n);
        reference_dt                  = static_cast<f64>(typed_reference_dt);
        require_materialized_dt(reference_dt, "reference_curve");
        if (reference_dt < kRecommendedMinimumDt && !numeric::almost_equal(typed_reference_dt, kRecommendedMinimumDt)) {
            interval_below_recommended_minimum = true;
        }
        visit_curve_dtype(comparison_dtype, [&] (auto comparison_tag) {
            using ComparisonT                  = typename decltype(comparison_tag)::type;
            const ComparisonT* comparison_data = static_cast<const ComparisonT*>(comparison_info.ptr);
            if (!numeric::almost_equal(real_component(comparison_data[0]), 0)) {
                time_start_not_zero = true;
            }
            const auto typed_comparison_dt = materialize_typed_dt<ComparisonT>(comparison_info, "comparison_curve", n);
            const f64  comparison_dt       = static_cast<f64>(typed_comparison_dt);
            require_materialized_dt(comparison_dt, "comparison_curve");
            if (comparison_dt < kRecommendedMinimumDt &&
                !numeric::almost_equal(typed_comparison_dt, kRecommendedMinimumDt)) {
                interval_below_recommended_minimum = true;
            }
            if (!numeric::almost_equal(typed_reference_dt, typed_comparison_dt)) {
                throw std::invalid_argument("Curves must have matching time intervals");
            }
        });
    });

    if (reference_dtype != comparison_dtype) {
        diagnostics.push_back({DiagnosticSeverity::Warning, engine::DiagnosticComponent::Validation,
                               engine::DiagnosticCode::InputDtypeMismatch});
    }
    if (interval_below_recommended_minimum) {
        diagnostics.push_back({DiagnosticSeverity::Warning, engine::DiagnosticComponent::Validation,
                               engine::DiagnosticCode::InputIntervalBelowRecommendedMinimum});
    }
    if (time_start_not_zero) {
        diagnostics.push_back({DiagnosticSeverity::Warning, engine::DiagnosticComponent::Validation,
                               engine::DiagnosticCode::InputTimeStartNotZero});
    }

    return reference_dt;
}

template<typename T>
void materialize_typed_curve_values (const py::buffer_info& info, std::ptrdiff_t n, std::span<f64> values) {
    const T*          data          = static_cast<const T*>(info.ptr);
    const std::size_t element_count = static_cast<std::size_t>(n) * 2U;
    auto              curve_values  = std::span<const T>(data + 1, element_count - 1U) | std::views::stride(2);

    std::size_t idx = 0;
    for (const T& value : curve_values) {
        values[idx] = static_cast<f64>(real_component(value));
        ++idx;
    }
}

std::vector<f64> materialize_curve_values (CurveInputDtype dtype, const py::buffer_info& info, std::ptrdiff_t n) {
    std::vector<f64> values(static_cast<std::size_t>(n));
    visit_curve_dtype(dtype, [&] (auto tag) {
        using T = typename decltype(tag)::type;
        materialize_typed_curve_values<T>(info, n, std::span<f64>(values));
    });
    return values;
}

py::buffer_info require_curve_shape (const py::array& curve, const char* curve_name) {
    py::buffer_info info = curve.request();
    if (info.ndim != 2) {
        throw std::invalid_argument(std::string(curve_name) + " must be a 2D array");
    }
    if (info.shape[1] != 2) {
        throw std::invalid_argument(std::string(curve_name) + " must have shape (n, 2)");
    }
    if (info.strides[1] != info.itemsize || info.strides[0] != 2 * info.itemsize) {
        throw std::invalid_argument(std::string(curve_name) + " must be C-contiguous");
    }
    return info;
}

CurveInputDtype require_curve_dtype (const py::array& curve, const py::buffer_info& info, const char* curve_name,
                                     std::vector<Diagnostic>& diagnostics) {
    const py::dtype   dtype     = curve.dtype();
    const std::string byteorder = py::cast<std::string>(dtype.attr("byteorder"));
    if (byteorder != "=" && byteorder != "|") {
        throw std::invalid_argument(std::string(curve_name) + " must use native byte order");
    }

    const CurveInputDtype input_dtype = curve_input_dtype(dtype, info);
    if (input_dtype == CurveInputDtype::Other) {
        throw std::invalid_argument(std::string(curve_name) + " has unsupported dtype");
    }
    if (input_dtype != CurveInputDtype::Float64) {
        diagnostics.push_back({DiagnosticSeverity::Warning, engine::DiagnosticComponent::Validation,
                               engine::DiagnosticCode::InputNonFloat64Dtype});
    }
    return input_dtype;
}

ValidatedCurves validate_curves (py::array reference_curve, py::array comparison_curve,
                                 std::vector<Diagnostic>& diagnostics) {
    py::buffer_info reference_info  = require_curve_shape(reference_curve, "reference_curve");
    py::buffer_info comparison_info = require_curve_shape(comparison_curve, "comparison_curve");

    const std::ptrdiff_t n = static_cast<std::ptrdiff_t>(reference_info.shape[0]);
    if (n != static_cast<std::ptrdiff_t>(comparison_info.shape[0])) {
        throw std::invalid_argument("Curves are not equal in size/dimension");
    }
    if (n < 9) {
        throw std::invalid_argument("Curves must have at least 9 samples");
    }

    CurveInputDtype reference_dtype =
        require_curve_dtype(reference_curve, reference_info, "reference_curve", diagnostics);
    CurveInputDtype comparison_dtype =
        require_curve_dtype(comparison_curve, comparison_info, "comparison_curve", diagnostics);

    require_curve_input(reference_dtype, reference_info, "reference_curve", n);
    require_curve_input(comparison_dtype, comparison_info, "comparison_curve", n);

    const f64 reference_dt =
        materialize_matching_dt(reference_dtype, reference_info, comparison_dtype, comparison_info, n, diagnostics);

    std::vector<f64> reference_values  = materialize_curve_values(reference_dtype, reference_info, n);
    std::vector<f64> comparison_values = materialize_curve_values(comparison_dtype, comparison_info, n);

    return {
        std::move(reference_values),
        std::move(comparison_values),
        reference_dt,
    };
}

void emit_component_warnings (const std::vector<Diagnostic>& diagnostics) {
    for (const Diagnostic& diagnostic : diagnostics) {
        if (diagnostic.severity != DiagnosticSeverity::Warning) {
            throw std::runtime_error("Unsupported ISO18571 native diagnostic severity");
        }
        const char* message = validation::warning_message_for_code(diagnostic.code);
        if (PyErr_WarnEx(PyExc_RuntimeWarning, message, 1) != 0) {
            throw py::error_already_set();
        }
    }
}

void add_score_fields (py::dict& out, const ScoreResult& result, const ValidatedCurves& curves) {
    out["Z"]  = result.corridor.score;
    out["EP"] = result.phase.score;
    out["EM"] = result.magnitude.score;
    out["ES"] = result.slope.score;
    out["R"]  = result.overall;
    out["dt"] = curves.dt;

    out["corridor_t_norm"]           = result.corridor.t_norm;
    out["corridor_inner_half_width"] = result.corridor.inner_half_width;
    out["corridor_outer_half_width"] = result.corridor.outer_half_width;

    out["phase_n_eps"]            = result.phase.n_eps;
    out["phase_rho_e"]            = result.phase.rho_e;
    out["phase_reference_start"]  = result.phase.reference_start;
    out["phase_comparison_start"] = result.phase.comparison_start;
    out["phase_shift_length"]     = result.phase.length;
    out["phase_max_shift"]        = result.phase.max_shift;

    out["magnitude_numerator"]   = result.magnitude.numerator;
    out["magnitude_denominator"] = result.magnitude.denominator;
    out["magnitude_error"]       = result.magnitude.error;

    out["slope_numerator"]   = result.slope.numerator;
    out["slope_denominator"] = result.slope.denominator;
    out["slope_error"]       = result.slope.error;
}

void add_timing_fields (py::dict& out, const ScoreResult& result) {
    out["corridor_ms"]  = result.timings.corridor_ms;
    out["phase_ms"]     = result.timings.phase_ms;
    out["magnitude_ms"] = result.timings.magnitude_ms;
    out["slope_ms"]     = result.timings.slope_ms;
    out["total_ms"]     = result.timings.total_ms;
}

py::tuple score_components (py::array reference_curve, py::array comparison_curve, py::dict params) {
    std::vector<Diagnostic> diagnostics;
    std::vector<Diagnostic> input_diagnostics;
    ValidatedCurves         curves;
    ScoreResult             result;
    try {
        curves                   = validate_curves(reference_curve, comparison_curve, input_diagnostics);
        ScoreParams score_params = score_params_from_dict(params);
        validation::validate_score_params(score_params);
        emit_component_warnings(input_diagnostics);
        const std::span<const f64> reference_values(curves.reference_values.data(), curves.reference_values.size());
        const std::span<const f64> comparison_values(curves.comparison_values.data(), curves.comparison_values.size());

        {
            py::gil_scoped_release release;
            result = engine::dispatch_table().score_components(reference_values, comparison_values, score_params,
                                                               curves.dt, diagnostics);
        }
    } catch (...) {
        emit_component_warnings(diagnostics);
        throw;
    }

    emit_component_warnings(diagnostics);

    py::dict scores;
    add_score_fields(scores, result, curves);
    py::dict timings;
    add_timing_fields(timings, result);
    return py::make_tuple(scores, timings);
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
