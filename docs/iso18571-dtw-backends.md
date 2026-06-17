# ISO/TS 18571 Native Scorer

The project now has one production scorer: `iso18571.ISO18571`, backed by
`iso18571_native.score_components`.

Older package-backend comparison work has been retired from production code.
The remaining non-native implementations are test-only parity references:
`original`, `dtw_python`, and `librosa`.

## Production Path

- Python accepts NumPy `(n, 2)` curves and passes them to the native extension.
- The C++ scorer owns phase alignment, corridor scoring, magnitude DTW, slope
  scoring, and weighted final scoring.
- Public Python no longer exposes backend selection.
- Public native bindings are intentionally small:
  - `backend_info()`
  - `warp_path(x, y, window_size)`
  - `magnitude_ratio(x, y, window_size)`
  - `score_components(reference_curve, comparison_curve, params)`

## Preserved DTW Behavior

The native scorer keeps the ISO behavior used by the previous audited local
reference:

- squared local cost;
- Sakoe-Chiba radius `max(1, ceil(window_size*n))`;
- valid cells where `abs(i-j) < radius`;
- predecessor tie priority vertical, horizontal, diagonal;
- official Annex `R`, `Z`, `EP`, `EM`, and `ES` within `0.001`.

## x86-64 Dispatch

The C++ source remains scalar. The build creates several compiler-optimized
source variants:

- `x86-64-v1` baseline, always built;
- `x86-64-v2`, `x86-64-v3`, and `x86-64-v4` when the compiler accepts the
  corresponding flag.

At module initialization, C++ detects the running CPU and operating-system save
support, then stores direct function pointers for the highest
compiled-and-supported level. Python neither exposes nor configures this choice.

Use `iso18571_native.backend_info()` to inspect the compiled levels and the
selected level for the current process.

## Validation

Install the editable package:

```bash
uv pip install -e .
```

Run style checks:

```bash
uv run --extra test ruff check --fix .
uv run --extra test ruff format .
uv run --extra test ruff check .
uv run --extra test ruff format --check .
git diff --check
```

Run parity tests:

```bash
uv run --extra test python -m pytest -q
```

Build and smoke-test a wheel:

```bash
uv build --wheel
uv run --with dist/euroncap-0.1.0-*.whl python tools/iso18571/wheel_smoke.py
```

## Parity Scope

The test suite uses two Annex sources:

- downloaded official Annex CSV files, checked against official expected scores;
- one generated Annex set combining fixed signal families and analytic
  phase-shift families at practical lengths.

Generated cases compare all four test-only scorer labels (`native`, `original`,
`dtw_python`, and `librosa`) for:

- `n_eps`;
- `rho_e`;
- unrounded `Z`, `EP`, `EM`, `ES`, and `R`;
- rounded three-decimal score outputs.

Degenerate generated cases pass only when all scorers produce matching numeric
results with `equal_nan=True` or matching exception types.
