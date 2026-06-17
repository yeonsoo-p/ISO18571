# ISO/TS 18571 DTW Experiment Log

## 2026-06-16 00:00 KST - Native Backend Kickoff

- Git status: new repository, no commits yet, all project files currently
  untracked.
- Hypothesis: a clean-room pybind11 C++ backend can preserve `local_iso_numpy`
  behavior while improving both first-use time and steady-state speed by avoiding
  Python-level DP loops and full warped-array allocation for production scoring.
- Files changed: pending native backend scaffolding, packaging, tests, and docs.
- Commands: initial git/status and project-layout inspection only.
- Validation result: not run yet for native backend.
- Benchmark result: not run yet for native backend.
- Conclusion: start with exact banded recurrence plus fast `magnitude_ratio`.
- Next hypothesis: a simple rolling-row C++ DP with direction storage is already
  sufficient to beat `dtw_python` prep and `librosa` steady-state on the Annex
  corpus.

## 2026-06-16 12:05 KST - Initial Native Backend Validation And Benchmark

- Git status: new repository, no commits yet; native backend, docs, tools, and
  packaging files are untracked/modified.
- Hypothesis: rolling-row C++ DP with direction storage and a direct
  `magnitude_ratio` fast path is sufficient to beat both target baselines.
- Files changed: `rating.py`, `pyproject.toml`, `setup.py`,
  `iso18571_native/`, `src/iso18571_native/`, `tests/`, `tools/iso18571/`,
  `docs/`, `AGENTS.md`, `.gitignore`.
- Commands:
  - `uv pip install -e .`
  - `uv run --with pytest --with pytest-benchmark python -m pytest -q tests/test_iso18571_backends.py --iso18571-backends local_iso_native`
  - `uv run --with pytest --with pytest-benchmark --with dtw-python --with librosa --with scipy python -m pytest -q tests/test_iso18571_benchmarks.py --iso18571-backends <backend> --benchmark-min-rounds=5 --benchmark-max-time=5 --benchmark-warmup=off --benchmark-json=/tmp/iso18571_native_bench_<backend>.json`
  - `uv run python tools/iso18571/summarize_benchmarks.py /tmp/iso18571_native_bench_*.json`
- Validation result: `local_iso_native` passed 6 tests, including Annex scores,
  native path equivalence to `local_iso_numpy`, and direct magnitude-ratio
  equivalence to the warped-curve formula.
- Benchmark result:
  - `local_iso_native`: first-use `0.1521s`, steady median `0.1513s`.
  - `dtw_python`: first-use `0.9179s`, steady median `0.4092s`.
  - `librosa`: first-use `7.1672s`, steady median `0.3951s`.
- Conclusion: initial native backend beats `dtw_python` prep/first-use and
  `librosa` steady-state on Linux by a wide margin.
- Next hypothesis: optimize for robustness and maintainability next: wheel
  build smoke test, all-backend validation behavior, and cleaner benchmark
  repeatability.

## 2026-06-16 12:06 KST - Linux Wheel Smoke Test

- Git status: new repository, no commits yet; build artifacts are ignored,
  source changes remain uncommitted.
- Hypothesis: setuptools/pybind11 packaging can produce a wheel that imports
  `rating.py` and `iso18571_native` outside editable mode.
- Files changed: no source changes after previous experiment.
- Commands:
  - `uv build --wheel`
  - install `dist/euroncap-0.1.0-cp313-cp313-linux_x86_64.whl` into a temp venv
  - run Annex C.1.1 AC1 CAE1 with `dtw_backend='local_iso_native'`
  - `uv run --with pytest --with pytest-benchmark python -m pytest -q tests/test_iso18571_backends.py --iso18571-backends local_iso_numpy,local_iso_native`
- Validation result:
  - wheel smoke score `0.91688574`, expected `0.9169 +/- 0.001`;
  - combined reference/native backend validation: `10 passed`.
- Benchmark result: not rerun in this experiment.
- Conclusion: Linux wheel packaging works for the native backend and top-level
  `rating` import.
- Next hypothesis: all passing comparison backends can be validated with the new
  native backend in the matrix; then benchmark reporting should be reproducible
  through `tools/iso18571/`.

## 2026-06-16 12:08 KST - Eligible Matrix And Tooling Smoke

- Git status: new repository, no commits yet; source changes remain
  uncommitted.
- Hypothesis: the new native backend can coexist with every production-eligible
  comparison backend, and benchmark tooling can reproduce native timing output.
- Files changed: no source changes after previous experiment.
- Commands:
  - `uv run --with pytest --with pytest-benchmark --with dtwalign --with dtaidistance --with dtw-python --with librosa --with scipy python -m pytest -q tests/test_iso18571_backends.py --iso18571-backends local_iso_numpy,local_iso_native,dtwalign,dtaidistance,dtw_python,librosa`
  - `uv run --with pytest --with pytest-benchmark python tools/iso18571/run_backend_benchmarks.py --output-dir .benchmarks/iso18571-smoke --backends local_iso_native --max-time 1 --min-rounds 3`
  - `uv run python tools/iso18571/summarize_benchmarks.py .benchmarks/iso18571-smoke/*.json`
- Validation result: eligible backend matrix passed: `26 passed`.
- Benchmark result: native tooling smoke first-use `0.1555s`, steady median
  `0.1524s`.
- Conclusion: comparison backend validation and repeatable benchmark tooling are
  working.
- Next hypothesis: diff review will reveal minor cleanup opportunities before
  deeper speed profiling.

## 2026-06-16 12:10 KST - Native Robustness Tests

- Git status: new repository, no commits yet; source changes remain
  uncommitted.
- Hypothesis: deterministic random curves and tie-heavy zero curves will catch
  native path drift not covered by the Annex cases.
- Files changed: `tests/test_iso18571_backends.py`.
- Commands:
  - `uv run --with pytest --with pytest-benchmark python -m pytest -q tests/test_iso18571_backends.py --iso18571-backends local_iso_native`
- Validation result: native backend robustness validation passed: `8 passed`.
- Benchmark result: not rerun in this experiment.
- Conclusion: native path and magnitude-ratio behavior match the Python
  reference for Annex, random, boundary-window, and all-zero tie-heavy cases.
- Next hypothesis: accepting strided NumPy views directly may reduce native
  call overhead for shifted curve columns.

## 2026-06-16 12:12 KST - Strided View Native Binding

- Git status: new repository, no commits yet; source changes remain
  uncommitted.
- Hypothesis: accepting 1D strided NumPy arrays directly avoids unnecessary
  pybind11 contiguous copies for shifted curve columns.
- Files changed: `src/iso18571_native/_core.cpp`.
- Commands:
  - `uv pip install -e .`
  - `uv run --with pytest --with pytest-benchmark python -m pytest -q tests/test_iso18571_backends.py --iso18571-backends local_iso_native`
  - `uv run --with pytest --with pytest-benchmark python tools/iso18571/run_backend_benchmarks.py --output-dir .benchmarks/iso18571-strided --backends local_iso_native --max-time 5 --min-rounds 5`
  - `uv run python tools/iso18571/summarize_benchmarks.py .benchmarks/iso18571-strided/*.json`
  - `uv build --wheel`, followed by temp-venv wheel smoke on Annex C.1.1 AC1 CAE1
- Validation result:
  - native backend validation passed: `8 passed`;
  - wheel smoke score `0.91688574`, expected `0.9169 +/- 0.001`.
- Benchmark result:
  - native first-use `0.1505s`;
  - native steady median `0.1482s` over 34 rounds.
- Conclusion: strided view support preserved correctness and improved
  steady-state full Annex pass by roughly 2-3 ms.
- Next hypothesis: native DTW is no longer the runtime bottleneck; remaining
  speed work should target phase cross-correlation only if expanding beyond the
  DTW backend is desired.

## 2026-06-16 12:14 KST - Vectorized Phase Correlation

- Git status: new repository, no commits yet; source changes remain
  uncommitted.
- Hypothesis: replacing repeated `np.corrcoef` calls with cumulative sums and
  one `np.correlate` per case will preserve existing shift/tie behavior and
  materially reduce full Annex pass time.
- Files changed: `rating.py`, `tests/test_iso18571_backends.py`.
- Commands:
  - `uv run --with pytest --with pytest-benchmark python -m pytest -q tests/test_iso18571_backends.py --iso18571-backends local_iso_numpy,local_iso_native`
  - target benchmark trio for `local_iso_native`, `dtw_python`, and `librosa`
    with `--benchmark-min-rounds=5`, `--benchmark-max-time=5`, and warmup off
  - eligible matrix validation for
    `local_iso_numpy,local_iso_native,dtwalign,dtaidistance,dtw_python,librosa`
  - cProfile pass over all Annex cases with `local_iso_native`
