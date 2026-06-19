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

## 2026-06-17 10:18 KST - Dead/Inconsistent Code Review Pass

- Git status: dirty at start of review with `src/iso18571_native/_core.cpp`
  already modified to remove an unused `<cmath>` include.
- Hypothesis:
  - after the native-only cleanup, remaining dead or inconsistent code is most
    likely in packaging metadata, test extras, and x86-64 dispatch platform
    branches rather than in the main scorer path.
- Files changed:
  - `docs/iso18571-dtw-experiment-log.md` only, to record this review pass.
- Commands:
  - `sed -n '1,240p' AGENTS.md`
  - `tail -n 160 docs/iso18571-dtw-experiment-log.md`
  - `git status --short --branch`
  - `rg --files -g '!ref/**' -g '!dist/**' -g '!build/**' -g '!*.egg-info/**' -g '!.benchmarks/**'`
  - `git diff -- src/iso18571_native/_core.cpp`
  - line-numbered source/test/doc reads with `nl -ba`
  - `rg` scans for retired backend, variant, SIMD, benchmark, and public-surface
    terms
  - `uv run --extra test ruff check .`
  - `uv run --extra test ruff format --check .`
  - `git diff --check`
  - `uv pip install -e .`
  - `uv run --extra test python -m pytest -q`
  - `uv build --wheel`
  - `uv run --with dist/euroncap-0.1.0-cp313-cp313-linux_x86_64.whl python tools/iso18571/wheel_smoke.py`
  - `uv run python -m zipfile -l dist/euroncap-0.1.0-cp313-cp313-linux_x86_64.whl`
  - `uv run --with vulture vulture iso18571 iso18571_native src tests tools main.py --min-confidence 60`
  - `nl -ba .github/workflows/wheels.yml`
  - `git status --short --ignored .github`
- Validation result:
  - Ruff check passed;
  - Ruff format check passed;
  - whitespace diff check passed;
  - editable CMake build passed;
  - default parity suite passed: `3 passed`;
  - wheel build passed and installed `main.py` at the wheel root;
  - wheel smoke passed;
  - Vulture found only the expected pytest hook false positive in
    `tests/conftest.py`.
- Conclusion:
  - no dead code was found in the primary native scorer implementation;
  - review findings are concentrated in MSVC dispatch feature detection,
    stale optional test dependencies, an unadvertised packaged `main.py`, and
    ignored stale workflow/package metadata.
- Next hypothesis:
  - clean the low-risk stale packaging/test-extra items first, then add or
    adjust a Windows dispatch probe before relying on v3/v4 selection in MSVC
    wheels.

## 2026-06-17 10:21 KST - Native Efficiency And Correctness Review Probes

- Git status: dirty at start of review with `src/iso18571_native/_core.cpp`
  already modified and the previous review log entry present.
- Hypothesis:
  - the native-only scorer's main Annex path is likely correct, while remaining
    risks should appear around API-edge validation, oversized windows, and
    platform dispatch.
- Files changed:
  - `docs/iso18571-dtw-experiment-log.md` only, to record the review probes.
- Commands:
  - `git status --short --branch`
  - `tail -n 160 docs/iso18571-dtw-experiment-log.md`
  - line-numbered reads of `src/iso18571_native/`, `iso18571_native/`,
    `iso18571/rating.py`, `CMakeLists.txt`, `pyproject.toml`, and parity tests
  - `git diff -- src/iso18571_native/_core.cpp`
  - `uv run --extra test python -m pytest -q`
  - native API probes for short curves, strided curves, DTW window validation,
    parameter domains, weight-sum equality, pybind integer casting, and
    non-finite curve values
  - reference-oracle type/value spot checks for short curves and selected
    parameter edges
- Validation result:
  - default parity suite passed: `3 passed`;
  - selected runtime backend reported compiled levels
    `x86-64-v1,x86-64-v2,x86-64-v3,x86-64-v4` and selected `x86-64-v3`;
  - negative-stride curve views scored successfully;
  - curves shorter than 9 samples fail with a native `ValueError` before slope
    scoring;
  - exact weight equality rejected several mathematically normalized weight
    combinations;
  - `dt=0.0` produced `nan` slope and overall scores rather than an early
    validation error.
- Conclusion:
  - Annex parity is healthy on this Linux host;
  - highest-value fixes are parameter-domain validation, capping oversized DTW
    radii, and correcting Windows/MSVC dispatch feature detection.
- Next hypothesis:
  - add focused edge tests for parameter validation and `window_size > 1`, then
    fix those without changing Annex parity.

## 2026-06-17 10:43 KST - Production Package Rename And True Reference Parity

- Git status: dirty at start with prior review-log entries and `_core.cpp`
  include cleanup already present.
- Hypothesis:
  - moving reference scorers into source-only `iso18571_reference` and placing
    the native extension directly under public `iso18571` will make parity
    labels truthful while keeping the production import stable.
- Files changed:
  - renamed native C++ source from `src/iso18571_native/` to `src/iso18571/`;
  - removed importable `iso18571_native`;
  - added source-only `iso18571_reference` full scorers for dtwalign,
    dtw-python, and librosa;
  - updated CMake install layout, tests, wheel smoke, README, AGENTS, backend
    docs, `.gitignore`, package metadata, and native validation/dispatch code.
- Commands:
  - `git status --short --branch`
  - `tail -n 80 docs/iso18571-dtw-experiment-log.md`
  - `sed -n '1,140p' AGENTS.md`
  - targeted `rg`, `sed`, and `git diff` inspection commands
  - `uv pip install -e .`
  - `uv run --extra test python -m pytest -q`
  - `uv run --extra test ruff check --fix .`
  - `uv run --extra test ruff format .`
  - `uv run --extra test ruff check .`
  - `uv run --extra test ruff format --check .`
  - `git diff --check`
  - `uv build --wheel`
  - `uv run --with dist/euroncap-1.0.0-cp313-cp313-linux_x86_64.whl python tools/iso18571/wheel_smoke.py`
  - `uv run python -m zipfile -l dist/euroncap-1.0.0-cp313-cp313-linux_x86_64.whl`
  - isolated `/tmp` wheel import probe for `iso18571`, `iso18571_reference`,
    and `iso18571_native`.
- Validation result:
  - full scorer parity and edge suite passed: `6 passed`;
  - Ruff check and format gates passed;
  - whitespace diff check passed;
  - editable CMake build passed;
  - Linux wheel build passed and installed only `iso18571/_core`,
    `iso18571/__init__.py`, and `iso18571/rating.py`;
  - wheel smoke passed;
  - isolated wheel import reported `backend_info()["name"] == "iso18571"` and
    no import specs for `iso18571_reference` or `iso18571_native`.
- Conclusion:
  - production import remains `from iso18571 import ISO18571`;
  - reference parity now compares four full scorers: native, dtwalign,
    dtw-python, and librosa;
  - stale `iso18571_native` and top-level wheel `main.py` surfaces are removed;
  - native dispatch now guards CPUID leaf 7 and uses the correct extended LZCNT
    bit on MSVC.
- Next hypothesis:
  - the next useful work is broader Windows wheel validation for MSVC dispatch
    behavior, then any remaining public API polishing around parameter errors.

## 2026-06-17 11:02 KST - Original Reference Large-Signal Memory Check

- Git status: clean `master` at start.
- Hypothesis:
  - `ref/rating_original.py` may not handle 30,000-sample signals because its
    DTW magnitude path materializes dense `n x n` arrays before applying the
    Sakoe-Chiba window.
- Files changed:
  - `docs/iso18571-dtw-experiment-log.md` only, to record the investigation.
- Commands:
  - `git status --short --branch`
  - `sed -n '1,220p' AGENTS.md`
  - `tail -n 120 docs/iso18571-dtw-experiment-log.md`
  - `rg --files -u ref | rg 'rating_original\.py$|rating_.*\.py$'`
  - `nl -ba ref/rating_original.py | sed -n '1,420p'`
  - `uv run python` memory-size calculation for dense and banded DTW layouts.
- Validation result:
  - `magnitude_rating()` calls `_compute_magnitude()` with `window_size=0.1`;
  - `_compute_magnitude()` builds `scipy.spatial.distance.cdist(...)` as a
    dense `n x n` float64 matrix before constructing the `dtwalign`
    Sakoe-Chiba window;
  - for `n=30000`, that single dense matrix is 900,000,000 float64 values, or
    about 6.71 GiB, before additional DTW/window/path allocations.
- Conclusion:
  - `ref/rating_original.py` is expected to fail or become impractical for
    30,000-sample magnitude/overall scoring on ordinary machines due to memory,
    even though the nominal 10% Sakoe-Chiba band is much smaller than the full
    square matrix.
- Next hypothesis:
  - use the production native scorer for large-signal runs, and keep
    `rating_original.py` only as ignored historical reference material.

## 2026-06-17 11:52 KST - Cached Annexes, Strict Typing, In-Test Benchmarks, And Public API Cleanup

- Git status: dirty from the implementation batch.
- Hypothesis:
  - a fresh clone should be able to run parity tests without manually
    downloading Annex CSV files, while benchmark tooling should live in pytest
    and report peak memory plus swap invalidation for large reference backends.
- Files changed:
  - cached official Annex download and generated Annex materialization in
    `tests/`;
  - strict mypy typing support with `iso18571/_core.pyi` and `py.typed`;
  - public package exports narrowed to `ISO18571` and `backend_info`;
  - Python bindings for standalone `warp_path` and `magnitude_ratio` removed;
  - `tools/` removed, including wheel smoke;
  - pytest-benchmark matrix added under `tests/`;
  - README, AGENTS, and backend docs updated.
- Commands:
  - `uv run --extra test ruff check --fix .`
  - `uv run --extra test ruff format .`
  - `uv run --extra test ruff check .`
  - `uv run --extra test ruff format --check .`
  - `uv run --extra test mypy iso18571 iso18571_reference tests`
  - `git diff --check`
  - `uv pip install -e .`
  - `uv run --extra test python -m pytest -q`
  - `uv build`
  - `uv run --extra test python -m pytest -q -m benchmark --benchmark-json .benchmarks/iso18571-readme/benchmarks.json`
  - `uv run python -m zipfile -l dist/euroncap-1.0.0-cp313-cp313-linux_x86_64.whl`
- Validation result:
  - Ruff check and format gates passed;
  - mypy strict passed for `iso18571`, `iso18571_reference`, and `tests`;
  - default pytest passed: `5 passed, 32 deselected`;
  - wheel build passed with no C++ warnings after removing dead DTW path export
    code;
  - wheel contents include only `iso18571` package files and dist metadata.
- Benchmark result:
  - full 32-row pytest-benchmark matrix passed in `472.27s`;
  - benchmark JSON records `peak_rss_mib`, `peak_swap_mib`, and
    `swap_invalidated`;
  - `dtwalign` at length `32768` used swap during runtime and is timing
    invalidated;
  - `dtw_python` at length `32768` used swap during load and runtime and is
    timing invalidated;
  - native rows did not use swap, including length `32768`
    (`load_ms=776.39`, `peak_rss_mib=250.84`, `runtime_ms=610.56`).
- Conclusion:
  - Annex tests are now cache-backed and clone-friendly;
  - benchmark support is pytest-native and cross-platform in approach;
  - standalone native DTW helpers are no longer Python API surface;
  - large reference backends can complete under added swap, but swapped rows are
    explicitly marked as stress outcomes.
- Next hypothesis:
  - consider whether `score_components` should remain `_core`-private or move
    fully behind a pybind class wrapper.

## 2026-06-17 12:45 KST - Annex Hash Pinning And ISO PDF Correctness Review

- Git status: dirty from review follow-up edits.
- Hypothesis:
  - official Annex cache tests should verify a pinned upstream ZIP hash instead
    of silently accepting changed bytes;
  - dependency locking should remain out of scope for the distributable native
    module source tree, with `uv.lock` ignored;
  - the local ISO/TS 18571:2024 PDF can distinguish true metric correctness
    gaps from NumPy API compatibility gaps.
- Files changed:
  - `tests/iso18571_annex.py` pins the official Annex ZIP SHA-256 and writes an
    official cache manifest;
  - `.gitignore` ignores the out-of-scope virtual van qualification draft and
    keeps ignoring `uv.lock`;
  - `virtual_van_frontal_collision_avoidance_qualification.md` removed from the
    Git index only;
  - `README.md` and `docs/iso18571-dtw-backends.md` left without lock-file
    validation steps.
- Commands:
  - `pdftotext -layout ISO/ISO_TS_18571_2024(en).pdf -`
  - `pdfinfo ISO/ISO_TS_18571_2024(en).pdf`
  - `curl -fsSL` of the official Annex ZIP followed by `sha256sum`
  - `git rm --cached -- virtual_van_frontal_collision_avoidance_qualification.md`
  - `uv run --extra test ruff check .`
  - `uv run --extra test ruff format --check .`
  - `uv run --extra test mypy iso18571 iso18571_reference tests`
  - `uv run --extra test python -m pytest -q`
  - `git diff --check`
  - `uv build --wheel`
  - isolated `/tmp` wheel import probe with the built wheel.
- Validation result:
  - official Annex ZIP downloaded from ISO matched SHA-256
    `cbc8c5a1ea5677ece8aa097387f9d9d2e6fe7a2a5bb2ce5d17ecf84fe52271d7`
    and contained 42 CSV files;
  - Ruff, format check, mypy, default pytest, whitespace check, and wheel build
    passed;
  - isolated wheel import reported `euroncap==1.0.0`, public exports
    `ISO18571` and `backend_info`, and no importable `iso18571_reference`.
- Conclusion:
  - Annex parity now fails clearly if the upstream official ZIP bytes change;
  - dependency lock-file handling remains ignored for this source tree;
  - ISO PDF review confirms default weights, DTW window, DTW tie order, phase
    tie order, magnitude denominator, and slope smoothing match the current
    implementation for default-parameter Annex-style inputs;
  - true ISO input correctness is about equal sample counts, synchronized
    time-history curves, constant time interval, and evaluation interval, not
    NumPy memory layout.
- Next hypothesis:
  - add focused scorer validation for time-column/dt consistency and ISO
    parameter bounds before changing any public API behavior.

## 2026-06-17 12:59 KST - Native Validation Translation Unit

- Git status: dirty from existing review follow-up edits plus this validation
  refactor.
- Hypothesis:
  - scalar scorer parameter requirements should live in reusable native C++
    validation code, while `_core.cpp` should only parse Python values and keep
    Python array shape checks;
  - ISO corridor bounds require `0 <= a_0 <= 1`, `0 <= b_0 <= 1`, and
    `a_0 < b_0`.
- Files changed:
  - added `src/iso18571/validation.hpp` and `src/iso18571/validation.cpp`;
  - wired `validation.cpp` into `CMakeLists.txt`;
  - moved scalar `ScoreParams` checks out of `_core.cpp`;
  - deleted `test_native_rejects_invalid_params` without adding replacement
    tests.
- Commands:
  - `uv run --extra test ruff check .`
  - `uv run --extra test ruff format --check .`
  - `uv run --extra test mypy iso18571 iso18571_reference tests`
  - `uv run --extra test python -m pytest -q`
  - `git diff --check`
  - `uv build --wheel`
- Validation result:
  - Ruff check and format check passed;
  - mypy strict passed for `iso18571`, `iso18571_reference`, and `tests`;
  - default pytest passed: `4 passed, 32 deselected`;
  - whitespace check passed;
  - wheel build passed and compiled `validation.cpp` into the native extension.
- Conclusion:
  - native scalar validation is separated from pybind parsing and now enforces
    the ISO upper bounds for `a_0` and `b_0`.
- Next hypothesis:
  - after API behavior is settled, consider focused native validation coverage
    for scalar requirements without reintroducing pybind-specific checks.

## 2026-06-17 13:02 KST - Native Weight Sum Tolerance

- Git status: dirty from existing review follow-up edits plus the native
  validation refactor.
- Hypothesis:
  - the native weight-sum validation should allow small binary floating-point
    normalization error while still rejecting materially invalid weight sets.
- Files changed:
  - `src/iso18571/validation.hpp` defines
    `kWeightSumAbsoluteTolerance = 1.0e-12`;
  - `src/iso18571/validation.cpp` uses that tolerance for the total weight
    check.
- Commands:
  - `uv run --extra test ruff check .`
  - `uv run --extra test ruff format --check .`
  - `uv run --extra test mypy iso18571 iso18571_reference tests`
  - `uv run --extra test python -m pytest -q`
  - `git diff --check`
  - `uv build --wheel`
- Validation result:
  - Ruff check, format check, mypy, default pytest, whitespace check, and wheel
    build passed;
  - default pytest passed: `4 passed, 32 deselected`.
- Conclusion:
  - weight validation now accepts sums within `1.0e-12` of `1.0` instead of
    requiring exact floating-point equality.
- Next hypothesis:
  - keep validation tolerances named in native requirement definitions so future
    scalar checks are auditable.

## 2026-06-17 13:07 KST - v1.0.1 Release Validation

- Git status: dirty from validation, Annex hash, ignore-list, and release
  version edits before staging.
