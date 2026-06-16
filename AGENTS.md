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
  - keep the Sakoe-Chiba rule as `radius=max(1, ceil(window_size*n))` with valid
    cells where `abs(i-j) < radius`;
  - preserve predecessor tie priority: vertical, horizontal, diagonal;
  - Annex scores must match expected `R`, `Z`, `EP`, `EM`, and `ES` within
    `0.001`.
- Benchmark rules:
  - run backend benchmarks in fresh interpreter processes;
  - compare prep/first-use against `dtw_python`;
  - compare steady-state Annex-pass median against `librosa`;
  - classify first-pass-only overhead as preparation, not calculation.
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