- Validation result:
  - reference/native validation passed: `15 passed`;
  - eligible backend matrix passed: `39 passed`;
  - shifted correlations match the previous `np.corrcoef` reference within
    `1e-12` on deterministic random cases.
- Benchmark result:
  - `local_iso_native`: first-use `0.0159s`, steady median `0.0147s`.
  - `dtw_python`: first-use `0.4431s`, steady median `0.2624s`.
  - `librosa`: first-use `0.9856s`, steady median `0.2526s`.
- Profile result: for all 42 Annex cases, native magnitude DTW is about `0.008s`
  and phase alignment is about `0.005s`; CSV loading dominates only when not
  using the pytest fixture.
- Conclusion: full Annex pass is now roughly an order of magnitude faster than
  the first native implementation and still validates across all eligible
  backends.
- Next hypothesis: rebuild wheel after the phase change, then add a concise
  developer command reference for validation and benchmarking.

## 2026-06-16 12:15 KST - Wheel Smoke And Command Reference

- Git status: new repository, no commits yet; source changes remain
  uncommitted.
- Hypothesis: packaged wheel still works after the phase optimization, and
  future agents need concise commands to avoid benchmark drift.
- Files changed: `docs/iso18571-dtw-backends.md`,
  `docs/iso18571-dtw-experiment-log.md`.
- Commands:
  - `uv build --wheel`
  - install `dist/euroncap-0.1.0-cp313-cp313-linux_x86_64.whl` into a temp venv
  - run Annex C.1.1 AC1 CAE1 with `dtw_backend='local_iso_native'`
- Validation result: wheel smoke score `0.91688574`, expected
  `0.9169 +/- 0.001`.
- Benchmark result: not rerun in this experiment.
- Conclusion: Linux wheel remains healthy and docs now include validation,
  benchmark, and wheel-build commands.
- Next hypothesis: inspect working tree and generated lock/build artifacts, then
  decide whether a first commit/checkpoint is useful.

## 2026-06-16 12:17 KST - Wheel CI Configuration

- Git status: new repository, no commits yet; source changes remain
  uncommitted.
- Hypothesis: adding `cibuildwheel` configuration and a GitHub Actions workflow
  provides a concrete route for Linux and Windows prebuilt wheels.
- Files changed: `pyproject.toml`, `.github/workflows/wheels.yml`, `uv.lock`.
- Commands:
  - `uv lock`
  - `uv run --with pytest --with pytest-benchmark python -m pytest -q tests/test_iso18571_backends.py --iso18571-backends local_iso_numpy,local_iso_native`
- Validation result: reference/native validation passed: `15 passed`.
- Benchmark result: not rerun in this experiment.
- Conclusion: wheel CI configuration is added without breaking local validation.
- Next hypothesis: inspect final file state and run one final eligible-matrix
  validation if no cleanup changes are needed.

## 2026-06-16 12:19 KST - Wheel Smoke Script And Final Matrix Gate

- Git status: new repository, no commits yet; source changes remain
  uncommitted.
- Hypothesis: moving the wheel smoke into a dedicated script improves CI
  maintainability without changing runtime behavior.
- Files changed: `pyproject.toml`, `tools/iso18571/wheel_smoke.py`, `uv.lock`.
- Commands:
  - `uv lock`
  - `uv run python tools/iso18571/wheel_smoke.py`
  - `uv run --with pytest --with pytest-benchmark --with dtwalign --with dtaidistance --with dtw-python --with librosa --with scipy python -m pytest -q tests/test_iso18571_backends.py --iso18571-backends local_iso_numpy,local_iso_native,dtwalign,dtaidistance,dtw_python,librosa`
- Validation result:
  - wheel smoke script passed locally;
  - eligible backend matrix passed: `39 passed`.
- Benchmark result: latest benchmark remains the phase-optimized table:
  native first-use `0.0159s`, native steady median `0.0147s`.
- Conclusion: implementation, docs, tools, and CI configuration are internally
  consistent on Linux.
- Next hypothesis: use git to create a clean checkpoint if the user wants a
  commit; otherwise continue with Windows CI verification when a runner is
  available.

## 2026-06-16 13:04 KST - Full Native Scorer Port And Signal-Aware Tests

- Git status: checkpoint commit `5e29069` exists; full native scorer and tests
  are uncommitted follow-on changes.
- Hypothesis: porting the complete ISO/TS 18571 scorer to native code behind
  `local_iso_native` will preserve Annex and `rating_original.py` behavior while
  removing Python component-call overhead for NumPy-fed curves.
- Files changed:
  - `src/iso18571_native/_core.cpp`;
  - `iso18571_native/__init__.py`;
  - `rating.py`;
  - `pytest.ini`;
  - `tests/conftest.py`;
  - `tests/iso18571_signals.py`;
  - `tests/test_rating_original_parity.py`;
  - `tests/test_iso18571_signal_benchmarks.py`.
- Commands:
  - `git add -A && git commit -m "checkpoint native ISO18571 backend baseline"`
  - `uv pip install -e .`
  - `uv run --with pytest --with dtwalign --with scipy python -m pytest -q tests/test_rating_original_parity.py`
  - `uv run --with pytest --with pytest-benchmark python -m pytest -q tests/test_iso18571_backends.py --iso18571-backends local_iso_native`
  - `uv run --with pytest --with pytest-benchmark --with dtwalign --with dtaidistance --with dtw-python --with librosa --with scipy python -m pytest -q tests/test_iso18571_backends.py --iso18571-backends local_iso_numpy,local_iso_native,dtwalign,dtaidistance,dtw_python,librosa`
  - `uv run --with pytest --with pytest-benchmark --with dtwalign --with scipy python -m pytest -q`
- Validation result:
  - signal-aware `rating_original.py` parity: `99 passed`, long stress skipped
    unless `--run-stress`;
  - native Annex/backend validation: `9 passed`;
  - eligible backend matrix: `39 passed`;
  - full non-stress suite: `110 passed`, `61 skipped`.
- Benchmark result before native scorer optimizations:
  - `local_iso_native`: first-use `0.0162s`, steady median `0.0154s`;
  - `dtw_python`: first-use `0.4457s`, steady median `0.2618s`;
  - `librosa`: first-use `0.9754s`, steady median `0.2526s`.
- Signal-family benchmark before native scorer optimizations:
  - `short_sine_noise_129`: `20.095 us`;
  - `annex_like_sine_amp_offset_1430`: `1.518 ms`;
  - `long_smooth_chirp_8192`: `48.964 ms`;
  - `long_noisy_gaussian_8192`: `99.921 ms`;
  - `long_sparse_spikes_8192`: `47.854 ms`.
- Conclusion: the full native scorer is correct against Annex and
  non-degenerate `rating_original.py` parity cases. Perfectly affine ramps and
  exact square-wave ties exposed library/numerical tie artifacts, so the shared
  signal generator keeps the family shape but avoids strict oracle dependence
  on NumPy/BLAS last-bit behavior.
- Next hypothesis: phase alignment still repeats sum/square work per shift; a
  prefix-sum phase path should improve all signal families without changing the
  scorer API.

## 2026-06-16 13:04 KST - Native Scorer Optimization Iterations

- Git status: checkpoint commit `5e29069`; accepted native scorer optimization
  changes remain uncommitted.
- Hypothesis: optimize one native hotspot at a time and accept only changes that
  keep parity/Annex green and avoid signal-family regressions.
- Files changed:
  - `src/iso18571_native/_core.cpp`;
  - `docs/iso18571-dtw-backends.md`;
  - `AGENTS.md`;
  - `docs/iso18571-dtw-experiment-log.md`.
- Commands:
  - `uv pip install -e .`
  - `uv run --with pytest --with pytest-benchmark --with dtwalign --with scipy python -m pytest -q tests/test_rating_original_parity.py tests/test_iso18571_backends.py --iso18571-backends local_iso_native`
  - `uv run --with pytest --with pytest-benchmark python tools/iso18571/run_backend_benchmarks.py --output-dir .benchmarks/iso18571-phase-prefix --backends local_iso_native --max-time 5 --min-rounds 5`
  - `uv run --with pytest --with pytest-benchmark python -m pytest -q tests/test_iso18571_signal_benchmarks.py --run-stress --benchmark-warmup off --benchmark-min-rounds 3 --benchmark-max-time 3 --benchmark-json .benchmarks/iso18571-phase-prefix/signal_family_native.json`
  - same benchmark commands for `.benchmarks/iso18571-dtw-loop`,
    `.benchmarks/iso18571-dtw-valid-simplified`,
    `.benchmarks/iso18571-dtw-branchless-only`, and
    `.benchmarks/iso18571-contiguous-magnitude`;
  - targeted long stress timing for `sparse_spikes`, `chirp`, and
    `gaussian_noise` at `16384` and `32768` samples;
  - final full non-stress suite and eligible backend matrix.