- Hypothesis:
  - the validation refactor, `a_0`/`b_0` ISO-bound fix, Annex hash pinning, and
    ignored virtual-van material are ready to ship as `v1.0.1`.
- Files changed:
  - `pyproject.toml` bumps the package version to `1.0.1`;
  - native validation, CMake wiring, parity-test removal, Annex hash pinning,
    and ignore-list changes remain in the release diff.
- Commands:
  - `uv run --extra test ruff check --fix .`
  - `uv run --extra test ruff format .`
  - `uv run --extra test ruff check .`
  - `uv run --extra test ruff format --check .`
  - `uv run --extra test mypy iso18571 iso18571_reference tests`
  - `uv run --extra test python -m pytest -q`
  - `git diff --check`
  - `uv build --wheel`
- Validation result:
  - Ruff fix/check and format/check passed;
  - mypy strict passed for `iso18571`, `iso18571_reference`, and `tests`;
  - default pytest passed: `4 passed, 32 deselected`;
  - whitespace check passed;
  - wheel build produced
    `dist/euroncap-1.0.1-cp313-cp313-linux_x86_64.whl`.
- Conclusion:
  - release candidate `v1.0.1` is ready to commit, tag, and push.
- Next hypothesis:
  - confirm the remote tag and main branch receive the release commit cleanly.

## 2026-06-17 13:55 KST - Zig Linux SIMD Flag Mapping

- Git status: clean `main` at `f82d266`, then branched to
  `experiment/zig-linux-simd` for the experiment.
- Hypothesis:
  - Zig can build the Linux Python extension with the same internal x86-64
    dispatch coverage if CMake uses Zig's underscore CPU names for
    `-march=x86_64_v2`, `-march=x86_64_v3`, and `-march=x86_64_v4`.
- Files changed:
  - `CMakeLists.txt` detects `zig c++` narrowly and uses Zig-specific
    x86-64-v2/v3/v4 compile flags;
  - this experiment log records the baseline and mapped Zig results.
- Commands:
  - `CC="zig cc" CXX="zig c++" uv build --wheel --out-dir /tmp/iso18571-zig-dist --clear -Cbuild-dir=/tmp/iso18571-zig-build`
  - smoke import/scoring run with the baseline Zig wheel from `/tmp`
  - `CC="zig cc" CXX="zig c++" uv build --wheel --out-dir /tmp/iso18571-zig-mapped-dist --clear -Cbuild-dir=/tmp/iso18571-zig-mapped-build`
  - smoke import/scoring run with the mapped Zig wheel from `/tmp`
  - `ldd` on the mapped Zig-built `_core` extension
  - copied `tests/` and `iso18571_reference/` into `/tmp/iso18571-zig-parity`
    without copying the source `iso18571/` package
  - import-location probe against the mapped Zig wheel from the temp parity
    workspace
  - `uv run --with /tmp/iso18571-zig-mapped-dist/iso18571-1.0.1-cp313-cp313-linux_x86_64.whl --with pytest --with dtwalign --with dtw-python --with librosa --with scipy python -m pytest -q tests/test_iso18571_parity.py`
  - `uv run --extra test ruff check --fix .`
  - `uv run --extra test ruff format .`
  - `uv run --extra test ruff check .`
  - `uv run --extra test ruff format --check .`
  - `uv run --extra test mypy iso18571 iso18571_reference tests`
  - `uv run --extra test python -m pytest -q`
  - `git diff --check`
  - `uv build --wheel`
- Validation result:
  - baseline Zig Linux wheel built, imported, and scored `1.0`, but reported
    only `compiled_x86_64_levels == "x86-64-v1"`;
  - after CMake flag mapping, Zig built v1/v2/v3/v4 source variants and the
    smoke run reported
    `compiled_x86_64_levels == "x86-64-v1,x86-64-v2,x86-64-v3,x86-64-v4"`;
  - mapped Zig smoke scoring returned `1.0` and selected `x86-64-v3` on this
    host;
  - `ldd` showed only `libm`, `libc`, and the system dynamic loader;
  - the temp-workspace import probe confirmed `iso18571` and `_core` loaded
    from the installed Zig wheel, not the repo source tree;
  - wheel-backed parity suite passed: `4 passed in 25.40s`;
  - Ruff, format check, mypy, default pytest, whitespace check, and normal GCC
    wheel build passed.
- Conclusion:
  - Zig 0.16.0 is viable for Linux local extension builds with internal
    dispatch preserved after translating the x86-64 level CPU flag spelling.
- Next hypothesis:
  - try the same Zig mapping inside Linux `cibuildwheel`/manylinux before
    considering any Windows cross-build experiment.

## 2026-06-17 14:09 KST - Zig Windows PYD Probe Under Wine

- Git status: clean `experiment/zig-linux-simd` before recording the Windows
  probe.
- Hypothesis:
  - Zig can cross-compile a Windows `_core.pyd` from Linux, and Wine can provide
    enough Windows Python runtime coverage to smoke-test import behavior.
- Files changed:
  - experiment log only.
- Commands:
  - initialized a disposable 64-bit Wine prefix under
    `/home/user/.cache/iso18571-wine/win64`;
  - downloaded and unpacked the CPython 3.13.8 NuGet package into `/tmp`;
  - verified the Windows `python.exe` runs under Wine;
  - configured a `x86_64-windows-msvc` Zig/CMake build with Windows Python
    headers and `python313.lib`;
  - configured and built a `x86_64-windows-gnu` Zig/CMake build with the same
    Windows Python inputs;
  - imported the resulting `_core.cp313-win_amd64.pyd` under Wine;
  - installed Windows NumPy wheels under the Wine Python and attempted a scoring
    smoke.
- Validation result:
  - `x86_64-windows-msvc` configured but failed to compile because MSVC/Windows
    SDK headers such as `io.h`, `new.h`, and `vcruntime_exception.h` are absent
    on this Linux host;
  - `x86_64-windows-gnu` built `_core.cp313-win_amd64.pyd` with v1/v2/v3/v4
    source variants;
  - Wine import of `iso18571._core` succeeded and `backend_info()` reported
    `compiled_x86_64_levels == "x86-64-v1,x86-64-v2,x86-64-v3,x86-64-v4"`;
  - Windows NumPy import failed under Ubuntu Wine 9.0 with unimplemented
    `ucrtbase.dll.crealf`, so full scoring/parity under Wine is blocked by
    Wine/NumPy runtime coverage rather than `_core` import.
- Conclusion:
  - Zig can produce a loadable Windows `.pyd` through the `windows-gnu` target,
    but the distributable MSVC-ABI path needs Windows SDK/MSVC CRT headers, and
    full runtime validation needs either newer Wine coverage or a real Windows
    VM/host.
- Next hypothesis:
  - provision MSVC headers/libs via an `xwin`-style SDK layout for the
    `x86_64-windows-msvc` target, then validate on real Windows or KVM if Wine
    remains blocked by NumPy.

## 2026-06-17 14:37 KST - Zig Windows uv/Wine Parity Probe

- Git status: clean `experiment/zig-linux-simd` before appending this entry.
- Hypothesis:
  - Windows `uv` under Wine can replace NuGet as the Python source for runtime
    testing and development artifacts;
  - `xwin` can provide the missing MSVC CRT/Windows SDK headers for Zig's
    `x86_64-windows-msvc` path;
  - if any Windows NumPy lane imports under Wine, the full parity suite should
    run against a Zig-built Windows `.pyd`.
- Files changed:
  - experiment log only.
- Commands:
  - initialized a fresh 64-bit Wine prefix under
    `/home/user/.cache/iso18571-wine/uv-win64`;
  - downloaded the official Windows `uv` release zip into `/tmp` after Wine
    PowerShell did not expose an installed `uv.exe`;
  - ran Windows `uv` under Wine for CPython 3.13 and 3.11 NumPy probes;
  - installed `vcrun2022` with `winetricks`, set a `ucrtbase` native/builtin
    DLL override, and retried NumPy imports;
  - located uv-managed `Python.h`, `python311.lib`, `python311.dll`,
    `python313.lib`, and `python313.dll`;
  - installed `xwin` with Cargo and splatted the x86_64 MSVC CRT/Windows SDK to
    `/home/user/.cache/iso18571-xwin`;
  - configured Zig/CMake for `x86_64-windows-msvc` against uv's CPython 3.11
    artifacts and the xwin sysroot;
  - built a `x86_64-windows-gnu` `_core.cp311-win_amd64.pyd` against uv's
    CPython 3.11 artifacts;
  - checked the GNU-target `.pyd` imports with `objdump -p`;
  - staged `iso18571`, `iso18571_reference`, and `tests` in `/tmp` with the
    Zig-built Windows `.pyd`;
  - smoke-tested import/scoring under Wine with `numpy==1.26.4`;
  - ran
    `wine /tmp/iso18571-win-uv/uv.exe run --isolated --python 3.11 --with pytest --with numpy==1.26.4 --with scipy --with dtwalign --with dtw-python --with librosa python -m pytest -q tests/test_iso18571_parity.py`.
- Validation result:
  - Windows `uv` 0.11.21 runs under Wine and installs uv-managed CPython
    3.13.14 and 3.11.15;
  - uv-managed Python includes usable headers, import libraries, and runtime
    DLLs, so NuGet is unnecessary for this probe;
  - current Windows NumPy on CPython 3.13 and 3.11 still fails under Ubuntu
    Wine 9.0 with unimplemented `ucrtbase.dll.crealf`, even after `vcrun2022`
    and a `ucrtbase` override;
  - CPython 3.11 with `numpy==1.26.4` imports successfully under Wine;
  - `xwin` provides the previously missing MSVC headers, and CMake configures
    Zig's `x86_64-windows-msvc` target with v1/v2/v3/v4 source variants;
  - the MSVC-ABI build compiles objects but does not link: CMake emits linker
    flags Zig rejects (`/MANIFEST:EMBED`, `/version:0.0`), and after removing
    those generated flags Zig's C++ runtime build for `windows-msvc` fails while
    building libc++/libc++abi because `vcruntime_exception.h` is not available
    to the internal runtime compilation;
  - a Zig `x86_64-windows-gnu` CPython 3.11 `.pyd` builds successfully with
    `compiled_x86_64_levels == "x86-64-v1,x86-64-v2,x86-64-v3,x86-64-v4"`;
  - `objdump -p` shows the GNU-target `.pyd` imports `python311.dll`
    dynamically, plus Windows/UCRT DLLs, and does not statically link Python;
  - Wine smoke test with `numpy==1.26.4` imports the Windows `.pyd`, selects
    `x86-64-v3`, and scores identical curves as `1.0`;
  - full Windows/Wine parity passed against the Zig-built GNU `.pyd`:
    `4 passed in 39.39s`.
- Conclusion:
  - Windows `uv` under Wine is a better runtime/dev-artifact source than NuGet;
  - Wine parity is viable today only on the CPython 3.11 plus NumPy 1.26 lane;
  - Zig can build and validate a Windows GNU `.pyd` with full internal dispatch
    from Linux;
  - Zig's MSVC C++ path remains blocked by driver/CMake integration and Zig
    libc++/libc++abi support for `windows-msvc`, not by missing Python
    artifacts.
- Next hypothesis:
  - use real Windows/MSVC or clang-cl with xwin for distributable
    `win_amd64` wheels, while treating Zig `windows-gnu` as a useful local
    experiment rather than the release path.

## 2026-06-17 14:56 KST - clang-cl Windows 10 KVM Setup

- Git status: dirty from the existing experiment-log entry before appending
  this follow-up.
- Hypothesis:
  - `clang-cl` plus `lld-link` and the `xwin` SDK can build a real MSVC-ABI
    Windows `.pyd` from Linux;
  - a Windows 10 KVM VM can provide real Windows runtime coverage for parity,
    avoiding Wine's NumPy/UCRT limitations.
- Files changed:
  - experiment log only.
- Commands:
  - copied `/media/user/Ventoy/iso/Win10_22H2_English_x64v1.iso` to
    `/home/user/.cache/iso18571-win-vm/iso/`;
  - created cache-only CMake helper wrappers for Wine Python and
    `lld-link /lib`;
  - configured and built a CPython 3.11 MSVC-ABI extension with `clang-cl`,
    `lld-link`, `llvm-rc`, `llvm-mt`, xwin CRT/SDK paths, pybind11, and
    uv-managed Windows Python development artifacts;
  - checked the `.pyd` with `file` and `objdump -p`;
  - staged `iso18571`, `iso18571_reference`, `tests`, Windows `uv.exe`, and
    `run-parity.ps1`/`run-parity.cmd` under the VM payload directory;
  - created `win10.qcow2` and copied OVMF vars into the VM cache;
  - launched QEMU/KVM with the Windows ISO, qcow2 disk, user-mode networking,
    and the payload as a writable USB mass-storage FAT drive.
- Validation result:
  - `clang-cl` configured and built `_core.cp311-win_amd64.pyd`;
  - CMake compiled the expected MSVC dispatch set:
    `x86-64-v1,x86-64-v3,x86-64-v4`;
  - `objdump -p` shows dynamic imports for `python311.dll`, `MSVCP140.dll`,
    `VCRUNTIME140.dll`, and UCRT API DLLs;
  - the VM is running as QEMU process `14976` with KVM acceleration;
  - first QEMU launch inherited Snap library variables from VS Code and failed
    with an incompatible `libpthread`; relaunching with a clean environment
    fixed the issue.
- Conclusion:
  - the Linux-hosted clang-cl MSVC-ABI build path is viable for CPython 3.11;
  - Windows installation/OOBE is now the blocking step before real parity can
    run in the VM.
- Next hypothesis:
  - after Windows reaches the desktop, run the shared `run-parity.cmd` payload
    to validate current NumPy/SciPy parity on real Windows.

## 2026-06-17 16:29 KST - clang-cl Windows 10 KVM Parity Result

- Git status: dirty from prior experiment-log entries before appending this
  follow-up.
- Hypothesis:
  - the Linux-built `clang-cl`/xwin MSVC-ABI CPython 3.11 `.pyd` should import
    and pass full scorer parity under a real Windows 10 KVM guest with current
    Windows NumPy/SciPy wheels.
- Files changed:
  - experiment log only.
- Commands:
  - booted the installed Windows 10 KVM guest from
    `/home/user/.cache/iso18571-win-vm/win10.qcow2`;
  - replaced the writable QEMU vvfat payload with a read-only generated payload
    ISO after vvfat crashed while Windows wrote volume metadata;
  - staged Microsoft VC runtime DLLs next to `iso18571/_core.cp311-win_amd64.pyd`
    after the first Windows smoke failed with a missing dependent DLL;
  - changed the Windows runner to capture uv stdout/stderr via `Start-Process`
    instead of PowerShell native-command redirection;
  - seeded the official Annex zip into `.pytest_cache/d/iso18571_annex` from the
    host cache after Windows Python failed certificate verification when
    fetching it over HTTPS;
  - ran `run-parity.cmd` from the `ISO18571V4` payload CD inside the guest.
- Validation result:
  - `objdump -p` confirms the `.pyd` imports `python311.dll` dynamically, along
    with `MSVCP140.dll`, `VCRUNTIME140.dll`, and UCRT API DLLs;
  - Windows `uv` 0.11.21 installed CPython 3.11.15 and current test
    dependencies in the guest;
  - smoke imported NumPy and `iso18571`, selected `x86-64-v3`, and scored
    identical curves as `1.0`;
  - `backend_info()` reported
    `compiled_x86_64_levels == "x86-64-v1,x86-64-v3,x86-64-v4"`;
  - full parity under Windows 10 KVM passed:
    `4 passed in 36.42s`.
- Conclusion:
  - the Linux-hosted `clang-cl` plus xwin build path can produce a usable
    MSVC-ABI CPython 3.11 `win_amd64` extension;
  - real Windows KVM validation avoids the Wine/UCRT NumPy limitation and gives
    a viable local test harness for Windows wheels;
  - payload transfer should use read-only ISO or another non-vvfat path, and
    Windows parity runs should seed the Annex data or configure trusted
    certificates before relying on live HTTPS downloads.
- Next hypothesis:
  - repeat the same `clang-cl`/xwin build and Windows KVM validation for CPython
    3.13, then decide whether to automate the process behind cibuildwheel-style
    local commands.

## 2026-06-17 16:57 KST - Host-Specific Wheel Builder

- Git status: dirty on `main` from carrying forward experiment-log entries and
  implementing the build-system follow-up.
- Hypothesis:
  - KVM proved that a Linux-built MSVC-ABI Windows `.pyd` can work on Windows,
    but the durable release workflow should build wheels from the host without
    treating KVM or Wine as required infrastructure.
- Files changed:
  - `CMakeLists.txt`, `pyproject.toml`, `README.md`,
    `tools/build_wheels.py`, and this experiment log.
- Commands:
  - added a host-aware wheel builder that dispatches Linux wheels to
    `cibuildwheel` and Windows wheels to the Linux-hosted
    `clang-cl`/`lld-link`/`xwin` cross-build lane;
  - added a CMake cross-build option for target Python development artifacts
    without a target interpreter;
  - added a controlled extension suffix override for cross-built Windows
    `.pyd` names;
  - added scikit-build tag overrides selected by
    `ISO18571_CROSS_WINDOWS_TAG`.
