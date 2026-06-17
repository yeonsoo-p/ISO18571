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

The package also exposes a native diagnostic helper:

- `iso18571.backend_info()`

## References And Tests

Production scoring is native-only. Python reference scorers live in
`iso18571_reference` for parity tests and research; they are not installed in
production wheels.

The official ISO/TS 18571 Annex CSV data is downloaded into the pytest cache on
first test run. Generated fixed-signal and phase-shift Annex cases are also
written into the pytest cache so a fresh clone can run the same parity suite.

Run the main validation gates:

```bash
uv pip install -e .
uv run --extra test ruff check --fix .
uv run --extra test ruff format .
uv run --extra test ruff check .
uv run --extra test ruff format --check .
uv run --extra test mypy iso18571 iso18571_reference tests
uv run --extra test python -m pytest -q
uv build
```

## Benchmarks

Benchmark tests are excluded from default pytest. They compare `native`,
`dtwalign`, `dtw_python`, and `librosa` on a mixed signal at lengths `512`,
`2048`, `8192`, and `32768`.

```bash
uv run --extra test python -m pytest -q -m benchmark --benchmark-json .benchmarks/iso18571-readme/benchmarks.json
```

The benchmark report separates setup/load behavior from dynamic calculation
behavior:

- `load_memory` rows measure a fresh spawned Python process importing the
  backend, generating data, scoring once, and reporting peak process memory and
  peak swap/pagefile usage.
- `runtime` rows measure repeated scoring in a warmed spawned process.
- Any row with `extra_info.swap_invalidated == true` leaked into swap/pagefile;
  keep it as a stress outcome, but do not treat its timing as a valid in-memory
  runtime number.

Large reference-backend rows may need substantial swap to complete. On Linux,
create temporary swap outside the test runner before the full benchmark matrix:

```bash
sudo fallocate -l 64G /swap_iso18571_bench.img
sudo chmod 600 /swap_iso18571_bench.img
sudo mkswap /swap_iso18571_bench.img
sudo swapon /swap_iso18571_bench.img
```

After benchmark collection, remove the temporary swap file when it is no longer
needed:

```bash
sudo swapoff /swap_iso18571_bench.img
sudo rm /swap_iso18571_bench.img
```

Benchmark snapshot from this machine:

### Load Time, ms

| Backend | 512 | 2048 | 8192 | 32768 |
| --- | ---: | ---: | ---: | ---: |
| native | 123.92 | 123.23 | 163.67 | 776.39 |
| dtwalign | 3898.99 | 3813.41 | 4897.01 | 22929.49 |
| dtw_python | 931.82 | 995.43 | 1803.67 | 38670.55 |
| librosa | 1231.87 | 1274.18 | 2253.83 | 17590.40 |

### Peak RSS, MiB

| Backend | 512 | 2048 | 8192 | 32768 |
| --- | ---: | ---: | ---: | ---: |
| native | 46.05 | 45.89 | 58.86 | 250.84 |
| dtwalign | 359.48 | 450.83 | 2105.46 | 28585.85 |
| dtw_python | 253.07 | 431.89 | 3278.38 | 29065.60 |
| librosa | 312.04 | 385.59 | 1759.17 | 24522.72 |

### Runtime, ms

| Backend | 512 | 2048 | 8192 | 32768 |
| --- | ---: | ---: | ---: | ---: |
| native | 0.19 | 2.44 | 35.78 | 610.56 |
| dtwalign | 6.97 | 87.72 | 1177.31 | 22621.32 |
| dtw_python | 7.01 | 62.88 | 936.57 | 36806.96 |
| librosa | 7.02 | 77.73 | 1049.70 | 16130.18 |

### Swap Invalidated

| Backend | 512 | 2048 | 8192 | 32768 |
| --- | --- | --- | --- | --- |
| native | no | no | no | no |
| dtwalign | no | no | no | yes |
| dtw_python | no | no | no | yes |
| librosa | no | no | no | no |
