#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>

#include "simd.hpp"

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cmath>
#include <cstdint>
#include <exception>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace py = pybind11;

namespace {

using iso18571_native::SimdCapabilities;
using iso18571_native::SimdLevel;
using iso18571_native::SimdSelection;

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

struct BitDtwState {
    py::ssize_t n = 0;
    py::ssize_t radius = 0;
    py::ssize_t band_width = 0;
    std::vector<std::uint8_t> directions;
};

class ReusableBarrier {
public:
    explicit ReusableBarrier(std::size_t parties) : parties_(parties), waiting_(0), generation_(0) {}

    void wait() {
        std::unique_lock<std::mutex> lock(mutex_);
        const auto generation = generation_;
        ++waiting_;
        if (waiting_ == parties_) {
            waiting_ = 0;
            ++generation_;
            condition_.notify_all();
            return;
        }
        condition_.wait(lock, [&] { return generation != generation_; });
    }

private:
    std::mutex mutex_;
    std::condition_variable condition_;
    std::size_t parties_;
    std::size_t waiting_;
    std::size_t generation_;
};

struct ArrayView {
    const char* data = nullptr;
    py::ssize_t stride = 0;
    py::ssize_t n = 0;

    double operator[](py::ssize_t index) const {
        return *reinterpret_cast<const double*>(data + index * stride);
    }
};

std::vector<double> copy_values(const ArrayView& values);
ArrayView view_from_vector(const std::vector<double>& values);

struct CurveView {
    const char* data = nullptr;
    py::ssize_t row_stride = 0;
    py::ssize_t column_stride = 0;
    py::ssize_t n = 0;