- Validation result:
  - normal pytest passed: `4 passed, 32 deselected`;
  - normal local wheel build passed for the host interpreter;
  - Linux-hosted Windows cross-build produced
    `cp312-cp312-win_amd64`, `cp313-cp313-win_amd64`, and
    `cp314-cp314-win_amd64` wheels;
  - each Windows wheel contained the expected `_core.cp3xx-win_amd64.pyd`;
  - `objdump -p` validation confirmed dynamic imports for the matching
    `python3xx.dll`, `MSVCP140.dll`, and `VCRUNTIME140.dll`;
  - Linux cibuildwheel produced CPython 3.12/3.13/3.14 manylinux wheels, and
    `auditwheel repair` retagged them as
    `manylinux_2_24_x86_64.manylinux_2_28_x86_64`.
- Conclusion:
  - `cibuildwheel` remains useful only for the Linux/manylinux lane in this
    repository's release workflow;
  - Windows runtime DLLs should remain a user/runtime prerequisite, not wheel
    payload contents;
  - KVM remains useful for occasional manual runtime proof, but is out of scope
    for the normal build and validation flow.
- Next hypothesis:
  - use `tools/build_wheels.py --platform all --out-dir dist` as the local
    release-wheel entrypoint after the remaining style/type/whitespace gates
    pass.

## 2026-06-17 17:47 KST - v1.0.2 Build Polish

- Git status: clean `main` one commit ahead of `origin/main` before this
  follow-up.
- Hypothesis:
  - release-build instructions can be made more direct by documenting simple
    prerequisite install commands, defaulting xwin license acceptance, and
    hiding low-level backend experiment details from `backend_info()`.
- Files changed:
  - `pyproject.toml`, `iso18571/__init__.py`, `iso18571/_core.pyi`,
    `src/iso18571/_core.cpp`, `tests/test_iso18571_parity.py`,
    `tools/build_wheels.py`, `README.md`, backend docs, and this experiment
    log.
- Commands:
  - bump package version from `1.0.1` to `1.0.2`;
  - reduce public `backend_info()` to `name`, `implementation`, `version`, and
    `optimization`;
  - default xwin license acceptance in `tools/build_wheels.py` and replace
    `--accept-ms-license` with `--no-accept-ms-license`;
  - allow Windows hosts to build Linux wheels through
    `cibuildwheel --platform linux` and Docker, while keeping native Windows
    MSVC wheels on `cibuildwheel --platform windows`;
  - add Debian/Ubuntu prerequisite install commands to the README, while
    pointing Docker, Cargo, and VC++ redistributable setup to official vendor
    documentation.
- Validation result:
  - `uv run --extra test ruff check --fix .` passed;
  - `uv run --extra test ruff format .` passed;
  - `uv run --extra test ruff check .` passed;
  - `uv run --extra test ruff format --check .` passed;
  - `uv run --extra test mypy iso18571 iso18571_reference tests tools/build_wheels.py`
    passed;
  - `uv run --extra test python -m pytest -q` passed:
    `4 passed, 32 deselected`;
  - `git diff --check` passed;
  - `uv build --wheel` produced
    `dist/iso18571-1.0.2-cp314-cp314-linux_x86_64.whl`;
  - `uv run python tools/build_wheels.py --help` showed `dist` as the output
    default and documented `--no-accept-ms-license`;
  - `uv run python tools/build_wheels.py --platform windows --python 3.12
    --out-dir /tmp/iso18571-default-check` produced
    `iso18571-1.0.2-cp312-cp312-win_amd64.whl` without an explicit license flag
    and validated dynamic imports for `python312.dll`, `MSVCP140.dll`, and
    `VCRUNTIME140.dll`.
- Conclusion:
  - v1.0.2 build polish is ready to commit; the public backend diagnostic is
    intentionally user-facing, release-wheel commands now default to `dist/`,
    Windows cross-builds accept xwin provisioning by default, and Windows hosts
    can build Linux wheels through cibuildwheel's Docker lane while retaining
    native MSVC/cibuildwheel Windows wheels.
- Next hypothesis:
  - push `main` with both the host-specific wheel builder and v1.0.2 polish
    commits.

## 2026-06-17 18:28 KST - Float Exponent Native Param Parsing

- Git status: clean before changes.
- Hypothesis:
  - native scoring can accept integral float exponent parameters such as
    `k_z=2.0` without widening the ISO exponent domain or exposing the private
    `_score_components` worker as installed typed API.
- Files changed:
  - `iso18571/rating.py`, `iso18571/_core.pyi`, native binding/validation
    sources, and Annex parity tests/helpers.
- Commands:
  - widen public `ISO18571` annotations for `k_z`, `k_p`, and `k_m` to
    `int | float`;
  - make `rating.py` resolve `_score_components` as an internal native hook
    instead of advertising it in `_core.pyi`;
  - require an explicit native params dict at the pybind boundary;
  - move score-exponent domain coercion into `validation.cpp`;
  - exercise integral float exponents through the native Annex parity backend.
- Validation result:
  - `uv pip install -e .` passed;
  - `uv run --extra test python -m pytest -q tests/test_iso18571_parity.py`
    passed: `4 passed`;
  - `uv run --extra test python -m pytest -q` passed:
    `4 passed, 32 deselected`;
  - `uv run --extra test ruff check --fix .` passed;
  - `uv run --extra test ruff format .` passed;
  - `uv run --extra test ruff check .` passed;
  - `uv run --extra test ruff format --check .` passed;
  - `uv run --extra test mypy iso18571 iso18571_reference tests` passed;
  - `git diff --check` passed;
  - `uv build --wheel` produced
    `dist/iso18571-1.0.2-cp314-cp314-linux_x86_64.whl`.
- Conclusion:
  - public callers can pass integral float exponent values through the normal
    `ISO18571` path; invalid exponent-domain decisions remain centralized in
    validation; `_score_components` remains an underscored implementation hook
    rather than an installed typed surface.
- Next hypothesis:
  - commit the param-boundary cleanup and keep any future `_core` changes
    exercised through public scorer parity unless a private native regression
    cannot be observed through `ISO18571`.

## 2026-06-17 18:35 KST - Annex Phase Alignment Parity Tightening

- Git status: clean before changes.
- Hypothesis:
  - Annex parity should compare not only final scores, but also phase alignment
    start/length fields and shifted curves; generated degenerate parity should
    not allow warning/assertion failures to masquerade as matching backend
    exceptions.
- Files changed:
  - `tests/iso18571_test_helpers.py`;
  - `tests/test_iso18571_parity.py`;
  - `tests/iso18571_annex.py`;
  - `iso18571_reference/_common.py`;
  - `docs/iso18571-dtw-backends.md`;
  - `docs/iso18571-dtw-experiment-log.md`.
- Commands:
  - add source-only reference diagnostic phase start/length properties;
  - carry official Annex `Test_Phase_Shifted` and `CAE_Phase_Shifted` columns
    through downloaded Annex fixtures;
  - replace score-only parity helper results with an Annex parity result that
    includes shifted curves;
  - keep official Annex scoring on direct public-constructor calls and restrict
    generated exception parity to real scorer exceptions.
- Validation result:
  - `uv run --extra test python -m pytest -q tests/test_iso18571_parity.py`
    passed: `4 passed`;
  - `uv run --extra test ruff check .` passed;
  - `uv run --extra test ruff format --check .` passed;
  - `uv run --extra test mypy iso18571 iso18571_reference tests` passed;
  - `uv run --extra test python -m pytest -q` passed:
    `4 passed, 32 deselected`;
  - `git diff --check` passed.
- Conclusion:
  - Annex parity now covers official and generated Annex-shaped score parity,
    phase start/length parity, shifted-curve parity, official phase-shifted
    columns within `0.001`, and stricter generated exception semantics without
    changing production API.
- Next hypothesis:
  - if future parity gaps appear, inspect DTW warped-curve intermediates against
    the official Annex columns separately from this default score/phase parity
    surface.

## 2026-06-17 18:41 KST - v1.0.3 Annex Parity Release Validation

- Git status: dirty from Annex parity tightening and version bump.
- Hypothesis:
  - the tightened Annex parity changes can ship as `1.0.3` without changing the
    production API or breaking wheel builds.
- Files changed:
  - `pyproject.toml`;
  - `README.md`;
  - `tests/iso18571_test_helpers.py`;
  - `tests/test_iso18571_parity.py`;
  - `tests/iso18571_annex.py`;
  - `iso18571_reference/_common.py`;
  - `docs/iso18571-dtw-backends.md`;
  - `docs/iso18571-dtw-experiment-log.md`.
- Commands:
  - bump package version and README diagnostic example from `1.0.2` to `1.0.3`;
  - `uv pip install -e .`;
  - `uv run --extra test ruff check --fix .`;
  - `uv run --extra test ruff format .`;
  - `uv run --extra test ruff check .`;
  - `uv run --extra test ruff format --check .`;
  - `uv run --extra test mypy iso18571 iso18571_reference tests`;
  - `git diff --check`;
  - `uv run --extra test python -m pytest -q tests/test_iso18571_parity.py`;
  - `uv run --extra test python -m pytest -q`;
  - `uv build --wheel`.
- Validation result:
  - editable install updated from `iso18571==1.0.2` to `iso18571==1.0.3`;
  - Ruff fix/check and format/check passed;
  - mypy passed;
  - whitespace check passed;
  - parity pytest passed: `4 passed`;
  - default pytest passed: `4 passed, 32 deselected`;
  - wheel build produced
    `dist/iso18571-1.0.3-cp314-cp314-linux_x86_64.whl`.
- Conclusion:
  - `1.0.3` is validated locally with strengthened Annex parity and unchanged
    production API.
- Next hypothesis:
  - push `main` with the existing parameter-boundary commit plus the new Annex
    parity/version commit.

## 2026-06-17 19:14 KST - Native Time-Derived Slope Interval And Corridor Edge Correction

- Git status: clean before changes.
- Hypothesis:
  - production native scoring should derive slope `dt` from validated curve time
    columns, reject inconsistent native time grids, and keep Annex parity while
    preserving source-only reference `dt` constructor compatibility.
- Files changed:
  - native binding, validation, scorer headers/implementation;
  - production Python wrapper, CLI, README, parity/benchmark native call sites;
  - source-only reference corridor edge handling; this experiment log.
- Commands:
  - removed public production `ISO18571(..., dt=...)` plumbing and CLI `--dt`;
  - added native time-column validation for finite, strictly increasing,
    uniformly spaced, equal reference/comparison grids using
    `max(1e-12, 1e-9 * dt)` tolerance;
  - derived native `ScoreParams.dt` from the validated time column;
  - split native `k_z` validation to accept positive integers while keeping
    `k_p` and `k_m` in `{1, 2, 3}`;
  - made zero-`Tnorm` corridor scoring exact-match based;
  - kept reference constructors accepting `dt` and continued passing `dt` only
    to reference backends in parity and benchmark helpers.
- Validation result:
  - initial parity run before rebuilding the extension failed on the generated
    zero case because it used the stale native extension;
  - `uv pip install -e .` rebuilt the native extension successfully;
  - `uv run --extra test python -m pytest -q tests/test_iso18571_parity.py`
    passed: `4 passed`;
  - `uv run --extra test python -m pytest -q` passed:
    `4 passed, 32 deselected`;
  - `uv run --extra test ruff check .` passed;
  - `uv run --extra test ruff format --check .` passed;
  - `uv run --extra test mypy iso18571 iso18571_reference tests` passed.
- Conclusion:
  - official and generated Annex parity remain green after stricter native
    timing validation and the production API no longer accepts caller-provided
    `dt`; source-only references retain their `dt` parameter.
- Next hypothesis:
  - run the final whitespace check and consider a follow-up test-focused change
    only if future invalid-time-grid coverage is desired.

## 2026-06-17 19:16 KST - v1.0.4 Release Version Bump

- Git status: dirty from native time-derived interval and corridor edge
  correction before this follow-up.
- Hypothesis:
  - the native timing/corridor correction can ship as `1.0.4` after the normal
    commit gates pass.
- Files changed:
  - `pyproject.toml`;
  - `README.md`;
  - this experiment log.
- Commands:
  - bump package version and README diagnostic example from `1.0.3` to
    `1.0.4`.
- Validation result:
  - editable install updated from `iso18571==1.0.3` to `iso18571==1.0.4`;
  - `uv run --extra test ruff check --fix .` passed;
  - `uv run --extra test ruff format .` passed;
  - `uv run --extra test ruff check .` passed;
  - `uv run --extra test ruff format --check .` passed;
  - `uv run --extra test mypy iso18571 iso18571_reference tests` passed;
  - `git diff --check` passed;
  - `uv run --extra test python -m pytest -q tests/test_iso18571_parity.py`
    passed: `4 passed`;
  - `uv run --extra test python -m pytest -q` passed:
    `4 passed, 32 deselected`.
- Conclusion:
  - native timing/corridor correction is validated locally as `1.0.4`.
- Next hypothesis:
  - commit the implementation and create/push git tag `v1.0.4` if validation
    passes.

## 2026-06-17 19:42 KST - Fresh User CSV Example And Notebook Data Stages

- Git status: dirty from example-script, notebook, example-data tooling, and
  documentation changes.
- Hypothesis:
  - fresh users need a no-argument CSV script, tracked demo CSVs, and a guided
    notebook that visualizes the bundled CSV stage, official downloaded Annex
    stage, and generated Annex stage without adding plotting/Jupyter to normal
    runtime dependencies.
- Files changed:
  - `main.py`;
  - `examples/reference.csv`;
  - `examples/comparison.csv`;
  - `examples/quickstart.ipynb`;
  - `tools/__init__.py`;
  - `tools/example_data.py`;
  - `tests/iso18571_annex.py`;
  - `tests/iso18571_signals.py`;
  - `.gitignore`;
  - `pyproject.toml`;
  - `README.md`;
  - this experiment log.
- Commands:
  - added optional `examples = ["jupyter", "matplotlib"]`;
  - rewrote `main.py` as a no-argument bundled-CSV reference script that prints
    backend info, raw scores, and rounded component ratings as JSON;
  - added `tools/example_data.py` for demo CSV generation, official Annex
    download/extract, generated Annex CSV creation, and notebook loaders;
  - moved generated Annex signal families from test-only code into the shared
    example-data tool and made parity tests consume that tool;
  - generated tracked `examples/reference.csv` and `examples/comparison.csv`;
  - added `examples/quickstart.ipynb` with guided descriptions, signal plots,
    strict timing rejection, official Annex scoring, and generated Annex scoring.
- Validation result:
  - `uv run python tools/example_data.py` passed and rewrote the bundled CSVs;
  - `uv run python tools/example_data.py --generate-annex` passed and wrote
    `118` generated Annex CSVs under ignored `examples/data/annex/generated`;
  - `uv run python main.py` passed and printed a JSON score report for `600`
    samples;
  - `uv run python -m json.tool examples/quickstart.ipynb` passed;
  - initial notebook execution failed because `tools.example_data` was not
    importable from the notebook kernel path;
  - splitting notebook repository-root bootstrap from imports fixed Ruff `E402`
    and notebook import behavior;
  - `uv run --extra examples jupyter nbconvert --execute --to notebook --stdout
    examples/quickstart.ipynb > /tmp/iso18571-quickstart.ipynb` passed;
  - `uv run --extra test ruff format .` passed;
  - `uv run --extra test ruff check .` passed;
  - `uv run --extra test ruff format --check .` passed;
  - `uv run --extra test mypy iso18571 iso18571_reference tests` passed;
  - `uv run --extra test mypy tools` passed;
  - `uv run --extra test python -m pytest -q` passed:
    `4 passed, 32 deselected`;
  - `git diff --check` passed.
- Conclusion:
  - the fresh-user path now has tracked demo CSVs, a no-argument script, a
    visual notebook with bundled/downloaded/generated stages, and shared
    reusable example-data tooling while keeping runtime dependencies light.
- Next hypothesis:
  - if these examples are promoted into packaged documentation later, decide
    whether repository-local `tools/` should become an installed helper module
    or remain a source-tree-only convenience.

## 2026-06-18 08:46 KST - README References And Dtw-Python GPL Boundary

- Git status: dirty from README reference/licensing text and
  `rating_dtw_python.py` GPL notice cleanup.
- Hypothesis:
  - documenting the official ISO page and OpenVT/TU Graz project in the README,
    while making only the dtw-python reference wrapper GPL-covered, clarifies
    provenance without changing production scorer behavior or wheel contents.
- Files changed:
  - `README.md`;
  - `iso18571_reference/rating_dtw_python.py`;
  - this experiment log.
- Commands:
  - added README links for `https://www.iso.org/standard/85791.html` and
    `https://openvt.eu/validation-metrics/ISO18571`;
  - documented the MIT production package boundary and the GPL-only
    `rating_dtw_python.py` reference wrapper boundary;
  - kept CMake/scikit-build wheel install rules unchanged.
