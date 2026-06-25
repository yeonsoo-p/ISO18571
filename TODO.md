# TODO

- Implement more robust floating point number equality
  - ULP or rtol + atol
- Implement warning messages for non f64 inputs

- Revamp test suite including
  - Against Annex:
    - Overall rating R
      - Expected response: Error < 0.001
    - Corridor score Z
      - Expected response: Error < 0.001
    - Phase score EP
      - Expected response: Error < 0.001
    - Magnitude score EM
      - Expected response: Error < 0.001
    - Slope score ES
      - Expected response: Error < 0.001
    - Inner corridor upper/lower curves
      - Expected response: Compare scorer-generated curves directly; Error < 0.001
    - Outer corridor upper/lower curves
      - Expected response: Compare scorer-generated curves directly; Error < 0.001
    - Phase-shifted test curve
      - Expected response: Compare scorer-selected shifted curve directly; Error < 0.001
    - Phase-shifted CAE curve
      - Expected response: Compare scorer-selected shifted curve directly; Error < 0.001
    - Magnitude numerator, denominator, and error from warped time-shifted curves
      - Expected response: Error < 0.001

  - Input data lengths
    - Shorter than 9
      - Expected response: Reject
    - Shorter than 9 when preferred shift makes it so
      - Expected response: Clamp

  - Input data types
    - Object
      - Expected response: Reject
    - Bool
      - Expected response: Reject
    - String
      - Expected response: Reject
    - Masked arrays
      - Expected response: Reject
    - Unsigned integer: [u8, u16, u32, u64]
      - Expected response: Accept, scores and validation fields match f64 materialization
    - Signed integer:   [i8, i16, i32, i64]
      - Expected response: Accept, scores and validation fields match f64 materialization
    - Non-f64 floats:   [f16, f32]
      - Expected response: Accept, scores and validation fields match f64 materialization
    - f64 floats:       [f64]
      - Expected response: Accept, scores and validation fields match f64 materialization
    - Non-zero imaginary complex floats:   [c64, c128, c256]
      - Expected response: Reject
    - Zero imaginary complex floats:   [c64, c128, c256]
      - Expected response: Ignore imaginary and accept, scores and validation fields match f64 materialization

  - Input signal type
    - Non-zero identical signals:
      - Expected response: 1.0 for all scores
    - Constant identical signals
      - Expected response: 1.0 for all scores, expected warnings, and zero-denominator fallback fields
    - Phase shifted identical signals
      - Expected response: 0.0 EP for shifts beyond threshold
    - Phase tie priority with equal maximum correlations
      - Expected response: Prefer fewer shifts, then left shift when shift count is also tied
    - Zero signals
      - Expected response: 1.0 for all scores, expected warnings, and zero-denominator fallback fields
    - Constant signals with constant offset
      - Expected response: Z is 1.0 within the inner corridor and 0.0 beyond the outer corridor; EM is 0.0 once magnitude error reaches or exceeds eps_m; ES uses the zero-slope fallback fields
    - Non-constant signals with constant offset
      - Expected response: Z is 0.0 beyond the outer corridor; EM is 0.0 once magnitude error reaches or exceeds eps_m; EP and ES are asserted separately from corridor and magnitude behavior
    - Signal types of different classes:
      - ramp
      - piecewise_ramp
      - impulse
      - sparse_spikes
      - sine
      - chirp
      - square_step
      - gaussian_noise
      - ramp_impulses
      - piecewise_discontinuous
      - sine_noise

  - Input signal values
    - NaN/Inf
      - Expected response: Reject
    - Finite large amplitudes
      - Expected response: Accept, no overflow, scores are not NaN/Inf
    - Finite small amplitudes
      - Expected response: Accept, no overflow, scores are not NaN/Inf
  
  - Input signal times
    - Non-uniform
      - Expected response: Reject
    - Uniform small dt
      - Expected response: Accept, no overflow, scores are not NaN/Inf
    - Uniform large dt
      - Expected response: Accept, no overflow, scores are not NaN/Inf

  - Need to put more edge cases into tests in general

