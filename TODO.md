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
