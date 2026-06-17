# Agent Notes

## ISO/TS 18571 Native Scorer

- The native scorer must remain clean-room. Do not copy implementation code from
  third-party DTW packages.

## Current Package Shape

- The public production scorer is `iso18571.ISO18571`.
- The native extension module is `iso18571._core`; public package exports are
  `ISO18571` and `backend_info`.
- Reference scorers live in source-only `iso18571_reference` and are used for
  tests/research only:
  - `rating_dtwalign.ISO18571`;
  - `rating_dtw_python.ISO18571`;
  - `rating_librosa.ISO18571`.
- Do not install `iso18571_reference` into production wheels.

## Correctness

- Preserve the NumPy `(n, 2)` curve input contract, including strided arrays and numeric force-casting to `float64`.
- Preserve ISO/TS 18571 DTW behavior already captured in this repo:
  - squared local cost;
  - Sakoe-Chiba radius `min(n, max(1, ceil(window_size*n)))`;
  - valid cells where `abs(i-j) < radius`;
  - predecessor tie priority vertical, horizontal, diagonal;
  - Annex `R`, `Z`, `EP`, `EM`, and `ES` within `0.001`.
- Generated Annex parity compares four full scorers: `native`, `dtwalign`,
  `dtw_python`, and `librosa`.
- Generated parity compares `n_eps`, `rho_e`, unrounded scores, and rounded
  three-decimal scores. Degenerate generated cases pass only when all four
  scorers produce matching numeric results with `equal_nan=True` or matching
  exception types.
- Avoid perfectly affine synthetic ramps as strict phase-oracle cases because
  NumPy/BLAS last-bit behavior can decide strict-correlation ties.

## Native Build

- The native extension is built with CMake through scikit-build-core. Do not
  reintroduce `setup.py`.
- Implementation source belongs under `src/iso18571/`.
- Keep GCC/Clang `-O3`, MSVC `/O2 /fp:precise`, and no fast-math or native-CPU
  flags.
- C++ should build with warning flags enabled. Suppress warnings only narrowly
  and document why.
- The scorer is scalar-source C++ with internal x86-64 level dispatch:
  - v1 builds always;
  - GCC/Clang compile v2/v3/v4 source variants on a best-effort basis;
  - MSVC compiles v3/v4 variants on a best-effort basis because it has no direct
    x86-64-v2 flag;
  - runtime C++ initialization selects the highest compiled-and-supported level
    once and stores direct function pointers;
  - Python does not configure dispatch, but `backend_info()` reports it.

## Test And Style

- Do not use pytest outcome helpers in tests or `tests/conftest.py`; outcomes
  should be normal `assert` or explicit `raise AssertionError`.
- Expected degenerate numeric warnings must be caught and asserted explicitly;
  unexpected warnings should fail tests.
- Avoid inline imports in project Python. Use module-level imports or shared
  loading helpers.
- `ref/` is ignored reference material. Do not lint, package, or build files from
  `ref/`.
- Before committing code changes, run:
  - `uv run --extra test ruff check --fix .`
  - `uv run --extra test ruff format .`
  - `uv run --extra test ruff check .`
  - `uv run --extra test ruff format --check .`
  - `uv run --extra test mypy iso18571 iso18571_reference tests`
  - `git diff --check`
- Benchmarks live in `tests/test_iso18571_benchmarks.py` and are deselected by
  default through the `benchmark` marker. They use pytest-benchmark plus spawned
  Python workers to measure setup/load time, warm runtime, and peak process
  memory on Linux and Windows. Benchmark rows record peak swap/pagefile usage;
  rows with `swap_invalidated` are stress outcomes, not valid in-memory timing
  numbers.

## Validation Commands

- Editable build: `uv pip install -e .`
- Tests: `uv run --extra test python -m pytest -q`
- Wheel: `uv build --wheel`
- Benchmarks:
  `uv run --extra test python -m pytest -q -m benchmark --benchmark-json .benchmarks/iso18571-readme/benchmarks.json`

## Experiment Tracking

- Append every meaningful experiment to
  `docs/iso18571-dtw-experiment-log.md`.
- Include timestamp, git status summary, hypothesis, files changed, commands,
  validation result, conclusion, and next hypothesis.
- Keep root tidy:
  - durable notes belong under `docs/`.
