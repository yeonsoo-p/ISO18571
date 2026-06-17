#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>

#include "scorer.hpp"
#include "validation.hpp"

#include <limits>
#include <stdexcept>
#include <string>

namespace py = pybind11;

namespace {

using iso18571::CurveView;
using iso18571::Index;
using iso18571::ScoreParams;
using iso18571::ScoreResult;

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
    out.k_z = get_required_score_exponent(params, "k_z");
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
    out.dt = get_required_double_param(params, "dt");

    iso18571::validate_score_params(out);
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

std::pair<CurveView, CurveView> validate_curves(
    const py::array_t<double, py::array::forcecast>& reference_curve,
    const py::array_t<double, py::array::forcecast>& comparison_curve
) {
    const CurveView reference = curve_view_from_array(reference_curve, "reference_curve");
    const CurveView comparison = curve_view_from_array(comparison_curve, "comparison_curve");
    if (reference.n != comparison.n) {
        throw std::invalid_argument("Curves are not equal in size/dimension");
    }
    return {reference, comparison};
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
    const auto views = validate_curves(reference_curve, comparison_curve);
    const ScoreParams score_params = score_params_from_dict(params);

    ScoreResult score;
    {
        py::gil_scoped_release release;
        score = iso18571::dispatch_table().score_components(views.first, views.second, score_params);
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