- Validation result:
  - `uv run --extra test ruff check --fix .` passed;
  - `uv run --extra test ruff format .` passed with `20 files left unchanged`;
  - `uv run --extra test ruff check .` passed;
  - `uv run --extra test ruff format --check .` passed;
  - `uv run --extra test mypy iso18571 iso18571_reference tests` passed;
  - `git diff --check` passed;
  - `uv run --extra test python -m pytest -q` passed:
    `4 passed, 32 deselected`;
  - `uv build --wheel` passed and produced
    `dist/iso18571-1.0.4-cp314-cp314-linux_x86_64.whl`;
  - `uv build --sdist` passed and produced `dist/iso18571-1.0.4.tar.gz`;
  - wheel archive inspection found no `iso18571_reference` package and no
    `rating_dtw_python.py`;
  - sdist archive inspection found `iso18571_reference/rating_dtw_python.py`,
    and that file contains the GPL text.
- Conclusion:
  - README references now point to both the official ISO page and OpenVT/TU
    Graz project, while production wheels remain MIT-only native package
    artifacts.
- Next hypothesis:
  - no scorer follow-up is needed unless the project later adopts a central
    license-file convention for non-production reference modules.

## 2026-06-18 10:05 KST - Temporary Native Fuzz Discovery Suite

- Git status: dirty from fuzz dependency/marker/profile additions and a new
  optional fuzz discovery test module.
- Hypothesis:
  - a temporary Hypothesis suite can find native robustness anomalies without
    using historical reference scorers as the oracle, while keeping default
    pytest focused on established parity behavior.
- Files changed:
  - `pyproject.toml`;
  - `pytest.ini`;
  - `tests/conftest.py`;
  - `tests/test_iso18571_fuzz_discovery.py`;
  - this experiment log.
- Commands:
  - added `hypothesis` to the test extra;
  - added a `fuzz` marker and excluded it from default pytest;
  - registered a `discovery` Hypothesis profile with `max_examples=200`;
  - implemented fuzz discovery around valid finite curves, equivalent input
    representations, parameter boundaries, invalid inputs, and larger smoke
    lengths;
  - saved the full fuzz run output to `/tmp/iso18571_fuzz_discovery.log`;
  - programmatically scanned that log for repeated anomaly categories and then
    manually inspected each failure section by line number.
- Validation result:
  - `uv pip install -e .` passed;
  - `uv run --extra test python -m pytest -q -m fuzz --hypothesis-profile
    discovery --hypothesis-show-statistics` failed as an exploratory result:
    `6 failed, 23 passed, 36 deselected`;
  - scripted log scan grouped the failures as:
    - zero-reference magnitude `NaN`;
    - zero-slope/constant-reference slope `NaN`;
    - valid finite curves whose selected shifted length is below the slope
      scorer minimum;
    - nonfinite signal values raising `RuntimeError` instead of `ValueError`;
    - two metamorphic-oracle cases involving periodic or numerically
      near-degenerate sine inputs;
  - manual log inspection confirmed the minimized examples:
    - zero identical `n=9` has `EM=NaN`;
    - shifted/noisy `n=9` cases can raise `Shifted curves must have at least 9
      samples for slope rating`;
    - `nan_signal` reaches DTW and raises `No valid ISO DTW path found`;
    - `sine/identical/n65` picks a 13-sample phase shift with `EP=0`;
    - `sine/offset/n10` scaling changes `ES` only because the sampled sine is
      effectively a near-zero signal;
  - `uv run --extra test python -m pytest -q` passed:
    `4 passed, 61 deselected`;
  - `uv run --extra test ruff check --fix .` passed;
  - `uv run --extra test ruff format .` passed and reformatted the new fuzz
    test module;
  - `uv run --extra test ruff check .` passed;
  - `uv run --extra test ruff format --check .` passed;
  - `uv run --extra test mypy iso18571 iso18571_reference tests` passed;
  - `uv build --wheel` passed and produced
    `dist/iso18571-1.0.4-cp314-cp314-linux_x86_64.whl`;
  - `git diff --check` passed.
- Conclusion:
  - the optional fuzz suite is in place and excluded from default runs;
  - source robustness gaps to fix next are finite-signal denominator handling
    for magnitude/slope, shift selection that can leave too few samples for
    slope scoring, and signal-column finiteness validation before scoring;
  - expected/discovery-oracle findings are the periodic identical-sine phase
    ambiguity and the aliased `n=10` sine scaling case, both of which should
    refine future fuzz generators rather than drive source-code changes.
- Next hypothesis:
  - fix source robustness for degenerate denominators, post-shift slope-window
    viability, and signal-value validation, then rerun this fuzz suite to see
    which anomalies remain before promoting minimized cases into permanent
    regressions.

## 2026-06-18 10:37 KST - Native Fuzz Discovery Robustness Fixes

- Git status: dirty from native robustness changes, fuzz discovery tests,
  deterministic regression tests, and generated-parity edge-policy updates.
- Hypothesis:
  - package-defined behavior for undocumented ISO edge cases can keep finite
    valid inputs user-friendly without changing the public API or official
    Annex parity.
- Files changed:
  - `src/iso18571/_core.cpp`;
  - `src/iso18571/scorer.hpp`;
  - `src/iso18571/scorer_impl.hpp`;
  - `tests/test_iso18571_robustness.py`;
  - `tests/test_iso18571_fuzz_discovery.py`;
  - `tests/test_iso18571_parity.py`;
  - `tests/iso18571_test_helpers.py`;
  - fuzz setup files and this experiment log.
- Commands:
  - added native signal-column finiteness validation before scoring;
  - added finite phase, magnitude, and slope fallbacks for undefined
    correlations and zero reference denominators;
  - added a phase clamp to unshifted alignment when the best shift leaves fewer
    than 9 samples for slope scoring;
  - emitted component-level `RuntimeWarning`s from the Python extension boundary
    after native scoring returns;
  - added deterministic regression tests for zero, constant, short-shift,
    nonfinite-signal, periodic-sine, and aliased-sine fuzz discoveries;
  - updated optional fuzz discovery to accept only expected fallback warnings
    and to exclude the hard-coded periodic/aliased oracle traps from strict
    metamorphic properties;
  - kept ordinary generated parity on the four-backend path and added a narrow
    native edge-policy carve-out for generated zero/constant and two generated
    n=9 short-shift cases.
- Validation result:
  - `uv pip install -e .` passed;
  - `uv run --extra test python -m pytest -q -m fuzz --hypothesis-profile
    discovery --hypothesis-show-statistics` passed:
    `29 passed, 43 deselected`;
  - `uv run --extra test python -m pytest -q` passed:
    `11 passed, 61 deselected`;
  - `uv run --extra test ruff check --fix .` passed;
  - `uv run --extra test ruff format .` passed and reformatted one test file;
  - `uv run --extra test ruff check .` passed;
  - `uv run --extra test ruff format --check .` passed;
  - `uv run --extra test mypy iso18571 iso18571_reference tests` passed:
    `17 source files`;
  - `uv build --wheel` passed and produced
    `dist/iso18571-1.0.4-cp314-cp314-linux_x86_64.whl`;
  - `git diff --check` passed.
- Conclusion:
  - native scoring now returns finite public scores for the discovered finite
    degenerate inputs, warns on implementation fallbacks, rejects nonfinite
    signal values clearly, and no longer raises for the discovered short-shift
    valid inputs;
  - fuzz discovery is now a clean optional guard instead of an expected failure
    harness.
- Next hypothesis:
  - run the same fuzz/default validation on Windows wheels before treating the
    warning/fallback edge policy as release-ready across supported platforms.

## 2026-06-18 10:56 KST - Native Vector Diagnostics Refactor

- Git status: dirty from native diagnostic hierarchy changes, robustness
  regressions, parity edge-policy updates, and documentation.
- Hypothesis:
  - native fallback warnings should be carried as structured engine diagnostics
    instead of flat booleans on the score result, while Python remains only an
    adapter that renders diagnostics as `RuntimeWarning`s.
- Files changed:
  - `src/iso18571/scorer.hpp`;
  - `src/iso18571/scorer_impl.hpp`;
  - `src/iso18571/_core.cpp`;
  - `tests/iso18571_test_helpers.py`;
  - `tests/test_iso18571_robustness.py`;
  - `docs/iso18571-dtw-backends.md`;
  - this experiment log.
- Commands:
  - replaced fallback booleans on `ScoreResult` with enum-based
    component-local diagnostics;
  - reshaped `ScoreResult` around ISO components: corridor, phase, magnitude,
    slope, and top-level overall `R`;
  - split phase metadata into `PhaseAlignment` for window geometry and
    `PhaseCorrelation` for `rho_e`;
  - removed the intermediate internal `CorrelationState`/`CorrelationResult`
    pair and let selected phase candidates carry their own diagnostics;
  - mapped diagnostic enum codes to the existing Python `RuntimeWarning`
    messages at the `_core.cpp` boundary by iterating component diagnostics in
    ISO order;
  - centralized warning-message constants in the test helper and kept warning
    behavior in deterministic robustness regressions;
  - removed the temporary fuzz discovery module, Hypothesis dependency, fuzz
    marker, and Hypothesis profile because the discoveries are now promoted to
    permanent robustness behavior.
- Validation result:
  - `uv pip install -e .` passed;
  - the fuzz discovery profile was run once before removal and passed:
    `29 passed, 43 deselected`;
  - after removing fuzz/Hypothesis, `uv run --extra test python -m pytest -q`
    passed: `11 passed, 32 deselected`;
  - `uv run --extra test ruff check --fix .` passed;
  - `uv run --extra test ruff format .` passed with `21 files left
    unchanged`;
  - `uv run --extra test ruff check .` passed;
  - `uv run --extra test ruff format --check .` passed;
  - `uv run --extra test mypy iso18571 iso18571_reference tests` passed:
    `16 source files`;
  - `uv build --wheel` passed and produced
    `dist/iso18571-1.0.4-cp314-cp314-linux_x86_64.whl`;
  - `git diff --check` passed.
- Conclusion:
  - native result hierarchy now mirrors the ISO component structure while
    preserving the public Python API and warning messages;
  - warning behavior is covered by the deterministic robustness suite rather
    than a temporary fuzz suite.
- Next hypothesis:
  - if non-Python consumers become concrete, move curve validation and scoring
    into a separate reusable native target without changing the Python adapter
    contract.

## 2026-06-18 11:15 KST - Native-Only Benchmark Refresh

- Git status: dirty from the component-first diagnostics refactor, robustness
  regressions, docs, and README benchmark table updates.
- Hypothesis:
  - the native-only benchmark can refresh README native rows without rerunning
    the memory-heavy reference backend matrix.
- Files changed:
  - `README.md`;
  - this experiment log.
- Commands:
  - ran `uv run --extra test python -m pytest -q -m benchmark -k native
    --benchmark-json .benchmarks/iso18571-native-only/benchmarks.json`;
  - extracted load/runtime medians and peak RSS from the benchmark JSON;
  - updated only the README `native` rows, leaving reference backend rows from
    the previous full matrix untouched.
- Validation result:
  - native-only benchmark passed: `8 passed, 35 deselected`;
  - refreshed native load time, ms: `124.52`, `133.15`, `164.68`, `734.03`;
  - refreshed native peak RSS, MiB: `47.87`, `48.74`, `61.00`, `253.88`;
  - refreshed native runtime, ms: `0.23`, `2.37`, `36.02`, `612.62`.
- Conclusion:
  - the component-first diagnostics refactor preserves native benchmark scale;
    README now reflects the current native-only snapshot for this machine.
- Next hypothesis:
  - rerun the full benchmark matrix only when reference-backend rows need a
    fresh comparison snapshot.

## 2026-06-18 11:17 KST - Native Robustness Release Readiness

- Git status: dirty from component-first diagnostics, robustness regressions,
  README native benchmark refresh, documentation, and release-readiness checks.
- Hypothesis:
  - the component-first diagnostics refactor and deterministic robustness suite
    are ready for the next release after the normal validation gate, with the
    version bump handled separately.
- Files changed:
  - `README.md`;
  - native scorer/binding sources;
  - robustness/parity tests;
  - documentation and this experiment log.
- Commands:
  - split generated Annex fixtures into parity cases and robustness edge cases;
  - tightened generated parity so non-edge generated cases must score and match
    across all four implementations, with exceptions treated as failures;
  - moved native surface and short-curve behavior checks from parity into
    robustness;
  - prepared to rerun editable install, pytest, ruff, mypy, wheel build, and
    whitespace validation before committing.
- Validation result:
  - targeted downloaded Annex parity passed: `1 passed`;
  - targeted generated non-edge score-and-match parity passed: `1 passed`;
  - targeted robustness suite passed: `11 passed`;
  - `uv pip install -e .` passed and installed `iso18571==1.0.4`;
  - `uv run --extra test python -m pytest -q` passed:
    `13 passed, 32 deselected`;
  - `uv run --extra test ruff check --fix .` passed;
  - `uv run --extra test ruff format .` passed;
  - `uv run --extra test ruff check .` passed;
  - `uv run --extra test ruff format --check .` passed;
  - `uv run --extra test mypy iso18571 iso18571_reference tests` passed:
    `16 source files`;
  - `uv build --wheel` passed and produced
    `dist/iso18571-1.0.4-cp314-cp314-linux_x86_64.whl`;
  - `git diff --check` passed.
- Conclusion:
  - component-first diagnostics, robustness regressions, parity/robustness test
    separation, and native benchmark refresh are validated.
- Next hypothesis:
  - commit the implementation once validation is green; defer the version bump.

## 2026-06-18 11:40 KST - Generated Parity Corpus Cleanup

- Git status:
  - dirty from native diagnostics/robustness work, README benchmark
    refresh, generated parity corpus cleanup, notebook refresh, and docs.
- Hypothesis:
  - generated Annex data should be parity data by construction: every generated
    case should score successfully without warnings and match across all four
    backends; native edge behavior should live in explicit robustness tests.
- Files changed:
  - `tools/example_data.py`;
  - `tests/test_iso18571_parity.py`;
  - `tests/test_iso18571_robustness.py`;
  - `tests/iso18571_test_helpers.py`;
  - `examples/quickstart.ipynb`;
  - `docs/iso18571-dtw-backends.md`;
  - this experiment log.
- Commands:
  - removed zero/constant generated families and excluded `impulse`/`sparse_spikes`
    at `n=9` from generated Annex materialization;
  - bumped the generated cache version to `generated-v2`;
  - changed generated parity to use all generated cases directly and fail on
    warnings or exceptions;
  - moved former generated edge coverage into direct native robustness fixtures;
  - ran targeted downloaded Annex parity:
    `uv run --extra test python -m pytest -q tests/test_iso18571_parity.py::test_downloaded_annex_scores_match_official_and_parity`;
  - ran targeted generated parity:
    `uv run --extra test python -m pytest -q tests/test_iso18571_parity.py::test_generated_annex_scores_match_together tests/test_iso18571_parity.py::test_generated_annex_cases_are_parity_corpus`;
  - ran targeted robustness:
    `uv run --extra test python -m pytest -q tests/test_iso18571_robustness.py`;
  - refreshed generated example data:
    `uv run python tools/example_data.py --generate-annex`;
  - refreshed the notebook:
    `uv run --extra examples jupyter nbconvert --to notebook --execute --inplace examples/quickstart.ipynb`;
  - reran the full gate through editable install, full pytest, ruff, mypy,
    wheel build, and `git diff --check`.
- Validation result:
  - generated Annex count is now `102`;
  - targeted downloaded Annex parity passed: `1 passed`;
  - targeted generated parity passed: `2 passed`;
  - targeted robustness passed: `11 passed`;
  - `uv pip install -e .` passed and installed `iso18571==1.0.4`;
  - `uv run --extra test python -m pytest -q` passed:
    `14 passed, 32 deselected`;
  - `uv run --extra test ruff check --fix .` passed;
  - `uv run --extra test ruff format .` passed and reformatted one file;
  - `uv run --extra test ruff check .` passed;
  - `uv run --extra test ruff format --check .` passed;
  - `uv run --extra test mypy iso18571 iso18571_reference tests` passed:
    `16 source files`;
  - `uv build --wheel` passed and produced
    `dist/iso18571-1.0.4-cp314-cp314-linux_x86_64.whl`;
  - `git diff --check` passed.
- Conclusion:
  - generated Annex now means parity-safe no-warning/no-exception cases only;
    native robustness edge behavior is covered directly and no generated-case
    filtering remains in tests.
- Next hypothesis:
  - after one final diff review, commit the implementation with the version bump
    deferred.

## 2026-06-18 11:48 KST - Exact Perfect Phase Early Return

- Git status:
  - clean before the phase early-return edit.
- Hypothesis:
  - if the unshifted phase candidate has exact `rho_e == 1.0`, the remaining
    shifted phase scan cannot improve the selected alignment because phase
    replacement uses strict `>` and correlations are clamped to at most `1.0`.
- Files changed:
  - `src/iso18571/scorer_impl.hpp`;
  - this experiment log.
