# TODO

## Robustness Follow-Ups

- Define and test the expected behavior for complex-valued curve inputs.
- Add coverage for finite, extreme-amplitude signal values.
- Revisit float64 time-grid validation for large absolute timestamps; the
  current tolerance can be too strict.
- Align or document non-default `init_min` behavior where native C++
  half-away-from-zero rounding can diverge from the Python reference.
- Prevent finite subnormal signal amplitudes from producing `NaN` corridor and
  overall scores.
- Prevent extremely tiny but finite `dt` values from producing `NaN` slope and
  overall scores.
- Handle non-native-endian `float32` grids consistently with equivalent
  native-endian `float32` grids.
- Decide whether `np.ma.MaskedArray` masks should be rejected or respected;
  masks are currently ignored.
- Tighten time-grid interval validation for very small `dt` values.
- Tighten reference/comparison time equality validation for very small `dt`
  values.
- Ensure accepted weight sums slightly below `1` cannot make a perfect-match
  score fall below `1`.
- Reject boolean scoring parameters explicitly instead of accepting them through
  Python numeric coercion.
- Document and test short-curve phase behavior where a best shifted alignment is
  clamped back to unshifted if the shifted span would contain fewer than 9
  samples.