- Validation result:
  - optimization validation gate: `108 passed`, `56 skipped`;
  - final full non-stress suite: `110 passed`, `61 skipped`;
  - final eligible backend matrix: `39 passed`.
  - full long native stress sweep:
    `56 passed, 99 deselected` for
    `tests/test_rating_original_parity.py --run-stress -k native_scorer_handles_long_signal_families`.
- Accepted optimization 1, phase-prefix correlation:
  - replaces repeated sum/square passes with prefix sums plus one product pass
    per shift, falling back to the two-pass NumPy-like correlation for short or
    near-zero-variance windows;
  - Annex steady median improved from `15.414 ms` to `10.494 ms`;
  - signal medians improved to `17.409 us`, `1.027 ms`, `32.622 ms`,
    `83.746 ms`, and `31.721 ms` for the five benchmark cases.
- Rejected optimization, branchless/simplified DTW predecessor loop:
  - branchless plus simplified validity checks improved noisy 8192 from
    `83.746 ms` to `69.081 ms`, but regressed Annex from `10.494 ms` to
    `11.688 ms` and smooth/sparse 8192 to roughly `37-40 ms`;
  - branchless-only also regressed Annex and all signal rows;
  - reverted under the no-signal-family-regression rule.
- Accepted optimization 2, contiguous magnitude input:
  - copies shifted `(n, 2)` value columns to contiguous native buffers before
    full-scorer magnitude DTW when the value view is strided;
  - Annex steady median improved from `10.494 ms` to `10.364 ms`;
  - final signal medians: `short_sine_noise_129` `16.465 us`,
    `annex_like_sine_amp_offset_1430` `1.020 ms`,
    `long_smooth_chirp_8192` `32.487 ms`,
    `long_noisy_gaussian_8192` `83.732 ms`,
    `long_sparse_spikes_8192` `31.715 ms`.
- Targeted long stress result:
  - `sparse_spikes`: `139.527 ms` at `16384`, `552.897 ms` at `32768`;
  - `chirp`: `140.624 ms` at `16384`, `559.584 ms` at `32768`;
  - `gaussian_noise`: `341.641 ms` at `16384`, `1352.243 ms` at `32768`.
- Conclusion: the accepted native full scorer beats both production comparison
  targets and is sub-100 ms for the 8192-point signal benchmark set. Exact
  32768-point Gaussian DTW remains above one second with the current
  single-threaded exact Sakoe-Chiba dynamic program.
- Next hypothesis: further 32k improvements require a larger DTW implementation
  change, likely band-row memory/layout specialization, SIMD-friendly min
  selection with signal-aware dispatch, or parallelization, each gated by the
  same parity, Annex, and signal-family benchmarks.

## 2026-06-16 13:47 KST - Test Hygiene, Generated Fixed-Signal Annexes, And Parallel Threshold BFS

- Git status: checkpoint commits `5e29069` and `c99081f` exist; the test
  hygiene, generated fixed-signal Annex, and experimental DTW variant changes
  are uncommitted at the start of this batch.
- Hypothesis: stress and benchmark coverage should move from a standalone
  signal-family benchmark file into generated Annex-shaped cases, so official
  Annex validation and synthetic fixed-signal coverage share the same test and
  benchmark harness. Pytest outcomes should use only `assert`/explicit raise;
  category selection should be marker deselection, not runtime skips.
- Files changed:
  - `pytest.ini`;
  - `tests/conftest.py`;
  - `tests/iso18571_annex.py`;
  - `tests/test_rating_original_parity.py`;
  - `tests/test_iso18571_backends.py`;
  - `tests/test_iso18571_benchmarks.py`;
  - removed `tests/test_iso18571_signal_benchmarks.py`;
  - added `tests/test_pytest_policy.py`;
  - added `tests/test_iso18571_threshold_benchmarks.py`;
  - `src/iso18571_native/_core.cpp`;
  - `setup.py`;
  - `tools/iso18571/run_backend_benchmarks.py`;
  - `tools/iso18571/summarize_benchmarks.py`;
  - added `tools/iso18571/analyze_parallel_threshold.py`;
  - `AGENTS.md`;
  - `docs/iso18571-dtw-backends.md`.
- Commands:
  - `uv pip install -e .`
  - `rg -n "pytest\\.(skip|fail|importorskip|raises|xfail)|@pytest\\.mark\\.(skip|xfail)" tests pytest.ini`
  - `rg -n "^\\s*return\\s*$" tests`
  - `uv run --with pytest --with pytest-benchmark python -m pytest -q`
  - `uv run --with pytest --with pytest-benchmark --with dtwalign --with scipy python -m pytest -q tests/test_rating_original_parity.py -o addopts= -m oracle`
  - `uv run --with pytest --with pytest-benchmark python -m pytest -q tests/test_rating_original_parity.py -o addopts= -m stress`
  - `uv run --with pytest --with pytest-benchmark --with dtwalign --with dtaidistance --with dtw-python --with librosa --with scipy python -m pytest -q tests/test_iso18571_backends.py --iso18571-backends local_iso_numpy,local_iso_native,dtwalign,dtaidistance,dtw_python,librosa`
  - `uv run --with pytest --with pytest-benchmark python tools/iso18571/run_backend_benchmarks.py --output-dir .benchmarks/iso18571-annex-fixed-smoke --backends local_iso_native --max-time 1 --min-rounds 3`
  - `uv run python tools/iso18571/summarize_benchmarks.py .benchmarks/iso18571-annex-fixed-smoke/*.json`
  - `uv run --with pytest --with pytest-benchmark python -m pytest -q tests/test_iso18571_threshold_benchmarks.py -o addopts= -m "benchmark and not threshold" --benchmark-warmup off --benchmark-min-rounds 3 --benchmark-max-time 3 --benchmark-json .benchmarks/iso18571-layout-bfs/layout.json`
  - `uv run --with pytest --with pytest-benchmark python -m pytest -q tests/test_iso18571_threshold_benchmarks.py -o addopts= -m threshold --benchmark-warmup off --benchmark-min-rounds 1 --benchmark-max-time 0.1 --benchmark-json .benchmarks/iso18571-threshold-full/threshold.json`
  - `uv run python tools/iso18571/analyze_parallel_threshold.py .benchmarks/iso18571-threshold-full/threshold.json`
- Validation result:
  - forbidden pytest outcome API scan found no matches;
  - silent bare-return scan in `tests/` found no matches;
  - default suite: `12 passed, 157 deselected`, no skips;
  - oracle fixed-signal Annex parity: `1 passed, 2 deselected`, no skips;
  - long fixed-signal Annex stress: `1 passed, 2 deselected`, no skips;
  - eligible backend matrix: `20 passed`, no skips;
  - private native variant invariant test passed for `serial_current`,
    `contiguous_serial`, `band_row`, `bitpacked_direction`, and
    `diagonal_parallel`.
- Benchmark result:
  - local native official Annex smoke: first-use `0.0111s`, steady
    `0.0104s`;
  - generated fixed-signal Annex pass: `0.1495s`;
  - layout BFS medians at 8192 samples rejected `band_row` and
    `bitpacked_direction`: they were slower than `serial_current` across
    chirp, sparse-spike, and Gaussian-noise families;
  - full diagonal-parallel threshold matrix rejected every tested combination
    of `n` in `1430, 4096, 8192, 12288, 16384, 24576, 32768` and thread counts
    `2, 4, 8`; the reusable-barrier anti-diagonal implementation never met
    the 10% faster/no-family-regression threshold.
- Conclusion:
  - fixed signal coverage now lives as generated Annex-shaped cases, with
    normal oracle parity, long stress, and benchmark fixtures sharing the Annex
    loader conventions;
  - production remains on the current serial native DTW kernel;
  - the private experimental hook is useful for BFS benchmarking but is not
    exported from `iso18571_native/__init__.py`.
- Next hypothesis: the next speed search should avoid per-diagonal global
  barriers and focus on larger-grain parallelism, cache-aware tiling inside the
  band, or a compiled scorer-level split that preserves exact ISO predecessor
  order while reducing synchronization frequency.

## 2026-06-16 14:12 KST - Analytic Phase Annexes And Native Regime Atlas

- Git status: clean at start of batch on `master` after commit `9daa63b`.
- Hypothesis: performance work should map which exact variant is better by
  data-size regime rather than accepting or rejecting variants globally.
  Analytic phase-shift cases should be part of the generated Annex coverage,
  and parity should compare `rating_original`, `local_iso_native`,
  `dtw_python`, and `librosa`.