- Commands:
  - added an exact `result.correlation.rho_e == 1.0` return immediately after
    the unshifted phase candidate is computed and before phase-cache
    construction;
  - intentionally did not add tests, did not add a full-score fast path, did
    not use tolerance, and did not bump the package version;
  - ran the requested validation gate.
- Validation result:
  - `uv pip install -e .` passed and installed `iso18571==1.0.4`;
  - `uv run --extra test python -m pytest -q` passed:
    `14 passed, 32 deselected`;
  - `uv run --extra test ruff check --fix .` passed;
  - `uv run --extra test ruff format .` passed with `21 files left unchanged`;
  - `uv run --extra test ruff check .` passed;
  - `uv run --extra test ruff format --check .` passed;
  - `uv run --extra test mypy iso18571 iso18571_reference tests` passed:
    `16 source files`;
  - `uv build --wheel` passed and produced
    `dist/iso18571-1.0.4-cp314-cp314-linux_x86_64.whl`.
- Conclusion:
  - exact perfect unshifted phase matches now skip phase-cache construction and
    shifted phase scanning while preserving the selected unshifted result and
    downstream scoring path.
- Next hypothesis:
  - run benchmarks only if this micro-optimization needs a quantified runtime
    snapshot.

## 2026-06-18 11:51 KST - v1.0.5 Release Bump

- Git status:
  - dirty from the exact perfect phase early return and version bump.
- Hypothesis:
  - the phase early-return optimization and robustness/parity cleanup can ship
    as `1.0.5` after the normal validation gate.
- Files changed:
  - `pyproject.toml`;
  - `README.md`;
  - `examples/quickstart.ipynb`;
  - `src/iso18571/scorer_impl.hpp`;
  - this experiment log.
- Commands:
  - bumped package metadata and user-facing version examples from `1.0.4` to
    `1.0.5`;
  - refreshed the quickstart notebook after reinstalling the editable package;
  - reran the requested validation gate before committing.
- Validation result:
  - `uv pip install -e .` passed and installed `iso18571==1.0.5`;
  - `uv run --extra examples jupyter nbconvert --to notebook --execute --inplace examples/quickstart.ipynb`
    passed;
  - `uv run --extra test python -m pytest -q` passed:
    `14 passed, 32 deselected`;
  - `uv run --extra test ruff check --fix .` passed;
  - `uv run --extra test ruff format .` passed with `21 files left unchanged`;
  - `uv run --extra test ruff check .` passed;
  - `uv run --extra test ruff format --check .` passed;
  - `uv run --extra test mypy iso18571 iso18571_reference tests` passed:
    `16 source files`;
  - `uv build --wheel` passed and produced
    `dist/iso18571-1.0.5-cp314-cp314-linux_x86_64.whl`.
- Conclusion:
  - local validation is green for `1.0.5`.
- Next hypothesis:
  - commit and push `main`.

## 2026-06-18 19:11 KST - Public ScoreComponents Typing

- Git status:
  - dirty with public API typing edits in `iso18571/` and updated robustness
    tests before this log entry.
- Hypothesis:
  - exposing `ScoreComponents` as a public typed score result while typing the
    private native `_score_components` hook in `_core.pyi` can remove the
    dynamic cast from `rating.py` and keep tests strict under mypy.
- Files changed:
  - `iso18571/rating.py`;
  - `iso18571/__init__.py`;
  - `iso18571/_core.pyi`;
  - `tests/test_iso18571_robustness.py`;
  - this experiment log.
- Commands:
  - added `ScoreComponents` as a public `TypedDict` export;
  - typed `_score_components` in the native extension stub;
  - imported `_score_components` directly in `rating.py` and returned a copied
    typed score result from `.scores`;
  - updated robustness tests for the new public export and current float32
    shifted-curve dtype behavior;
  - marked robustness score-key iteration as `Final` so mypy treats keys as
    typed-dict literals.
- Validation result:
  - `uv run --extra test ruff check --fix .` passed;
  - `uv run --extra test ruff format .` passed with
    `21 files left unchanged`;
  - `uv run --extra test ruff check .` passed;
  - `uv run --extra test ruff format --check .` passed;
  - `uv run --extra test mypy iso18571 iso18571_reference tests` passed:
    `16 source files`;
  - `uv pip install -e .` passed and installed `iso18571==1.0.5`;
  - `uv run --extra test python -m pytest -q` passed:
    `17 passed, 32 deselected`.
- Conclusion:
  - package typing, the native stub, and robustness tests agree on the public
    `ScoreComponents` surface.
- Next hypothesis:
  - commit the public score-component typing cleanup after the final diff
    whitespace check.

## 2026-06-18 19:23 KST - Native Score Parameter Defaults Cleanup

- Git status:
  - dirty with native scorer parameter-carrier cleanup.
- Hypothesis:
  - keeping public defaults only in `ISO18571.__init__` and removing native
    `ScoreParams` defaults eliminates duplicate production defaults without
    changing scoring behavior.
- Files changed:
  - `src/iso18571/_core.cpp`;
  - `src/iso18571/scorer.hpp`;
  - `src/iso18571/scorer_impl.hpp`;
  - `src/iso18571/validation.cpp`;
  - `src/iso18571/validation.hpp`;
  - this experiment log.
- Commands:
  - removed default member initializers from native `ScoreParams`;
  - removed unused native `a_0` and `b_0` default constants;
  - kept Python constructor defaults inline as the public API owner;
  - kept `dt` out of `ScoreParams` and passed the validated time interval
    separately to native slope scoring.
- Validation result:
  - `uv pip install -e .` passed and installed `iso18571==1.0.5`;
  - `uv run --extra test python -m pytest -q tests/test_iso18571_robustness.py`
    passed: `14 passed`;
  - `uv run --extra test ruff check --fix .` passed;
  - `uv run --extra test ruff format .` passed with
    `21 files left unchanged`;
  - `uv run --extra test ruff check .` passed;
  - `uv run --extra test ruff format --check .` passed with
    `21 files already formatted`;
  - `uv run --extra test mypy iso18571 iso18571_reference tests` passed:
    `16 source files`;
  - `uv run --extra test python -m pytest -q` passed:
    `17 passed, 32 deselected`;
  - `git diff --check` passed.
- Conclusion:
  - duplicate native scoring defaults are removed, `dt` remains derived from
    validated curve time columns, and existing validation is green.
- Next hypothesis:
  - commit this native cleanup after final review.

## 2026-06-19 10:57 KST - Native Engine Source Rename

- Git status:
  - dirty with native scorer source/header files renamed to engine naming,
    CMake source references updated, and wording updates in binding metadata and
    TODO notes.
- Hypothesis:
  - renaming the native C++ implementation files from scorer to engine can
    clarify the public production engine shape without changing ISO/TS 18571
    scoring behavior or dispatch behavior.
- Files changed:
  - `.clang-format`;
  - `CMakeLists.txt`;
  - `TODO.md`;
  - `src/iso18571/_core.cpp`;
  - `src/iso18571/dispatch.cpp`;
  - `src/iso18571/validation.hpp`;
  - `src/iso18571/scorer.hpp` -> `src/iso18571/engine.hpp`;
  - `src/iso18571/scorer_impl.hpp` -> `src/iso18571/engine_impl.hpp`;
  - `src/iso18571/scorer_v1.cpp` -> `src/iso18571/engine_v1.cpp`;
  - `src/iso18571/scorer_v2.cpp` -> `src/iso18571/engine_v2.cpp`;
  - `src/iso18571/scorer_v3.cpp` -> `src/iso18571/engine_v3.cpp`;
  - `src/iso18571/scorer_v4.cpp` -> `src/iso18571/engine_v4.cpp`;
  - this experiment log.
- Commands:
  - compared the renamed headers against the previous `scorer*` files;
  - confirmed `engine.hpp` is identical to the old public native header;
  - confirmed `engine_impl.hpp` only changes include/error text and formatting;
  - rebuilt the editable native package;
  - ran the normal validation gate plus the requested native benchmark filter.
- Validation result:
  - `uv run --extra test ruff check --fix .` passed;
  - `uv run --extra test ruff format .` passed with
    `21 files left unchanged`;
  - `uv run --extra test ruff check .` passed;
  - `uv run --extra test ruff format --check .` passed with
    `21 files already formatted`;
  - `uv run --extra test mypy iso18571 iso18571_reference tests` passed:
    `16 source files`;
  - `uv pip install -e .` passed and installed `iso18571==1.0.5`;
  - `uv run --extra test python -m pytest -q` passed:
    `17 passed, 32 deselected`;
  - `uv run --extra test python -m pytest -q -m benchmark -k native --benchmark-json .benchmarks/iso18571-readme/benchmarks-native.json`
    passed: `8 passed, 41 deselected`.
- Conclusion:
  - the native engine source rename builds, passes parity/robustness coverage,
    and preserves the benchmarked native path.
- Next hypothesis:
  - commit the rename after the final whitespace and staged diff checks.

## 2026-06-19 11:19 KST - Native Snapshot Before GIL Release

- Git status:
  - dirty with CMake, README, Python shim/stub, native engine/binding, and
    backend-string expectation updates.
- Hypothesis:
  - copying validated NumPy curves into C++-owned split `time`/`value` vectors
    before releasing the GIL can make native scoring independent from
    Python-owned buffer mutation while letting scoring use `std::span` slices.
- Files changed:
  - `CMakeLists.txt`;
  - `README.md`;
  - `iso18571/_core.pyi`;
  - `iso18571/rating.py`;
  - `src/iso18571/_core.cpp`;
  - `src/iso18571/engine.hpp`;
  - `src/iso18571/engine_impl.hpp`;
  - `tests/test_iso18571_robustness.py`;
  - this experiment log.
- Commands:
  - refactored scoring interfaces from raw strided curve/value views to
    span-backed `SignalView` and `ArrayView`;
  - added native split-vector curve snapshots at the pybind boundary before
    `py::gil_scoped_release`;
  - moved shifted-curve output construction into the native snapshot path;
  - bumped the extension from C++17 to C++20 and added a configure-time
    `std::span` check.
- Validation result:
  - `uv pip install -e .` passed and installed `iso18571==1.0.5`;
  - `uv run --extra test python -m pytest -q` passed:
    `17 passed, 32 deselected`;
  - initial native benchmark command failed before collection because
    `.benchmarks/iso18571-native/` did not exist;
  - after `mkdir -p .benchmarks/iso18571-native`,
    `uv run --extra test python -m pytest -q -m benchmark -k native tests/test_iso18571_benchmarks.py --benchmark-json .benchmarks/iso18571-native/benchmarks.json`
    passed: `8 passed, 24 deselected`;
  - `uv run --extra test mypy iso18571 iso18571_reference tests` passed:
    `16 source files`;
  - `git diff --check` passed.
- Conclusion:
  - scoring now runs from C++-owned split vectors after GIL release, shifted
    outputs come from the same snapshot, and current tests/native benchmarks are
    green.
- Next hypothesis:
  - compare native benchmark memory rows against the previous run to quantify
    the expected full-curve snapshot overhead.

## 2026-06-19 13:23 KST - Shifted Output as Indices Only

- Git status:
  - dirty with production scorer binding/wrapper changes, native engine cleanup,
    parity helper updates, robustness surface assertions, backend docs, and
    quickstart notebook wording/output cleanup.
- Hypothesis:
  - returning only phase-alignment indices from the production scorer can remove
    eager shifted-curve array ownership and the redundant native time snapshot
    while preserving scoring, start/length parity, and official shifted-value
    validation through source curve slicing.
- Files changed:
  - `docs/iso18571-dtw-backends.md`;
  - `examples/quickstart.ipynb`;
  - `iso18571/_core.pyi`;
  - `iso18571/rating.py`;
  - `src/iso18571/_core.cpp`;
  - `src/iso18571/engine.hpp`;
  - `src/iso18571/engine_impl.hpp`;
  - `tests/iso18571_test_helpers.py`;
  - `tests/test_iso18571_robustness.py`;
  - this experiment log.
- Commands:
  - removed shifted `(n, 2)` arrays from native `_score_components` and the
    public `ISO18571` object;
  - removed `OwnedCurve::time`, `SignalView::time_values`, shifted-array
    helpers, the `magnitude_error_impl` wrapper, and the file-scope
    `direction_index` helper;
  - updated parity helpers to compare scores and phase indices only, while
    checking official Annex shifted values by slicing the original curves;
  - updated quickstart guidance to derive aligned curves from
    `reference_start`, `comparison_start`, and `shift_length`;
  - rebuilt the editable native package and ran the requested native-only
    benchmark.
- Validation result:
  - `uv run --extra test ruff check --fix .` passed;
  - `uv run --extra test ruff format .` passed with one file reformatted;
  - `uv run --extra test ruff check .` passed;
  - `uv run --extra test ruff format --check .` passed with
    `21 files already formatted`;
  - `uv run --extra test mypy iso18571 iso18571_reference tests` passed:
    `16 source files`;
  - `uv run --extra test python -m pytest -q` passed:
    `17 passed, 32 deselected`;
  - `uv pip install -e .` passed and installed `iso18571==1.0.5`;
  - `uv run --extra test python -m pytest -q tests/test_iso18571_benchmarks.py -m benchmark -k native --benchmark-json .benchmarks/iso18571-native-only/benchmarks.json`
    passed: `8 passed, 24 deselected`;
  - `git diff --check` passed.
- Conclusion:
  - the production scorer now records shifted alignment by indices only, native
    scoring keeps only the value snapshot needed for GIL-free scoring, and
    validation plus native-only benchmarks are green.
- Next hypothesis:
  - commit this indices-only shifted-output API change.

## 2026-06-19 13:39 KST - Native Views To Spans

- Git status:
  - dirty with native scorer refactor changes in `_core.cpp`, `engine.hpp`, and
    `engine_impl.hpp`.
- Hypothesis:
  - removing `ArrayView`, `CurveView`, `SignalView`, `CurveDType`, and
    `NativeCurve` can simplify the native engine interface while preserving
    strided NumPy validation, float32 tolerance, numeric force-casting, and
    GIL-free scoring from owned value snapshots.
- Files changed:
  - `src/iso18571/_core.cpp`;
  - `src/iso18571/engine.hpp`;
  - `src/iso18571/engine_impl.hpp`;
  - `tests/test_iso18571_robustness.py`;
  - this experiment log.
- Commands:
  - replaced native scorer function declarations and definitions with
    `std::span<const double>` reference/comparison inputs;
  - rewired engine implementation helpers to use span size, indexing, and
    `subspan` for aligned slices;
  - localized NumPy dtype, shape, stride, time-grid, and signal-value validation
    in `_core.cpp`;
  - copied only validated signal values into owned `std::vector<double>`
    snapshots before releasing the GIL.
  - added robustness coverage for strided float32/float64 inputs and non-float
    numeric force-casting.
- Validation result:
  - `uv pip install -e .` passed and installed `iso18571==1.0.5`;
  - `uv run --extra test python -m pytest -q tests/test_iso18571_robustness.py`
    passed: `16 passed`;
  - `uv run --extra test python -m pytest -q tests/test_iso18571_parity.py`
    passed: `3 passed`;
  - `uv run --extra test ruff check --fix .` passed;
  - `uv run --extra test ruff format .` passed with `21 files left unchanged`;
  - `uv run --extra test ruff check .` passed;
  - `uv run --extra test ruff format --check .` passed with
    `21 files already formatted`;
  - `uv run --extra test mypy iso18571 iso18571_reference tests` passed:
    `16 source files`;
  - `git diff --check` passed.
- Conclusion:
  - the native scorer now exposes span-only read paths below the pybind boundary,
    while Python-facing behavior and parity/robustness checks remain green.
- Next hypothesis:
  - run the native benchmark suite to confirm the refactor does not change
    steady-state timing or memory behavior.

## 2026-06-19 13:43 KST - Span Refactor Native Benchmark

- Git status:
  - dirty with span refactor source changes, robustness test coverage, and this
    experiment log update.
- Hypothesis:
  - after the span refactor, rebuilding the native extension and running the
    native-only benchmark should preserve benchmark collection success and
    produce normal runtime/load-memory rows.
- Files changed:
  - this experiment log;
  - benchmark JSON written under `.benchmarks/iso18571-native-only/` (ignored
    generated output).
- Commands:
  - `mkdir -p .benchmarks/iso18571-native-only`;
  - `uv pip install -e .`;
  - `uv run --extra test python -m pytest -q tests/test_iso18571_benchmarks.py -m benchmark -k native --benchmark-json .benchmarks/iso18571-native-only/benchmarks.json`.
- Validation result:
  - editable rebuild passed and installed `iso18571==1.0.5`;
  - native benchmark passed: `8 passed, 24 deselected`;
  - benchmark JSON was written to
    `.benchmarks/iso18571-native-only/benchmarks.json`;
  - runtime median rows were about `215 us` for n=512, `2.245 ms` for n=2048,
    `33.877 ms` for n=8192, and `578.478 ms` for n=32768.
- Conclusion:
  - the rebuilt span-refactor extension collects the native benchmark suite
    successfully, including runtime and load-memory benchmark rows.
