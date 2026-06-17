# Euro NCAP ISO/TS 18571 Scoring

This repository contains a native ISO/TS 18571 scorer used for Euro NCAP
validation work.

## Public API

```python
from iso18571 import ISO18571

score = ISO18571(reference_curve, comparison_curve, dt=0.0001)
overall = score.overall_rating()
```

Curves are NumPy-compatible arrays with shape `(n, 2)`: time in the first column
and signal value in the second column.

The package also exposes native diagnostics and lower-level helpers:

- `iso18571.score_components(...)`
- `iso18571.magnitude_ratio(...)`
- `iso18571.warp_path(...)`
- `iso18571.backend_info()`

## References And Tests

Production scoring is native-only. Python reference scorers live in
`iso18571_reference` for parity tests and research; they are not installed in
production wheels.

Run the main validation gates:

```bash
uv pip install -e .
uv run --extra test python -m pytest -q
uv build --wheel
uv run --with dist/euroncap-0.1.0-*.whl python tools/iso18571/wheel_smoke.py
```
