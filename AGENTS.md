# Agent Notes

## ISO/TS 18571 Native Scorer

- The native scorer must remain clean-room. Do not copy implementation code from third-party DTW packages.

## Current Package Shape

- The public production scorer is `iso18571.ISO18571`.
- The native extension module is `iso18571._core`; public package exports are `ISO18571`, `backend_info`, and `ScoreComponents`.
- Reference scorers live in source-only `reference` and are used for tests/research only:
  - `rating_dtwalign.ISO18571`;
  - `rating_dtw_python.ISO18571`;
  - `rating_librosa.ISO18571`.
- Do not install `reference` `tools` `tests` into production wheels.

## Correctness

- Preserve the NumPy `(n, 2)` curve input contract.
- Preserve ISO/TS 18571 DTW behavior already captured in this repo:

## Native Build

- The native extension is built with CMake through scikit-build-core.
- Implementation source belongs under `src/`.
- Native C++ uses the fixed-width and numeric aliases from `src/types.h`: `u8`, `u16`, `u32`, `u64`, `i8`, `i16`, `i32`, `i64`, `f16`, `f32`, `f64`, `f128`, `c64`, `c128`, and `c256`.
- `f128` and `c256` intentionally mean `long double` and `std::complex<long double>`; they are not guaranteed IEEE binary128.
- Do not replace semantic/platform-sized types with fixed-width aliases. Keep `std::size_t`, `std::uintptr_t`, `py::ssize_t`, and `std::ptrdiff_t` as written.
- Keep GCC/Clang `-O3`, MSVC `/O2 /fp:precise`, and no fast-math or native-CPU
  flags.
- C++ should build with warning flags enabled. Suppress warnings only narrowly
  and document why.
- The scorer engine is scalar-source C++ with internal x86-64 level dispatch:
  - v1 builds always;
  - GCC/Clang compile v2/v3/v4 source variants on a best-effort basis;
  - MSVC compiles v3/v4 variants on a best-effort basis because it has no direct
    x86-64-v2 flag;
  - runtime C++ initialization selects the highest compiled-and-supported level
    once and stores direct function pointers;
  - Python does not configure dispatch, but `backend_info()` reports it.
- Other artifacts that consist the binary should be built in v1 to guarantee maximum portability

## Test And Style

- Test outcomes should explicitly `assert` or `pytest.raise`.
- Avoid inline imports in project Python. Use module-level imports or shared
  loading helpers.
- Avoid resorting to `assert` or `cast` to satisfy mypy constraints.
- `reference/` is ignored reference material.
- Install commit hooks once per checkout with:
  - `uv run --extra test pre-commit install`
- Before committing code changes, let the installed hook run or run:
  - `uv run --extra test pre-commit run --all-files`
- The hook runs `clang-format` for native source files plus:
  - `uv run --extra test ruff check --fix .`
  - `uv run --extra test ruff format .`
  - `uv run --extra test ruff check .`
  - `uv run --extra test ruff format --check .`
  - `uv run --extra test mypy python/iso18571 reference tests`
  - `git diff --check --cached`
- Benchmarks live in `tests/test_benchmarks.py` and are deselected by
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