- Next hypothesis:
  - compare the generated benchmark JSON against the pre-refactor native-only
    benchmark data if a historical artifact is available.

## 2026-06-19 13:57 KST - Shared Native DoubleSpan Alias

- Git status:
  - dirty with native alias updates in `_core.cpp`, `engine.hpp`, and
    `engine_impl.hpp`.
- Hypothesis:
  - moving the native `DoubleSpan` alias into `engine.hpp` is safe because
    `engine.hpp` already includes `<span>` and declares the span-based scorer
    function type and variant entry points.
- Files changed:
  - `src/iso18571/_core.cpp`;
  - `src/iso18571/engine.hpp`;
  - `src/iso18571/engine_impl.hpp`;
  - this experiment log.
- Commands:
  - moved `using DoubleSpan = std::span<const double>;` into the
    `iso18571` namespace in `engine.hpp`;
  - replaced native scorer declarations, the variant definition, and pybind
    boundary local span variables with the shared alias;
  - removed redundant direct `<span>` includes from `_core.cpp` and
    `engine_impl.hpp`;
  - `uv pip install -e .`;
  - `uv run --extra test ruff check --fix .`;
  - `uv run --extra test ruff format .`;
  - `uv run --extra test ruff check .`;
  - `uv run --extra test ruff format --check .`;
  - `uv run --extra test mypy iso18571 iso18571_reference tests`;
  - `uv run --extra test python -m pytest -q`;
  - `git diff --check`.
- Validation result:
  - editable rebuild passed and installed `iso18571==1.0.6`;
  - ruff fix/check and format/check passed with no file changes;
  - mypy passed: `16 source files`;
  - pytest passed: `19 passed, 32 deselected`;
  - `git diff --check` passed.
- Conclusion:
  - the alias can live safely in `engine.hpp`; the native implementation and
    pybind boundary now share that single declaration without changing scorer
    behavior.
- Next hypothesis:
  - keep future native type aliases in `engine.hpp` only when they are part of
    the internal scorer interface shared across translation units.

## 2026-06-19 14:24 KST - Production Artifact Cleanup

- Git status:
  - dirty with packaging metadata, public typing, reference validation, Annex
    loader strictness, release artifact validation, robustness/Annex test helper
    updates, and this experiment log entry;
  - an unrelated `.gitignore` change was present and left unstaged.
- Hypothesis:
  - excluding reference/test material from sdists, validating release archives,
    aligning reference `k_z` with the native positive-integer contract, and
    typing public curve inputs as `ArrayLike` can clean up the production
    packaging and runtime contract without changing scorer behavior.
- Files changed:
  - `pyproject.toml`;
  - `iso18571/rating.py`;
  - `iso18571/_core.pyi`;
  - `iso18571_reference/_common.py`;
  - `tools/build_wheels.py`;
  - `tools/example_data.py`;
  - `tests/iso18571_annex.py`;
  - `tests/test_iso18571_robustness.py`;
  - this experiment log.
- Commands:
  - `.venv/bin/ruff check .`;
  - `.venv/bin/ruff format tools/build_wheels.py tools/example_data.py`;
  - `.venv/bin/ruff check .`;
  - `.venv/bin/ruff format --check .`;
  - `.venv/bin/mypy iso18571 iso18571_reference tests tools/build_wheels.py`;
  - `.venv/bin/python -m pytest -q`;
  - `env UV_CACHE_DIR=/tmp/iso18571-uv-cache uv build --sdist`;
  - `env UV_CACHE_DIR=/tmp/iso18571-uv-cache uv build --wheel`;
  - `env UV_CACHE_DIR=/tmp/iso18571-uv-cache uv run --no-project --with cibuildwheel python tools/build_wheels.py --platform all --cache-dir /tmp/iso18571-wheel-build --xwin-root /tmp/iso18571-xwin`.
- Validation result:
  - final ruff check and format check passed;
  - mypy passed for `17 source files`;
  - pytest passed: `19 passed, 32 deselected`;
  - sdist and local CPython 3.13 Linux wheel builds passed;
  - full wheel-builder run passed for Linux CPython 3.12/3.13/3.14
    manylinux wheels and Windows CPython 3.12/3.13/3.14 cross-built wheels;
  - release artifact validation passed for `8` current-version artifacts with
    no `iso18571_reference`, `tests`, or `ref` archive members.
- Conclusion:
  - production sdists and wheels now have explicit clean-package intent and an
    automated archive guard, while the public Python contract accepts
    NumPy-compatible inputs and the reference scorer follows the documented
    positive-integer `k_z` behavior.
- Next hypothesis:
  - handle DTW allocation failure behavior separately after deciding whether a
    deliberate memory check is preferable to relying on allocator failure.

## 2026-06-19 14:31 KST - Review Float32 Time Grid Probe

- Git status:
  - dirty before this review with `src/iso18571/engine_impl.hpp` modified;
  - this experiment log entry was added for the review probe.
- Hypothesis:
  - the native validator's float32-specific time-grid tolerance might reject
    ordinary uniform float32 grids because it uses a fixed absolute tolerance.
- Files changed:
  - this experiment log.
- Commands:
  - `uv run python - <<'PY' ...` to score identical float32 curves over
    `dt` values `1e-4`, `1e-3`, `1e-2`, `0.1`, `1.0`, and `1000.0` at lengths
    `32`, `1000`, and `100000`;
  - `uv run python - <<'PY' ...` to inspect float32 step-error ranges for the
    same grids.
- Validation result:
  - `dt=0.0001` passed at `n=32` but failed at `n=1000`;
  - `dt=0.001`, `dt=0.01`, and `dt=0.1` failed already at `n=32`;
  - integer-valued `dt` grids passed in the sampled cases;
  - step-error inspection showed float32 uniform grids can exceed the current
    `1e-9` absolute tolerance even when generated by `np.arange(...,
    dtype=np.float32) * np.float32(dt)`.
- Conclusion:
  - the advertised float32 input acceptance is narrower than users will expect;
    the validator needs a scale-aware tolerance or a cast-and-validate policy
    that matches NumPy-compatible input behavior.
- Next hypothesis:
  - add explicit regression tests for representative float32 sample intervals
    before changing the native tolerance rule.

## 2026-06-19 14:44 KST - DTW Direction Bitplane Storage

- Git status:
  - dirty with a pre-existing experiment-log entry and the direction-storage
    experiment in `src/iso18571/engine_impl.hpp`.
- Hypothesis:
  - sub-byte direction storage may reduce DTW memory traffic enough to improve
    large-signal runtime, but byte-lane 2-bit packing may lose to bit operation
    overhead.
- Files changed:
  - `README.md`;
  - `src/iso18571/engine_impl.hpp`;
  - this experiment log.
- Commands:
  - reused same-machine native byte-cell baseline at
    `.benchmarks/iso18571-bitpack-baseline/benchmarks.json`;
  - reused 2-bit byte-lane result at
    `.benchmarks/iso18571-bitpack-packed/benchmarks.json`;
  - built and benchmarked 2-bit `uint64_t` lanes:
    `.benchmarks/iso18571-bitpack-wordlanes/benchmarks.json`;
  - built and benchmarked branchy streaming `uint64_t` bitplanes:
    `.benchmarks/iso18571-bitpack-bitplanes/benchmarks.json`;
  - built and benchmarked direct `uint64_t` bitplanes:
    `.benchmarks/iso18571-bitpack-bitplanes-direct/benchmarks.json`;
  - built and benchmarked branchless streaming `uint64_t` bitplanes:
    `.benchmarks/iso18571-bitpack-bitplanes-branchless/benchmarks.json`;
  - each candidate rebuild used `uv pip install -e .`;
  - each candidate passed `uv run --extra test python -m pytest -q`;
  - final candidate passed `git diff --check`.
- Validation result:
  - final branchy streaming bitplane code passed: `19 passed, 32 deselected`;
  - final `git diff --check` passed.
- Benchmark result:
  - runtime medians versus byte cells:
    - 512 samples: byte cells best, bitplanes `+8.0%`;
    - 2048 samples: byte cells best, bitplanes `+1.5%`;
    - 8192 samples: branchy streaming bitplanes best, `-12.1%`;
    - 32768 samples: branchy streaming bitplanes best, `-18.0%`;
  - peak RSS versus byte cells:
    - 8192 samples: bitplanes saved about `9.5 MiB`;
    - 32768 samples: bitplanes saved about `151.8 MiB`;
  - 2-bit byte lanes and 2-bit `uint64_t` lanes were slower than byte cells at
    8192 and 32768 despite the same memory savings;
  - direct bitplanes were faster than byte cells at 8192 and 32768, but slower
    than the streaming writer;
  - branchless streaming bitplanes were close to branchy streaming at large
    sizes, but branchy was consistently faster in this run.
- Conclusion:
  - keep the branchy streaming `uint64_t` bitplane direction storage as the
    current candidate: it trades a small short-signal regression for a clear
    large-signal runtime and memory win.
- Next hypothesis:
  - if the short-signal regression matters, add a size threshold that keeps
    byte-cell directions below about 4096 samples and uses bitplanes for larger
    cases.

## 2026-06-19 14:50 KST - DTW Bitplane Word Size Sweep

- Git status:
  - dirty with the streaming bitplane direction candidate, previous experiment
    log additions, and this word-size sweep.
- Hypothesis:
  - narrower bitplane words might improve the streaming writer by using cheaper
    stores or less register pressure, while wider words might reduce flushes and
    backtracking index work.
- Files changed:
  - `src/iso18571/engine_impl.hpp`;
  - this experiment log.
- Commands:
  - refactored bitplane storage to a `DirectionWord` alias plus
    `DIRECTION_WORD_BITS`;
  - benchmarked streaming bitplanes with `std::uint8_t`, `std::uint16_t`,
    `std::uint32_t`, `std::uint64_t`, and GNU/Clang `unsigned __int128`;
  - wrote benchmark JSON to `.benchmarks/iso18571-bitplanes-wordsize-u8`,
    `u16`, `u32`, `u64`, and `u128`;
  - each candidate rebuild used `uv pip install -e .`;
  - each candidate passed `uv run --extra test python -m pytest -q`;
  - final `std::uint64_t` candidate passed `git diff --check`.
- Validation result:
  - final `std::uint64_t` candidate passed: `19 passed, 32 deselected`;
  - final `git diff --check` passed.
- Benchmark result:
  - runtime medians versus byte cells at 32768 samples:
    - `uint8_t`: `0.4721s`, `-17.8%`;
    - `uint16_t`: `0.5603s`, `-2.5%`;
    - `uint32_t`: `0.5055s`, `-12.0%`;
    - `uint64_t`: `0.4709s`, `-18.0%`;
    - `unsigned __int128`: `0.4762s`, `-17.1%`;
  - runtime medians at 8192 samples:
    - `uint8_t`: `0.0299s`, `-11.0%`;
    - `uint16_t`: `0.0352s`, `+4.8%`;
    - `uint32_t`: `0.0323s`, `-4.1%`;
    - `uint64_t`: `0.0298s`, `-11.3%`;
    - `unsigned __int128`: `0.0303s`, `-10.0%`;
  - `uint64_t` was the best target-size runtime in this sweep and remains
    portable across GCC/Clang/MSVC;
  - `unsigned __int128` built and passed on this Linux compiler, but is not a
    portable production type for MSVC wheels and did not beat `uint64_t`.
- Conclusion:
  - keep `std::uint64_t` for streaming bitplane direction storage.
- Next hypothesis:
  - any further direction-storage tuning should focus on a byte-cell/bitplane
    threshold for short inputs rather than wider or narrower bitplane words.

## 2026-06-19 14:58 KST - Wheel-Only Release Policy

- Git status:
  - dirty with README release-policy text, release artifact validation, and
    this experiment log entry.
- Hypothesis:
  - making releases wheel-only will let unsupported architectures fail with no
    compatible distribution instead of falling through to an unsupported native
    source build.
- Files changed:
  - `README.md`;
  - `tools/build_wheels.py`;
  - this experiment log.
- Commands:
  - updated README prerequisites/building text to state the wheel-only
    Linux x86_64 and Windows AMD64 support matrix;
  - removed release instructions that suggested publishing an sdist;
  - changed `validate_release_artifacts` to reject current-version sdists and
    validate only wheels;
  - `uv run python - <<'PY' ...` against current `dist/`, which rejected
    `iso18571-1.0.6.tar.gz` as expected;
  - `uv run python - <<'PY' ...` against a temporary wheel-only directory,
    which validated successfully;
  - `uv run --extra test ruff check .`;
  - `uv run --extra test ruff format --check .`;
  - `uv run --extra test mypy iso18571 iso18571_reference tests tools/build_wheels.py`;
  - `uv run --extra test python -m pytest -q`;
  - `git diff --check`.
- Validation result:
  - no tests were added;
  - release validator rejected the existing current-version sdist in `dist/`;
  - release validator accepted a temporary directory containing one current
    wheel and no sdist;
  - ruff check passed;
  - ruff format check passed: `21 files already formatted`;
  - mypy passed for `17 source files`;
  - pytest passed: `19 passed, 32 deselected`;
  - `git diff --check` passed.
- Conclusion:
  - release tooling and docs now encode the wheel-only policy needed for
    unsupported architecture fast-fail behavior.
- Next hypothesis:
  - before publishing, remove any current-version sdists from `dist/` and from
    the public release file set.

## 2026-06-19 15:04 KST - Flat DTW Bitplane Direction Writes

- Git status:
  - clean at start after wheel-only release policy changes were already present.
- Hypothesis:
  - the bitplane direction write path can match the repo's flat helper style by
    inlining the streaming write state into the DTW row loop, without changing
    storage layout or scoring behavior.
- Files changed:
  - `src/iso18571/engine_impl.hpp`;
  - this experiment log.
- Commands:
  - removed the `DirectionWord` alias and used `std::uint64_t` directly for the
    direction bitplane vector and row-local words;
  - removed `BitplaneDirectionWriter` and moved its low/high word, mask, dirty,
    and flush logic directly into `compute_directions_index_incremental`.
  - `uv pip install -e .`;
  - `uv run --extra test ruff check --fix .`;
  - `uv run --extra test ruff format .`;
  - `uv run --extra test ruff check .`;
  - `uv run --extra test ruff format --check .`;
  - `uv run --extra test mypy iso18571 iso18571_reference tests`;
  - `uv run --extra test python -m pytest -q`;
  - `uv run --extra test python -m pytest -q tests/test_iso18571_benchmarks.py -m benchmark -k native --benchmark-warmup off --benchmark-min-rounds 1 --benchmark-max-time 0.05 --benchmark-quiet --benchmark-json .benchmarks/iso18571-bitplanes-flat/benchmarks.json`.
  - updated the README native benchmark snapshot from
    `.benchmarks/iso18571-bitplanes-flat/benchmarks.json`.
- Validation result:
  - editable build passed and installed `iso18571==1.0.6`;
  - ruff check/fix and format/check passed with no file changes;
  - mypy passed for `16 source files`;
  - pytest passed: `19 passed, 32 deselected`;
  - native-only benchmark passed: `8 passed, 24 deselected`.
- Benchmark result:
  - flat bitplane runtime median at 8192 samples: `0.0299s`, `-11.1%`
    versus byte cells, peak RSS `49.7 MiB`, `-9.4 MiB`;
  - flat bitplane runtime median at 32768 samples: `0.4754s`, `-17.3%`
    versus byte cells, peak RSS `99.3 MiB`, `-151.8 MiB`;
  - the previous writer-shaped `uint64_t` bitplane run was slightly faster at
    target sizes (`0.0298s` at 8192 and `0.4709s` at 32768), but the flat
    refactor preserved the material speed and memory win over byte cells.
- Conclusion:
  - the writer abstraction is removed from the native DTW direction path while
    retaining the selected two-plane `std::uint64_t` storage strategy.

## 2026-06-19 15:20 KST - Pre-Commit Quality Gate

- Git status:
  - dirty at start with a pre-existing `src/iso18571/engine_impl.hpp` edit;
  - hook setup changed `.pre-commit-config.yaml`, `.clang-format`,
    `pyproject.toml`, `README.md`, `AGENTS.md`, and this log.
- Hypothesis:
  - versioned pre-commit hooks can replace manually running Ruff, Mypy, and
    clang-format before routine commits while preserving the repo's existing
    quality gate commands.
- Files changed:
  - `.pre-commit-config.yaml`;
  - `.clang-format`;
  - `pyproject.toml`;
  - `README.md`;
  - `AGENTS.md`;
  - this experiment log.
- Commands:
  - `uv add --optional test pre-commit clang-format`;
  - `uv run --extra test clang-format --version`;
  - `uv run --extra test pre-commit --version`;
  - `uv run --extra test pre-commit validate-config`;
  - `uv run --extra test clang-format --dry-run -Werror ...`;
  - attempted `uv add --optional test 'clang-format>=23'`, which failed
    because PyPI only had `clang-format<=22.1.5`;
  - removed unsupported `.clang-format` key
    `SpaceBeforeEnumUnderlyingTypeColon`, which is unused by the current native
    sources;
  - `uv run --extra test pre-commit run --all-files`;
  - `uv run --extra test pre-commit install`;
  - `git diff --check`.
