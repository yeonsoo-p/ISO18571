# TODO

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
  - test_nonconstant_offset_expectations_are_score_specific
  - I have serious doubts about type conversion. Need to figure out what exactly is tested and how it is evaluated

- Near-threshold / nonfinite score follow-up from temp characterization
  - Implementation narrowing needed:
    - Decide and enforce the finite accepted-input score contract:
      scores `Z`, `EP`, `EM`, `ES`, and `R` should either be finite and
      inside `[0.0, 1.0]`, or the scorer should reject the input before
      returning scores with a clear error.
    - Near-`1.0` raw scores such as `0.999999999` are expected formula
      behavior, not necessarily bugs. Document that default rating methods
      round these to `1.0`, and any grade/classification logic must use raw
      `R`, not rounded `overall_rating()`.
  - New tests needed to confirm the decisions:
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
