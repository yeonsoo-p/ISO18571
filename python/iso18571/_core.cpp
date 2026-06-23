#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>

#include "engine.h"
#include "float16.h"
#include "validation.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>
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
    LongDouble,
    ComplexFloat32,
    ComplexFloat64,
    ComplexLongDouble,
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
        std::forward<T>(visitor)(TypeTag<std::int8_t> {});
        return;
    case CurveInputDtype::Int16:
        std::forward<T>(visitor)(TypeTag<std::int16_t> {});
        return;
    case CurveInputDtype::Int32:
        std::forward<T>(visitor)(TypeTag<std::int32_t> {});
        return;
    case CurveInputDtype::Int64:
        std::forward<T>(visitor)(TypeTag<std::int64_t> {});
        return;
    case CurveInputDtype::UInt8:
        std::forward<T>(visitor)(TypeTag<std::uint8_t> {});
        return;
    case CurveInputDtype::UInt16:
        std::forward<T>(visitor)(TypeTag<std::uint16_t> {});
        return;
    case CurveInputDtype::UInt32:
        std::forward<T>(visitor)(TypeTag<std::uint32_t> {});
        return;
    case CurveInputDtype::UInt64:
        std::forward<T>(visitor)(TypeTag<std::uint64_t> {});
        return;
    case CurveInputDtype::Float16:
        std::forward<T>(visitor)(TypeTag<Float16> {});
        return;
    case CurveInputDtype::Float32:
        std::forward<T>(visitor)(TypeTag<float> {});
        return;
    case CurveInputDtype::Float64:
        std::forward<T>(visitor)(TypeTag<double> {});
        return;
    case CurveInputDtype::LongDouble:
        std::forward<T>(visitor)(TypeTag<long double> {});
        return;
    case CurveInputDtype::ComplexFloat32:
        std::forward<T>(visitor)(TypeTag<std::complex<float>> {});
        return;
    case CurveInputDtype::ComplexFloat64:
        std::forward<T>(visitor)(TypeTag<std::complex<double>> {});
        return;
    case CurveInputDtype::ComplexLongDouble:
        std::forward<T>(visitor)(TypeTag<std::complex<long double>> {});
        return;
    case CurveInputDtype::Other:
        return;
    }
    throw std::runtime_error("Unknown curve input dtype");
}

