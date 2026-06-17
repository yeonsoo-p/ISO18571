#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>

#include "scorer.hpp"

#include <cmath>
#include <stdexcept>
#include <string>

namespace py = pybind11;

namespace {

using iso18571::ArrayView;
using iso18571::CurveView;
using iso18571::Index;
using iso18571::ScoreParams;
using iso18571::ScoreResult;

template <typename T>
T get_param(const py::dict& params, const char* name, T default_value) {
    if (params.contains(name)) {
        return py::cast<T>(params[py::str(name)]);
    }
    return default_value;
}

void require_finite(double value, const char* name) {
    if (!std::isfinite(value)) {
        throw std::invalid_argument(std::string(name) + " must be finite");
    }
}

void require_positive(double value, const char* name) {
    require_finite(value, name);
    if (value <= 0.0) {
        throw std::invalid_argument(std::string(name) + " must be positive");
    }
}

void require_non_negative(double value, const char* name) {
    require_finite(value, name);
    if (value < 0.0) {
        throw std::invalid_argument(std::string(name) + " must be non-negative");
    }
}

ScoreParams score_params_from_dict(const py::dict& params) {
    ScoreParams out;
    out.k_z = get_param<int>(params, "k_z", out.k_z);
    out.k_p = get_param<int>(params, "k_p", out.k_p);
    out.k_m = get_param<int>(params, "k_m", out.k_m);
    out.eps_m = get_param<double>(params, "eps_m", out.eps_m);
    out.e_s = get_param<double>(params, "e_s", out.e_s);
    out.init_min = get_param<double>(params, "init_min", out.init_min);
    out.a_0 = get_param<double>(params, "a_0", out.a_0);
    out.b_0 = get_param<double>(params, "b_0", out.b_0);
    out.w_z = get_param<double>(params, "w_z", out.w_z);
    out.w_p = get_param<double>(params, "w_p", out.w_p);
    out.w_m = get_param<double>(params, "w_m", out.w_m);
    out.w_s = get_param<double>(params, "w_s", out.w_s);
    out.dt = get_param<double>(params, "dt", out.dt);

    if (out.k_z < 1 || out.k_z > 3) {
        throw std::invalid_argument("k_z has to be 1, 2, or 3");
    }
    if (out.k_p < 1 || out.k_p > 3) {
        throw std::invalid_argument("k_p has to be 1, 2, or 3");
    }
    if (out.k_m < 1 || out.k_m > 3) {
        throw std::invalid_argument("k_m has to be 1, 2, or 3");
    }
    require_positive(out.eps_m, "eps_m");
    require_positive(out.e_s, "e_s");
    require_finite(out.init_min, "init_min");
    if (out.init_min < 0.0 || out.init_min >= 1.0) {
        throw std::invalid_argument("init_min must be finite and satisfy 0 <= init_min < 1");
    }
    require_non_negative(out.a_0, "a_0");
    require_non_negative(out.b_0, "b_0");
    if (out.b_0 <= out.a_0) {
        throw std::invalid_argument("b_0 must be greater than a_0");
    }
    require_non_negative(out.w_z, "w_z");
    require_non_negative(out.w_p, "w_p");
    require_non_negative(out.w_m, "w_m");
    require_non_negative(out.w_s, "w_s");
    require_positive(out.dt, "dt");
    const double weights_sum = out.w_z + out.w_m + out.w_p + out.w_s;
    if (weights_sum != 1.0) {
        throw std::invalid_argument("Sum of weighting factors (w_z, w_m, w_p, w_s) must be 1");
    }
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

ArrayView view_from_array(const py::array_t<double, py::array::forcecast>& array, const char* name) {
    const py::buffer_info info = array.request();
    if (info.ndim != 1) {
        throw std::invalid_argument(std::string(name) + " must be a 1D array");
    }
    if (info.shape[0] <= 0) {
        throw std::invalid_argument(std::string(name) + " must not be empty");
    }
    return {
        static_cast<const char*>(info.ptr),
        static_cast<Index>(info.strides[0]),
        static_cast<Index>(info.shape[0]),
    };
}

std::pair<ArrayView, ArrayView> validate_inputs(
    const py::array_t<double, py::array::forcecast>& x,
    const py::array_t<double, py::array::forcecast>& y
) {
    const ArrayView x_view = view_from_array(x, "x");
    const ArrayView y_view = view_from_array(y, "y");
    if (x_view.n != y_view.n) {
        throw std::invalid_argument("ISO DTW expects equal-length arrays");
    }
    return {x_view, y_view};
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

py::array_t<Index> warp_path(
    py::array_t<double, py::array::forcecast> x,
    py::array_t<double, py::array::forcecast> y,
    double window_size
) {
    const auto views = validate_inputs(x, y);
    iso18571::WarpPath pairs;
    {
        py::gil_scoped_release release;
        pairs = iso18571::dispatch_table().warp_path(views.first, views.second, window_size);
    }

    py::array_t<Index> out({static_cast<Index>(pairs.size()), static_cast<Index>(2)});
    auto mutable_out = out.mutable_unchecked<2>();
    for (Index idx = 0; idx < static_cast<Index>(pairs.size()); ++idx) {
        mutable_out(idx, 0) = pairs[static_cast<std::size_t>(idx)].first;
        mutable_out(idx, 1) = pairs[static_cast<std::size_t>(idx)].second;
    }
    return out;
}

double magnitude_ratio(
    py::array_t<double, py::array::forcecast> x,
    py::array_t<double, py::array::forcecast> y,
    double window_size
) {
    const auto views = validate_inputs(x, y);
    double ratio = 0.0;
    {
        py::gil_scoped_release release;
        ratio = iso18571::dispatch_table().magnitude_ratio(views.first, views.second, window_size);
    }
    return ratio;
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
    info["name"] = "iso18571";
    info["language"] = "C++17";
    info["window"] = "abs(i-j) < min(n, max(1, ceil(window_size*n)))";
    info["tie_order"] = "vertical,horizontal,diagonal";
    info["cost"] = "squared";
    info["dtw_layout"] = "index_incremental";
    info["reduction_mode"] = "all";
    info["parallelism"] = "none";
    info["x86_64_dispatch"] = "internal_best_effort";
    info["compiled_x86_64_levels"] = iso18571::compiled_x86_64_levels();
    info["selected_x86_64_level"] = iso18571::dispatch_table().level;
    return info;
}

}  // namespace

PYBIND11_MODULE(_core, m) {
    m.doc() = "Clean-room native ISO/TS 18571 scorer";
    m.def("backend_info", &backend_info);
    m.def("warp_path", &warp_path, py::arg("x"), py::arg("y"), py::arg("window_size"));
    m.def("magnitude_ratio", &magnitude_ratio, py::arg("x"), py::arg("y"), py::arg("window_size"));
    m.def(
        "score_components",
        &score_components,
        py::arg("reference_curve"),
        py::arg("comparison_curve"),
        py::arg("params") = py::dict()
    );
}