    double value(py::ssize_t index) const {
        return *reinterpret_cast<const double*>(data + index * row_stride + column_stride);
    }
};

struct ScoreParams {
    int k_z = 2;
    int k_p = 1;
    int k_m = 1;
    double eps_m = 0.50;
    double e_s = 2.0;
    double init_min = 0.8;
    double a_0 = 0.05;
    double b_0 = 0.5;
    double w_z = 0.4;
    double w_p = 0.2;
    double w_m = 0.2;
    double w_s = 0.2;
    double dt = 0.0001;
};

struct ShiftResult {
    py::ssize_t reference_start = 0;
    py::ssize_t comparison_start = 0;
    py::ssize_t length = 0;
    py::ssize_t n_eps = 0;
    double rho_e = 0.0;
    double max_shift = 0.2;
};

struct ScoreResult {
    double z = 0.0;
    double ep = 0.0;
    double em = 0.0;
    double es = 0.0;
    double r = 0.0;
    ShiftResult shift;
};

struct PhaseCache {
    std::vector<double> reference_sum;
    std::vector<double> comparison_sum;
    std::vector<double> reference_square_sum;
    std::vector<double> comparison_square_sum;
};

enum class DtwLayout {
    Current,
    RangePrecompute,
    IndexIncremental,
    CompactDirection,
};

enum class ReductionMode {
    None,
    PhaseDualProduct,
    FusedSlope,
    SharedShiftWorkspace,
    All,
};

enum class ParallelMode {
    None,
    Diagonal,
    Blocked,
};

struct VariantConfig {
    DtwLayout dtw_layout = DtwLayout::Current;
    ReductionMode reduction_mode = ReductionMode::None;
    ParallelMode parallel_mode = ParallelMode::None;
    bool phase_dual_product = false;
    bool fused_slope = false;
    bool shared_shift_workspace = false;
    py::ssize_t block_size = 0;
    SimdLevel requested_simd = SimdLevel::Scalar;
    SimdSelection simd_selection;
};

struct RowRanges {
    std::vector<py::ssize_t> starts;
    std::vector<py::ssize_t> stops;
    std::vector<py::ssize_t> direction_bases;
};

struct BlockBoundary {
    std::vector<double> bottom;
    std::vector<double> right;
    double bottom_right = std::numeric_limits<double>::infinity();
    bool active = false;
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

py::ssize_t ceil_div2(py::ssize_t value) {
    if (value >= 0) {
        return (value + 1) / 2;
    }
    return value / 2;
}

py::ssize_t bit_direction_size(py::ssize_t n, py::ssize_t band_width) {
    return (n * band_width * 2 + 7) / 8;
}

RowRanges build_row_ranges(py::ssize_t n, py::ssize_t radius, py::ssize_t band_width) {
    RowRanges ranges;
    ranges.starts.resize(static_cast<std::size_t>(n));
    ranges.stops.resize(static_cast<std::size_t>(n));
    ranges.direction_bases.resize(static_cast<std::size_t>(n));
    for (py::ssize_t i = 0; i < n; ++i) {
        ranges.starts[static_cast<std::size_t>(i)] = std::max<py::ssize_t>(0, i - radius + 1);
        ranges.stops[static_cast<std::size_t>(i)] = std::min<py::ssize_t>(n, i + radius);
        ranges.direction_bases[static_cast<std::size_t>(i)] = i * band_width;
    }
    return ranges;
}

void set_bit_direction(BitDtwState& state, py::ssize_t i, py::ssize_t j, std::uint8_t direction) {
    const py::ssize_t cell_index = direction_index(i, j, state.radius, state.band_width);
    const py::ssize_t bit_index = cell_index * 2;
    const auto byte_index = static_cast<std::size_t>(bit_index / 8);
    const auto shift = static_cast<unsigned>(bit_index % 8);
    const std::uint8_t mask = static_cast<std::uint8_t>(0x03u << shift);
    state.directions[byte_index] = static_cast<std::uint8_t>(
        (state.directions[byte_index] & ~mask) | ((direction & 0x03u) << shift)
    );
}

std::uint8_t get_bit_direction(const BitDtwState& state, py::ssize_t i, py::ssize_t j) {
    const py::ssize_t cell_index = direction_index(i, j, state.radius, state.band_width);
    const py::ssize_t bit_index = cell_index * 2;
    const auto byte_index = static_cast<std::size_t>(bit_index / 8);
    const auto shift = static_cast<unsigned>(bit_index % 8);
    return static_cast<std::uint8_t>((state.directions[byte_index] >> shift) & 0x03u);
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

template <bool PrecomputeRanges>
DtwState compute_directions_incremental(
    const ArrayView& x,
    const ArrayView& y,
    py::ssize_t n,
    double window_size
) {
    DtwState state;
    state.n = n;
    state.radius = window_radius(n, window_size);
    state.band_width = 2 * state.radius - 1;
    state.directions.assign(static_cast<std::size_t>(n * state.band_width), DIR_NONE);

    RowRanges ranges;
    if constexpr (PrecomputeRanges) {
        ranges = build_row_ranges(n, state.radius, state.band_width);
    }

    const double inf = std::numeric_limits<double>::infinity();
    std::vector<double> previous(static_cast<std::size_t>(n), inf);
    std::vector<double> current(static_cast<std::size_t>(n), inf);

    for (py::ssize_t i = 0; i < n; ++i) {
        const py::ssize_t j_start = PrecomputeRanges
            ? ranges.starts[static_cast<std::size_t>(i)]
            : std::max<py::ssize_t>(0, i - state.radius + 1);
        const py::ssize_t j_stop = PrecomputeRanges
            ? ranges.stops[static_cast<std::size_t>(i)]
            : std::min<py::ssize_t>(n, i + state.radius);
        const py::ssize_t direction_offset = j_start - (i - state.radius + 1);
        const py::ssize_t direction_base = (
            PrecomputeRanges
                ? ranges.direction_bases[static_cast<std::size_t>(i)]
                : i * state.band_width
        ) + direction_offset;
        const py::ssize_t previous_start = i > 0
            ? (PrecomputeRanges
                ? ranges.starts[static_cast<std::size_t>(i - 1)]
                : std::max<py::ssize_t>(0, i - state.radius))
            : 0;
        const py::ssize_t previous_stop = i > 0
            ? (PrecomputeRanges
                ? ranges.stops[static_cast<std::size_t>(i - 1)]
                : std::min<py::ssize_t>(n, i + state.radius - 1))
            : 0;

        py::ssize_t direction_idx = direction_base;
        for (py::ssize_t j = j_start; j < j_stop; ++j, ++direction_idx) {
            const double delta = x[i] - y[j];
            const double local_cost = delta * delta;
            double accumulated = inf;
            std::uint8_t direction = DIR_NONE;

            if (i == 0 && j == 0) {
                accumulated = local_cost;
            } else {
                double best_previous = inf;

                if (i > 0 && j >= previous_start && j < previous_stop) {
                    best_previous = previous[static_cast<std::size_t>(j)];
                    direction = DIR_VERTICAL;
                }
                if (j > j_start) {
                    const double candidate = current[static_cast<std::size_t>(j - 1)];
                    if (candidate < best_previous) {
                        best_previous = candidate;
                        direction = DIR_HORIZONTAL;
                    }
                }
                if (i > 0 && j > 0 && j - 1 >= previous_start && j - 1 < previous_stop) {
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
            state.directions[static_cast<std::size_t>(direction_idx)] = direction;
        }

        std::swap(previous, current);
    }

    if (!std::isfinite(previous[static_cast<std::size_t>(n - 1)])) {
        throw std::runtime_error("No valid ISO DTW path found");
    }

    return state;
}

DtwState compute_directions_index_incremental(
    const ArrayView& x,
    const ArrayView& y,
    py::ssize_t n,
    double window_size
) {
    return compute_directions_incremental<false>(x, y, n, window_size);
}

DtwState compute_directions_range_precompute(
    const ArrayView& x,
    const ArrayView& y,
    py::ssize_t n,
    double window_size
) {
    return compute_directions_incremental<true>(x, y, n, window_size);
}

template <typename State, typename SetDirection>
State compute_directions_band_row(
    const ArrayView& x,
    const ArrayView& y,
    py::ssize_t n,
    double window_size,
    SetDirection&& set_direction
) {
    State state;
    state.n = n;
    state.radius = window_radius(n, window_size);
    state.band_width = 2 * state.radius - 1;
    if constexpr (std::is_same_v<State, BitDtwState>) {
        state.directions.assign(static_cast<std::size_t>(bit_direction_size(n, state.band_width)), 0);
    } else {
        state.directions.assign(static_cast<std::size_t>(n * state.band_width), DIR_NONE);
    }

    const double inf = std::numeric_limits<double>::infinity();
    std::vector<double> previous(static_cast<std::size_t>(state.band_width), inf);
    std::vector<double> current(static_cast<std::size_t>(state.band_width), inf);

    for (py::ssize_t i = 0; i < n; ++i) {
        std::fill(current.begin(), current.end(), inf);
        const py::ssize_t j_start = std::max<py::ssize_t>(0, i - state.radius + 1);
        const py::ssize_t j_stop = std::min<py::ssize_t>(n, i + state.radius);

        for (py::ssize_t j = j_start; j < j_stop; ++j) {
            const py::ssize_t offset = j - i + state.radius - 1;
            const double delta = x[i] - y[j];
            const double local_cost = delta * delta;
            double accumulated = inf;
            std::uint8_t direction = DIR_NONE;

            if (i == 0 && j == 0) {
                accumulated = local_cost;
            } else {
                double best_previous = inf;

                if (i > 0 && valid_cell(i - 1, j, n, state.radius)) {
                    best_previous = previous[static_cast<std::size_t>(j - i + state.radius)];
                    direction = DIR_VERTICAL;
                }
                if (j > 0 && j - 1 >= j_start) {
                    const double candidate = current[static_cast<std::size_t>(offset - 1)];
                    if (candidate < best_previous) {
                        best_previous = candidate;
                        direction = DIR_HORIZONTAL;
                    }
                }
                if (i > 0 && j > 0 && valid_cell(i - 1, j - 1, n, state.radius)) {
                    const double candidate = previous[static_cast<std::size_t>(j - i + state.radius - 1)];
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

            current[static_cast<std::size_t>(offset)] = accumulated;
            set_direction(state, i, j, direction);
        }

        std::swap(previous, current);
    }

    const py::ssize_t final_offset = state.radius - 1;
    if (!std::isfinite(previous[static_cast<std::size_t>(final_offset)])) {
        throw std::runtime_error("No valid ISO DTW path found");
    }

    return state;
}

DtwState compute_directions_band_row(
    const ArrayView& x,
    const ArrayView& y,
    py::ssize_t n,
    double window_size
) {
    return compute_directions_band_row<DtwState>(
        x,
        y,
        n,
        window_size,
        [](DtwState& state, py::ssize_t i, py::ssize_t j, std::uint8_t direction) {
            state.directions[static_cast<std::size_t>(direction_index(i, j, state.radius, state.band_width))] = direction;
        }
    );
}

BitDtwState compute_directions_bitpacked(
    const ArrayView& x,
    const ArrayView& y,
    py::ssize_t n,
    double window_size
) {
    return compute_directions_band_row<BitDtwState>(
        x,
        y,
        n,
        window_size,
        [](BitDtwState& state, py::ssize_t i, py::ssize_t j, std::uint8_t direction) {
            set_bit_direction(state, i, j, direction);
        }
    );
}

py::ssize_t diagonal_j_start(py::ssize_t diagonal, py::ssize_t n, py::ssize_t radius) {
    const py::ssize_t matrix_start = std::max<py::ssize_t>(0, diagonal - (n - 1));
    const py::ssize_t band_start = (diagonal - radius) / 2 + 1;
    return std::max(matrix_start, band_start);
}

py::ssize_t diagonal_j_stop(py::ssize_t diagonal, py::ssize_t n, py::ssize_t radius) {
    const py::ssize_t matrix_stop = std::min<py::ssize_t>(n - 1, diagonal) + 1;
    const py::ssize_t band_stop = ceil_div2(diagonal + radius);
    return std::min(matrix_stop, band_stop);
}

DtwState compute_directions_diagonal_parallel(
    const ArrayView& x,
    const ArrayView& y,
    py::ssize_t n,
    double window_size,
    py::ssize_t max_threads
) {
    if (max_threads <= 1) {
        return compute_directions_band_row(x, y, n, window_size);
    }

    DtwState state;
    state.n = n;
    state.radius = window_radius(n, window_size);
    state.band_width = 2 * state.radius - 1;
    state.directions.assign(static_cast<std::size_t>(n * state.band_width), DIR_NONE);

    const std::size_t hardware_threads = std::max(1u, std::thread::hardware_concurrency());
    const std::size_t thread_count = static_cast<std::size_t>(
        std::max<py::ssize_t>(1, std::min<py::ssize_t>(max_threads, static_cast<py::ssize_t>(hardware_threads)))
    );
    if (thread_count <= 1) {
        return compute_directions_band_row(x, y, n, window_size);
    }

    const double inf = std::numeric_limits<double>::infinity();
    const py::ssize_t total_diagonals = 2 * n - 1;
    ReusableBarrier barrier(thread_count);
    std::exception_ptr worker_exception = nullptr;
    std::mutex exception_mutex;

    std::vector<double> previous_previous;
    std::vector<double> previous;
    std::vector<double> current;
    py::ssize_t previous_previous_j_start = 0;
    py::ssize_t previous_j_start = 0;
    py::ssize_t current_j_start = 0;
    py::ssize_t current_length = 0;

    auto range_lookup = [](const std::vector<double>& values, py::ssize_t start, py::ssize_t j) -> double {
        const py::ssize_t offset = j - start;
        if (offset < 0 || offset >= static_cast<py::ssize_t>(values.size())) {
            return std::numeric_limits<double>::infinity();
        }
        return values[static_cast<std::size_t>(offset)];
    };

    auto worker = [&](std::size_t thread_index) {
        try {
            for (py::ssize_t diagonal = 0; diagonal < total_diagonals; ++diagonal) {
                if (thread_index == 0) {
                    current_j_start = diagonal_j_start(diagonal, n, state.radius);
                    const py::ssize_t j_stop = diagonal_j_stop(diagonal, n, state.radius);
                    current_length = std::max<py::ssize_t>(0, j_stop - current_j_start);
                    current.assign(static_cast<std::size_t>(current_length), inf);
                }
                barrier.wait();

                for (py::ssize_t offset = static_cast<py::ssize_t>(thread_index);
                     offset < current_length;
                     offset += static_cast<py::ssize_t>(thread_count)) {
                    const py::ssize_t j = current_j_start + offset;
                    const py::ssize_t i = diagonal - j;
                    const double delta = x[i] - y[j];
                    const double local_cost = delta * delta;
                    double accumulated = inf;
                    std::uint8_t direction = DIR_NONE;

                    if (i == 0 && j == 0) {
                        accumulated = local_cost;
                    } else {
                        double best_previous = inf;

                        if (i > 0 && valid_cell(i - 1, j, n, state.radius)) {
                            best_previous = range_lookup(previous, previous_j_start, j);
                            direction = DIR_VERTICAL;
                        }
                        if (j > 0 && valid_cell(i, j - 1, n, state.radius)) {
                            const double candidate = range_lookup(previous, previous_j_start, j - 1);
                            if (candidate < best_previous) {
                                best_previous = candidate;
                                direction = DIR_HORIZONTAL;
                            }
                        }
                        if (i > 0 && j > 0 && valid_cell(i - 1, j - 1, n, state.radius)) {
                            const double candidate = range_lookup(previous_previous, previous_previous_j_start, j - 1);
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

                    current[static_cast<std::size_t>(offset)] = accumulated;
                    state.directions[static_cast<std::size_t>(direction_index(i, j, state.radius, state.band_width))] = direction;
                }
                barrier.wait();

                if (thread_index == 0) {
                    previous_previous_j_start = previous_j_start;
                    previous_previous.swap(previous);
                    previous_j_start = current_j_start;
                    previous.swap(current);
                }
                barrier.wait();
            }
        } catch (...) {
            std::lock_guard<std::mutex> lock(exception_mutex);
            if (!worker_exception) {
                worker_exception = std::current_exception();
            }
        }
    };

    std::vector<std::thread> threads;
    threads.reserve(thread_count);
    for (std::size_t thread_index = 0; thread_index < thread_count; ++thread_index) {
        threads.emplace_back(worker, thread_index);
    }
    for (auto& thread : threads) {
        thread.join();
    }
    if (worker_exception) {
        std::rethrow_exception(worker_exception);
    }

    if (previous.empty() || !std::isfinite(previous.back())) {
        throw std::runtime_error("No valid ISO DTW path found");
    }
    return state;
}

bool block_intersects_band(
    py::ssize_t i0,
    py::ssize_t i1,
    py::ssize_t j0,
    py::ssize_t j1,
    py::ssize_t radius
) {
    py::ssize_t distance = 0;
    if (i1 <= j0) {
        distance = j0 - (i1 - 1);
    } else if (j1 <= i0) {
        distance = i0 - (j1 - 1);
    }
    return distance < radius;
}

void compute_block_wavefront_task(
    const ArrayView& x,
    const ArrayView& y,
    DtwState& state,
    std::vector<BlockBoundary>& boundaries,
    py::ssize_t block_count,
    py::ssize_t block_size,
    py::ssize_t block_i,
    py::ssize_t block_j
) {
    const double inf = std::numeric_limits<double>::infinity();
    const py::ssize_t i0 = block_i * block_size;
    const py::ssize_t i1 = std::min<py::ssize_t>(state.n, i0 + block_size);
    const py::ssize_t j0 = block_j * block_size;
    const py::ssize_t j1 = std::min<py::ssize_t>(state.n, j0 + block_size);
    const py::ssize_t row_count = i1 - i0;
    const py::ssize_t column_count = j1 - j0;
    const std::size_t boundary_index = static_cast<std::size_t>(block_i * block_count + block_j);

    BlockBoundary& boundary = boundaries[boundary_index];
    boundary.bottom.assign(static_cast<std::size_t>(column_count), inf);
    boundary.right.assign(static_cast<std::size_t>(row_count), inf);
    boundary.bottom_right = inf;
    boundary.active = true;

    const BlockBoundary* top = nullptr;
    const BlockBoundary* left = nullptr;
    const BlockBoundary* corner = nullptr;
    if (block_i > 0) {
        const BlockBoundary& candidate = boundaries[static_cast<std::size_t>((block_i - 1) * block_count + block_j)];
        if (candidate.active) {
            top = &candidate;
        }
    }
    if (block_j > 0) {
        const BlockBoundary& candidate = boundaries[static_cast<std::size_t>(block_i * block_count + block_j - 1)];
        if (candidate.active) {
            left = &candidate;
        }
    }
    if (block_i > 0 && block_j > 0) {
        const BlockBoundary& candidate = boundaries[static_cast<std::size_t>((block_i - 1) * block_count + block_j - 1)];
        if (candidate.active) {
            corner = &candidate;
        }
    }

    std::vector<double> previous(static_cast<std::size_t>(column_count), inf);
    std::vector<double> current(static_cast<std::size_t>(column_count), inf);

    auto top_value = [&](py::ssize_t column_offset) -> double {
        if (top == nullptr || column_offset < 0 || column_offset >= static_cast<py::ssize_t>(top->bottom.size())) {
            return inf;
        }
        return top->bottom[static_cast<std::size_t>(column_offset)];
    };
    auto left_value = [&](py::ssize_t row_offset) -> double {
        if (left == nullptr || row_offset < 0 || row_offset >= static_cast<py::ssize_t>(left->right.size())) {
            return inf;
        }
        return left->right[static_cast<std::size_t>(row_offset)];
    };
    const double corner_value = corner == nullptr ? inf : corner->bottom_right;

    for (py::ssize_t row_offset = 0; row_offset < row_count; ++row_offset) {
        std::fill(current.begin(), current.end(), inf);
        const py::ssize_t i = i0 + row_offset;
        const py::ssize_t j_start = std::max<py::ssize_t>(j0, i - state.radius + 1);
        const py::ssize_t j_stop = std::min<py::ssize_t>(j1, i + state.radius);

        for (py::ssize_t j = j_start; j < j_stop; ++j) {
            const py::ssize_t column_offset = j - j0;
            const double delta = x[i] - y[j];
            const double local_cost = delta * delta;
            double accumulated = inf;
            std::uint8_t direction = DIR_NONE;

            if (i == 0 && j == 0) {
                accumulated = local_cost;
            } else {
                double best_previous = inf;

                if (i > 0 && valid_cell(i - 1, j, state.n, state.radius)) {
                    best_previous = row_offset > 0 ? previous[static_cast<std::size_t>(column_offset)] : top_value(column_offset);
                    direction = DIR_VERTICAL;
                }
                if (j > 0 && valid_cell(i, j - 1, state.n, state.radius)) {
                    const double candidate = column_offset > 0
                        ? current[static_cast<std::size_t>(column_offset - 1)]
                        : left_value(row_offset);
                    if (candidate < best_previous) {
                        best_previous = candidate;
                        direction = DIR_HORIZONTAL;
                    }
                }
                if (i > 0 && j > 0 && valid_cell(i - 1, j - 1, state.n, state.radius)) {
                    double candidate = inf;
                    if (row_offset > 0 && column_offset > 0) {
                        candidate = previous[static_cast<std::size_t>(column_offset - 1)];
                    } else if (row_offset == 0 && column_offset > 0) {
                        candidate = top_value(column_offset - 1);
                    } else if (row_offset > 0 && column_offset == 0) {
                        candidate = left_value(row_offset - 1);
                    } else {
                        candidate = corner_value;
                    }
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

            current[static_cast<std::size_t>(column_offset)] = accumulated;
            state.directions[static_cast<std::size_t>(
                direction_index(i, j, state.radius, state.band_width)
            )] = direction;
        }

        boundary.right[static_cast<std::size_t>(row_offset)] = current[static_cast<std::size_t>(column_count - 1)];
        std::swap(previous, current);
    }

    boundary.bottom = previous;
    boundary.bottom_right = boundary.bottom.empty() ? inf : boundary.bottom.back();
}

DtwState compute_directions_blocked_wavefront(
    const ArrayView& x,
    const ArrayView& y,
    py::ssize_t n,
    double window_size,
    py::ssize_t block_size,
    py::ssize_t max_threads
) {
    if (block_size <= 0) {
        throw std::invalid_argument("Blocked ISO DTW requires a positive block size");
    }

    DtwState state;
    state.n = n;
    state.radius = window_radius(n, window_size);
    state.band_width = 2 * state.radius - 1;
    state.directions.assign(static_cast<std::size_t>(n * state.band_width), DIR_NONE);

    const py::ssize_t block_count = (n + block_size - 1) / block_size;
    std::vector<std::vector<std::pair<py::ssize_t, py::ssize_t>>> diagonals(
        static_cast<std::size_t>(2 * block_count - 1)
    );
    for (py::ssize_t block_i = 0; block_i < block_count; ++block_i) {
        const py::ssize_t i0 = block_i * block_size;
        const py::ssize_t i1 = std::min<py::ssize_t>(n, i0 + block_size);
        for (py::ssize_t block_j = 0; block_j < block_count; ++block_j) {
            const py::ssize_t j0 = block_j * block_size;
            const py::ssize_t j1 = std::min<py::ssize_t>(n, j0 + block_size);
            if (block_intersects_band(i0, i1, j0, j1, state.radius)) {
                diagonals[static_cast<std::size_t>(block_i + block_j)].emplace_back(block_i, block_j);
            }
        }
    }

    std::vector<BlockBoundary> boundaries(static_cast<std::size_t>(block_count * block_count));
    const std::size_t hardware_threads = std::max(1u, std::thread::hardware_concurrency());
    const std::size_t thread_count = static_cast<std::size_t>(
        std::max<py::ssize_t>(1, std::min<py::ssize_t>(max_threads, static_cast<py::ssize_t>(hardware_threads)))
    );

    if (thread_count <= 1) {
        for (const auto& tasks : diagonals) {
            for (const auto& task : tasks) {
                compute_block_wavefront_task(x, y, state, boundaries, block_count, block_size, task.first, task.second);
            }
        }
    } else {
        ReusableBarrier barrier(thread_count);
        std::atomic<std::size_t> next_task{0};
        std::exception_ptr worker_exception = nullptr;
        std::mutex exception_mutex;

        auto worker = [&](std::size_t) {
            try {
                for (const auto& tasks : diagonals) {
                    next_task.store(0, std::memory_order_relaxed);
                    barrier.wait();
                    while (true) {
                        const std::size_t task_index = next_task.fetch_add(1, std::memory_order_relaxed);
                        if (task_index >= tasks.size()) {
                            break;
                        }
                        const auto& task = tasks[task_index];
                        compute_block_wavefront_task(
                            x,
                            y,
                            state,
                            boundaries,
                            block_count,
                            block_size,
                            task.first,
                            task.second
                        );
                    }
                    barrier.wait();
                }
            } catch (...) {
                std::lock_guard<std::mutex> lock(exception_mutex);
                if (!worker_exception) {
                    worker_exception = std::current_exception();
                }
            }
        };

        std::vector<std::thread> threads;
        threads.reserve(thread_count);
        for (std::size_t thread_index = 0; thread_index < thread_count; ++thread_index) {
            threads.emplace_back(worker, thread_index);
        }
        for (auto& thread : threads) {
            thread.join();
        }
        if (worker_exception) {
            std::rethrow_exception(worker_exception);
        }
    }

    const BlockBoundary& final_boundary = boundaries[static_cast<std::size_t>((block_count - 1) * block_count + block_count - 1)];
    if (!final_boundary.active || !std::isfinite(final_boundary.bottom_right)) {
        throw std::runtime_error("No valid ISO DTW path found");
    }
    return state;
}

double magnitude_ratio_from_views(const ArrayView& x, const ArrayView& y, double window_size) {
    const auto n = x.n;
    const auto state = compute_directions(x, y, n, window_size);

    double numerator = 0.0;
    double denominator = 0.0;
    py::ssize_t i = n - 1;
    py::ssize_t j = n - 1;

    while (true) {
        numerator += std::abs(x[i] - y[j]);
        denominator += std::abs(y[j]);

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

    return numerator / denominator;
}

double magnitude_ratio_from_state(const ArrayView& x, const ArrayView& y, const DtwState& state) {
    double numerator = 0.0;
    double denominator = 0.0;
    py::ssize_t i = state.n - 1;
    py::ssize_t j = state.n - 1;

    while (true) {
        numerator += std::abs(x[i] - y[j]);
        denominator += std::abs(y[j]);

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

    return numerator / denominator;
}

double magnitude_ratio_from_bit_state(const ArrayView& x, const ArrayView& y, const BitDtwState& state) {
    double numerator = 0.0;
    double denominator = 0.0;
    py::ssize_t i = state.n - 1;
    py::ssize_t j = state.n - 1;

    while (true) {
        numerator += std::abs(x[i] - y[j]);
        denominator += std::abs(y[j]);

        if (i == 0 && j == 0) {
            break;
        }

        const auto direction = get_bit_direction(state, i, j);

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

    return numerator / denominator;
}

double magnitude_ratio_with_kernel(
    const ArrayView& x,
    const ArrayView& y,
    double window_size,
    const VariantConfig& config,
    py::ssize_t max_threads
) {
    if (config.parallel_mode == ParallelMode::Diagonal) {
        const auto state = compute_directions_diagonal_parallel(x, y, x.n, window_size, max_threads);
        return magnitude_ratio_from_state(x, y, state);
    }
    if (config.parallel_mode == ParallelMode::Blocked) {
        const auto state = compute_directions_blocked_wavefront(
            x,
            y,
            x.n,
            window_size,
            config.block_size,
            max_threads
        );
        return magnitude_ratio_from_state(x, y, state);
    }

    if (config.dtw_layout == DtwLayout::Current) {
        return magnitude_ratio_from_views(x, y, window_size);
    }
    if (config.dtw_layout == DtwLayout::RangePrecompute) {
        const auto state = compute_directions_range_precompute(x, y, x.n, window_size);
        return magnitude_ratio_from_state(x, y, state);
    }
    if (config.dtw_layout == DtwLayout::IndexIncremental) {
        const auto state = compute_directions_index_incremental(x, y, x.n, window_size);
        return magnitude_ratio_from_state(x, y, state);
    }
    if (config.dtw_layout == DtwLayout::CompactDirection) {
        const auto state = compute_directions_bitpacked(x, y, x.n, window_size);
        return magnitude_ratio_from_bit_state(x, y, state);
    }
    throw std::invalid_argument("Unsupported ISO DTW kernel variant");
}

double magnitude_ratio_variant_from_views(
    const ArrayView& x,
    const ArrayView& y,
    double window_size,
    const std::string& variant,
    py::ssize_t max_threads
) {
    if (variant == "serial_current") {
        return magnitude_ratio_from_views(x, y, window_size);
    }
    if (variant == "contiguous_serial") {
        const std::vector<double> contiguous_x = copy_values(x);
        const std::vector<double> contiguous_y = copy_values(y);
        return magnitude_ratio_from_views(view_from_vector(contiguous_x), view_from_vector(contiguous_y), window_size);
    }
    if (variant == "band_row") {
        const auto state = compute_directions_band_row(x, y, x.n, window_size);
        return magnitude_ratio_from_state(x, y, state);
    }
    if (variant == "range_precompute") {
        const auto state = compute_directions_range_precompute(x, y, x.n, window_size);
        return magnitude_ratio_from_state(x, y, state);
    }
    if (variant == "index_incremental") {
        const auto state = compute_directions_index_incremental(x, y, x.n, window_size);
        return magnitude_ratio_from_state(x, y, state);
    }
    if (variant == "bitpacked_direction") {
        const auto state = compute_directions_bitpacked(x, y, x.n, window_size);
        return magnitude_ratio_from_bit_state(x, y, state);
    }
    if (variant == "diagonal_parallel") {
        const auto state = compute_directions_diagonal_parallel(x, y, x.n, window_size, max_threads);
        return magnitude_ratio_from_state(x, y, state);
    }
    throw std::invalid_argument("Unknown ISO DTW variant: " + variant);
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

template <typename T>
T get_param(const py::dict& params, const char* name, T default_value) {
    if (params.contains(name)) {
        return py::cast<T>(params[py::str(name)]);
    }
    return default_value;
}

VariantConfig variant_config_from_spec(
    DtwLayout dtw_layout,
    ReductionMode reduction_mode,
    ParallelMode parallel_mode,
    py::ssize_t block_size,
    SimdLevel simd_level
) {
    VariantConfig config;
    config.dtw_layout = dtw_layout;
    config.reduction_mode = reduction_mode;
    config.parallel_mode = parallel_mode;
    config.block_size = block_size;
    config.requested_simd = simd_level;
    config.simd_selection = iso18571_native::select_simd_level(simd_level);

    if (parallel_mode == ParallelMode::Blocked && block_size <= 0) {
        throw std::invalid_argument("Blocked ISO DTW requires a positive block size");
    }
    if (parallel_mode != ParallelMode::Blocked && block_size != 0) {
        throw std::invalid_argument("block_size must be 0 unless ParallelMode.Blocked is selected");
    }

    config.phase_dual_product =
        reduction_mode == ReductionMode::PhaseDualProduct || reduction_mode == ReductionMode::All;
    config.fused_slope =
        reduction_mode == ReductionMode::FusedSlope || reduction_mode == ReductionMode::All;
    config.shared_shift_workspace =
        reduction_mode == ReductionMode::SharedShiftWorkspace || reduction_mode == ReductionMode::All;

    return config;
}

VariantConfig parse_variant(const std::string& variant) {
    VariantConfig config;
    config.simd_selection = iso18571_native::select_simd_level(config.requested_simd);
    if (variant.empty() || variant == "serial_current" || variant == "dtw_current" || variant == "current") {
        return config;
    }

    std::size_t start = 0;
    while (start <= variant.size()) {
        const std::size_t end = variant.find('+', start);
        const std::string token = variant.substr(start, end == std::string::npos ? std::string::npos : end - start);

        if (token.empty() || token == "serial_current" || token == "dtw_current" || token == "reduce_none" ||
            token == "parallel_none") {
        } else if (token == "dtw_range_precompute") {
            config.dtw_layout = DtwLayout::RangePrecompute;
        } else if (token == "dtw_index_incremental") {
            config.dtw_layout = DtwLayout::IndexIncremental;
        } else if (token == "dtw_compact_direction") {
            config.dtw_layout = DtwLayout::CompactDirection;
        } else if (token == "phase_dual_product") {
            config.reduction_mode = ReductionMode::PhaseDualProduct;
            config.phase_dual_product = true;
        } else if (token == "fused_slope") {
            config.reduction_mode = ReductionMode::FusedSlope;
            config.fused_slope = true;
        } else if (token == "shared_shift_workspace") {
            config.reduction_mode = ReductionMode::SharedShiftWorkspace;
            config.shared_shift_workspace = true;
        } else if (token == "all_reductions") {
            config.reduction_mode = ReductionMode::All;
            config.phase_dual_product = true;
            config.fused_slope = true;
            config.shared_shift_workspace = true;
        } else if (token == "diagonal_parallel") {
            config.parallel_mode = ParallelMode::Diagonal;
            config.block_size = 0;
        } else if (token == "blocked64" || token == "blocked128" || token == "blocked256" || token == "blocked512") {
            config.parallel_mode = ParallelMode::Blocked;
            config.block_size = static_cast<py::ssize_t>(std::stoll(token.substr(7)));
        } else if (token == "simd_scalar") {
            config.requested_simd = SimdLevel::Scalar;
            config.simd_selection = iso18571_native::select_simd_level(config.requested_simd);
        } else if (token == "simd_sse2") {
            config.requested_simd = SimdLevel::Sse2;
            config.simd_selection = iso18571_native::select_simd_level(config.requested_simd);
        } else if (token == "simd_avx2") {
            config.requested_simd = SimdLevel::Avx2;
            config.simd_selection = iso18571_native::select_simd_level(config.requested_simd);
        } else if (token == "simd_avx2_fma") {
            config.requested_simd = SimdLevel::Avx2Fma;
            config.simd_selection = iso18571_native::select_simd_level(config.requested_simd);
        } else if (token == "simd_auto") {
            config.requested_simd = SimdLevel::Auto;
            config.simd_selection = iso18571_native::select_simd_level(config.requested_simd);
        } else {
            throw std::invalid_argument("Unknown ISO scorer variant token: " + token);
        }

        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
    }

    return config;
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

    CurveView view;
    view.data = static_cast<const char*>(info.ptr);
    view.row_stride = info.strides[0];
    view.column_stride = info.strides[1];
    view.n = info.shape[0];
    return view;
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

    ArrayView view;
    view.data = static_cast<const char*>(info.ptr);
    view.stride = info.strides[0];
    view.n = info.shape[0];
    return view;
}

ArrayView value_view_from_curve(const CurveView& curve, py::ssize_t start, py::ssize_t length) {
    ArrayView view;
    view.data = curve.data + start * curve.row_stride + curve.column_stride;
    view.stride = curve.row_stride;
    view.n = length;
    return view;
}

std::vector<double> copy_values(const ArrayView& values) {
    std::vector<double> out(static_cast<std::size_t>(values.n));
    for (py::ssize_t idx = 0; idx < values.n; ++idx) {
        out[static_cast<std::size_t>(idx)] = values[idx];
    }
    return out;
}

ArrayView view_from_vector(const std::vector<double>& values) {
    ArrayView view;
    view.data = reinterpret_cast<const char*>(values.data());
    view.stride = static_cast<py::ssize_t>(sizeof(double));
    view.n = static_cast<py::ssize_t>(values.size());
    return view;
}

const double* contiguous_data(const ArrayView& values) {
    return reinterpret_cast<const double*>(values.data);
}

double correlation_for_shift(
    const CurveView& reference,
    const CurveView& comparison,
    py::ssize_t reference_start,
    py::ssize_t comparison_start,
    py::ssize_t length
) {
    double reference_sum = 0.0;
    double comparison_sum = 0.0;

    for (py::ssize_t idx = 0; idx < length; ++idx) {
        reference_sum += reference.value(reference_start + idx);
        comparison_sum += comparison.value(comparison_start + idx);
    }

    const double n = static_cast<double>(length);
    const double reference_mean = reference_sum / n;
    const double comparison_mean = comparison_sum / n;
    double reference_cov = 0.0;
    double comparison_cov = 0.0;
    double cross_cov = 0.0;

    for (py::ssize_t idx = 0; idx < length; ++idx) {
        const double x = reference.value(reference_start + idx) - reference_mean;
        const double y = comparison.value(comparison_start + idx) - comparison_mean;
        reference_cov += x * x;
        comparison_cov += y * y;
        cross_cov += x * y;
    }

    const double fact = n - 1.0;
    reference_cov /= fact;
    comparison_cov /= fact;
    cross_cov /= fact;

    double correlation = cross_cov / std::sqrt(reference_cov);
    correlation /= std::sqrt(comparison_cov);
    if (correlation > 1.0) {
        correlation = 1.0;
    } else if (correlation < -1.0) {
        correlation = -1.0;
    }
    return correlation;
}

PhaseCache build_phase_cache(const CurveView& reference, const CurveView& comparison) {
    PhaseCache cache;
    const std::size_t size = static_cast<std::size_t>(reference.n + 1);
    cache.reference_sum.assign(size, 0.0);
    cache.comparison_sum.assign(size, 0.0);
    cache.reference_square_sum.assign(size, 0.0);
    cache.comparison_square_sum.assign(size, 0.0);

    for (py::ssize_t idx = 0; idx < reference.n; ++idx) {
        const std::size_t current = static_cast<std::size_t>(idx + 1);
        const std::size_t previous = static_cast<std::size_t>(idx);
        const double x = reference.value(idx);
        const double y = comparison.value(idx);
        cache.reference_sum[current] = cache.reference_sum[previous] + x;
        cache.comparison_sum[current] = cache.comparison_sum[previous] + y;
        cache.reference_square_sum[current] = cache.reference_square_sum[previous] + x * x;
        cache.comparison_square_sum[current] = cache.comparison_square_sum[previous] + y * y;
    }
    return cache;
}

double prefix_range(const std::vector<double>& values, py::ssize_t start, py::ssize_t length) {
    return values[static_cast<std::size_t>(start + length)] - values[static_cast<std::size_t>(start)];
}

double correlation_for_shift(
    const CurveView& reference,
    const CurveView& comparison,
    const PhaseCache& cache,
    py::ssize_t reference_start,
    py::ssize_t comparison_start,
    py::ssize_t length
) {
    if (length < 32) {
        return correlation_for_shift(reference, comparison, reference_start, comparison_start, length);
    }

    const double n = static_cast<double>(length);
    const double reference_sum = prefix_range(cache.reference_sum, reference_start, length);
    const double comparison_sum = prefix_range(cache.comparison_sum, comparison_start, length);
    const double reference_square_sum = prefix_range(cache.reference_square_sum, reference_start, length);
    const double comparison_square_sum = prefix_range(cache.comparison_square_sum, comparison_start, length);

    double product_sum = 0.0;
    for (py::ssize_t idx = 0; idx < length; ++idx) {
        product_sum += reference.value(reference_start + idx) * comparison.value(comparison_start + idx);
    }

    const double numerator = product_sum - (reference_sum * comparison_sum / n);
    const double reference_var = reference_square_sum - (reference_sum * reference_sum / n);
    const double comparison_var = comparison_square_sum - (comparison_sum * comparison_sum / n);
    const double reference_scale = std::max(reference_square_sum, std::abs(reference_sum * reference_sum / n));
    const double comparison_scale = std::max(comparison_square_sum, std::abs(comparison_sum * comparison_sum / n));
    const double reference_tol = std::numeric_limits<double>::epsilon() * std::max(1.0, reference_scale) * 64.0;
    const double comparison_tol = std::numeric_limits<double>::epsilon() * std::max(1.0, comparison_scale) * 64.0;
    if (reference_var <= reference_tol || comparison_var <= comparison_tol) {
        return correlation_for_shift(reference, comparison, reference_start, comparison_start, length);
    }

    double correlation = numerator / std::sqrt(reference_var * comparison_var);
    if (correlation > 1.0) {
        correlation = 1.0;
    } else if (correlation < -1.0) {
        correlation = -1.0;
    }
    return correlation;
}

double correlation_from_cached_product(
    const PhaseCache& cache,
    py::ssize_t reference_start,
    py::ssize_t comparison_start,
    py::ssize_t length,
    double product_sum
) {
    const double n = static_cast<double>(length);
    const double reference_sum = prefix_range(cache.reference_sum, reference_start, length);
    const double comparison_sum = prefix_range(cache.comparison_sum, comparison_start, length);
    const double reference_square_sum = prefix_range(cache.reference_square_sum, reference_start, length);
    const double comparison_square_sum = prefix_range(cache.comparison_square_sum, comparison_start, length);

    const double numerator = product_sum - (reference_sum * comparison_sum / n);
    const double reference_var = reference_square_sum - (reference_sum * reference_sum / n);
    const double comparison_var = comparison_square_sum - (comparison_sum * comparison_sum / n);
    const double reference_scale = std::max(reference_square_sum, std::abs(reference_sum * reference_sum / n));
    const double comparison_scale = std::max(comparison_square_sum, std::abs(comparison_sum * comparison_sum / n));
    const double reference_tol = std::numeric_limits<double>::epsilon() * std::max(1.0, reference_scale) * 64.0;
    const double comparison_tol = std::numeric_limits<double>::epsilon() * std::max(1.0, comparison_scale) * 64.0;
    if (reference_var <= reference_tol || comparison_var <= comparison_tol) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    double correlation = numerator / std::sqrt(reference_var * comparison_var);
    if (correlation > 1.0) {
        correlation = 1.0;
    } else if (correlation < -1.0) {
        correlation = -1.0;
    }
    return correlation;
}

std::pair<double, double> dual_correlations_for_shift(
    const CurveView& reference,
    const CurveView& comparison,
    const PhaseCache& cache,
    py::ssize_t shift,
    py::ssize_t length
) {
    if (length < 32) {
        return {
            correlation_for_shift(reference, comparison, 0, shift, length),
            correlation_for_shift(reference, comparison, shift, 0, length),
        };
    }

    double left_product_sum = 0.0;
    double right_product_sum = 0.0;
    for (py::ssize_t idx = 0; idx < length; ++idx) {
        left_product_sum += reference.value(idx) * comparison.value(shift + idx);
        right_product_sum += reference.value(shift + idx) * comparison.value(idx);
    }

    double left = correlation_from_cached_product(cache, 0, shift, length, left_product_sum);
    double right = correlation_from_cached_product(cache, shift, 0, length, right_product_sum);
    if (std::isnan(left)) {
        left = correlation_for_shift(reference, comparison, 0, shift, length);
    }
    if (std::isnan(right)) {
        right = correlation_for_shift(reference, comparison, shift, 0, length);
    }
    return {left, right};
}

ShiftResult compute_shift(const CurveView& reference, const CurveView& comparison, const ScoreParams& params) {
    ShiftResult shift;
    shift.length = reference.n;
    shift.max_shift = std::round((1.0 - params.init_min) * 100.0) / 100.0;
    const py::ssize_t window_size = static_cast<py::ssize_t>(
        std::floor(static_cast<double>(comparison.n) * shift.max_shift) + 1.0
    );
    const py::ssize_t bounded_window_size = std::min(window_size, reference.n);
    const PhaseCache cache = build_phase_cache(reference, comparison);

    double ccr_max = correlation_for_shift(reference, comparison, cache, 0, 0, reference.n);
    shift.rho_e = ccr_max;

    for (py::ssize_t idx = 1; idx < bounded_window_size; ++idx) {
        const py::ssize_t length = reference.n - idx;
        const double ccr_left = correlation_for_shift(reference, comparison, cache, 0, idx, length);
        if (ccr_left > ccr_max) {
            ccr_max = ccr_left;
            shift.n_eps = idx;
            shift.reference_start = 0;
            shift.comparison_start = idx;
            shift.length = length;
            shift.rho_e = ccr_left;
        }

        const double ccr_right = correlation_for_shift(reference, comparison, cache, idx, 0, length);
        if (ccr_right > ccr_max) {
            ccr_max = ccr_right;
            shift.n_eps = idx;
            shift.reference_start = idx;
            shift.comparison_start = 0;
            shift.length = length;
            shift.rho_e = ccr_right;
        }
    }

    return shift;
}

ShiftResult compute_shift_dual_product(const CurveView& reference, const CurveView& comparison, const ScoreParams& params) {
    ShiftResult shift;
    shift.length = reference.n;
    shift.max_shift = std::round((1.0 - params.init_min) * 100.0) / 100.0;
    const py::ssize_t window_size = static_cast<py::ssize_t>(
        std::floor(static_cast<double>(comparison.n) * shift.max_shift) + 1.0
    );
    const py::ssize_t bounded_window_size = std::min(window_size, reference.n);
    const PhaseCache cache = build_phase_cache(reference, comparison);

    double ccr_max = correlation_for_shift(reference, comparison, cache, 0, 0, reference.n);
    shift.rho_e = ccr_max;

    for (py::ssize_t idx = 1; idx < bounded_window_size; ++idx) {
        const py::ssize_t length = reference.n - idx;
        const auto correlations = dual_correlations_for_shift(reference, comparison, cache, idx, length);
        const double ccr_left = correlations.first;
        if (ccr_left > ccr_max) {
            ccr_max = ccr_left;
            shift.n_eps = idx;
            shift.reference_start = 0;
            shift.comparison_start = idx;
            shift.length = length;
            shift.rho_e = ccr_left;
        }

        const double ccr_right = correlations.second;
        if (ccr_right > ccr_max) {
            ccr_max = ccr_right;
            shift.n_eps = idx;
            shift.reference_start = idx;
            shift.comparison_start = 0;
            shift.length = length;
            shift.rho_e = ccr_right;
        }
    }

    return shift;
}

double corridor_score(const CurveView& reference, const CurveView& comparison, const ScoreParams& params) {
    double t_norm = 0.0;
    for (py::ssize_t idx = 0; idx < reference.n; ++idx) {
        t_norm = std::max(t_norm, std::abs(reference.value(idx)));
    }

    const double inner_corridor = params.a_0 * t_norm;
    const double outer_corridor = params.b_0 * t_norm;
    double sum = 0.0;
    for (py::ssize_t idx = 0; idx < reference.n; ++idx) {
        const double diff = std::abs(reference.value(idx) - comparison.value(idx));
        double c_i = std::pow(
            (outer_corridor - diff) / (outer_corridor - inner_corridor),
            static_cast<double>(params.k_z)
        );
        if (diff < inner_corridor) {
            c_i = 1.0;
        }
        if (diff > outer_corridor) {
            c_i = 0.0;
        }
        sum += c_i;
    }
    return sum / static_cast<double>(reference.n);
}

double phase_score(const CurveView& reference, const ScoreParams& params, const ShiftResult& shift) {
    const double max_allowable_time_shift_threshold = static_cast<double>(reference.n) * shift.max_shift;
    if (shift.n_eps == 0) {
        return 1.0;
    }
    if (std::abs(static_cast<double>(shift.n_eps)) >= max_allowable_time_shift_threshold) {
        return 0.0;
    }
    return std::pow(
        (max_allowable_time_shift_threshold - std::abs(static_cast<double>(shift.n_eps))) /
            max_allowable_time_shift_threshold,
        static_cast<double>(params.k_p)
    );
}

double magnitude_score(const CurveView& reference, const CurveView& comparison, const ScoreParams& params, const ShiftResult& shift) {
    const ArrayView comparison_values = value_view_from_curve(comparison, shift.comparison_start, shift.length);
    const ArrayView reference_values = value_view_from_curve(reference, shift.reference_start, shift.length);
    double e_mag = 0.0;
    if (comparison_values.stride == static_cast<py::ssize_t>(sizeof(double)) &&
        reference_values.stride == static_cast<py::ssize_t>(sizeof(double))) {
        e_mag = magnitude_ratio_from_views(comparison_values, reference_values, 0.1);
    } else {
        const std::vector<double> contiguous_comparison = copy_values(comparison_values);
        const std::vector<double> contiguous_reference = copy_values(reference_values);
        e_mag = magnitude_ratio_from_views(
            view_from_vector(contiguous_comparison),
            view_from_vector(contiguous_reference),
            0.1
        );
    }

    if (e_mag == 0.0) {
        return 1.0;
    }
    if (e_mag > params.eps_m) {
        return 0.0;
    }
    return std::pow((params.eps_m - e_mag) / params.eps_m, static_cast<double>(params.k_m));
}

double magnitude_score_from_values(
    const ArrayView& reference_values,
    const ArrayView& comparison_values,
    const ScoreParams& params,
    const VariantConfig& config,
    py::ssize_t max_threads
) {
    const bool comparison_contiguous = comparison_values.stride == static_cast<py::ssize_t>(sizeof(double));
    const bool reference_contiguous = reference_values.stride == static_cast<py::ssize_t>(sizeof(double));
    double e_mag = 0.0;
    if (comparison_contiguous && reference_contiguous) {
        e_mag = magnitude_ratio_with_kernel(comparison_values, reference_values, 0.1, config, max_threads);
    } else {
        const std::vector<double> contiguous_comparison = copy_values(comparison_values);
        const std::vector<double> contiguous_reference = copy_values(reference_values);
        e_mag = magnitude_ratio_with_kernel(
            view_from_vector(contiguous_comparison),
            view_from_vector(contiguous_reference),
            0.1,
            config,
            max_threads
        );
    }

    if (e_mag == 0.0) {
        return 1.0;
    }
    if (e_mag > params.eps_m) {
        return 0.0;
    }
    return std::pow((params.eps_m - e_mag) / params.eps_m, static_cast<double>(params.k_m));
}

void gradient_values(const ArrayView& values, double dt, std::vector<double>& gradient, SimdLevel simd_level) {
    const py::ssize_t n = values.n;
    gradient.assign(static_cast<std::size_t>(n), 0.0);
    if (values.stride == static_cast<py::ssize_t>(sizeof(double))) {
        iso18571_native::gradient_contiguous(
            contiguous_data(values),
            static_cast<std::size_t>(n),
            dt,
            gradient.data(),
            simd_level
        );
        return;
    }

    gradient[0] = (values[1] - values[0]) / dt;
    for (py::ssize_t idx = 1; idx < n - 1; ++idx) {
        gradient[static_cast<std::size_t>(idx)] = (values[idx + 1] - values[idx - 1]) / (2.0 * dt);
    }
    gradient[static_cast<std::size_t>(n - 1)] = (values[n - 1] - values[n - 2]) / dt;
}

void smooth_slopes(const std::vector<double>& gradient, std::vector<double>& smoothed) {
    const py::ssize_t n = static_cast<py::ssize_t>(gradient.size());
    smoothed.assign(static_cast<std::size_t>(n), 0.0);

    const py::ssize_t windows[4] = {1, 3, 5, 7};
    for (py::ssize_t idx = 0; idx < 4; ++idx) {
        const py::ssize_t nr = windows[idx];
        double start_sum = 0.0;
        double end_sum = 0.0;
        for (py::ssize_t j = 0; j < nr; ++j) {
            start_sum += gradient[static_cast<std::size_t>(j)];
            end_sum += gradient[static_cast<std::size_t>(n - nr + j)];
        }
        smoothed[static_cast<std::size_t>(idx)] = start_sum / static_cast<double>(nr);
        smoothed[static_cast<std::size_t>(n - idx - 1)] = end_sum / static_cast<double>(nr);
    }

    for (py::ssize_t idx = 4; idx < n - 4; ++idx) {
        double sum = 0.0;
        for (py::ssize_t j = idx - 4; j <= idx + 4; ++j) {
            sum += gradient[static_cast<std::size_t>(j)];
        }
        smoothed[static_cast<std::size_t>(idx)] = sum / 9.0;
    }
}

double slope_score_from_values(
    const ArrayView& reference_values,
    const ArrayView& comparison_values,
    const ScoreParams& params,
    SimdLevel simd_level
) {
    if (reference_values.n < 9) {
        throw std::invalid_argument("Shifted curves must have at least 9 samples for slope rating");
    }

    std::vector<double> comparison_gradient;
    std::vector<double> reference_gradient;
    std::vector<double> comparison_smoothed;
    std::vector<double> reference_smoothed;
    gradient_values(comparison_values, params.dt, comparison_gradient, simd_level);
    gradient_values(reference_values, params.dt, reference_gradient, simd_level);
    smooth_slopes(comparison_gradient, comparison_smoothed);
    smooth_slopes(reference_gradient, reference_smoothed);

    double numerator = 0.0;
    double denominator = 0.0;
    for (py::ssize_t idx = 0; idx < reference_values.n; ++idx) {
        numerator += std::abs(
            comparison_smoothed[static_cast<std::size_t>(idx)] -
            reference_smoothed[static_cast<std::size_t>(idx)]
        );
        denominator += std::abs(reference_smoothed[static_cast<std::size_t>(idx)]);
    }
    const double e_slope = numerator / denominator;

    if (e_slope <= 0.0) {
        return 1.0;
    }
    if (e_slope >= params.e_s) {
        return 0.0;
    }
    return (params.e_s - e_slope) / params.e_s;
}

double slope_score_from_values(const ArrayView& reference_values, const ArrayView& comparison_values, const ScoreParams& params) {
    return slope_score_from_values(reference_values, comparison_values, params, SimdLevel::Scalar);
}

double smoothed_slope_at(const std::vector<double>& gradient, py::ssize_t idx) {
    const py::ssize_t n = static_cast<py::ssize_t>(gradient.size());
    if (idx < 4) {
        const py::ssize_t windows[4] = {1, 3, 5, 7};
        const py::ssize_t nr = windows[idx];
        double sum = 0.0;
        for (py::ssize_t j = 0; j < nr; ++j) {
            sum += gradient[static_cast<std::size_t>(j)];
        }
        return sum / static_cast<double>(nr);
    }
    if (idx >= n - 4) {
        const py::ssize_t edge_idx = n - idx - 1;
        const py::ssize_t windows[4] = {1, 3, 5, 7};
        const py::ssize_t nr = windows[edge_idx];
        double sum = 0.0;
        for (py::ssize_t j = 0; j < nr; ++j) {
            sum += gradient[static_cast<std::size_t>(n - nr + j)];
        }
        return sum / static_cast<double>(nr);
    }

    double sum = 0.0;
    for (py::ssize_t j = idx - 4; j <= idx + 4; ++j) {
        sum += gradient[static_cast<std::size_t>(j)];
    }
    return sum / 9.0;
}

double fused_slope_score_from_values(
    const ArrayView& reference_values,
    const ArrayView& comparison_values,
    const ScoreParams& params,
    SimdLevel simd_level
) {
    if (reference_values.n < 9) {
        throw std::invalid_argument("Shifted curves must have at least 9 samples for slope rating");
    }

    std::vector<double> comparison_gradient;
    std::vector<double> reference_gradient;
    gradient_values(comparison_values, params.dt, comparison_gradient, simd_level);
    gradient_values(reference_values, params.dt, reference_gradient, simd_level);

    double numerator = 0.0;
    double denominator = 0.0;
    for (py::ssize_t idx = 0; idx < reference_values.n; ++idx) {
        const double comparison_smoothed = smoothed_slope_at(comparison_gradient, idx);
        const double reference_smoothed = smoothed_slope_at(reference_gradient, idx);
        numerator += std::abs(comparison_smoothed - reference_smoothed);
        denominator += std::abs(reference_smoothed);
    }

    const double e_slope = numerator / denominator;
    if (e_slope <= 0.0) {
        return 1.0;
    }
    if (e_slope >= params.e_s) {
        return 0.0;
    }
    return (params.e_s - e_slope) / params.e_s;
}

double slope_score(const CurveView& reference, const CurveView& comparison, const ScoreParams& params, const ShiftResult& shift) {
    const ArrayView comparison_values = value_view_from_curve(comparison, shift.comparison_start, shift.length);
    const ArrayView reference_values = value_view_from_curve(reference, shift.reference_start, shift.length);
    return slope_score_from_values(reference_values, comparison_values, params);
}

ScoreResult score_components_native(const CurveView& reference, const CurveView& comparison, const ScoreParams& params) {
    ScoreResult score;
    score.shift = compute_shift(reference, comparison, params);
    score.z = corridor_score(reference, comparison, params);
    score.ep = phase_score(reference, params, score.shift);
    score.em = magnitude_score(reference, comparison, params, score.shift);
    score.es = slope_score(reference, comparison, params, score.shift);
    score.r = params.w_z * score.z + params.w_p * score.ep + params.w_m * score.em + params.w_s * score.es;
    return score;
}

ScoreResult score_components_native_variant(
    const CurveView& reference,
    const CurveView& comparison,
    const ScoreParams& params,
    const VariantConfig& config,
    py::ssize_t max_threads
) {
    ScoreResult score;
    score.shift = config.phase_dual_product
        ? compute_shift_dual_product(reference, comparison, params)
        : compute_shift(reference, comparison, params);
    score.z = corridor_score(reference, comparison, params);
    score.ep = phase_score(reference, params, score.shift);

    const ArrayView comparison_values = value_view_from_curve(comparison, score.shift.comparison_start, score.shift.length);
    const ArrayView reference_values = value_view_from_curve(reference, score.shift.reference_start, score.shift.length);

    if (config.shared_shift_workspace) {
        const std::vector<double> contiguous_comparison = copy_values(comparison_values);
        const std::vector<double> contiguous_reference = copy_values(reference_values);
        const ArrayView contiguous_comparison_view = view_from_vector(contiguous_comparison);
        const ArrayView contiguous_reference_view = view_from_vector(contiguous_reference);
        score.em = magnitude_score_from_values(
            contiguous_reference_view,
            contiguous_comparison_view,
            params,
            config,
            max_threads
        );
        score.es = config.fused_slope
            ? fused_slope_score_from_values(
                contiguous_reference_view,
                contiguous_comparison_view,
                params,
                config.simd_selection.selected
            )
            : slope_score_from_values(
                contiguous_reference_view,
                contiguous_comparison_view,
                params,
                config.simd_selection.selected
            );
    } else {
        score.em = magnitude_score_from_values(reference_values, comparison_values, params, config, max_threads);
        score.es = config.fused_slope
            ? fused_slope_score_from_values(reference_values, comparison_values, params, config.simd_selection.selected)
            : slope_score_from_values(reference_values, comparison_values, params, config.simd_selection.selected);
    }

    score.r = params.w_z * score.z + params.w_p * score.ep + params.w_m * score.em + params.w_s * score.es;
    return score;
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

    double ratio = 0.0;
    {
        py::gil_scoped_release release;
        ratio = magnitude_ratio_from_views(views.first, views.second, window_size);
    }

    return ratio;
}

double magnitude_ratio_variant(
    py::array_t<double, py::array::forcecast> x,
    py::array_t<double, py::array::forcecast> y,
    double window_size,
    const std::string& variant,
    py::ssize_t max_threads
) {
    const auto views = validate_inputs(x, y);

    double ratio = 0.0;
    {
        py::gil_scoped_release release;
        ratio = magnitude_ratio_variant_from_views(views.first, views.second, window_size, variant, max_threads);
    }

    return ratio;
}

double magnitude_ratio_variant_spec(
    py::array_t<double, py::array::forcecast> x,
    py::array_t<double, py::array::forcecast> y,
    double window_size,
    DtwLayout dtw_layout,
    ParallelMode parallel_mode,
    py::ssize_t block_size,
    SimdLevel simd_level,
    py::ssize_t max_threads
) {
    const auto views = validate_inputs(x, y);
    const VariantConfig config = variant_config_from_spec(
        dtw_layout,
        ReductionMode::None,
        parallel_mode,
        block_size,
        simd_level
    );

    double ratio = 0.0;
    {
        py::gil_scoped_release release;
        ratio = magnitude_ratio_with_kernel(views.first, views.second, window_size, config, max_threads);
    }

    return ratio;
}

void parallel_barrier_overhead(py::ssize_t iterations, py::ssize_t max_threads) {
    const std::size_t hardware_threads = std::max(1u, std::thread::hardware_concurrency());
    const std::size_t thread_count = static_cast<std::size_t>(
        std::max<py::ssize_t>(1, std::min<py::ssize_t>(max_threads, static_cast<py::ssize_t>(hardware_threads)))
    );
    if (thread_count <= 1) {
        for (py::ssize_t iteration = 0; iteration < iterations; ++iteration) {
        }
        return;
    }

    ReusableBarrier barrier(thread_count);
    std::vector<std::thread> threads;
    threads.reserve(thread_count);
    for (std::size_t thread_index = 0; thread_index < thread_count; ++thread_index) {
        threads.emplace_back([&] {
            for (py::ssize_t iteration = 0; iteration < iterations; ++iteration) {
                barrier.wait();
                barrier.wait();
                barrier.wait();
            }
        });
    }
    for (auto& thread : threads) {
        thread.join();
    }
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
        score = score_components_native(views.first, views.second, score_params);
    }

    py::dict out;
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
    return out;
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

py::dict score_components_variant(
    py::array_t<double, py::array::forcecast> reference_curve,
    py::array_t<double, py::array::forcecast> comparison_curve,
    py::dict params,
    const std::string& variant,
    py::ssize_t max_threads
) {
    const auto views = validate_curves(reference_curve, comparison_curve);
    const ScoreParams score_params = score_params_from_dict(params);
    const VariantConfig config = parse_variant(variant);

    ScoreResult score;
    {
        py::gil_scoped_release release;
        score = score_components_native_variant(views.first, views.second, score_params, config, max_threads);
    }

    py::dict out;
    add_score_fields(out, score);
    out["requested_simd_level"] = iso18571_native::simd_level_name(config.simd_selection.requested);
    out["selected_simd_level"] = iso18571_native::simd_level_name(config.simd_selection.selected);
    out["simd_fallback"] = config.simd_selection.fallback;
    return out;
}

py::dict score_components_variant_spec(
    py::array_t<double, py::array::forcecast> reference_curve,
    py::array_t<double, py::array::forcecast> comparison_curve,
    py::dict params,
    DtwLayout dtw_layout,
    ReductionMode reduction_mode,
    ParallelMode parallel_mode,
    py::ssize_t block_size,
    SimdLevel simd_level,
    py::ssize_t max_threads
) {
    const auto views = validate_curves(reference_curve, comparison_curve);
    const ScoreParams score_params = score_params_from_dict(params);
    const VariantConfig config = variant_config_from_spec(
        dtw_layout,
        reduction_mode,
        parallel_mode,
        block_size,
        simd_level
    );

    ScoreResult score;
    {
        py::gil_scoped_release release;
        score = score_components_native_variant(views.first, views.second, score_params, config, max_threads);
    }

    py::dict out;
    add_score_fields(out, score);
    out["requested_simd_level"] = iso18571_native::simd_level_name(config.simd_selection.requested);
    out["selected_simd_level"] = iso18571_native::simd_level_name(config.simd_selection.selected);
    out["simd_fallback"] = config.simd_selection.fallback;
    return out;
}

py::dict simd_info() {
    const SimdCapabilities capabilities = iso18571_native::simd_capabilities();
    py::dict out;
    out["compiled_scalar"] = capabilities.compiled_scalar;
    out["compiled_sse2"] = capabilities.compiled_sse2;
    out["compiled_avx2"] = capabilities.compiled_avx2;
    out["compiled_avx2_fma"] = capabilities.compiled_avx2_fma;
    out["detected_sse2"] = capabilities.detected_sse2;
    out["detected_avx2"] = capabilities.detected_avx2;
    out["detected_fma"] = capabilities.detected_fma;
    out["auto_level"] = iso18571_native::simd_level_name(capabilities.auto_level);
    return out;
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
    py::enum_<DtwLayout>(m, "DtwLayout")
        .value("Current", DtwLayout::Current)
        .value("RangePrecompute", DtwLayout::RangePrecompute)
        .value("IndexIncremental", DtwLayout::IndexIncremental)
        .value("CompactDirection", DtwLayout::CompactDirection);
    py::enum_<ReductionMode>(m, "ReductionMode")
        .value("None", ReductionMode::None)
        .value("NoReduction", ReductionMode::None)
        .value("PhaseDualProduct", ReductionMode::PhaseDualProduct)
        .value("FusedSlope", ReductionMode::FusedSlope)
        .value("SharedShiftWorkspace", ReductionMode::SharedShiftWorkspace)
        .value("All", ReductionMode::All);
    py::enum_<ParallelMode>(m, "ParallelMode")
        .value("None", ParallelMode::None)
        .value("NoParallel", ParallelMode::None)
        .value("Diagonal", ParallelMode::Diagonal)
        .value("Blocked", ParallelMode::Blocked);
    py::enum_<SimdLevel>(m, "SimdLevel")
        .value("Scalar", SimdLevel::Scalar)
        .value("Sse2", SimdLevel::Sse2)
        .value("Avx2", SimdLevel::Avx2)
        .value("Avx2Fma", SimdLevel::Avx2Fma)
        .value("Auto", SimdLevel::Auto);
    m.def("backend_info", &backend_info);
    m.def("_simd_info", &simd_info);
    m.def("warp_path", &warp_path, py::arg("x"), py::arg("y"), py::arg("window_size"));
    m.def("magnitude_ratio", &magnitude_ratio, py::arg("x"), py::arg("y"), py::arg("window_size"));
    m.def("_magnitude_ratio_variant", &magnitude_ratio_variant, py::arg("x"), py::arg("y"), py::arg("window_size"), py::arg("variant"), py::arg("max_threads") = 1);
    m.def(
        "_magnitude_ratio_variant_spec",
        &magnitude_ratio_variant_spec,
        py::arg("x"),
        py::arg("y"),
        py::arg("window_size"),
        py::arg("dtw_layout"),
        py::arg("parallel_mode"),
        py::arg("block_size"),
        py::arg("simd_level"),
        py::arg("max_threads") = 1
    );
    m.def("_parallel_barrier_overhead", &parallel_barrier_overhead, py::arg("iterations"), py::arg("max_threads"));
    m.def("score_components", &score_components, py::arg("reference_curve"), py::arg("comparison_curve"), py::arg("params") = py::dict());
    m.def("_score_components_variant", &score_components_variant, py::arg("reference_curve"), py::arg("comparison_curve"), py::arg("params"), py::arg("variant"), py::arg("max_threads") = 1);
    m.def(
        "_score_components_variant_spec",
        &score_components_variant_spec,
        py::arg("reference_curve"),
        py::arg("comparison_curve"),
        py::arg("params"),
        py::arg("dtw_layout"),
        py::arg("reduction_mode"),
        py::arg("parallel_mode"),
        py::arg("block_size"),
        py::arg("simd_level"),
        py::arg("max_threads") = 1
    );
}