- Files changed:
  - `tests/iso18571_signals.py`;
  - `tests/iso18571_annex.py`;
  - `tests/conftest.py`;
  - `tests/test_rating_original_parity.py`;
  - `tests/test_iso18571_backends.py`;
  - added `tests/test_iso18571_regime_benchmarks.py`;
  - `rating.py`;
  - `src/iso18571_native/_core.cpp`;
  - added `tools/iso18571/analyze_variant_regimes.py`;
  - `pytest.ini`;
  - `AGENTS.md`;
  - `docs/iso18571-dtw-backends.md`;
  - `docs/iso18571-dtw-experiment-log.md`.
- Commands:
  - `uv pip install -e .`
  - `uv run --with pytest --with pytest-benchmark python -m pytest -q tests/test_iso18571_backends.py --iso18571-backends local_iso_native -k "score_component_variants or experimental_variants"`
  - `uv run --with pytest --with pytest-benchmark python -m pytest -q`
  - `uv run --with pytest --with pytest-benchmark --with dtwalign --with scipy --with dtw-python --with librosa python -m pytest -q tests/test_rating_original_parity.py -o addopts= -m oracle`
  - `uv run --with pytest --with pytest-benchmark --with dtwalign --with dtaidistance --with dtw-python --with librosa --with scipy python -m pytest -q tests/test_iso18571_backends.py --iso18571-backends local_iso_numpy,local_iso_native,dtwalign,dtaidistance,dtw_python,librosa`
  - `uv run --with pytest --with pytest-benchmark python -m pytest -q tests/test_rating_original_parity.py -o addopts= -m stress`
  - `uv run --with pytest --with pytest-benchmark python -m pytest -q tests/test_iso18571_regime_benchmarks.py -o addopts= -m regime -k "sine_amp_offset__n64" --benchmark-warmup off --benchmark-min-rounds 1 --benchmark-max-time 0.1 --benchmark-json .benchmarks/iso18571-regime-smoke/regime.json`
  - `uv run python tools/iso18571/analyze_variant_regimes.py .benchmarks/iso18571-regime-smoke/regime.json`
  - `rg -n "pytest\\.(skip|fail|importorskip|raises|xfail)|@pytest\\.mark\\.(skip|xfail)" tests pytest.ini`
  - `rg -n "^\\s*return\\s*$" tests`
- Validation result:
  - default suite: `13 passed, 24637 deselected`, no skips;
  - cross-backend generated Annex parity: `1 passed, 2 deselected`, no skips;
  - long generated Annex native stress, including 65536-point cases:
    `1 passed, 2 deselected`;
  - eligible backend matrix: `21 passed`;
  - private scorer variant smoke: `2 passed, 9 deselected`;
  - forbidden pytest outcome API scan and silent-return scan found no matches.
- Correctness finding:
  - cross-backend parity exposed that the shared Python scorer could turn
    near-zero-variance constant correlations into `1.0` while
    `rating_original` returned `NaN`;
  - `rating.py` now falls back to `np.corrcoef` for numerically degenerate
    shifted-correlation windows, preserving oracle behavior for constants and
    tiny phase-shift cases.
- Benchmark/result:
  - added analytic phase-shift generated Annex families:
    multitone shifts, chirp shift, pulse shift, and smooth-step shift;
  - added `_score_components_variant` as a private native hook for full-scorer
    variants;
  - added exact native variants for DTW range precompute, index-incremental
    direction indexing, compact direction storage, shared shifted workspace,
    phase dual-product, fused slope, and blocked wavefront DTW;
  - regime smoke for `sine_amp_offset`, `n=64` ran 340 benchmark rows;
    current serial variants were preferred/competitive in the tiny regime,
    while blocked/threaded variants were dominated there, as expected.
- Conclusion:
  - the implementation now supports performance-regime mapping by
    `effective_n` and DTW cell count instead of global accept/reject decisions;
  - production `score_components` remains unchanged until a stable
    data-size-dispatch policy is proven across families;
  - blocked wavefront is now a real kernel behind the private variant hook, but
    it still needs large-regime atlas runs to find its crossover.
- Next hypothesis: run a filtered atlas over `8192`, `16384`, `32768`, and
  `65536` for the phase/chirp/noise families to characterize where blocked
  wavefront starts beating serial, then use the analyzer to propose an
  `effective_n` dispatch threshold only if it is stable across families.

## 2026-06-16 14:34 KST - Large-Regime Blocked Wavefront Atlas

- Git status: clean at start of batch on `master` after commit `a72939c`.
- Hypothesis: blocked wavefront parallel DTW should become better than serial
  at sufficiently large effective sequence lengths, and the crossover should be
  characterized by regime rather than by a global accept/reject decision.
- Files changed:
  - `tests/test_iso18571_regime_benchmarks.py`;
  - `tools/iso18571/analyze_variant_regimes.py`;
  - `docs/iso18571-dtw-backends.md`;
  - `docs/iso18571-dtw-experiment-log.md`.
- Commands:
  - `ISO18571_REGIME_FAMILIES=chirp,gaussian_noise,phase_multitone_shift_020,phase_chirp_shift_050,phase_pulses_shift_100,phase_smooth_step_shift_180 ISO18571_REGIME_LENGTHS=8192,16384,32768,65536 ISO18571_REGIME_THREADS=2,4,8 ISO18571_REGIME_VARIANTS=dtw_current+reduce_none+parallel_none,dtw_current+reduce_none+blocked64,dtw_current+reduce_none+blocked128,dtw_current+reduce_none+blocked256,dtw_current+reduce_none+blocked512 uv run --with pytest --with pytest-benchmark python -m pytest -q tests/test_iso18571_regime_benchmarks.py -o addopts= -m regime --benchmark-warmup off --benchmark-min-rounds 1 --benchmark-max-time 0.05 --benchmark-quiet --benchmark-json .benchmarks/iso18571-regime-large/regime_blocked_reduce_none.json`
  - `ISO18571_REGIME_FAMILIES=chirp,gaussian_noise,phase_multitone_shift_020,phase_chirp_shift_050,phase_pulses_shift_100,phase_smooth_step_shift_180 ISO18571_REGIME_LENGTHS=8192,16384,32768,65536 ISO18571_REGIME_THREADS=4,8 ISO18571_REGIME_VARIANTS=dtw_current+all_reductions+parallel_none,dtw_current+all_reductions+blocked128,dtw_current+all_reductions+blocked256 uv run --with pytest --with pytest-benchmark python -m pytest -q tests/test_iso18571_regime_benchmarks.py -o addopts= -m regime --benchmark-warmup off --benchmark-min-rounds 1 --benchmark-max-time 0.05 --benchmark-quiet --benchmark-json .benchmarks/iso18571-regime-large/regime_blocked_all_reductions.json`
  - `uv run python tools/iso18571/analyze_variant_regimes.py .benchmarks/iso18571-regime-large/regime_blocked_reduce_none.json .benchmarks/iso18571-regime-large/regime_blocked_all_reductions.json`
- Validation result:
  - reduce-none blocked atlas: `312 passed`;
  - all-reductions blocked atlas: `120 passed`;
  - all benchmark rows also performed exact score parity checks against
    production native scores before timing.
- Benchmark/result:
  - `dtw_current+all_reductions+blocked128` with 8 threads was the best
    measured variant for every measured family and nominal length in the
    phase/chirp/noise slice.
  - Best-case table versus current serial production baseline:
    - `n=8192`: ratios ranged from `0.238` for Gaussian noise to `0.706` for
      smooth-step phase shift;
    - `n=16384`: ratios ranged from `0.240` to `0.605`;
    - `n=32768`: ratios ranged from `0.203` to `0.539`;
    - `n=65536`: ratios ranged from `0.199` to `0.509`.
  - Analyzer dispatch candidates:
    - stable threshold: `effective_n >= 6717`, `cells >= 9020931`,
      variant `dtw_current+all_reductions+blocked128`, threads `8`,
      covered rows `24`, mean ratio to current serial `0.490`;
    - secondary threshold: `effective_n >= 16384`, `cells >= 53690368`,
      variant `dtw_current+all_reductions+blocked256`, threads `8`,
      covered rows `13`, mean ratio `0.441`.
- Conclusion:
  - blocked wavefront parallelism does benefit at large enough sequence sizes;
  - for the measured phase/chirp/noise slice, the earliest stable threshold is
    `effective_n >= 6717` using `blocked128` with 8 threads plus all reduction
    optimizations;
  - this is evidence for a possible dispatch policy, but production should not
    switch until the threshold is rechecked on the official Annex cases and the
    broader fixed-signal benchmark set.
- Next hypothesis:
  - add a production-dispatch prototype guarded by params, run official Annex
    and generated fixed/phase parity, then run a broader benchmark to make sure
    the `effective_n >= 6717` threshold does not regress smaller or
    non-phase/chirp/noise cases.