- Validation result:
  - dependency-provided tools report `clang-format version 22.1.5` and
    `pre-commit 4.6.0`;
  - `pre-commit validate-config` passed;
  - dependency-provided `clang-format --dry-run -Werror` passed for all native
    sources after the config compatibility change;
  - `pre-commit run --all-files` passed Ruff fix, Ruff format, Ruff check,
    Ruff format check, Mypy, clang-format, and staged whitespace hooks;
  - local Git hook installed at `.git/hooks/pre-commit`;
  - `git diff --check` passed.
- Conclusion:
  - the repo now has a versioned pre-commit gate that runs the existing Ruff and
    Mypy commands through `uv --extra test`, formats native files with the
    dependency-provided clang-format, and checks staged whitespace.
- Next hypothesis:
  - CI can use `uv run --extra test pre-commit run --all-files` as the same
    quality gate entry point used by local commits.

## 2026-06-19 15:24 KST - Direct DTW Magnitude Accumulation

- Git status:
  - clean at start of this change;
  - dirty after replacing DTW direction storage in `src/iso18571/engine_impl.hpp`
    and adding this log entry.
- Hypothesis:
  - production magnitude scoring does not need to store the DTW predecessor
    direction matrix; carrying the selected path's magnitude numerator and
    denominator through the rolling DTW recurrence should preserve scores while
    cutting large-input peak RSS.
- Files changed:
  - `README.md`;
  - `src/iso18571/engine_impl.hpp`;
  - this experiment log.
- Commands:
  - removed `DtwState`, direction constants, bitplane helpers, direction writes,
    and backtracking from the production magnitude path;
  - added `magnitude_error_from_dtw`, which keeps rolling rows for cumulative
    squared cost, magnitude numerator, and magnitude denominator;
  - preserved predecessor selection order as vertical, horizontal, diagonal with
    strict `<` replacement;
  - `uv pip install -e .`;
  - `uv run --extra test pre-commit run --all-files` failed once because
    `clang-format` modified `src/iso18571/engine_impl.hpp`;
  - reran `uv run --extra test pre-commit run --all-files`;
  - `uv run --extra test python -m pytest -q`;
  - `mkdir -p .benchmarks/iso18571-direct-magnitude`;
  - `uv run --extra test python -m pytest -q tests/test_iso18571_benchmarks.py
    -m benchmark -k native --benchmark-warmup off --benchmark-min-rounds 1
    --benchmark-max-time 0.05 --benchmark-quiet --benchmark-json
    .benchmarks/iso18571-direct-magnitude/benchmarks.json`;
  - updated the README native benchmark snapshot from
    `.benchmarks/iso18571-direct-magnitude/benchmarks.json`;
  - `git diff --check`.
- Validation result:
  - no tests were added;
  - editable build passed and installed `iso18571==1.0.6`;
  - final pre-commit all-files run passed Ruff, Mypy, clang-format, whitespace,
    and config validation hooks;
  - pytest passed: `19 passed, 32 deselected`;
  - native-only benchmark passed: `8 passed, 24 deselected`;
  - `git diff --check` passed.
- Benchmark result:
  - 8192-sample native runtime median: `0.0299s`, peak RSS `46.5 MiB`;
  - 32768-sample native runtime median: `0.4550s`, peak RSS `49.7 MiB`;
  - compared with the previous README snapshot, 32768-sample peak RSS dropped
    from about `99.3 MiB` to about `49.7 MiB`.
- Conclusion:
  - direct DTW magnitude accumulation removes the production direction matrix
    without changing the public scorer or existing parity tests.
- Next hypothesis:
  - remaining large-signal runtime work should focus on phase cross-correlation,
    not magnitude path storage.

## 2026-06-19 16:16 KST - FFT Phase Product Backend Shootout

- Git status:
  - dirty from experimental phase-product backend selectors in `CMakeLists.txt`
    and `src/iso18571/engine_impl.hpp`;
  - ignored scratch sources, builds, harnesses, and CSV results under
    `.benchmarks/fft-candidates/`.
- Hypothesis:
  - computing all phase lag products with FFT convolution can remove the
    remaining quadratic phase-product scan while preserving Annex parity if the
    final selected and near-tied candidates are recomputed with the direct
    product path.
- Files changed:
  - `CMakeLists.txt`;
  - `src/iso18571/engine_impl.hpp`;
  - this experiment log.
- Commands:
  - added opt-in `ISO18571_PHASE_BACKEND` CMake values: `direct`, `pocketfft`,
    `ducc`, `fftw`, and `mkl`, with `direct` as the default;
  - fetched pinned scratch sources:
    - pocketfft `5f27d5a8f51c5c25030cb22abf434decc9faf0ff`;
    - DUCC `8c28af6f4e3513c3b7922eab625fb0ad56152fd9`;
  - built and parity-tested each backend with `uv pip install -e .
    --reinstall --config-settings=...` followed by `.venv/bin/python -m pytest
    -q tests/test_iso18571_parity.py`;
  - built a scratch `-O3` C++ phase harness at
    `.benchmarks/fft-candidates/fft_phase_benchmark`;
  - wrote phase harness results to
    `.benchmarks/fft-candidates/phase_kernel_results.csv`;
  - wrote full native scorer timings to
    `.benchmarks/fft-candidates/native_timing.csv`;
  - restored the default direct backend;
  - ran `uv run --extra test python -m pytest -q`;
  - ran `uv run --extra test pre-commit run --all-files`;
  - ran `git diff --check`.
- Validation result:
  - direct, pocketfft, DUCC, FFTW single-thread, FFTW all-core, oneMKL
    single-thread, and oneMKL all-core all passed Annex parity:
    `3 passed` per backend/mode;
  - default direct pytest passed: `19 passed, 32 deselected`;
  - pre-commit passed Ruff, Mypy, clang-format, whitespace, and config
    validation hooks;
  - `git diff --check` passed.
- Benchmark result:
  - phase harness median `phase_ms` at 32768 samples:
    - direct `147.511`;
    - pocketfft single `2.631`;
    - DUCC single `2.176`;
    - FFTW single `4.018`;
    - FFTW all-core `3.836`;
    - oneMKL single `1.301`;
    - oneMKL all-core `0.889`;
  - full native scorer median at 32768 samples:
    - direct `453.064 ms`;
    - pocketfft single `306.971 ms`;
    - DUCC single `307.096 ms`;
    - FFTW single `313.532 ms`;
    - FFTW all-core `313.810 ms`;
    - oneMKL single `309.393 ms`;
    - oneMKL all-core `317.036 ms`;
  - all FFT backends matched the direct phase alignment and score metadata on
    the native timing corpus.
- Conclusion:
  - the FFT product path is parity-safe with direct near-tie refinement and
    clearly faster for isolated large phase scans;
  - in the full scorer, single-thread pocketfft/DUCC were the best practical
    variants in this run, cutting 32768-sample median runtime from about
    `453 ms` to about `307 ms`;
  - oneMKL won the isolated phase harness, but its all-core overhead did not
    improve full-scorer runtime on this workload.
- Next hypothesis:
  - a production FFT path should start with a thresholded single-thread
    pocketfft/DUCC-style backend or a real-FFT oneMKL prototype, then benchmark
    with plan reuse before accepting any dependency.

## 2026-06-19 16:23 KST - Single-Thread FFT Backend Load/RSS Benchmark JSON

- Git status:
  - still dirty from the FFT phase-product backend experiment;
  - ignored JSON benchmark outputs under `.benchmarks/fft-candidates/json/`.
- Hypothesis:
  - the single-thread FFT backend comparison needs the same load-time,
    runtime, and peak-RSS measurements as the repo's native benchmark harness,
    not only scratch CSV timings.
- Files changed:
  - this experiment log.
- Commands:
  - rebuilt `direct`, `pocketfft`, `ducc`, `fftw`, and `mkl` backend variants
    one at a time with `uv pip install -e . --reinstall --config-settings=...`;
  - for each variant, ran `.venv/bin/python -m pytest -q
    tests/test_iso18571_benchmarks.py -m benchmark -k native --benchmark-warmup
    off --benchmark-quiet --benchmark-json
    .benchmarks/fft-candidates/json/<backend>.json`;
  - forced oneMKL single-thread with `MKL_NUM_THREADS=1 OMP_NUM_THREADS=1`;
  - restored the default `direct` backend after the matrix.
- Validation result:
  - every backend benchmark run passed: `8 passed, 24 deselected`;
  - no swap was recorded in any row.
- Benchmark result:
  - 32768-sample load/setup time and load peak RSS:
    - direct `460.416 ms`, `49.7 MiB`;
    - pocketfft `317.921 ms`, `52.7 MiB`;
    - DUCC `314.318 ms`, `52.0 MiB`;
    - FFTW `318.799 ms`, `53.3 MiB`;
    - oneMKL `326.821 ms`, `66.8 MiB`;
  - 32768-sample runtime median and runtime peak RSS:
    - direct `454.325 ms`, `49.8 MiB`;
    - pocketfft `308.981 ms`, `53.1 MiB`;
    - DUCC `306.190 ms`, `51.7 MiB`;
    - FFTW `309.023 ms`, `53.6 MiB`;
    - oneMKL `306.568 ms`, `66.8 MiB`.
- Conclusion:
  - DUCC had the best single-thread full-scorer 32768-sample runtime and the
    lowest FFT-backend peak RSS in this run;
  - pocketfft was nearly tied with DUCC but used about `1.4 MiB` more runtime
    peak RSS at 32768 samples;
  - oneMKL matched the fastest runtime but carried about `15 MiB` more peak RSS.
- Next hypothesis:
  - if productionizing this path, benchmark a thresholded DUCC/pocketfft
    real-FFT implementation with plan reuse before accepting a dependency.

## 2026-06-19 16:36 KST - README Native-Only Benchmark Command

- Git status:
  - dirty at start with pre-existing FFT benchmark work in
    `docs/iso18571-dtw-experiment-log.md`, `src/iso18571/engine_impl.hpp`, and
    untracked `src/iso18571/fft.hpp`;
  - this change only updates `README.md` and appends this log entry.
- Hypothesis:
  - documenting the native-only benchmark command beside the full benchmark
    matrix command makes the common quick benchmark path easier to reproduce.
- Files changed:
  - `README.md`;
  - this experiment log.
- Commands:
  - inspected `README.md` benchmark section;
  - added a native-only benchmark snippet using `-k native` and a dedicated
    `.benchmarks/iso18571-native/` output directory.
  - ran `mkdir -p .benchmarks/iso18571-native`;
  - ran `uv run --extra test python -m pytest -q
    tests/test_iso18571_benchmarks.py -m benchmark -k native --benchmark-json
    .benchmarks/iso18571-native/benchmarks.json`.
- Validation result:
  - native-only benchmark passed: `8 passed, 24 deselected`;
  - no swap was recorded in any row;
  - targeted README whitespace check passed with `git diff --check -- README.md`.
- Benchmark result:
  - load/setup median, ms: `512=125.59`, `2048=152.83`, `8192=172.71`,
    `32768=445.65`;
  - runtime median, ms: `512=0.21`, `2048=1.98`, `8192=21.45`,
    `32768=307.39`;
  - load/setup peak RSS, MiB: `512=51.68`, `2048=46.10`, `8192=47.18`,
    `32768=53.17`;
  - runtime peak RSS, MiB: `512=46.02`, `2048=46.07`, `8192=47.11`,
    `32768=52.48`.
- Conclusion:
  - README now documents both the full benchmark matrix command and the
    native-only benchmark command;
  - the current dirty checkout's native-only benchmark completed cleanly.
- Next hypothesis:
  - once the FFT worktree changes are settled, run the relevant documentation
    and benchmark validation together.

## 2026-06-19 16:56 KST - Remove Vestigial FFT Threading

- Git status:
  - dirty at start with pre-existing README benchmark documentation,
    FFT phase-product experiment changes in `src/iso18571/engine_impl.hpp`,
    previous experiment-log additions, and untracked `src/iso18571/fft.hpp`;
  - this change removes single-thread-only threading plumbing from the FFT
    header and the current engine call site.
- Hypothesis:
  - `src/iso18571/fft.hpp` is intentionally single-threaded in this checkout, so
    removing always-one thread helpers and `nthreads` parameters should simplify
    the code without changing FFT results.
- Files changed:
  - `src/iso18571/fft.hpp`;
  - `src/iso18571/engine_impl.hpp`;
  - this experiment log.
- Commands:
  - removed `util::thread_count`, `namespace threading`, and the share-splitting
    branch from `multi_iter`;
  - removed `nthreads` parameters from FFT internals and wrapper functions;
  - removed `phase_fft_thread_count`, `<thread>`, and thread-count arguments
    from `fft_product_sums`;
  - `uv pip install -e .`;
  - `uv run --extra test clang-format -i src/iso18571/fft.hpp
    src/iso18571/engine_impl.hpp`;
  - `uv run --extra test python -m pytest -q tests/test_iso18571_parity.py`;
  - `uv run --extra test python -m pytest -q`;
  - `uv run --extra test pre-commit run --all-files`;
  - `uv run --extra test clang-format --dry-run -Werror
    src/iso18571/fft.hpp`;
  - `git diff --check`;
  - `rg -n "thread_count|threading::|thread_id|num_threads|thread_map|nthreads|hardware_concurrency|<thread>|PHASE_PRODUCT_ALL_THREADS"
    src/iso18571/fft.hpp src/iso18571/engine_impl.hpp CMakeLists.txt -S`.
- Validation result:
  - no tests were added;
  - editable build passed and installed `iso18571==1.0.6`;
  - parity passed: `3 passed`;
  - default pytest passed: `19 passed, 32 deselected`;
  - pre-commit all-files passed Ruff, Mypy, clang-format, whitespace, and config
    validation hooks;
  - explicit clang-format dry-run passed for untracked `src/iso18571/fft.hpp`;
  - `git diff --check` passed;
  - vestigial threading identifier scan found no remaining matches.
- Conclusion:
  - the FFT header is now explicitly single-threaded instead of carrying a dead
    threading-shaped API.
- Next hypothesis:
  - benchmark the cleaned single-thread FFT path only after the surrounding FFT
    experiment is ready for a performance comparison.

## 2026-06-19 17:16 KST - FFT Scratch Allocation Alignment A/B

- Git status:
  - clean at start of the timed allocation experiment;
  - `src/iso18571/fft.hpp` was temporarily patched from `::aligned_alloc(...)`
    to `::malloc(size)` for measurement and then restored.
- Hypothesis:
  - if the FFT scratch-buffer alignment is not materially helping current
    generated code, replacing the aligned allocation call with plain `malloc`
    should preserve or improve native benchmark timings.
- Files changed:
  - temporary-only edit to `src/iso18571/fft.hpp`;
  - benchmark artifacts under `.benchmarks/iso18571-fft-alloc-aligned/` and
    `.benchmarks/iso18571-fft-alloc-malloc/`;
  - this experiment log.
- Commands:
  - `uv pip install -e .`;
  - `mkdir -p .benchmarks/iso18571-fft-alloc-aligned`;
  - `uv run --extra test python -m pytest -q tests/test_iso18571_benchmarks.py
    -m benchmark -k native --benchmark-warmup off --benchmark-min-rounds 1
    --benchmark-max-time 0.05 --benchmark-quiet --benchmark-json
    .benchmarks/iso18571-fft-alloc-aligned/benchmarks.json`;
  - patched `src/iso18571/fft.hpp` allocation wrapper to ignore `align` and use
    `::malloc(size)`;
  - `uv pip install -e .`;
  - `mkdir -p .benchmarks/iso18571-fft-alloc-malloc`;
  - `uv run --extra test python -m pytest -q tests/test_iso18571_benchmarks.py
    -m benchmark -k native --benchmark-warmup off --benchmark-min-rounds 1
    --benchmark-max-time 0.05 --benchmark-quiet --benchmark-json
    .benchmarks/iso18571-fft-alloc-malloc/benchmarks.json`;
  - ran a repeated warmed native scorer timing script for the malloc build and
    wrote `.benchmarks/iso18571-fft-alloc-malloc/runtime_repeated.csv`;
  - restored the aligned allocation wrapper;
  - `uv pip install -e .`;
  - ran the same repeated warmed native scorer timing script for the aligned
    build and wrote `.benchmarks/iso18571-fft-alloc-aligned/runtime_repeated.csv`.
- Validation result:
  - aligned native benchmark passed: `8 passed, 24 deselected`;
  - malloc native benchmark passed: `8 passed, 24 deselected`;
  - both editable rebuilds completed successfully.
- Benchmark result:
  - pytest-benchmark median runtime, aligned vs malloc, ms:
    `512=0.169/0.194`, `2048=1.579/3.838`, `8192=21.447/21.380`,
    `32768=306.886/309.379`;
  - repeated warmed scorer median runtime, aligned vs malloc, ms:
    `512=0.178/0.180`, `2048=1.479/1.467`, `8192=20.795/20.777`,
    `32768=305.840/307.986`;
  - repeated warmed scorer means, aligned vs malloc, ms:
    `512=0.174/0.180`, `2048=1.692/1.566`, `8192=20.989/20.783`,
    `32768=305.859/309.584`.
