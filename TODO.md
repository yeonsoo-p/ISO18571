# TODO

## Robustness Follow-Ups

- Add coverage for finite, extreme-amplitude signal values.
- Prevent finite subnormal signal amplitudes from producing `NaN` corridor and
  overall scores.
- Document and test short-curve phase behavior where a best shifted alignment is clamped back to unshifted if the shifted span would contain fewer than 9
  samples.
- Align f16 implementation with existing stdfloat
- pack_long_double() is extremely expensive
- The scorer does not validate matching evaluation intervals.
- The scorer accepts intervals shorter than the ISO minimum
- signed integer time validation can overflow
- dtw Structural Validity
- dtw Tie-Breaking
- floating point number equality(ULP or rtol atol)
