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

Generated fixed-signal Annex benchmark for `local_iso_native`, using the same
Annex pass harness over five representative generated cases in
`tests/test_iso18571_benchmarks.py`:

| Annex pass | Median |
|---|---:|
| Official 42 CSV cases | 10.3895 ms |
| Generated fixed-signal cases | 149.4609 ms |

The generated fixed-signal cases are Annex-shaped fixtures, not official ISO CSV
files. They cover short sine/noise, Annex-like amplitude/offset sine, long
chirp, long Gaussian noise, and long sparse-spike families through the same
scorer path used for the official Annex benchmarks.

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

Validate generated Annex parity across `rating_original`, `local_iso_native`,
`dtw_python`, and `librosa`:

```bash
uv run --with pytest --with pytest-benchmark \
  --with dtwalign --with scipy --with dtw-python --with librosa \
  python -m pytest -q tests/test_rating_original_parity.py \
  -o addopts= -m oracle
```

Run long generated fixed-signal Annex stress cases:

```bash
uv run --with pytest --with pytest-benchmark \
  python -m pytest -q tests/test_rating_original_parity.py \
  -o addopts= -m stress
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

Benchmark official and generated fixed-signal Annex cases:

```bash
uv run --with pytest --with pytest-benchmark \
  python -m pytest -q tests/test_iso18571_benchmarks.py \
  -o addopts= -m benchmark \
  --benchmark-warmup off \
  --benchmark-min-rounds 3 \
  --benchmark-max-time 3
```

Run the native DTW layout and parallelization threshold benchmark categories:

```bash
uv run --with pytest --with pytest-benchmark \
  python -m pytest -q tests/test_iso18571_threshold_benchmarks.py \
  -o addopts= -m "benchmark and not threshold" \
  --benchmark-warmup off \
  --benchmark-min-rounds 3 \
  --benchmark-max-time 3

uv run --with pytest --with pytest-benchmark \
  python -m pytest -q tests/test_iso18571_threshold_benchmarks.py \
  -o addopts= -m threshold \
  --benchmark-warmup off \
  --benchmark-min-rounds 1 \
  --benchmark-max-time 0.1 \
  --benchmark-json .benchmarks/iso18571-threshold/threshold.json

uv run python tools/iso18571/analyze_parallel_threshold.py \
  .benchmarks/iso18571-threshold/threshold.json
```

Run the native full-scorer performance-regime atlas:

```bash
uv run --with pytest --with pytest-benchmark \
  python -m pytest -q tests/test_iso18571_regime_benchmarks.py \
  -o addopts= -m regime \
  --benchmark-warmup off \
  --benchmark-min-rounds 1 \
  --benchmark-max-time 0.1 \
  --benchmark-json .benchmarks/iso18571-regime/regime.json

uv run python tools/iso18571/analyze_variant_regimes.py \
  .benchmarks/iso18571-regime/regime.json
```

The regime atlas reports each variant by `effective_n`, DTW cell count, signal
family, thread count, median runtime, IQR, speed ratio to current serial native,
and a per-regime class. It is intentionally not a universal ranking.

SIMD experiments are an additional enum-based atlas axis. Benchmark environment
variables remain readable strings, but tests map them to native `DtwLayout`,
`ReductionMode`, `ParallelMode`, and `SimdLevel` values before calling C++.
Supported SIMD levels are `scalar`, `sse2`, `avx2`, `avx2_fma`, and `auto`;
AVX-512 is intentionally excluded. `auto` is runtime dispatch behavior for
smoke/parity checks, not a regime benchmark axis.

Focused atlas runs can be filtered with environment variables:

```bash
ISO18571_REGIME_FAMILIES=chirp,gaussian_noise,phase_chirp_shift_050 \
ISO18571_REGIME_LENGTHS=8192,16384,32768,65536 \
ISO18571_REGIME_THREADS=1,2,4,8,12,16,24 \
ISO18571_REGIME_VARIANTS=dtw_current+reduce_none+parallel_none,dtw_current+all_reductions+blocked128 \
ISO18571_REGIME_SIMD_LEVELS=scalar,sse2,avx2,avx2_fma \
uv run --with pytest --with pytest-benchmark \
  python -m pytest -q tests/test_iso18571_regime_benchmarks.py \
  -o addopts= -m regime \
  --benchmark-warmup off \
  --benchmark-min-rounds 1 \
  --benchmark-max-time 0.05 \
  --benchmark-json .benchmarks/iso18571-regime/focused.json
```

Emit and inspect native SIMD assembly without changing source:

```bash
uv run python tools/iso18571/emit_native_assembly.py \
  --output-dir .benchmarks/iso18571-asm
uv run python tools/iso18571/report_assembly_wrinkles.py \
  .benchmarks/iso18571-asm
```

Latest no-auto thread-ceiling atlas result: for nominal lengths `4096, 8192,
12288, 16384, 32768, 65536` across chirp, Gaussian noise, sparse spikes, and
analytic phase families, all 1296 enum/SIMD rows passed parity checks.
`dtw_current+all_reductions+blocked128` beat the scalar serial baseline in every
tested family, but the best thread count was size dependent: 8 threads was
strong around input length 8192, 12 threads around 12288-16384, and 16/24
threads around 32768-65536. The analyzer produced candidate thresholds, but
this remains an atlas result rather than a production dispatch policy.

Build a Linux wheel:

```bash
uv build --wheel
```