- Conclusion:
  - plain `malloc` did not show a material runtime win and was slightly worse
    for the largest repeated timing case, so the aligned allocation path was
    restored.
- Next hypothesis:
  - if pursuing FFT allocation performance further, benchmark an aligned
    allocator for the caller-owned FFT vectors and explicit local alignment
    assumptions in hot FFT code, rather than removing internal scratch
    alignment.

## 2026-06-19 17:24 KST - Phase FFT Dead-Code Prune

- Git status:
  - dirty at start from the prior FFT scratch-allocation experiment log entry;
  - implementation changed `src/iso18571/fft.hpp` and
    `src/iso18571/engine_impl.hpp`;
  - this experiment log was appended.
- Hypothesis:
  - the native phase scorer only needs a 1-D contiguous in-place complex FFT at
    power-of-two lengths, so the generic real/DCT/DST/Hartley/N-D/Bluestein FFT
    support can be removed without changing benchmark collection behavior.
- Files changed:
  - `src/iso18571/fft.hpp`;
  - `src/iso18571/engine_impl.hpp`;
  - this experiment log;
  - benchmark JSON under `.benchmarks/iso18571-native/`.
- Commands:
  - `rg -n "fft::(c2c|shape_t|stride_t|r2c|c2r|dct|dst|r2r)|c2c_power_of_two|class rfftp|class fftblue|class fft_r|T_dct|T_dst|T_dcst|general_nd|multi_iter|shape_t|stride_t|pass3|pass5|pass7|pass11|passg|good_size|largest_prime|cost_guess"
    src/iso18571/fft.hpp src/iso18571/engine_impl.hpp`;
  - `uv pip install -e .`;
  - `mkdir -p .benchmarks/iso18571-native`;
  - `uv run --extra test python -m pytest -q tests/test_iso18571_benchmarks.py
    -m benchmark -k native --benchmark-json
    .benchmarks/iso18571-native/benchmarks.json`;
  - `uv run python - <<'PY' ... PY` to summarize benchmark JSON.
- Validation result:
  - no tests were added;
  - editable native rebuild passed and installed `iso18571==1.0.6`;
  - native-only benchmark passed: `8 passed, 24 deselected`;
  - stale FFT API/dead-code scan found only the new `c2c_power_of_two` call
    sites and definition.
- Benchmark result:
  - load/setup elapsed, ms:
    `512=141.34`, `2048=153.33`, `8192=182.87`, `32768=459.37`;
  - peak RSS, MiB:
    `512=45.96`, `2048=46.11`, `8192=46.82`, `32768=52.22`;
  - runtime median, ms:
    `512=0.22`, `2048=1.73`, `8192=21.92`, `32768=307.12`.
- Conclusion:
  - the FFT header now carries only the phase-score power-of-two complex FFT
    path, and the requested native benchmark still completes successfully.
- Next hypothesis:
  - if more FFT cleanup is desired, compare reusable plan caching or a
    real-input convolution path against the current per-call complex FFT plans.

## 2026-06-19 17:32 KST - Native Namespace And Shared Dispatch Split

- Git status:
  - dirty at start from the FFT dead-code prune and experiment-log entries;
  - implementation changed native C++ namespace declarations/qualifiers and
    added `src/iso18571/dispatch.hpp`;
  - this experiment log was appended.
- Hypothesis:
  - separating scorer-owned C++ symbols into `engine` and generic CPU-level
    selection into `dispatch` will preserve the public Python package shape
    while making future FFT version dispatch share the same CPU feature logic.
- Files changed:
  - `src/iso18571/engine.hpp`, `src/iso18571/engine_impl.hpp`,
    `src/iso18571/validation.hpp`, `src/iso18571/validation.cpp`,
    `src/iso18571/_core.cpp`, `src/iso18571/dispatch.cpp`;
  - new `src/iso18571/dispatch.hpp`;
  - this experiment log.
- Commands:
  - `rg -n "namespace iso18571\\b|iso18571::|using iso18571|} // namespace iso18571|namespace engine\\b|engine::|namespace dispatch\\b|dispatch::"
    src/iso18571 -S`;
  - `uv pip install -e .`;
  - `uv run --extra test python -m pytest -q`;
  - `uv run --extra test clang-format --dry-run -Werror
    src/iso18571/dispatch.hpp src/iso18571/dispatch.cpp
    src/iso18571/engine.hpp src/iso18571/validation.hpp
    src/iso18571/validation.cpp src/iso18571/engine_impl.hpp
    src/iso18571/_core.cpp`;
  - `git diff --check`.
- Validation result:
  - no tests were added;
  - editable native rebuild passed and installed `iso18571==1.0.6`;
  - full pytest passed: `19 passed, 32 deselected`;
  - clang-format dry-run passed for touched C++ files;
  - `git diff --check` passed;
  - namespace scan found no remaining internal native `iso18571` namespace or
    `iso18571::` qualifiers under `src/iso18571`.
- Conclusion:
  - public Python/package naming remains `iso18571`;
  - scorer-owned native symbols now live under `engine`;
  - reusable CPU level selection now lives under `dispatch` and feeds
    `engine::dispatch_table()`.
- Next hypothesis:
  - when splitting FFT into `fft.hpp` and variant-specific `fft_impl.hpp`, use
    `dispatch::best_x86_64_level(...)` to select the FFT function table without
    coupling FFT to engine scorer types.

## 2026-06-19 17:55 KST - Encapsulated Engine Helper Build Fix

- Git status:
  - dirty at start from the namespace/file-layout refactor and FFT dead-code
    prune;
  - implementation kept the `.h`/`.cpp` file layout, changed engine helper
    linkage, and fixed the renamed validation source path;
  - this experiment log was appended.
- Hypothesis:
  - the generated engine implementation can avoid duplicate symbols across
    `engine_v1.cpp` through `engine_v4.cpp` by keeping private helpers in an
    anonymous namespace, while exporting only the variant scorer entrypoints in
    `namespace engine`.
- Files changed:
  - `CMakeLists.txt`;
  - `src/iso18571/engine.cpp`, `src/iso18571/engine.h`,
    `src/iso18571/engine_validation.cpp`, `src/iso18571/_core.cpp`;
  - this experiment log.
- Commands:
  - `uv pip install -e .`;
  - `uv run --extra test python -m pytest -q`;
  - `git diff --check`;
  - `rg -n "namespace iso18571\\b|iso18571::|using iso18571|} // namespace iso18571|engine_impl\\.hpp|validation\\.cpp"
    src/iso18571 CMakeLists.txt`;
  - `uv run --extra test clang-format --dry-run -Werror
    src/iso18571/_core.cpp src/iso18571/engine.cpp
    src/iso18571/engine.h src/iso18571/engine_validation.cpp
    src/iso18571/engine_dispatch.cpp src/iso18571/dispatch.cpp
    src/iso18571/dispatch.h src/iso18571/fft.h`.
- Validation result:
  - no tests were added;
  - editable native rebuild passed and installed `iso18571==1.0.6`;
  - full pytest passed: `19 passed, 32 deselected`;
  - clang-format dry-run passed for touched C++ files;
  - `git diff --check` passed;
  - namespace scan found no remaining internal native `iso18571` namespace or
    `iso18571::` qualifiers under `src/iso18571`; its only match was the
    intentional `engine_validation.cpp` filename.
- Conclusion:
  - private scorer helpers are again encapsulated without noisy per-helper
    variant macros;
  - the variant entrypoints remain exported for dispatch;
  - the renamed validation implementation is now part of the CMake source list.
- Next hypothesis:
  - if helper encapsulation needs further tuning, benchmark only targeted
    `static inline` changes in measured hot helpers rather than changing
    linkage style broadly.

## 2026-06-19 17:56 KST - Encapsulated Helper Native Benchmark

- Git status:
  - dirty at start from the namespace/file-layout refactor, FFT dead-code prune,
    and encapsulated helper build fix;
  - benchmark JSON was written under `.benchmarks/iso18571-native/`;
  - this experiment log was appended.
- Hypothesis:
  - restoring anonymous-namespace helper encapsulation should preserve native
    benchmark behavior after the file-layout and dispatch refactor.
- Files changed:
  - this experiment log;
  - benchmark JSON under `.benchmarks/iso18571-native/`.
- Commands:
  - `mkdir -p .benchmarks/iso18571-native && uv run --extra test python -m pytest -q tests/test_iso18571_benchmarks.py -m benchmark -k native --benchmark-json .benchmarks/iso18571-native/benchmarks.json`;
  - `uv run python - <<'PY' ... PY` to summarize benchmark JSON.
- Validation result:
  - native-only benchmark passed: `8 passed, 24 deselected`.
- Benchmark result:
  - load/setup elapsed, ms:
    `512=133.35`, `2048=153.79`, `8192=144.52`, `32768=469.72`;
  - peak RSS, MiB:
    `512=45.82`, `2048=45.96`, `8192=46.87`, `32768=52.08`;
  - runtime median, ms:
    `512=0.228`, `2048=2.170`, `8192=22.280`, `32768=312.076`.
- Conclusion:
  - the native benchmark remains in the same range as the prior FFT-prune run;
    the encapsulation cleanup did not create an obvious benchmark regression.
- Next hypothesis:
  - investigate `static inline` or `restrict` only with paired before/after
    benchmark runs, not as part of the namespace cleanup.

## 2026-06-19 18:22 KST - Independent FFT Variant Dispatch Split

- Git status:
  - clean at start;
  - implementation changed `CMakeLists.txt` and `src/iso18571/fft.h`;
  - added `src/iso18571/fft.cpp`, `src/iso18571/fft_dispatch.cpp`, and
    `src/iso18571/fft_v1.cpp` through `src/iso18571/fft_v4.cpp`;
  - benchmark JSON was written under `.benchmarks/iso18571-fft-dispatch/`.
- Hypothesis:
  - moving the phase FFT from header-only code into independently dispatched
    v1/v2/v3/v4 translation units should preserve behavior while decoupling FFT
    compiled-level macros and runtime selection from engine dispatch.
- Files changed:
  - `CMakeLists.txt`;
  - `src/iso18571/fft.h`;
  - `src/iso18571/fft.cpp`;
  - `src/iso18571/fft_dispatch.cpp`;
  - `src/iso18571/fft_v1.cpp`, `src/iso18571/fft_v2.cpp`,
    `src/iso18571/fft_v3.cpp`, `src/iso18571/fft_v4.cpp`;
  - this experiment log.
- Commands:
  - split `fft.h` into declarations, include-style `fft.cpp`, variant wrappers,
    and independent `fft_dispatch.cpp`;
  - added FFT-specific compiled macros `ISO18571_FFT_COMPILED_X86_64_V*` and
    separate CMake source registration for FFT variants;
  - `uv pip install -e .`;
  - `uv run --extra test python -m pytest -q`;
  - `uv run --extra test pre-commit run --all-files`;
  - `git diff --check`;
  - static scan for direct `fft.cpp` CMake registration, FFT macros,
    `engine::dispatch_table`, and `c2c_power_of_two` call sites;
  - `mkdir -p .benchmarks/iso18571-fft-dispatch`;
  - `uv run --extra test python -m pytest -q tests/test_iso18571_benchmarks.py
    -m benchmark -k native --benchmark-json
    .benchmarks/iso18571-fft-dispatch/benchmarks.json`;
  - summarized benchmark JSON with `uv run python`.
- Validation result:
  - editable native rebuild passed and installed `iso18571==1.0.6`;
  - full pytest passed: `19 passed, 32 deselected`;
  - pre-commit all-files passed Ruff, Mypy, clang-format, whitespace, and config
    validation hooks;
  - `git diff --check` passed;
  - native-only benchmark passed: `8 passed, 24 deselected`;
  - static scan showed `engine.cpp` still calls only `fft::c2c_power_of_two`,
    FFT dispatch uses FFT-specific compiled macros, and no direct CMake source
    registration for `src/iso18571/fft.cpp` was introduced.
- Benchmark result:
  - load/setup elapsed, ms:
    `512=115.215`, `2048=128.551`, `8192=173.059`, `32768=426.999`;
  - load/setup peak RSS, MiB:
    `512=46.04`, `2048=46.10`, `8192=47.12`, `32768=52.38`;
  - runtime median, ms:
    `512=0.163`, `2048=1.962`, `8192=21.436`, `32768=315.237`;
  - runtime peak RSS, MiB:
    `512=45.91`, `2048=46.03`, `8192=46.93`, `32768=52.36`.
- Conclusion:
  - FFT now has independent compiled-level macros and runtime dispatch while
    sharing only the generic CPU feature detector with engine dispatch;
  - public Python API and `backend_info()` shape remain unchanged.
- Next hypothesis:
  - with FFT dispatch isolated, storage modernization can be tested separately:
    first replace safe typed `arr<T>` uses with standard containers, then handle
    any alignment-sensitive scratch storage with an explicit measured design.

## 2026-06-19 18:28 KST - FFT std::complex Internal Replacement

- Git status:
  - started from committed FFT dispatch split `ff54180`;
  - implementation changed `src/iso18571/fft.cpp` and refreshed README native
    benchmark rows.
- Hypothesis:
  - because native inputs are promoted to `double` and the FFT entrypoint now
    accepts only `std::complex<double>*`, the private FFT implementation can
    replace the local `cmplx` type and unused numeric templates with concrete
    `std::complex<double>` code without changing the public FFT API.
- Files changed:
  - `README.md`;
  - `src/iso18571/fft.cpp`;
  - this experiment log.
- Commands:
  - removed the local `cmplx` struct;
  - introduced a private `Complex = std::complex<double>` alias;
  - simplified twiddle generation, FFT pass buffers, rotations, and the plan
    class from numeric templates to concrete double-complex storage;
  - removed the final `reinterpret_cast` in the FFT entrypoint;
  - `uv pip install -e .`;
  - `uv run --extra test clang-format -i src/iso18571/fft.cpp`;
  - `rg -n "cmplx|Thigh|T0|std::conditional|reinterpret_cast<.*complex|\\.r\\b|\\.i\\b|\\.Set\\("
    src/iso18571/fft.cpp src/iso18571/fft.h`;
  - `mkdir -p .benchmarks/iso18571-fft-std-complex`;
  - `uv run --extra test python -m pytest -q tests/test_iso18571_benchmarks.py
    -m benchmark -k native --benchmark-json
    .benchmarks/iso18571-fft-std-complex/benchmarks.json`;
  - summarized benchmark JSON with `uv run python`;
  - updated README native benchmark rows from the new JSON;
  - `git diff --check`.
- Validation result:
  - editable native rebuild passed and installed `iso18571==1.0.6`;
  - native-only benchmark passed: `8 passed, 24 deselected`;
  - no full pytest suite was run;
  - stale custom-complex scan found no remaining matches;
  - `git diff --check` passed.
- Benchmark result:
  - load/setup elapsed, ms:
    `512=154.708`, `2048=153.817`, `8192=174.053`, `32768=437.304`;
  - load/setup peak RSS, MiB:
    `512=46.13`, `2048=46.08`, `8192=46.86`, `32768=52.34`;
  - runtime median, ms:
    `512=0.255`, `2048=1.514`, `8192=21.431`, `32768=306.946`;
  - runtime peak RSS, MiB:
    `512=45.96`, `2048=46.13`, `8192=47.08`, `32768=52.91`.
- Conclusion:
  - FFT internals now use `std::complex<double>` directly and no longer require
    custom complex layout compatibility at the public FFT entrypoint.
- Next hypothesis:
  - replace the private aligned `arr<T>` storage with standard containers or a
    focused aligned RAII buffer in a separate measured change.

## 2026-06-19 18:01 KST - Validation Parameter Name String View Cleanup

- Git status:
  - clean at start from commit `8a004aa`;
  - implementation changed native engine validation declarations and
    definitions;
  - this experiment log was appended.
- Hypothesis:
  - engine validation APIs only borrow parameter names from literals/callers, so
    `std::string_view` expresses the contract better than `const char*` without
    forcing allocations on valid paths.
- Files changed:
  - `src/iso18571/engine.h`;
  - `src/iso18571/engine_validation.cpp`;
  - this experiment log.
- Commands:
  - `uv pip install -e .`;
  - `uv run --extra test python -m pytest -q`;
  - `uv run --extra test clang-format --dry-run -Werror
    src/iso18571/engine.h src/iso18571/engine_validation.cpp`;
  - `git diff --check`.
- Validation result:
  - no tests were added;
  - editable native rebuild passed and installed `iso18571==1.0.6`;
  - full pytest passed: `19 passed, 32 deselected`;
  - clang-format dry-run passed for touched C++ files;
  - `git diff --check` passed.
- Conclusion:
  - public Python behavior is unchanged;
  - engine validation parameter-name APIs now use `std::string_view`;
  - pybind-local `const char*` helpers were left unchanged.
- Next hypothesis:
  - if validation cleanup continues, consider moving private validation helpers
    into an anonymous namespace separately from the string type cleanup.