- Investigate edge cases
  - Integer overflow during time validation for signed integers
  - `NaN` corridor scores from finite subnormal signal amplitudes
  - Short-curve phase behavior
  - test_phase_shifted_identical_signals_score_zero_beyond_phase_threshold
  - test_nonconstant_offset_expectations_are_score_specific
  - I have serious doubts about type conversion. Need to figure out what exactly is tested and how it is evaluated

- Near-threshold / nonfinite score follow-up from temp characterization
  - Implementation narrowing needed:
    - Decide and enforce the finite accepted-input score contract:
      scores `Z`, `EP`, `EM`, `ES`, and `R` should either be finite and
      inside `[0.0, 1.0]`, or the scorer should reject the input before
      returning scores with a clear error.
    - Corridor: handle `Tnorm > 0.0` when `a_0 * Tnorm` and `b_0 * Tnorm`
      underflow to equal zero. Current reproducer:
      `np.full(16, np.nextafter(0.0, 1.0))` against itself returns
      `Z = NaN` and therefore `R = NaN`.
    - Slope: distinguish intentional zero-denominator fallback diagnostics
      from arithmetic overflow. Current reproducers include `dt=1e-307`
      with `10.0 * np.arange(16)` against itself, and huge alternating
      values; both can return `ES = NaN` and therefore `R = NaN`.
    - Slope: decide behavior for nonfinite `slope_error` where the score is
      still finite, for example one-sided overflow producing
      `slope_error = Inf` and `ES = 0.0`.
    - Magnitude / DTW: finite accepted-looking inputs can overflow squared
      DTW local costs and throw `No valid ISO DTW path found`; decide whether
      to make the DTW cost path overflow-resistant or reject such inputs with
      a clearer range/scale error before scoring.
    - Phase: keep `CORRELATION_TIE_TOLERANCE` unless it is replaced by another
      documented tie policy. Without it, ULP-scale correlation noise can select
      `n_eps = 1` instead of the ISO-preferred smaller shift `n_eps = 0`.
    - Phase: decide whether validation field `phase_rho_e` must always be
      finite when scores are finite; huge constant or huge alternating inputs
      can currently produce `phase_rho_e = NaN`.
    - Near-`1.0` raw scores such as `0.999999999` are expected formula
      behavior, not necessarily bugs. Document that default rating methods
      round these to `1.0`, and any grade/classification logic must use raw
      `R`, not rounded `overall_rating()`.
  - New tests needed to confirm the decisions:
    - Corridor smallest-subnormal identical signal:
      `np.nextafter(0.0, 1.0)` should follow the chosen policy and never
      return `NaN` scores.
    - Corridor subnormal scan around `k * np.nextafter(0.0, 1.0)` should show
      where the policy changes from the tiny-scale fallback/rejection to normal
      corridor scoring.
    - Slope overflow with tiny but accepted `dt` and moderate amplitudes should
      follow the chosen policy and never return `ES = NaN` or `R = NaN`.
    - Huge alternating identical signals should follow the same slope policy
      and never return `ES = NaN` or `R = NaN`.
    - One-sided slope overflow should confirm whether `slope_error = Inf`
      remains an exposed diagnostic with `ES = 0.0` or becomes a rejection.
    - DTW overflow / no-path cases should assert the chosen clearer behavior:
      either stable finite scoring or a specific pre-score rejection message.
    - Phase tie-priority tests should include near-identical signals where a
      no-tolerance selector would choose one shift; the production scorer
      should prefer the smaller shift under the documented tolerance.
    - Near-one raw score examples for `Z`, `EM`, `ES`, and `R` should confirm
      raw `.scores` preserve values below `1.0` while `*_rating()` rounds as
      documented.

- Reinforce f16 implementation
  - Align with C++23 stl implementation

- Need to wire annex.py and signals.py into main.py and notebook
- signal.py should be able to generate sufficent edge cases. In this case instead of hard coding the numbers, we need to be able to produce the array programmatically.

- Since the documentation does say about having same timeframe, and having interval, and 10ms, we need to emit warning at least.

- Need to check if the test implementations are faithful implementations of the descriptions here

- Default values source?

- Test helper functions are all over the place
- Study proper shifted n < 9 path; maybe the throw is unnecessary
- Need to find magic numbers
