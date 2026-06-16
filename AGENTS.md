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
  - signal-family parity tests against `rating_original.py` live in
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
  - current large-slice candidate: `effective_n >= 6717` dispatch to
    `dtw_current+all_reductions+blocked128` with 8 threads, pending broader
    production-dispatch validation.
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
