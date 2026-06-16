#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace py = pybind11;

namespace {

constexpr std::uint8_t DIR_NONE = 0;
constexpr std::uint8_t DIR_VERTICAL = 1;
constexpr std::uint8_t DIR_HORIZONTAL = 2;
constexpr std::uint8_t DIR_DIAGONAL = 3;

struct DtwState {
    py::ssize_t n = 0;
    py::ssize_t radius = 0;
    py::ssize_t band_width = 0;
    std::vector<std::uint8_t> directions;
};

struct ArrayView {
    const char* data = nullptr;
    py::ssize_t stride = 0;
    py::ssize_t n = 0;

    double operator[](py::ssize_t index) const {
        return *reinterpret_cast<const double*>(data + index * stride);
    }
};

py::ssize_t window_radius(py::ssize_t n, double window_size) {
    if (n <= 0) {
        throw std::invalid_argument("DTW input arrays must not be empty");
    }
    if (!std::isfinite(window_size) || window_size < 0.0) {
        throw std::invalid_argument("DTW window_size must be a finite non-negative value");
    }

    const auto raw = static_cast<py::ssize_t>(std::ceil(window_size * static_cast<double>(n)));
    return std::max<py::ssize_t>(1, raw);
}

bool valid_cell(py::ssize_t i, py::ssize_t j, py::ssize_t n, py::ssize_t radius) {
    return i >= 0 && i < n && j >= 0 && j < n && std::llabs(i - j) < radius;
}

py::ssize_t direction_index(py::ssize_t i, py::ssize_t j, py::ssize_t radius, py::ssize_t band_width) {
    const py::ssize_t row_start = i - radius + 1;
    const py::ssize_t offset = j - row_start;
    return i * band_width + offset;
}

template <typename Func>
DtwState compute_directions(
    const ArrayView& x,
    const ArrayView& y,
    py::ssize_t n,
    double window_size,
    Func&& on_accumulated
) {
    DtwState state;
    state.n = n;
    state.radius = window_radius(n, window_size);
    state.band_width = 2 * state.radius - 1;
    state.directions.assign(static_cast<std::size_t>(n * state.band_width), DIR_NONE);

    const double inf = std::numeric_limits<double>::infinity();
    std::vector<double> previous(static_cast<std::size_t>(n), inf);
    std::vector<double> current(static_cast<std::size_t>(n), inf);

    for (py::ssize_t i = 0; i < n; ++i) {
        const py::ssize_t j_start = std::max<py::ssize_t>(0, i - state.radius + 1);
        const py::ssize_t j_stop = std::min<py::ssize_t>(n, i + state.radius);

        for (py::ssize_t j = j_start; j < j_stop; ++j) {
            const double delta = x[i] - y[j];
            const double local_cost = delta * delta;
            double accumulated = inf;
            std::uint8_t direction = DIR_NONE;

            if (i == 0 && j == 0) {
                accumulated = local_cost;
            } else {
                double best_previous = inf;

                if (i > 0 && valid_cell(i - 1, j, n, state.radius)) {
                    best_previous = previous[static_cast<std::size_t>(j)];
                    direction = DIR_VERTICAL;
                }
                if (j > 0 && j - 1 >= j_start) {
                    const double candidate = current[static_cast<std::size_t>(j - 1)];
                    if (candidate < best_previous) {
                        best_previous = candidate;
                        direction = DIR_HORIZONTAL;
                    }
                }
                if (i > 0 && j > 0 && valid_cell(i - 1, j - 1, n, state.radius)) {
                    const double candidate = previous[static_cast<std::size_t>(j - 1)];
                    if (candidate < best_previous) {
                        best_previous = candidate;
                        direction = DIR_DIAGONAL;
                    }
                }

                if (std::isfinite(best_previous)) {
                    accumulated = local_cost + best_previous;
                } else {
                    direction = DIR_NONE;
                }
            }

            current[static_cast<std::size_t>(j)] = accumulated;
            state.directions[static_cast<std::size_t>(direction_index(i, j, state.radius, state.band_width))] = direction;
            on_accumulated(i, j, accumulated);
        }

        std::swap(previous, current);
    }

    if (!std::isfinite(previous[static_cast<std::size_t>(n - 1)])) {
        throw std::runtime_error("No valid ISO DTW path found");
    }

    return state;
}

DtwState compute_directions(
    const ArrayView& x,
    const ArrayView& y,
    py::ssize_t n,
    double window_size
) {
    return compute_directions(x, y, n, window_size, [](py::ssize_t, py::ssize_t, double) {});
}

std::vector<std::pair<py::ssize_t, py::ssize_t>> backtrack_pairs(const DtwState& state) {
    std::vector<std::pair<py::ssize_t, py::ssize_t>> reversed;
    reversed.reserve(static_cast<std::size_t>(2 * state.n));

    py::ssize_t i = state.n - 1;
    py::ssize_t j = state.n - 1;

    while (true) {
        reversed.emplace_back(i, j);
        if (i == 0 && j == 0) {
            break;
        }

        if (!valid_cell(i, j, state.n, state.radius)) {
            throw std::runtime_error("Invalid ISO DTW path cell encountered");
        }

        const auto direction = state.directions[static_cast<std::size_t>(
            direction_index(i, j, state.radius, state.band_width)
        )];

        if (direction == DIR_VERTICAL) {
            --i;
        } else if (direction == DIR_HORIZONTAL) {
            --j;
        } else if (direction == DIR_DIAGONAL) {
            --i;
            --j;
        } else {
            throw std::runtime_error("No valid ISO DTW predecessor found");
        }
    }

    std::reverse(reversed.begin(), reversed.end());
    return reversed;
}

ArrayView view_from_array(const py::array_t<double, py::array::forcecast>& array, const char* name) {
    const py::buffer_info info = array.request();
    if (info.ndim != 1) {
        throw std::invalid_argument(std::string(name) + " must be a 1D array");
    }
    if (info.shape[0] <= 0) {
        throw std::invalid_argument(std::string(name) + " must not be empty");
    }

    ArrayView view;
    view.data = static_cast<const char*>(info.ptr);
    view.stride = info.strides[0];
    view.n = info.shape[0];
    return view;
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
    if (x_view.n <= 0) {
        throw std::invalid_argument("ISO DTW expects 1D arrays");
    }
    return {x_view, y_view};
}

py::array_t<py::ssize_t> warp_path(
    py::array_t<double, py::array::forcecast> x,
    py::array_t<double, py::array::forcecast> y,
    double window_size
) {
    const auto views = validate_inputs(x, y);
    const auto n = views.first.n;

    DtwState state;
    std::vector<std::pair<py::ssize_t, py::ssize_t>> pairs;
    {
        py::gil_scoped_release release;
        state = compute_directions(views.first, views.second, n, window_size);
        pairs = backtrack_pairs(state);
    }

    py::array_t<py::ssize_t> out({static_cast<py::ssize_t>(pairs.size()), static_cast<py::ssize_t>(2)});
    auto mutable_out = out.mutable_unchecked<2>();
    for (py::ssize_t k = 0; k < static_cast<py::ssize_t>(pairs.size()); ++k) {
        mutable_out(k, 0) = pairs[static_cast<std::size_t>(k)].first;
        mutable_out(k, 1) = pairs[static_cast<std::size_t>(k)].second;
    }
    return out;
}

double magnitude_ratio(
    py::array_t<double, py::array::forcecast> x,
    py::array_t<double, py::array::forcecast> y,
    double window_size
) {
    const auto views = validate_inputs(x, y);
    const auto n = views.first.n;

    double numerator = 0.0;
    double denominator = 0.0;
    {
        py::gil_scoped_release release;
        const auto state = compute_directions(views.first, views.second, n, window_size);

        py::ssize_t i = n - 1;
        py::ssize_t j = n - 1;

        while (true) {
            numerator += std::abs(views.first[i] - views.second[j]);
            denominator += std::abs(views.second[j]);

            if (i == 0 && j == 0) {
                break;
            }

            const auto direction = state.directions[static_cast<std::size_t>(
                direction_index(i, j, state.radius, state.band_width)
            )];

            if (direction == DIR_VERTICAL) {
                --i;
            } else if (direction == DIR_HORIZONTAL) {
                --j;
            } else if (direction == DIR_DIAGONAL) {
                --i;
                --j;
            } else {
                throw std::runtime_error("No valid ISO DTW predecessor found");
            }
        }
    }

    return numerator / denominator;
}

py::dict backend_info() {
    py::dict info;
    info["name"] = "local_iso_native";
    info["language"] = "C++17";
    info["window"] = "abs(i-j) < max(1, ceil(window_size*n))";
    info["tie_order"] = "vertical,horizontal,diagonal";
    info["cost"] = "squared";
    return info;
}

}  // namespace

PYBIND11_MODULE(_core, m) {
    m.doc() = "Clean-room native ISO/TS 18571 DTW backend";
    m.def("backend_info", &backend_info);
    m.def("warp_path", &warp_path, py::arg("x"), py::arg("y"), py::arg("window_size"));
    m.def("magnitude_ratio", &magnitude_ratio, py::arg("x"), py::arg("y"), py::arg("window_size"));
}
