# TODO

## Robustness Follow-Ups

- Add coverage for finite, extreme-amplitude signal values.
- Revisit float64 time-grid validation for large absolute timestamps; the
  current tolerance can be too strict.
- Prevent finite subnormal signal amplitudes from producing `NaN` corridor and
  overall scores.
- Document and test short-curve phase behavior where a best shifted alignment is clamped back to unshifted if the shifted span would contain fewer than 9
  samples.
- Align f16 implementation with existing stdfloat
- pack_long_double() is extremely expensive
- The scorer does not validate matching evaluation intervals.
- The scorer accepts intervals shorter than the ISO minimum
- signed integer time validation can overflow

## What Is Missing

- The package does not yet implement the full ISO “data preparation” workflow.
- No synchronization/resampling utility.
- No filtering utility or filter-class tracking.
- No API for selecting tstart/tend; caller must pass already-trimmed curves.
- No explicit interval-duration validation for 10 ms.
- No grade/rank output such as Excellent, Good, Fair, Poor.
- No public access to intermediate curves such as shifted curves, slope curves, warped curves, or corridor bounds.
- No public DTW path output.
- No built-in validation report object saying which ISO preparation assumptions passed or failed.
- No multi-signal aggregation support, which is fine because ISO warns the single-signal scale does not directly apply to multiple responses.

## Support Required From Engine/Core For Full Validation

- To support full validation cleanly, engine/core would need to expose more than final scores.
- A structured validation result for input checks: n, dt, time-grid status, min interval duration, dtype/layout conversions, finite checks.
- Optional interval selection support: accept tstart/tend, slice consistently, and report the effective interval.
- Optional preprocessing hooks or helpers: synchronize/resample curves to a common time grid; probably filtering should stay explicit rather than hidden.
- Intermediate outputs for Annex/debug parity: corridor bounds, phase-shifted curves, derivative/smoothed slope curves, DTW warped curves, and possibly DTW path.
- Grade/rank calculation from R using the documented sliding scale.
- A higher-level “validation report” API that returns scores plus assumptions, warnings, and intermediate artifacts without expanding the small default public surface too much.

## Easy To Expose

- Corridor bounds: no issue. corridor_score() already computes t_norm, inner_corridor, and outer_corridor; bounds are just reference +/- width. Could either save them during the pass or reconstruct them exactly afterward.
- Shifted curves: no real engine work needed. Core already returns reference_start, comparison_start, and shift_length, so shifted curves are just slices of the original inputs. If the public API stores the input arrays, probes can expose these without recalculation.
- Slope curves: current implementation computes gradients and smoothed slopes in fused_slope_score_from_values(). It currently only accumulates numerator/denominator, but we can optionally save reference_slope and comparison_slope vectors in the same pass.

## Needs Extra DTW Trace Support

- DTW path and warped curves are not currently materialized. magnitude_error_from_dtw() uses a memory-efficient rolling DP and only carries cumulative cost plus magnitude numerator/denominator. It intentionally does not retain the full cumulative matrix or predecessor/backtracking info.
- So for public DTW probes, we need a trace/debug mode that stores predecessor choices or enough cumulative state to backtrack the optimal path.

1. Structural Validity
The path itself must satisfy the ISO DTW path rules:
Starts at first cell: (0, 0).
Ends at last cell: (n - 1, n - 1).
Monotonic: indices never decrease.
Continuous: each step is one of:vertical: (i + 1, j)
horizontal: (i, j + 1)
diagonal: (i + 1, j + 1).

Stays inside the Sakoe-Chiba band:abs(i - j) < radius
radius = min(n, max(1, ceil(0.1 * n))).

This proves the path is legal.
2. Optimality
The path must have the minimum possible cumulative cost under ISO’s local cost:
d(i, j) = (Cts[i] - Tts[j])^2
So validate:
sum(d(i, j) for (i, j) in path) == dtw[n-1, n-1]
where dtw[n-1, n-1] is produced by an independent dynamic-programming matrix using the same window and recurrence.
This proves the path is optimal.
3. Tie-Breaking
If multiple optimal paths have the same cumulative cost, ISO Annex A says the predecessor priority is:
vertical:   dtw[i-1, j]
horizontal: dtw[i, j-1]
diagonal:   dtw[i-1, j-1]
So validation needs deliberate tie cases where multiple predecessors have identical cost. For each tie case, backtracking must choose the path produced by that priority.
This proves the path is not just any optimal path, but the ISO-specified optimal path.
After those, validate derived artifacts:
Cts_w == Cts[path_i]
Tts_w == Tts[path_j]
EM computed from those warped curves matches the scorer’s EM.
For Annex cases, warped curves match Annex CAE_Warped / Test_Warped within tolerance.