template<typename T>
bool require_safe_element_layout (const py::buffer_info& info) {
    const auto item_size = static_cast<py::ssize_t>(sizeof(T));
    const auto pointer   = reinterpret_cast<std::uintptr_t>(info.ptr);
    const auto alignment = static_cast<std::uintptr_t>(alignof(T));
    if (alignment > 1U && pointer % alignment != 0U) {
        return false;
    }

    const auto stride_is_safe = [item_size] (py::ssize_t byte_stride) {
        if (byte_stride % item_size != 0) {
            return false;
        }

        const py::ssize_t element_stride = byte_stride / item_size;
        if (element_stride * item_size != byte_stride) {
            return false;
        }

        const auto index_stride = static_cast<Index>(element_stride);
        return static_cast<py::ssize_t>(index_stride) == element_stride;
    };

    return stride_is_safe(info.strides[0]) && stride_is_safe(info.strides[1]);
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
constexpr auto typed_time_tolerance () {
    if constexpr (std::is_same_v<T, Float16>) {
        return T {1.0e-3};
    }
    if constexpr (std::is_same_v<T, float>) {
        return 1.0e-7F;
    }
    if constexpr (!std::is_same_v<T, Float16> && !std::is_same_v<T, float>) {
        return T {1.0e-12};
    }
}

template<typename T>
void require_typed_time (const py::buffer_info& info, const char* curve_name, Index n) {
    using Time            = decltype(real_component(std::declval<T>()));
    const auto item_size  = static_cast<py::ssize_t>(sizeof(T));
    const auto row_stride = static_cast<Index>(info.strides[0] / item_size);
    const T*   data       = static_cast<const T*>(info.ptr);

    for (Index idx = 0; idx < n; ++idx) {
        const T value = data[idx * row_stride];
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

    const Time first = real_component(data[0]);
    const Time last  = real_component(data[(n - 1) * row_stride]);
    const Time dt    = (last - first) / Time(n - 1);

    Time current = first;
    for (Index idx = 1; idx < n; ++idx) {
        const Time next = real_component(data[idx * row_stride]);

        if (next <= current) {
            throw std::invalid_argument(std::string(curve_name) + " time values must be strictly increasing");
        }

        const Time step = next - current;
        if constexpr (std::is_integral_v<Time>) {
            if (step != dt) {
                throw std::invalid_argument(std::string(curve_name) + " time values must have a constant interval");
            }
        }
        if constexpr (std::is_floating_point_v<Time>) {
            const Time tolerance  = typed_time_tolerance<Time>();
            const Time difference = std::abs(step - dt);
            if (!std::isfinite(difference) || difference > tolerance) {
                throw std::invalid_argument(std::string(curve_name) + " time values must have a constant interval");
            }
        }

        current = next;
    }
}

template<typename T>
void require_typed_value (const py::buffer_info& info, const char* curve_name, Index n) {
    const auto item_size     = static_cast<py::ssize_t>(sizeof(T));
    const auto row_stride    = static_cast<Index>(info.strides[0] / item_size);
    const auto column_stride = static_cast<Index>(info.strides[1] / item_size);
    const T*   data          = static_cast<const T*>(info.ptr);

    for (Index idx = 0; idx < n; ++idx) {
        const T value = data[idx * row_stride + column_stride];
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
        if constexpr (std::is_same_v<decltype(real), long double>) {
            constexpr long double max_double = static_cast<long double>(std::numeric_limits<double>::max());
            if (real < -max_double || real > max_double) {
                throw std::invalid_argument(std::string(curve_name) + " must be representable as finite float64");
            }
        }
    }
}

CurveInputDtype curve_input_dtype (const py::dtype& dtype, const py::buffer_info& info) {
    switch (dtype.kind()) {
    case 'i':
        if (dtype.equal(py::dtype::of<std::int8_t>())) {
            return CurveInputDtype::Int8;
        }
        if (dtype.equal(py::dtype::of<std::int16_t>())) {
            return CurveInputDtype::Int16;
        }
        if (dtype.equal(py::dtype::of<std::int32_t>())) {
            return CurveInputDtype::Int32;
        }
        if (dtype.equal(py::dtype::of<std::int64_t>())) {
            return CurveInputDtype::Int64;
        }
        return CurveInputDtype::Other;
    case 'u':
        if (dtype.equal(py::dtype::of<std::uint8_t>())) {
            return CurveInputDtype::UInt8;
        }
        if (dtype.equal(py::dtype::of<std::uint16_t>())) {
            return CurveInputDtype::UInt16;
        }
        if (dtype.equal(py::dtype::of<std::uint32_t>())) {
            return CurveInputDtype::UInt32;
        }
        if (dtype.equal(py::dtype::of<std::uint64_t>())) {
            return CurveInputDtype::UInt64;
        }
        return CurveInputDtype::Other;
    case 'f':
        if (info.itemsize == static_cast<py::ssize_t>(sizeof(std::uint16_t))) {
            return CurveInputDtype::Float16;
        }
        if (dtype.equal(py::dtype::of<float>())) {
            return CurveInputDtype::Float32;
        }
        if (dtype.equal(py::dtype::of<double>())) {
            return CurveInputDtype::Float64;
        }
        if (dtype.equal(py::dtype::of<long double>())) {
            return CurveInputDtype::LongDouble;
        }
        return CurveInputDtype::Other;
    case 'c':
        if (dtype.equal(py::dtype::of<std::complex<float>>())) {
            return CurveInputDtype::ComplexFloat32;
        }
        if (dtype.equal(py::dtype::of<std::complex<double>>())) {
            return CurveInputDtype::ComplexFloat64;
        }
        if (dtype.equal(py::dtype::of<std::complex<long double>>())) {
            return CurveInputDtype::ComplexLongDouble;
        }
        return CurveInputDtype::Other;
    default:
        return CurveInputDtype::Other;
    }
}

bool require_curve_layout (CurveInputDtype dtype, const py::buffer_info& info) {
    bool layout_is_safe = false;
    visit_curve_dtype(dtype, [&] (auto tag) {
        using T        = typename decltype(tag)::type;
        layout_is_safe = require_safe_element_layout<T>(info);
    });
    return layout_is_safe;
}

void require_curve_input (CurveInputDtype dtype, const py::buffer_info& info, const char* curve_name, Index n) {
    visit_curve_dtype(dtype, [&] (auto tag) {
        using T = typename decltype(tag)::type;
        require_typed_time<T>(info, curve_name, n);
        require_typed_value<T>(info, curve_name, n);
    });
}

template<typename T>
double materialize_dt (const py::buffer_info& info, const char* curve_name, Index n) {
    const auto item_size  = static_cast<py::ssize_t>(sizeof(T));
    const auto row_stride = static_cast<Index>(info.strides[0] / item_size);
    const T*   data       = static_cast<const T*>(info.ptr);

    const auto   first = real_component(data[0]);
    const auto   last  = real_component(data[(n - 1) * row_stride]);
    const double dt    = static_cast<double>((last - first) / decltype(first)(n - 1));
    if (!std::isfinite(dt) || dt <= 0.0 || dt < std::numeric_limits<double>::min()) {
        throw std::invalid_argument(std::string(curve_name) + " time interval is too small");
    }
    return dt;
}

double materialize_matching_dt (CurveInputDtype reference_dtype, const py::buffer_info& reference_info,
                                CurveInputDtype comparison_dtype, const py::buffer_info& comparison_info, Index n) {
    double reference_dt = 0.0;
    visit_curve_dtype(reference_dtype, [&] (auto reference_tag) {
        using ReferenceT    = typename decltype(reference_tag)::type;
        using ReferenceTime = decltype(real_component(std::declval<ReferenceT>()));
        reference_dt        = materialize_dt<ReferenceT>(reference_info, "reference_curve", n);
        visit_curve_dtype(comparison_dtype, [&] (auto comparison_tag) {
            using ComparisonT          = typename decltype(comparison_tag)::type;
            using ComparisonTime       = decltype(real_component(std::declval<ComparisonT>()));
            const double comparison_dt = materialize_dt<ComparisonT>(comparison_info, "comparison_curve", n);
            if constexpr (std::is_integral_v<ReferenceTime> || std::is_integral_v<ComparisonTime>) {
                if (reference_dt != comparison_dt) {
                    throw std::invalid_argument("Curves must have matching time intervals");
                }
            } else {
                double reference_tolerance  = static_cast<double>(typed_time_tolerance<ReferenceTime>());
                double comparison_tolerance = static_cast<double>(typed_time_tolerance<ComparisonTime>());
                if (std::fabs(reference_dt - comparison_dt) > std::max(reference_tolerance, comparison_tolerance)) {
                    throw std::invalid_argument("Curves must have matching time intervals");
                }
            }
        });
    });

    return reference_dt;
}

template<typename T>
void materialize_typed_curve_values (const py::buffer_info& info, Index n, std::vector<double>& values) {
    const auto item_size     = static_cast<py::ssize_t>(sizeof(T));
    const auto row_stride    = static_cast<Index>(info.strides[0] / item_size);
    const auto column_stride = static_cast<Index>(info.strides[1] / item_size);
    const T*   data          = static_cast<const T*>(info.ptr);

    for (Index idx = 0; idx < n; ++idx) {
        values[static_cast<std::size_t>(idx)] =
            static_cast<double>(real_component(data[idx * row_stride + column_stride]));
    }
}

std::vector<double> materialize_curve_values (CurveInputDtype dtype, const py::buffer_info& info, Index n) {
    std::vector<double> values(static_cast<std::size_t>(n));
    visit_curve_dtype(dtype, [&] (auto tag) {
        using T = typename decltype(tag)::type;
        materialize_typed_curve_values<T>(info, n, values);
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
    return info;
}

CurveInputDtype require_curve_dtype (const py::array& curve, const py::buffer_info& info, const char* curve_name) {
    const py::dtype   dtype     = curve.dtype();
    const std::string byteorder = py::cast<std::string>(dtype.attr("byteorder"));
    if (byteorder != "=" && byteorder != "|") {
        throw std::invalid_argument(std::string(curve_name) + " must use native byte order");
    }

    const CurveInputDtype input_dtype = curve_input_dtype(dtype, info);
    if (input_dtype == CurveInputDtype::Other) {
        throw std::invalid_argument(std::string(curve_name) + " has unsupported dtype");
    }
    return input_dtype;
}

ValidatedCurves validate_curves (py::array reference_curve, py::array comparison_curve) {
    py::buffer_info reference_info  = require_curve_shape(reference_curve, "reference_curve");
    py::buffer_info comparison_info = require_curve_shape(comparison_curve, "comparison_curve");

    const Index n = static_cast<Index>(reference_info.shape[0]);
    if (n != static_cast<Index>(comparison_info.shape[0])) {
        throw std::invalid_argument("Curves are not equal in size/dimension");
    }
    if (n < 2) {
        throw std::invalid_argument("reference_curve must have at least 2 samples");
    }

    CurveInputDtype reference_dtype  = require_curve_dtype(reference_curve, reference_info, "reference_curve");
    CurveInputDtype comparison_dtype = require_curve_dtype(comparison_curve, comparison_info, "comparison_curve");

    py::array reference_effective  = reference_curve;
    py::array comparison_effective = comparison_curve;

    if (!require_curve_layout(reference_dtype, reference_info)) {
        const py::object numpy = py::module_::import("numpy");
        reference_effective =
            numpy.attr("require")(reference_curve, py::none(), py::make_tuple("C", "A")).cast<py::array>();
        reference_info = reference_effective.request();
    }
    if (!require_curve_layout(comparison_dtype, comparison_info)) {
        const py::object numpy = py::module_::import("numpy");
        comparison_effective =
            numpy.attr("require")(comparison_curve, py::none(), py::make_tuple("C", "A")).cast<py::array>();
        comparison_info = comparison_effective.request();
    }

    require_curve_input(reference_dtype, reference_info, "reference_curve", n);
    require_curve_input(comparison_dtype, comparison_info, "comparison_curve", n);

    const double reference_dt =
        materialize_matching_dt(reference_dtype, reference_info, comparison_dtype, comparison_info, n);

    std::vector<double> reference_values  = materialize_curve_values(reference_dtype, reference_info, n);
    std::vector<double> comparison_values = materialize_curve_values(comparison_dtype, comparison_info, n);

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
