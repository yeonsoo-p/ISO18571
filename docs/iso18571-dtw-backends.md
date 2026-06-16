# ISO/TS 18571 DTW Backends

This project keeps `local_iso_numpy` as the auditable Python reference backend
for ISO/TS 18571 magnitude DTW. The native backend target is
`local_iso_native`, a clean-room compiled implementation that preserves the same
windowing, recurrence, tie order, and Annex score behavior while improving
runtime.

Production recommendation criteria:

- all 42 Annex CSV cases pass within `0.001` for `R`, `Z`, `EP`, `EM`, and `ES`;
- first-use Annex pass is faster than `dtw_python`;
- steady-state Annex-pass median is faster than `librosa`;
- Linux and Windows wheels can import the module and run at least one Annex case.

## Benchmark Snapshot

Measured on 2026-06-16 using `tests/test_iso18571_benchmarks.py`, fresh pytest
process per backend, `--benchmark-min-rounds=5`, `--benchmark-max-time=5`,
and benchmark warmup disabled.

| Backend | First-use pass | Inferred prep | Steady median pass | Steady rounds |
|---|---:|---:|---:|---:|
| `dtw_python` | 0.4457s | 0.1839s | 0.2618s | 20 |
| `librosa` | 0.9754s | 0.7229s | 0.2526s | 20 |
| `local_iso_native` | 0.0111s | 0.0007s | 0.0104s | 477 |

Current native backend status: correctness validated for the Annex suite and
both performance targets beaten on Linux. The `local_iso_native` path now ports
the full scorer into native code for NumPy-fed curves; other backends still
share the Python scorer and only replace the magnitude-DTW step.

Signal-family benchmark medians for `local_iso_native`, using
`tests/test_iso18571_signal_benchmarks.py` with `--run-stress`:

| Signal benchmark | Median |
|---|---:|
| `short_sine_noise_129` | 16.465 us |
| `annex_like_sine_amp_offset_1430` | 1.020 ms |
| `long_smooth_chirp_8192` | 32.487 ms |
| `long_noisy_gaussian_8192` | 83.732 ms |
| `long_sparse_spikes_8192` | 31.715 ms |

## Command Reference

Build editable native extension:

```bash
uv pip install -e .
```

Validate the reference and native backends:

```bash
uv run --with pytest --with pytest-benchmark \
  python -m pytest -q tests/test_iso18571_backends.py \
  --iso18571-backends local_iso_numpy,local_iso_native
```

Validate all production-eligible backends:

```bash
uv run --with pytest --with pytest-benchmark \
  --with dtwalign --with dtaidistance --with dtw-python --with librosa --with scipy \
  python -m pytest -q tests/test_iso18571_backends.py \
  --iso18571-backends local_iso_numpy,local_iso_native,dtwalign,dtaidistance,dtw_python,librosa
```

Benchmark in fresh pytest processes:

```bash
uv run --with pytest --with pytest-benchmark --with dtw-python --with librosa --with scipy \
  python tools/iso18571/run_backend_benchmarks.py \
  --output-dir .benchmarks/iso18571 \
  --backends local_iso_native dtw_python librosa \
  --max-time 5 --min-rounds 5

uv run python tools/iso18571/summarize_benchmarks.py .benchmarks/iso18571/*.json
```

Benchmark native scoring by signal family:

```bash
uv run --with pytest --with pytest-benchmark \
  python -m pytest -q tests/test_iso18571_signal_benchmarks.py \
  --run-stress \
  --benchmark-warmup off \
  --benchmark-min-rounds 3 \
  --benchmark-max-time 3
```

Build a Linux wheel:

```bash
uv build --wheel
```
