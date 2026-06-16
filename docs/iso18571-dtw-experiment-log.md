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