## 2026-06-16 15:02 KST - SIMD Enum Atlas Scaffold And Assembly Report

- Git status: clean at start of batch on `master` after commit `fd2d023`.
- Hypothesis: SIMD should be added as an explicit experiment axis, but native
  execution should use typed enum specs rather than parsing variant strings in
  C++. Scalar fallback must remain safe for prebuilt wheels.
- Files changed:
  - native extension source and build setup;
  - backend, threshold, and regime benchmark tests;
  - regime analyzer and assembly-report tools;
  - ISO18571 backend docs, agent notes, and this experiment log.
- Commands:
  - `uv pip install -e .`
  - `uv run --with pytest --with pytest-benchmark python -m pytest -q tests/test_iso18571_backends.py`
  - `uv run --with pytest --with pytest-benchmark python -m pytest -q tests/test_iso18571_threshold_benchmarks.py -o addopts= -m "benchmark and not threshold" --benchmark-disable`
  - `ISO18571_REGIME_FAMILIES=chirp ISO18571_REGIME_LENGTHS=64 ISO18571_REGIME_THREADS=1 ISO18571_REGIME_VARIANTS=dtw_current+reduce_none+parallel_none,dtw_current+all_reductions+parallel_none ISO18571_REGIME_SIMD_LEVELS=scalar,auto uv run --with pytest --with pytest-benchmark python -m pytest -q tests/test_iso18571_regime_benchmarks.py -o addopts= -m regime --benchmark-disable`
  - `mkdir -p .benchmarks/iso18571-regime-simd-smoke && ISO18571_REGIME_FAMILIES=chirp ISO18571_REGIME_LENGTHS=64 ISO18571_REGIME_THREADS=1 ISO18571_REGIME_VARIANTS=dtw_current+reduce_none+parallel_none,dtw_current+all_reductions+parallel_none ISO18571_REGIME_SIMD_LEVELS=scalar,auto uv run --with pytest --with pytest-benchmark python -m pytest -q tests/test_iso18571_regime_benchmarks.py -o addopts= -m regime --benchmark-warmup off --benchmark-min-rounds 1 --benchmark-max-time 0.01 --benchmark-quiet --benchmark-json .benchmarks/iso18571-regime-simd-smoke/regime.json`
  - `uv run python tools/iso18571/analyze_variant_regimes.py .benchmarks/iso18571-regime-simd-smoke/regime.json`
  - `uv run python tools/iso18571/emit_native_assembly.py --output-dir .benchmarks/iso18571-asm`
  - `uv run python tools/iso18571/report_assembly_wrinkles.py .benchmarks/iso18571-asm`
- Validation result:
  - backend tests: `13 passed`;
  - threshold benchmark plumbing smoke: `12 passed, 140 deselected`;
  - regime enum/SIMD plumbing smoke: `4 passed`;
  - benchmark JSON analyzer smoke completed and reported requested/selected SIMD
    columns.
- Benchmark/result:
  - tiny smoke at `n=64` selected `avx2_fma` for `simd_auto` on this machine;
  - ratios in the tiny smoke were within noise and are not production evidence.
- Assembly observations:
  - emitted `.s` files for scalar, SSE2, AVX2, and AVX2+FMA units under
    `.benchmarks/iso18571-asm`;
  - AVX2 and SSE2 units showed vector instructions and no calls in the simple
    scan;
  - AVX2+FMA showed AVX/YMM instructions but no FMA instructions for the
    current gradient target;
  - scalar unit had calls and stack traffic that should be inspected before any
    future compiler-wrinkle optimization pass.
- Conclusion:
  - enum-based private hooks and SIMD metadata are in place;
  - production `score_components`, `magnitude_ratio`, and `warp_path` remain
    unchanged;
  - AVX-512 is not implemented.
- Next hypothesis:
  - run the focused SIMD atlas over the agreed families and lengths, then decide
    whether SIMD adds a stable size-regime dispatch rule or remains
    experimental.

## 2026-06-16 15:41 KST - Focused SIMD Atlas

- Git status: dirty with the SIMD enum implementation in progress.
- Hypothesis: adding scalar/SSE2/AVX2/AVX2+FMA/auto as an explicit atlas axis
  should reveal whether SIMD changes the preferred size-regime policy beyond
  the existing blocked-wavefront result.
- Commands:
  - `ISO18571_REGIME_FAMILIES=chirp,gaussian_noise,sparse_spikes,phase_multitone_shift_020,phase_chirp_shift_050,phase_smooth_step_shift_180 ISO18571_REGIME_LENGTHS=512,1430,4096,8192,16384,32768,65536 ISO18571_REGIME_THREADS=1,2,4,8 ISO18571_REGIME_VARIANTS=dtw_current+reduce_none+parallel_none,dtw_current+all_reductions+parallel_none,dtw_current+all_reductions+blocked128 ISO18571_REGIME_SIMD_LEVELS=scalar,sse2,avx2,avx2_fma,auto uv run --with pytest --with pytest-benchmark python -m pytest -q tests/test_iso18571_regime_benchmarks.py -o addopts= -m regime --benchmark-warmup off --benchmark-min-rounds 1 --benchmark-max-time 0.02 --benchmark-quiet --benchmark-json .benchmarks/iso18571-regime-simd-focused/regime.json`
  - `uv run python tools/iso18571/analyze_variant_regimes.py .benchmarks/iso18571-regime-simd-focused/regime.json > .benchmarks/iso18571-regime-simd-focused/summary.md`
- Validation result:
  - focused SIMD atlas: `1260 passed in 2015.53s`;
  - every timed row performed score parity checks against production native
    scores before benchmarking.
- Benchmark/result:
  - `simd_auto` selected `avx2_fma` on this machine with no fallback;
  - no tested SIMD level changed the main conclusion for short/medium cases;
  - the large and very-large win remains `all_reductions + blocked128` with 8
    threads;
  - dispatch candidates from this focused matrix:
    - earliest stable candidate: `effective_n >= 16286`,
      `cells >= 53043502`, variant
      `dtw_current+all_reductions+blocked128+simd_auto`, 8 threads, 15 rows,
      mean ratio `0.432`;
    - `avx2_fma` candidate: `effective_n >= 26870`, 12 rows, mean ratio
      `0.439`;
    - `sse2` candidate: `effective_n >= 32113`, 10 rows, mean ratio `0.425`.
- Conclusion:
  - SIMD is now represented in the atlas, but with the current conservative
    slope-gradient target it is a secondary effect compared with blocked
    wavefront parallelism;
  - production dispatch should still wait for a deliberate policy change, but
    the best current measured candidate is
    `effective_n >= 16286` to `all_reductions + blocked128 + simd_auto` with 8
    threads.
- Next hypothesis:
  - if more SIMD speed is desired, the next optimization batch should target
    phase-product reductions or DTW-local-cost staging, with explicit attention
    to reduction-order parity.

## 2026-06-16 15:41 KST - Final SIMD Implementation Validation

- Commands:
  - `uv run --with pytest --with pytest-benchmark python -m pytest -q`
  - `uv run --with pytest --with pytest-benchmark --with dtwalign --with dtaidistance --with dtw-python --with librosa --with scipy python -m pytest -q tests/test_iso18571_backends.py --iso18571-backends local_iso_numpy,local_iso_native,dtwalign,dtaidistance,dtw_python,librosa`
  - `uv run --with pytest --with pytest-benchmark --with dtwalign --with dtw-python --with librosa --with scipy python -m pytest -q tests/test_rating_original_parity.py -o addopts= -m oracle`
  - `uv run --with pytest --with pytest-benchmark --with dtw-python --with librosa --with scipy python -m pytest -q tests/test_rating_original_parity.py -o addopts= -m stress`
  - `uv build --wheel`
  - `uv run python tools/iso18571/wheel_smoke.py`
  - `uv run python tools/iso18571/emit_native_assembly.py --output-dir .benchmarks/iso18571-asm && uv run python tools/iso18571/report_assembly_wrinkles.py .benchmarks/iso18571-asm`
- Validation result:
  - default suite: `15 passed, 122557 deselected`;
  - eligible backend matrix: `23 passed`;
  - oracle parity: `1 passed, 2 deselected` with expected degenerate-signal
    runtime warnings;
  - long stress parity: `1 passed, 2 deselected`;
  - wheel build and wheel smoke succeeded.
- Assembly observations:
  - final report repeated the earlier observation: AVX2+FMA emits AVX/YMM for
    the current SIMD gradient target but no FMA instructions;
  - scalar assembly contains calls and stack traffic worth inspecting before a
    future compiler-focused optimization pass.

## 2026-06-16 16:08 KST - CMake, Ruff, Warning Hygiene, And SIMD Dispatch Cleanup

