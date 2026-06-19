# TODO

## Float32-compatible engine

- Implement and benchmark a true float32-compatible native engine path.
- Keep this separate from the current dtype-aware input validation, which accepts
  float32 arrays but still performs scoring calculations in double precision.
- Decide whether a future implementation should template only selected hot paths
  or add a full float32 scoring variant after benchmark evidence shows the payoff.
- Preserve ISO/TS 18571 parity expectations unless a deliberate native-only
  robustness behavior is documented and tested separately.
