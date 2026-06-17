#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>

#include "scorer.hpp"
#include "validation.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>

namespace py = pybind11;

namespace {

using iso18571::CurveView;
using iso18571::Index;
using iso18571::ScoreParams;
using iso18571::ScoreResult;

struct ValidatedCurves {
    CurveView reference;
    CurveView comparison;
    double dt = 0.0;
};

py::handle require_param(const py::dict& params, const char* name) {
    const py::str key(name);
    if (!params.contains(key)) {
        throw std::invalid_argument(std::string("Missing required score parameter: ") + name);
    }
    return params[key];
}

double get_required_double_param(const py::dict& params, const char* name) {
    try {
        return py::cast<double>(require_param(params, name));
    } catch (const py::cast_error&) {
        throw std::invalid_argument(std::string(name) + " must be numeric");
    }
}

int get_required_positive_integer_param(const py::dict& params, const char* name) {
    const py::handle value = require_param(params, name);
    if (PyNumber_Check(value.ptr()) == 0) {
        return iso18571::positive_integer_from_double(
            std::numeric_limits<double>::quiet_NaN(),
            name
        );
    }

    py::object number = py::reinterpret_steal<py::object>(PyNumber_Float(value.ptr()));
    if (!number) {
        PyErr_Clear();
        return iso18571::positive_integer_from_double(
            std::numeric_limits<double>::quiet_NaN(),
            name
        );
    }

    return iso18571::positive_integer_from_double(py::cast<double>(number), name);
}

int get_required_score_exponent(const py::dict& params, const char* name) {
    const py::handle value = require_param(params, name);
    if (PyNumber_Check(value.ptr()) == 0) {
        return iso18571::score_exponent_from_double(
            std::numeric_limits<double>::quiet_NaN(),
            name
        );
    }

    py::object number = py::reinterpret_steal<py::object>(PyNumber_Float(value.ptr()));
    if (!number) {
        PyErr_Clear();
        return iso18571::score_exponent_from_double(
            std::numeric_limits<double>::quiet_NaN(),
            name
        );
    }

    return iso18571::score_exponent_from_double(py::cast<double>(number), name);
}

ScoreParams score_params_from_dict(const py::dict& params) {
    ScoreParams out;
    out.k_z = get_required_positive_integer_param(params, "k_z");
    out.k_p = get_required_score_exponent(params, "k_p");
    out.k_m = get_required_score_exponent(params, "k_m");
    out.eps_m = get_required_double_param(params, "eps_m");
    out.e_s = get_required_double_param(params, "e_s");
    out.init_min = get_required_double_param(params, "init_min");
    out.a_0 = get_required_double_param(params, "a_0");
    out.b_0 = get_required_double_param(params, "b_0");
    out.w_z = get_required_double_param(params, "w_z");
    out.w_p = get_required_double_param(params, "w_p");
    out.w_m = get_required_double_param(params, "w_m");
    out.w_s = get_required_double_param(params, "w_s");
    return out;
}

CurveView curve_view_from_array(const py::array_t<double, py::array::forcecast>& array, const char* name) {
    const py::buffer_info info = array.request();
    if (info.ndim != 2) {
        throw std::invalid_argument(std::string(name) + " must be a 2D array");
    }
    if (info.shape[0] <= 0) {
        throw std::invalid_argument(std::string(name) + " must not be empty");
    }
    if (info.shape[1] != 2) {
        throw std::invalid_argument(std::string(name) + " must have shape (n, 2)");
    }

    return {
        static_cast<const char*>(info.ptr),
        static_cast<Index>(info.strides[0]),
        static_cast<Index>(info.strides[1]),
        static_cast<Index>(info.shape[0]),
    };
}

double time_grid_tolerance(double dt) {
    return std::max(1.0e-12, std::abs(dt) * 1.0e-9);
}

double derive_uniform_dt(const CurveView& curve, const char* name) {
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

    const double tolerance = time_grid_tolerance(dt);
    previous_time = second_time;
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

double validate_time_grids(const CurveView& reference, const CurveView& comparison) {
    const double reference_dt = derive_uniform_dt(reference, "reference_curve");
    const double comparison_dt = derive_uniform_dt(comparison, "comparison_curve");
    const double tolerance = std::max(
        time_grid_tolerance(reference_dt),
        time_grid_tolerance(comparison_dt)
    );
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

ValidatedCurves validate_curves(
    const py::array_t<double, py::array::forcecast>& reference_curve,
    const py::array_t<double, py::array::forcecast>& comparison_curve
) {
    const CurveView reference = curve_view_from_array(reference_curve, "reference_curve");
    const CurveView comparison = curve_view_from_array(comparison_curve, "comparison_curve");
    if (reference.n != comparison.n) {
        throw std::invalid_argument("Curves are not equal in size/dimension");
    }
    return {reference, comparison, validate_time_grids(reference, comparison)};
}

void add_score_fields(py::dict& out, const ScoreResult& score) {
    out["Z"] = score.z;
    out["EP"] = score.ep;
    out["EM"] = score.em;
    out["ES"] = score.es;
    out["R"] = score.r;
    out["n_eps"] = score.shift.n_eps;
    out["rho_e"] = score.shift.rho_e;
    out["reference_start"] = score.shift.reference_start;
    out["comparison_start"] = score.shift.comparison_start;
    out["shift_length"] = score.shift.length;
}

py::dict score_components(
    py::array_t<double, py::array::forcecast> reference_curve,
    py::array_t<double, py::array::forcecast> comparison_curve,
    py::dict params
) {
    const ValidatedCurves curves = validate_curves(reference_curve, comparison_curve);
    ScoreParams score_params = score_params_from_dict(params);
    score_params.dt = curves.dt;
    iso18571::validate_score_params(score_params);

    ScoreResult score;
    {
        py::gil_scoped_release release;
        score = iso18571::dispatch_table().score_components(
            curves.reference,
            curves.comparison,
            score_params
        );
    }

    py::dict out;
    add_score_fields(out, score);
    return out;
}

py::dict backend_info() {
    py::dict info;
    info["implementation"] = "C++17";
    info["optimization"] = iso18571::dispatch_table().level;
    return info;
}

}  // namespace

PYBIND11_MODULE(_core, m) {
    m.doc() = "Clean-room native ISO/TS 18571 scorer";
    m.def("backend_info", &backend_info);
    m.def(
        "_score_components",
        &score_components,
        py::arg("reference_curve"),
        py::arg("comparison_curve"),
        py::arg("params")
    );
}