- Git status: dirty at start of batch with the SIMD enum implementation and the
  root `default.py` move to ignored `ref/` in progress.
- Hypothesis: scikit-build/CMake can replace `setup.py`, Ruff can become a
  normal commit gate, and warning handling can be explicit without changing the
  public scorer or production dispatch policy.
- Files changed:
  - CMake/scikit-build project configuration;
  - native enum variant bindings and test helpers;
  - pytest policy tests and oracle parity warning handling;
  - AGENTS notes and this experiment log.
- Commands:
  - `uv run --with ruff ruff check --fix .`
  - `uv run --with ruff ruff format .`
  - `uv run --with ruff ruff check .`
  - `uv run --with ruff ruff format --check .`
  - `uv build --wheel`
  - `uv run --with dist/euroncap-0.1.0-cp313-cp313-linux_x86_64.whl python tools/iso18571/wheel_smoke.py`
  - `uv run --with pybind11 python tools/iso18571/emit_native_assembly.py --output-dir .benchmarks/iso18571-asm && uv run python tools/iso18571/report_assembly_wrinkles.py .benchmarks/iso18571-asm`
  - `uv pip install -e .`
  - `uv run --with pytest --with pytest-benchmark python -m pytest -q`
  - `uv run --with pytest --with pytest-benchmark --with scipy --with dtwalign --with dtaidistance --with dtw-python --with librosa python -m pytest -q tests/test_iso18571_backends.py --iso18571-backends local_iso_numpy,local_iso_native,dtwalign,dtaidistance,dtw_python,librosa`
  - `uv run --with pytest --with pytest-benchmark --with scipy --with dtwalign --with dtw-python --with librosa python -m pytest -q tests/test_rating_original_parity.py -o addopts= -m oracle`
  - `uv run --with pytest --with pytest-benchmark python -m pytest -q tests/test_rating_original_parity.py -o addopts= -m stress`
  - `uv build --wheel`
  - `uv run --with dist/euroncap-0.1.0-cp313-cp313-linux_x86_64.whl python tools/iso18571/wheel_smoke.py`
  - `uv run python tools/iso18571/emit_native_assembly.py --output-dir .benchmarks/iso18571-asm && uv run python tools/iso18571/report_assembly_wrinkles.py .benchmarks/iso18571-asm`
- Validation result:
  - Ruff fixed 11 mechanical issues, then `ruff check` and `ruff format
    --check` passed;
  - editable CMake build succeeded;
  - default pytest: `17 passed, 122557 deselected`;
  - eligible backend matrix: `23 passed`;
  - oracle parity: `1 passed, 2 deselected`;
  - long generated native stress: `1 passed, 2 deselected`;
  - wheel build and smoke test succeeded.
- Assembly observations:
  - `simd_avx2.s`: vector/YMM instructions present, no obvious wrinkle from the
    simple scan;
  - `simd_avx2_fma.s`: vector/YMM instructions present, but no FMA instructions
    emitted for the current gradient target;
  - `simd_scalar.s`: calls and stack traffic remain worth inspecting in a future
    compiler-focused optimization pass;
  - `simd_sse2.s`: no obvious wrinkle from the simple scan.
- Conclusion:
  - `setup.py` is removed and CMake/scikit-build is the authoritative native
    build path;
  - root `default.py` is deleted from package scope and treated as ignored
    reference material under `ref/`;
  - C++ execution paths use enum specs only for native variants;
  - `SimdLevel.Auto` is runtime auto-dispatch in the private enum variant path,
    while public `score_components()` remains on the existing production path;
  - known degenerate numeric oracle warnings are asserted explicitly.
- Next hypothesis:
  - continue optimization work by targeting SIMD work that can affect more than
    contiguous slope-gradient generation, likely phase-product reductions or
    DTW-local-cost staging, while preserving reduction-order parity.

## 2026-06-16 17:08 KST - Package API Cleanup And No-Auto Thread Atlas

- Git status: dirty at start of batch with package/API cleanup in progress.
- Hypothesis:
  - removing the root `rating.py` shim and using `iso18571.ISO18571` directly
    will reduce ambiguity without changing scorer behavior;
  - keeping `SimdLevel.Auto` out of the matrix will make the SIMD/thread atlas
    easier to interpret;
  - trying 12/16/24 threads in addition to 8 should reveal size regimes rather
    than a single universal thread count.
- Files changed:
  - moved the scorer implementation to `iso18571/rating.py` and exported
    `ISO18571` from `iso18571`;
  - removed root `rating.py` and root `rating_original.py`;
  - loaded the oracle explicitly from ignored `ref/rating_original.py`;
  - updated `main.py` as the operator CLI surface;
  - exported native enum/spec experiment APIs from `iso18571_native`;
  - removed weak one-use test wrappers and kept only shared oracle/warning
    helpers;
  - removed `simd_auto` from regime benchmark defaults and made explicit auto
    use fail in matrix specs;
  - added warning-as-error pytest policy and extended the no-inline-import scan
    across project Python;
  - enabled C++ warning flags and emitted `_core.cpp` assembly with a narrow
    pybind11 pedantic-warning suppression.
- Commands:
  - `uv run --with ruff ruff check --fix .`
  - `uv run --with ruff ruff format .`
  - `uv run --with ruff ruff check .`
  - `uv run --with ruff ruff format --check .`
  - `uv pip install -e .`
  - `uv run --with pytest --with pytest-benchmark python -m pytest -q`
  - `uv run --with pytest --with pytest-benchmark --with scipy --with dtwalign --with dtaidistance --with dtw-python --with librosa python -m pytest -q tests/test_iso18571_backends.py --iso18571-backends local_iso_numpy,local_iso_native,dtwalign,dtaidistance,dtw_python,librosa`
  - `uv run --with pytest --with pytest-benchmark --with scipy --with dtwalign --with dtw-python --with librosa python -m pytest -q tests/test_rating_original_parity.py -o addopts= -m oracle`
  - `uv run --with pytest --with pytest-benchmark python -m pytest -q tests/test_rating_original_parity.py -o addopts= -m stress`
  - `uv build --wheel`
  - `uv run --with dist/euroncap-0.1.0-cp313-cp313-linux_x86_64.whl python tools/iso18571/wheel_smoke.py`
  - `uv run --with pybind11 python tools/iso18571/emit_native_assembly.py --output-dir .benchmarks/iso18571-asm && uv run python tools/iso18571/report_assembly_wrinkles.py .benchmarks/iso18571-asm`
  - `ISO18571_REGIME_FAMILIES=chirp,gaussian_noise,sparse_spikes,phase_multitone_shift_020,phase_chirp_shift_050,phase_smooth_step_shift_180 ISO18571_REGIME_LENGTHS=4096,8192,12288,16384,32768,65536 ISO18571_REGIME_THREADS=1,2,4,8,12,16,24 ISO18571_REGIME_VARIANTS=dtw_current+reduce_none+parallel_none,dtw_current+all_reductions+parallel_none,dtw_current+all_reductions+blocked128 ISO18571_REGIME_SIMD_LEVELS=scalar,sse2,avx2,avx2_fma uv run --with pytest --with pytest-benchmark python -m pytest -q tests/test_iso18571_regime_benchmarks.py -o addopts= -m regime --benchmark-warmup off --benchmark-min-rounds 1 --benchmark-max-time 0.02 --benchmark-quiet --benchmark-json .benchmarks/iso18571-regime-no-auto-thread-ceiling/regime.json`
  - `uv run python tools/iso18571/analyze_variant_regimes.py .benchmarks/iso18571-regime-no-auto-thread-ceiling/regime.json > .benchmarks/iso18571-regime-no-auto-thread-ceiling/summary.md`
- Validation result:
  - Ruff fixed 4 mechanical issues, then `ruff check` and `ruff format --check`
    passed;
  - editable CMake build succeeded without visible compiler warnings;
  - default pytest: `17 passed, 111622 deselected`;
  - eligible backend matrix: `23 passed`;
  - oracle parity from `ref/rating_original.py`: `1 passed, 2 deselected`;
  - long generated native stress: `1 passed, 2 deselected`;
  - wheel build and wheel smoke succeeded;
  - assembly emission/report succeeded after `_core.cpp` pybind pedantic macro
    warning was handled narrowly;
  - no-auto thread/SIMD atlas: `1296 passed in 2143.33s`.
