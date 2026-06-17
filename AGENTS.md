# Agent Notes

## ISO/TS 18571 Native Scorer

- Continue iterating until the user explicitly says to stop.
- On every resume or context compaction, read this file, the latest entries in
  `docs/iso18571-dtw-experiment-log.md`, and run `git status --short --branch`.
- The native scorer must remain clean-room. Do not copy implementation code from
  third-party DTW packages.

## Correctness

- The public scorer is `iso18571.ISO18571`.
- The package is native-only: Python calls
  `iso18571_native.score_components(reference_curve, comparison_curve, params)`.
- Preserve the NumPy `(n, 2)` curve input contract, including strided arrays and
  numeric force-casting to `float64`.
- Preserve ISO/TS 18571 DTW behavior already captured in this repo:
  - squared local cost;
  - Sakoe-Chiba radius `max(1, ceil(window_size*n))`;
  - valid cells where `abs(i-j) < radius`;
  - predecessor tie priority vertical, horizontal, diagonal;
  - Annex `R`, `Z`, `EP`, `EM`, and `ES` within `0.001`.
- Generated Annex parity compares four test-only scorers: `native`, `original`,
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
- Implementation source belongs under `src/iso18571_native/`; importable native
  package files belong under `iso18571_native/`.
- Keep GCC/Clang `-O3`, MSVC `/O2 /fp:precise`, and no fast-math or native-CPU
  flags.
- C++ should build with warning flags enabled. Suppress warnings only narrowly
  and document why.
- The scorer is scalar-source C++ with internal x86-64 level dispatch:
  - v1 builds always;
  - v2/v3/v4 source variants are compiled on a best-effort basis when the
    compiler supports the requested flags;
  - runtime C++ initialization selects the highest compiled-and-supported level
    once and stores direct function pointers;
  - Python does not expose or configure this dispatch.

## Test And Style

- Do not use pytest outcome helpers in tests or `tests/conftest.py`; outcomes
  should be normal `assert` or explicit `raise AssertionError`.
- Keep tests parity-focused:
  - downloaded official Annex CSVs plus expected scores;
  - one generated Annex set combining fixed signal families and analytic
    phase-shift families at practical lengths;
  - four test-only scorers: `native`, `original`, `dtw_python`, and `librosa`.
- Expected degenerate numeric warnings must be caught and asserted explicitly;
  unexpected warnings should fail tests.
- Avoid inline imports in project Python. Use module-level imports or shared
  loading helpers.
- `ref/` is ignored reference material. Do not lint, package, or build files from
  `ref/`; oracle tests load `ref/rating_original.py` explicitly.
- Before committing code changes, run:
  - `uv run --extra test ruff check --fix .`
  - `uv run --extra test ruff format .`
  - `uv run --extra test ruff check .`
  - `uv run --extra test ruff format --check .`
  - `git diff --check`

## Validation Commands

- Editable build: `uv pip install -e .`
- Tests: `uv run --extra test python -m pytest -q`
- Wheel: `uv build --wheel`
- Wheel smoke:
  `uv run --with dist/euroncap-0.1.0-*.whl python tools/iso18571/wheel_smoke.py`

## Experiment Tracking

- Append every meaningful experiment to
  `docs/iso18571-dtw-experiment-log.md`.
- Include timestamp, git status summary, hypothesis, files changed, commands,
  validation result, conclusion, and next hypothesis.
- Keep root tidy:
  - repeatable tools belong under `tools/iso18571/`;
  - durable notes belong under `docs/`.
