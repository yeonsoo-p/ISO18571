# ISO/TS 18571 Native Scorer And Reference Parity

The production scorer is `iso18571.ISO18571`, backed by the native extension
`iso18571._core`.

The package no longer exposes backend selection. Reference implementations live
in `iso18571_reference` for tests and research only; they are not installed into
production wheels.

## Production Path

- Python accepts NumPy `(n, 2)` curves and passes them to the native extension.
- The C++ scorer owns phase alignment, corridor scoring, magnitude DTW, slope
  scoring, and weighted final scoring.
- Public package exports are intentionally small:
  - `ISO18571`
  - `score_components(reference_curve, comparison_curve, params)`
  - `magnitude_ratio(x, y, window_size)`
  - `warp_path(x, y, window_size)`
  - `backend_info()`

## Preserved DTW Behavior

The native scorer keeps the ISO behavior used by the audited local reference:

- squared local cost;
- Sakoe-Chiba radius `min(n, max(1, ceil(window_size*n)))`;
- valid cells where `abs(i-j) < radius`;
- predecessor tie priority vertical, horizontal, diagonal;
- official Annex `R`, `Z`, `EP`, `EM`, and `ES` within `0.001`.

## x86-64 Dispatch

The C++ source remains scalar. The build creates compiler-optimized source
variants:

- `x86-64-v1` baseline, always built;
- GCC/Clang `x86-64-v2`, `x86-64-v3`, and `x86-64-v4` when the compiler accepts
  the corresponding flag;
- MSVC v3/v4 variants when `/arch:AVX2` or `/arch:AVX512` are accepted. MSVC has
  no direct x86-64-v2 flag, so there is no MSVC v2 object.

At module initialization, C++ detects the running CPU and operating-system save
support, then stores direct function pointers for the highest
compiled-and-supported level. Python does not configure this choice.

Use `iso18571.backend_info()` to inspect compiled levels and the selected level
for the current process.

## Reference Parity

The test suite compares four full scorers:

- `native`: `iso18571.ISO18571`
- `dtwalign`: `iso18571_reference.rating_dtwalign.ISO18571`
- `dtw_python`: `iso18571_reference.rating_dtw_python.ISO18571`
- `librosa`: `iso18571_reference.rating_librosa.ISO18571`

All three reference modules share the same Python phase, corridor, slope, and
weighting logic. They differ only in the magnitude-DTW path implementation.

Parity covers:

- downloaded official Annex CSV files, checked against official expected scores;
- one generated Annex set combining fixed signal families and analytic
  phase-shift families;
- `n_eps`, `rho_e`, unrounded `Z`, `EP`, `EM`, `ES`, and `R`;
- rounded three-decimal score outputs.

Degenerate generated cases pass only when all scorers produce matching numeric
results with `equal_nan=True` or matching exception types.

## Validation

```bash
uv pip install -e .
uv run --extra test ruff check --fix .
uv run --extra test ruff format .
uv run --extra test ruff check .
uv run --extra test ruff format --check .
git diff --check
uv run --extra test python -m pytest -q
uv build --wheel
uv run --with dist/euroncap-0.1.0-*.whl python tools/iso18571/wheel_smoke.py
```