- Benchmark/result:
  - blocked wavefront with all reductions beat the plain scalar baseline for
    every tested family at input lengths 4096, 8192, 12288, 16384, 32768, and
    65536;
  - scalar baseline wins by input length: `0/6` families at every tested length;
  - best-thread counts by input length:
    - 4096: 12 threads won 3 families, 4 threads won 2, 8 threads won 1;
    - 8192: 8 threads won 4 families, 12 threads won 2;
    - 12288: 12 threads won 5 families, 8 threads won 1;
    - 16384: 12 threads won 3 families, 16 won 2, 8 won 1;
    - 32768: 24 threads won 3 families, 16 won 2, 12 won 1;
    - 65536: 16 threads won 4 families, 24 won 2.
  - mean best-row ratio to scalar baseline improved with size:
    - 4096: `0.690`;
    - 8192: `0.478`;
    - 12288: `0.394`;
    - 16384: `0.360`;
    - 32768: `0.356`;
    - 65536: `0.332`.
  - analyzer dispatch candidates started at `effective_n >= 12288` with
    `dtw_current+all_reductions+blocked128+simd_scalar` and 16 threads, but
    family-level winners were mixed enough that this remains an atlas result,
    not a production dispatch policy.
- Assembly observations:
  - `_core.s` is now emitted and contains many pybind-visible calls and stack
    traffic, so manual inspection should distinguish binding code from scorer
    hot loops before drawing conclusions;
  - `simd_avx2.s` and `simd_sse2.s` showed expected vector instructions in the
    simple scan;
  - `simd_avx2_fma.s` still emitted no FMA instructions for the current SIMD
    gradient target, so AVX2+FMA is not yet doing meaningful FMA work.
- Conclusion:
  - the package now exposes `iso18571.ISO18571` without a root `rating.py`
    compatibility shim;
  - `SimdLevel.Auto` remains runtime auto-dispatch for explicit smoke/parity
    checks, but benchmark matrices use only explicit SIMD levels;
  - 8 threads was not the maximum tested: 12, 16, and 24 were tested, and the
    best choice is size-regime dependent;
  - for the tested sub-16k inputs, naive scalar was not best, but the preferred
    parallel thread count was not universal.
- Next hypothesis:
  - repeat the no-auto atlas with a more production-like timing budget before
    enabling size-based dispatch, then target vectorization candidates in
    phase-product reductions, DTW local-cost staging, slope smoothing, and
    magnitude-path accumulation.

## 2026-06-16 17:47 KST - SIMD Hotspot Target Modes

- Git status: clean at start of batch.
- Hypothesis:
  - adding explicit SIMD target modes will let the atlas distinguish phase
    products, DTW local-cost staging, slope smoothing, and magnitude-path
    accumulation without changing public production scoring;
  - phase-product reductions are likely to be the first meaningful target
    because they avoid changing DTW recurrence order and can be scalar-confirmed
    before shift decisions are stored.
- Files changed:
  - native SIMD headers and scalar/SSE2/AVX2/AVX2+FMA units;
  - native variant config, pybind enum exports, and private variant signatures;
  - backend, regime, threshold, and analyzer tests/tools;
  - AGENTS notes, backend docs, and this experiment log.
- Commands:
  - `uv pip install -e .`
  - `uv run --with pytest --with pytest-benchmark python -m pytest -q tests/test_iso18571_backends.py --iso18571-backends local_iso_numpy,local_iso_native`
  - `ISO18571_REGIME_FAMILIES=chirp ISO18571_REGIME_LENGTHS=4096 ISO18571_REGIME_THREADS=1 ISO18571_REGIME_VARIANTS=dtw_current+all_reductions+parallel_none ISO18571_REGIME_SIMD_LEVELS=scalar,avx2 ISO18571_REGIME_SIMD_TARGETS=gradient_only,phase_products,dtw_local_cost,slope_smoothing,magnitude_path uv run --with pytest --with pytest-benchmark python -m pytest -q tests/test_iso18571_regime_benchmarks.py -o addopts= -m regime --benchmark-disable`
  - `mkdir -p .benchmarks/iso18571-simd-target-smoke && ISO18571_REGIME_FAMILIES=chirp,gaussian_noise,sparse_spikes,phase_multitone_shift_020,phase_chirp_shift_050,phase_smooth_step_shift_180 ISO18571_REGIME_LENGTHS=4096,8192 ISO18571_REGIME_THREADS=1,8 ISO18571_REGIME_VARIANTS=dtw_current+reduce_none+parallel_none,dtw_current+all_reductions+blocked128 ISO18571_REGIME_SIMD_LEVELS=scalar,avx2,avx2_fma ISO18571_REGIME_SIMD_TARGETS=gradient_only,phase_products,dtw_local_cost,slope_smoothing,magnitude_path uv run --with pytest --with pytest-benchmark python -m pytest -q tests/test_iso18571_regime_benchmarks.py -o addopts= -m regime --benchmark-warmup off --benchmark-min-rounds 1 --benchmark-max-time 0.01 --benchmark-quiet --benchmark-json .benchmarks/iso18571-simd-target-smoke/regime.json`
  - `uv run python tools/iso18571/analyze_variant_regimes.py .benchmarks/iso18571-simd-target-smoke/regime.json > .benchmarks/iso18571-simd-target-smoke/summary.md`
  - `uv run --with pytest --with pytest-benchmark python -m pytest -q`
  - `uv run --with pytest --with pytest-benchmark --with scipy --with dtwalign --with dtaidistance --with dtw-python --with librosa python -m pytest -q tests/test_iso18571_backends.py --iso18571-backends local_iso_numpy,local_iso_native,dtwalign,dtaidistance,dtw_python,librosa`
  - `uv run --with pytest --with pytest-benchmark --with scipy --with dtwalign --with dtw-python --with librosa python -m pytest -q tests/test_rating_original_parity.py -o addopts= -m oracle`
  - `uv run --with pytest --with pytest-benchmark python -m pytest -q tests/test_rating_original_parity.py -o addopts= -m stress`
  - `uv run --with ruff ruff check --fix .`
  - `uv run --with ruff ruff format .`
  - `uv run --with ruff ruff check .`
  - `uv run --with ruff ruff format --check .`
- Validation result:
  - editable CMake build succeeded without visible compiler warnings;
  - focused native backend tests: `17 passed`;
  - regime target-axis smoke without benchmarking: `10 passed`;
  - measured target smoke atlas: `540 passed in 57.93s`;
  - default pytest after keeping target modes opt-in for regime collection:
    `19 passed, 111622 deselected`;
  - eligible backend matrix: `25 passed`;
  - oracle parity: `1 passed, 2 deselected`;
  - long generated native stress: `1 passed, 2 deselected`;
  - Ruff check/format passed after reformatting two Python files.
  - wheel build and wheel smoke succeeded;
  - assembly emission/report succeeded.
- Implementation notes:
  - `SimdTargetMode` is exported through pybind and `iso18571_native`;
  - private variant hooks now require explicit target mode arguments;
  - phase-product SIMD uses contiguous value workspaces and scalar-confirms any
    candidate within `1e-10` of the current best or any candidate that could
    become the new best;
  - DTW local-cost staging SIMD prepares row/block costs but leaves recurrence
    and predecessor tie decisions scalar;
  - slope smoothing keeps edges scalar and vectorizes only interior 9-point
    windows;
  - magnitude-path accumulation backtracks the exact path, groups long
    contiguous diagonal/vertical/horizontal runs, and falls back to scalar for
    short or irregular runs.
- Benchmark/result:
  - in the 4096/8192 smoke atlas, all target modes preserved parity;
  - the analyzer's first candidate was
    `dtw_current+all_reductions+blocked128+simd_avx2+target_phase_products`
    with 8 threads at `effective_n >= 7782`, mean ratio `0.402` over 5 covered
    rows;
  - several target rows remained unstable or dominated in the small medium slice,
    so this result should guide the next broader atlas rather than production
    dispatch.
- Assembly observations:
  - `_core.s`: 126869 lines, vector instructions visible through inlined code,
    and pybind-visible calls/stack traffic still dominate the simple scan;
  - `simd_avx2.s`: 90 YMM lines and no obvious wrinkle from the simple scan;
  - `simd_avx2_fma.s`: 15 YMM lines and 2 FMA lines, so the phase dot-product
    FMA kernel is now visible in emitted assembly;
  - `simd_sse2.s`: vector instructions present with no obvious wrinkle.
- Conclusion:
  - the SIMD target-mode infrastructure is in place and correctness gates are
    green;
  - production scoring remains unchanged;
  - `phase_products` is the first target worth broadening across larger lengths
    and thread counts.
- Next hypothesis:
  - run a broader `phase_products` atlas across `4096,8192,12288,16384,32768,
    65536` and then test pairwise combinations only if a single target remains
    preferred or competitive across families.

## 2026-06-17 09:02 KST - Direct SIMD Variant Dispatch Checkpoint

