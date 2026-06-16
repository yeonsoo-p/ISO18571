# Agent Notes

## ISO/TS 18571 DTW Native Backend

- Continue iterating until the user explicitly says to stop.
- On every resume or context compaction, read this file, the latest entries in
  `docs/iso18571-dtw-experiment-log.md`, and run `git status --short --branch`.
- The native DTW backend must remain clean-room. Do not copy implementation code
  from `dtw-python`, `librosa`, or other third-party DTW packages.
- Correctness comes first:
  - preserve the ISO/TS 18571 magnitude DTW behavior already captured by
    `local_iso_numpy`;
  - preserve the full native scorer API
    `iso18571_native.score_components(reference_curve, comparison_curve,
    params)` for NumPy `(n, 2)` curves, including strided/forcecast numeric
    inputs;
  - keep the Sakoe-Chiba rule as `radius=max(1, ceil(window_size*n))` with valid
    cells where `abs(i-j) < radius`;
  - preserve predecessor tie priority: vertical, horizontal, diagonal;
  - Annex scores must match expected `R`, `Z`, `EP`, `EM`, and `ES` within
    `0.001`.
  - signal-family parity tests against `ref/rating_original.py` live in
    `tests/test_rating_original_parity.py` and use generated Annex-shaped cases
    from `tests/iso18571_annex.py`;
  - generated Annex parity must compare `rating_original`, `local_iso_native`,
    `dtw_python`, and `librosa` for `n_eps`, `rho_e`, unrounded scores, and
    rounded scores;
  - avoid perfectly affine synthetic ramps as strict phase-oracle cases because
    NumPy/BLAS last-bit behavior can decide strict-correlation ties.
- Test hygiene:
  - do not use pytest outcome helpers in tests or `tests/conftest.py`;
    outcomes should be normal `assert` or explicit `raise AssertionError`;
  - use marker categories and deselection instead of skips. Default pytest
    excludes `oracle`, `stress`, `benchmark`, `threshold`, and `regime`.
  - prefer shared helpers in `tests/iso18571_test_helpers.py` before adding new
    test-local plumbing;
  - do not add inline imports in project Python; module-level imports or shared
    optional-dependency loaders keep the test and package surfaces auditable.
  - expected degenerate numeric warnings must be caught and asserted explicitly;
    unexpected warnings should fail tests.
- Build and style gates:
  - the native extension is built with CMake through scikit-build-core. Do not
    reintroduce `setup.py`.
  - `ref/` is ignored reference material. Do not lint, package, or build files
    from `ref/`.
  - before committing code changes, run Ruff format/check:
    `uv run --with ruff ruff check --fix .`,
    `uv run --with ruff ruff format .`,
    `uv run --with ruff ruff check .`, and
    `uv run --with ruff ruff format --check .`.
  - CMake builds should keep GCC/Clang `-O3`, MSVC `/O2 /fp:precise`, and no
    fast-math or native-CPU flags.
  - project C++ code should build with warning flags enabled. The `_core.cpp`
    pybind11 module definition has a narrow pedantic-warning suppression for the
    pybind11 variadic macro; do not broaden warning suppressions without
    documenting the reason.
- Python API:
  - the public scorer is `iso18571.ISO18571`;
  - the root `rating.py` compatibility shim has been removed by user request;
  - keep `main.py` as the operator/CLI surface.
- Benchmark rules:
  - run backend benchmarks in fresh interpreter processes;
  - compare prep/first-use against `dtw_python`;
  - compare steady-state Annex-pass median against `librosa`;
  - classify first-pass-only overhead as preparation, not calculation.
  - run `tests/test_iso18571_benchmarks.py -o addopts= -m benchmark` when
    optimizing native code; it includes official Annex cases and generated
    fixed-signal Annex cases. Reject changes that improve one signal family
    while materially regressing another.
  - run `tests/test_iso18571_threshold_benchmarks.py -o addopts= -m threshold`
    only for parallelization threshold experiments, then analyze the JSON with
    `tools/iso18571/analyze_parallel_threshold.py`.
  - run `tests/test_iso18571_regime_benchmarks.py -o addopts= -m regime` for
    the performance atlas. Analyze its JSON with
    `tools/iso18571/analyze_variant_regimes.py`; classify by `effective_n` and
    DTW cell count rather than choosing a universal winner.
  - filter expensive regime runs with `ISO18571_REGIME_FAMILIES`,
    `ISO18571_REGIME_LENGTHS`, `ISO18571_REGIME_THREADS`, and
    `ISO18571_REGIME_VARIANTS`.
  - SIMD experiments use enum-based private hooks, not C++ string-token
    execution paths. Human-readable env vars are mapped to `DtwLayout`,
    `ReductionMode`, `ParallelMode`, and `SimdLevel` in Python before calling
    `_score_components_variant_spec` or `_magnitude_ratio_variant_spec`.
  - supported SIMD levels are scalar, SSE2, AVX2, AVX2+FMA, and auto; AVX-512
    is intentionally not implemented. Runtime dispatch must fall back safely
    for prebuilt wheels.
  - `SimdLevel.Auto` is a runtime dispatch feature for explicit smoke/parity
    tests, not a benchmark matrix axis. Regime benchmark matrices should use
    explicit `scalar`, `sse2`, `avx2`, and `avx2_fma` SIMD levels.
  - filter SIMD atlas runs with `ISO18571_REGIME_SIMD_LEVELS`, for example
    `scalar,avx2,avx2_fma`.
  - inspect compiler output with
    `tools/iso18571/emit_native_assembly.py` and
    `tools/iso18571/report_assembly_wrinkles.py`; assembly artifacts belong
    under `.benchmarks/` and findings are reporting-only unless the user asks
    for a follow-up optimization batch.
  - current no-auto thread-ceiling atlas result: blocked wavefront parallelism
    wins all tested fixed/phase/noise families at input lengths 4096 through
    65536, but the best thread count is size-dependent. In the 2026-06-16
    no-auto atlas, 8 threads was strongest around input length 8192, 12 threads
    around 12288-16384, and 16/24 threads around 32768-65536. Do not hard-code a
    universal thread count from this result.
  - current production-dispatch candidate remains experimental: use a
    size-regime policy over `dtw_current+all_reductions+blocked128` only after a
    repeated validation run confirms the crossover; keep public production
    scoring unchanged until then.
- Experiment tracking:
  - append every meaningful experiment to
    `docs/iso18571-dtw-experiment-log.md`;
  - include timestamp, git status summary, hypothesis, files changed, commands,
    validation result, benchmark result, conclusion, and next hypothesis.
- Keep root tidy:
  - implementation source belongs under `src/`;
  - importable native package files belong under `iso18571_native/`;
  - repeatable tools belong under `tools/iso18571/`;
  - durable notes belong under `docs/`.
