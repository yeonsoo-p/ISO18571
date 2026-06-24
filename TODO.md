# TODO

- Implement more robust floating point number equality
  - ULP or rtol + atol
- Implement warning messages for non f64 inputs

- Revamp test suite including
  - Against Annex & Inter-backend:
    - Overall rating R
    - Corridor score Z
    - Phase score EP
    - Magnitude score EM
    - Slope score ES
    - Original test and CAE signals
    - Inner corridor upper/lower curves
    - Outer corridor upper/lower curves
    - Phase-shifted test curve
    - Phase-shifted CAE curve
    - Slope of shifted test curve
    - Slope of shifted CAE curve

  - Input data lengths
    - Shorter than 9
    - Shorter than 9 when preferred shift makes it so

  - Input data types
    - Unsigned integer
    - Signed integer
    - Non-f64 floats
    - f64 floats
    - Complex floats

  - Input signal type
    - Non-zero identical signals
    - Constant identical signals
    - Phase shifted identical signals
    - Zero signals
    - Constant offset signals
    - Signal types of different classes
  
  - Input signal values
    - NaN/Inf
    - Finite large amplitudes
    - Finite small amplitudes
  
  - Input signal times
    - Non-uniform
    - Uniform small dt
    - Uniform large dt

- Investigate edge cases
  - Integer overflow during time validation for signed integers
  - `NaN` corridor scores from finite subnormal signal amplitudes
  - Short-curve phase behavior

- Reinforce f16 implementation
  - Align with C++23 stl implementation
  - Review pack_long_double()