- Git status: dirty at start of batch with SIMD dispatch cleanup in progress.
- Hypothesis:
  - replacing private string/spec calls with direct enum-selected monomorphic
    variant functions will lower measured SIMD dispatch overhead while keeping
    public production scoring unchanged;
  - if dispatch overhead was the main reason for SIMD losses, explicit AVX2 and
    AVX2+FMA target modes should become incrementally better than scalar across
    the smoke atlas.
- Files changed:
  - `iso18571_native/__init__.py` now exposes
    `score_variant_function(...)` and `magnitude_variant_function(...)` maps
    over direct monomorphic native variant functions;
  - `_core.cpp` registers SIMD-level and target-mode specific scorer and
    magnitude functions instead of one private string/spec-style execution path;
  - SIMD build definitions were simplified so x86_64/AMD64 builds always
    compile SSE2, AVX2, and AVX2+FMA translation units;
  - tests and benchmarks now resolve SIMD once before timing and call the
    selected function directly;
  - the temporary pytest policy scanner was removed by request.
- Commands:
  - `uv pip install -e .`
  - `uv run --with pytest --with pytest-benchmark python -m pytest -q tests/test_iso18571_backends.py --iso18571-backends local_iso_numpy,local_iso_native`
  - `ISO18571_REGIME_FAMILIES=chirp ISO18571_REGIME_LENGTHS=4096 ISO18571_REGIME_THREADS=1 ISO18571_REGIME_VARIANTS=dtw_current+all_reductions+parallel_none ISO18571_REGIME_SIMD_LEVELS=scalar,avx2 ISO18571_REGIME_SIMD_TARGETS=gradient_only,phase_products,dtw_local_cost,slope_smoothing,magnitude_path uv run --with pytest --with pytest-benchmark python -m pytest -q tests/test_iso18571_regime_benchmarks.py -o addopts= -m regime --benchmark-disable`
  - `mkdir -p .benchmarks/iso18571-highway-dispatch-smoke && ISO18571_REGIME_FAMILIES=chirp,gaussian_noise,sparse_spikes,phase_multitone_shift_020,phase_chirp_shift_050,phase_smooth_step_shift_180 ISO18571_REGIME_LENGTHS=4096,8192 ISO18571_REGIME_THREADS=1,8 ISO18571_REGIME_VARIANTS=dtw_current+reduce_none+parallel_none,dtw_current+all_reductions+blocked128 ISO18571_REGIME_SIMD_LEVELS=scalar,avx2,avx2_fma ISO18571_REGIME_SIMD_TARGETS=gradient_only,phase_products,dtw_local_cost,slope_smoothing,magnitude_path uv run --with pytest --with pytest-benchmark python -m pytest -q tests/test_iso18571_regime_benchmarks.py -o addopts= -m regime --benchmark-warmup off --benchmark-min-rounds 1 --benchmark-max-time 0.01 --benchmark-quiet --benchmark-json .benchmarks/iso18571-highway-dispatch-smoke/regime.json`
  - `uv run --with pybind11 python tools/iso18571/emit_native_assembly.py --output-dir .benchmarks/iso18571-highway-asm`
  - `uv run python tools/iso18571/report_assembly_wrinkles.py .benchmarks/iso18571-highway-asm`
- Validation result:
  - editable CMake build succeeded;
  - focused native backend tests: `18 passed`;
  - regime target-axis correctness smoke without benchmarking: `10 passed`;
  - measured SIMD dispatch smoke atlas: `540 passed in 58.00s`;
  - assembly emission/report succeeded.
- Benchmark/result:
  - SIMD was not universally incrementally better than scalar after dispatch
    cleanup;
  - across 360 SIMD-vs-same-target-scalar comparisons, 194 were slower at any
    margin, 45 were slower by more than 3%, and 7 were slower by more than 10%;
  - `phase_products` was the clearest target winner:
    - AVX2 mean ratio `0.892`, best ratio `0.676`, losses `12/36`;
    - AVX2+FMA mean ratio `0.925`, best ratio `0.755`, losses `13/36`;
  - the strongest rows were structured/phase-like medium cases:
    - chirp, 8192, blocked128, 8 threads, AVX2 phase products: `0.676x`;
    - phase chirp, 8192, blocked128, 8 threads, AVX2 phase products: `0.681x`;
    - sparse spikes, 8192, blocked128, 8 threads, AVX2 phase products:
      `0.715x`;
  - the clearest losses were irregular/shorter path cases:
    - Gaussian noise, 4096, blocked128, 8 threads, AVX2 magnitude path:
      `1.255x`;
    - Gaussian noise, 4096, blocked128, 8 threads, AVX2+FMA magnitude path:
      `1.220x`;
    - sparse spikes, 4096, blocked128, 8 threads, AVX2 DTW local cost:
      `1.214x`.
- Assembly observations:
  - `_core.s` still contains calls to `select_simd_level` and
    `simd_capabilities`, but targeted inspection placed those in binding/
    metadata helpers rather than direct scorer hot loops;
  - `simd_avx2.s` showed expected vectorized dot product, local-cost staging,
    smoothing, and L1 accumulation kernels;
  - `simd_avx2_fma.s` showed FMA in the dot-product kernel, while non-dot
    AVX2+FMA entrypoints jump through to AVX2 kernels, which is an avoidable
    call edge for a later cleanup;
  - `smooth9_contiguous_avx2` has overlapping-window load traffic;
  - `l1_pair_contiguous_avx2` shows stack traffic and horizontal-reduction
    spills, matching the weak `magnitude_path` smoke rows.
- Conclusion:
  - a clear winner exists by target axis: `phase_products` is the only SIMD
    target that currently looks broadly worth expanding;
  - there is no universal SIMD-level winner across all signal families and
    target modes in this smoke atlas;
  - AVX2 is the best phase-products candidate in the measured rows, while
    AVX2+FMA is not a clear overall improvement because only the dot-product
    kernel uses FMA and other kernels route to AVX2.
- Next hypothesis:
  - remove the avoidable AVX2+FMA non-dot wrapper jumps, then rerun the
    `phase_products` atlas at larger lengths before combining targets.

## 2026-06-17 09:46 KST - Great Chop To Native-Only x86-64 Dispatch

- Git status: dirty at start of batch with atlas-era native experiments still in
  source.
- Hypothesis:
  - the project is easier to maintain if production has one native scorer path;
  - previous atlas winners can be hard-coded as scalar-source C++ choices:
    index-incremental DTW layout plus all reduction/workspace optimizations;
  - x86-64 v1-v4 compiler variants can preserve wheel portability while letting
    the compiler optimize for newer CPU levels where available.
- Files changed:
  - replaced the public Python scorer with native-only `iso18571.ISO18571`;
  - removed public backend selection and optional backend loaders;
  - replaced the native module with a small binding surface:
    `backend_info`, `warp_path`, `magnitude_ratio`, and `score_components`;
  - deleted explicit SIMD intrinsic sources, parallel/variant APIs, benchmark
    tests, atlas analyzers, assembly tools, and backend-validation scripts;
  - added scalar-source C++ scorer variants for x86-64 v1/v2/v3/v4 plus
    internal runtime dispatch through direct function pointers;
  - rebuilt tests as parity-only comparisons over downloaded Annex cases and one
    generated Annex set across `native`, `original`, `dtw_python`, and
    `librosa`;
  - rewrote `AGENTS.md` and `docs/iso18571-dtw-backends.md` around the
    native-only workflow.
- Commands:
  - `uv run --extra test ruff check --fix .`
  - `uv run --extra test ruff format .`
  - `uv run --extra test ruff check .`
  - `uv run --extra test ruff format --check .`
  - `git diff --check`
  - `uv pip install -e .`
  - `uv run --extra test python -m pytest -q`
  - `uv build --wheel`
  - `uv run --with dist/euroncap-0.1.0-cp313-cp313-linux_x86_64.whl python tools/iso18571/wheel_smoke.py`
- Validation result:
  - Ruff check/format passed;
  - whitespace diff check passed;
  - editable CMake build passed;
  - default parity suite passed: `3 passed`;
  - Linux wheel build passed and compiled x86-64 v1, v2, v3, and v4 variants;
  - wheel smoke passed;
  - focused cleanup scans found no live ISO18571 source/tool/doc references to
    the removed experimental API terms.
- Runtime metadata:
  - `backend_info()` reports `name=iso18571_native`,
    `dtw_layout=index_incremental`, `reduction_mode=all`,
    `parallelism=none`, and compiled levels
    `x86-64-v1,x86-64-v2,x86-64-v3,x86-64-v4`.
- Conclusion:
  - atlas optimization is retired from production source and tooling;
  - production is now native-only with internal x86-64 level dispatch;
  - comparison scorers remain only inside tests for parity.
- Next hypothesis:
  - with the surface area reduced, the next useful work is either broader parity
    case design or targeted scalar-source C++ profiling inside the single
    production scorer.
