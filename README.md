# ISO/TS 18571 Scoring

This repository contains a native ISO/TS 18571 scorer for vehicle signal validation.

## Public API

```python
from iso18571 import ISO18571

score = ISO18571(reference_curve, comparison_curve)
overall = score.overall_rating()
```

Curves are NumPy-compatible arrays with shape `(n, 2)`: time in the first column and signal value in the second column. The native scorer derives the sample interval from matching, uniformly spaced time columns.

For a small runnable CSV example, use the bundled demo files in `examples/reference.csv` and `examples/comparison.csv`:

```bash
uv run python main.py
```

To regenerate those two demo CSVs:

```bash
uv run python tools/example_data.py
```

For a guided notebook with signal plots, open:

```bash
uv run --extra examples jupyter lab examples/quickstart.ipynb
```

The notebook walks through the bundled demo CSVs, a downloaded official Annex case, and a generated Annex case. The same data prep can be run from the terminal:

```bash
uv run python tools/example_data.py --all
```

The package also exposes a native diagnostic helper:

- `iso18571.backend_info()`

```python
from iso18571 import backend_info

print(backend_info())
# {'name': 'iso18571', 'implementation': 'C++20', 'version': '1.0.6', 'optimization': 'x86-64-v3'}
```

## Prerequisites

Install `uv` first; all project commands below assume it is available on `PATH`.

Production releases are wheel-only and intentionally support only CPython 3.12,
3.13, and 3.14 on Linux x86_64 and Windows AMD64. ARM/aarch64, macOS,
musllinux, and other targets are unsupported. Source builds from a checkout use
the same x86_64/AMD64-only native engine; unsupported architectures fail during
native configuration.

Native builds need one of these build environments:

- local build tools for editable installs and single-interpreter wheels: a C++20 compiler, CMake-compatible build tooling, and Python development headers;
- Docker or Podman for Linux manylinux wheels, including Docker Desktop when building Linux wheels from a Windows host;
- on Linux hosts, `clang-cl`, `lld-link`, `llvm-rc`, `llvm-mt`, `objdump`, and `xwin` for Windows cross-built wheels;
- on Windows hosts, Visual Studio Build Tools with MSVC for native Windows wheels.

`uv` creates the Python build and test environments from `pyproject.toml`.

Windows wheel users need the Microsoft Visual C++ Redistributable 2015-2022 x64 installed at runtime. Install it from Microsoft's latest supported Visual C++ Redistributable downloads page. The wheels link to those runtime DLLs dynamically and do not bundle them.

On Debian/Ubuntu, install Docker Engine from Docker's official Ubuntu instructions. Then install the remaining local release-build tools with:

```bash
curl -LsSf https://astral.sh/uv/install.sh | sh
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
sudo apt update
sudo apt install -y build-essential cmake binutils clang lld llvm
cargo install xwin --locked
```

If Docker requires group setup, follow Docker's Linux post-install instructions and log out and back in after changing group membership.

On other Linux distributions, use Docker's distribution-specific Engine docs or Podman packages, install Rust/Cargo with rustup, and install equivalent packages for binutils and LLVM/clang-cl/lld.

Official prerequisite pages:

- Docker Engine: <https://docs.docker.com/engine/install/>
- Docker Engine on Ubuntu: <https://docs.docker.com/engine/install/ubuntu/>
- Docker Desktop on Windows: <https://docs.docker.com/desktop/setup/install/windows-install/>
- Docker Linux post-install: <https://docs.docker.com/engine/install/linux-postinstall/>
- Rust/Cargo: <https://doc.rust-lang.org/cargo/getting-started/installation.html>
- Visual Studio Build Tools: <https://visualstudio.microsoft.com/downloads/?q=build+tools>
- Microsoft Visual C++ Redistributable: <https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist>

## Building

For an editable local native build:

```bash
uv pip install -e .
```

For a wheel targeting the current interpreter and platform:

```bash
uv build --wheel
```

For release-wheel builds, sync the build tooling once:

```bash
uv sync --extra build
```

The release wheel matrix targets CPython 3.12, 3.13, and 3.14 on:

- Linux x86_64;
- Windows AMD64.

Use the project wheel builder as the normal entrypoint. On a Linux host it builds Linux wheels with `cibuildwheel` manylinux containers and Windows wheels with `clang-cl` plus `xwin`. On a Windows host it uses `cibuildwheel` for both native Windows MSVC wheels and Linux wheels through Docker.

For a full local release build:

```bash
uv run python tools/build_wheels.py --platform all
```

Do not publish source distributions for release. The PyPI release artifact set is
wheel-only so unsupported architectures fail fast with no compatible
distribution instead of attempting an unsupported native source build.

For Windows wheels:

```bash
uv run python tools/build_wheels.py \
  --platform windows \
  --python 3.12 3.13 3.14
```

On Linux hosts, if the xwin SDK/CRT cache is missing, the Windows cross-build provisions it and accepts Microsoft's SDK redistribution terms by default. To require manual provisioning instead:

```bash
uv run python tools/build_wheels.py \
  --platform windows \
  --no-accept-ms-license
```

For Linux wheels only, keep Docker or Podman running:

```bash
uv run python tools/build_wheels.py --platform linux
```

The same Linux-wheel command is supported from Windows when Docker Desktop is running.

`uv build --wheel` and `tools/build_wheels.py` both write to `dist/` in the examples above. That is also the default input path for `uv publish`.

## Testing

Production scoring is native-only. Python reference scorers live in `iso18571_reference` for parity tests and research; they are not installed in production wheels.

The official ISO/TS 18571 Annex CSV data is downloaded into the pytest cache on first test run. Generated fixed-signal and phase-shift Annex cases are also written into the pytest cache so a fresh clone can run the same parity suite.

Run the standard checks:

```bash
uv run --extra test ruff check --fix .
uv run --extra test ruff format .
uv run --extra test ruff check .
uv run --extra test ruff format --check .
uv run --extra test mypy iso18571 iso18571_reference tests
uv run --extra test python -m pytest -q
```

## References And Licensing

This project implements
[ISO/TS 18571:2024](https://www.iso.org/standard/85791.html), "Road vehicles - Objective rating metric for non-ambiguous signals". It includes a clean-room native scorer and parity tests informed by public ISO/TS 18571 validation work, including the TU Graz/OpenVT [Objective Rating Metric ISO18571](https://openvt.eu/validation-metrics/ISO18571) project.

The production `iso18571` package is MIT-licensed under `LICENSE`. All source files remain MIT-licensed unless marked otherwise. The source-only `iso18571_reference/rating_dtw_python.py` test/research wrapper is `GPL-3.0-or-later` because it uses the GPL-licensed `dtw-python` backend. The reference modules are not installed in production wheels.

## Benchmarks

Benchmark tests are excluded from default pytest. They compare `native`, `dtwalign`, `dtw_python`, and `librosa` on a mixed signal at lengths `512`, `2048`, `8192`, and `32768`.

```bash
uv run --extra test python -m pytest -q -m benchmark --benchmark-json .benchmarks/iso18571-readme/benchmarks.json
```

The benchmark report separates setup/load behavior from dynamic calculation behavior:

- `load_memory` rows measure a fresh spawned Python process importing the backend, generating data, scoring once, and reporting peak process memory and peak swap/pagefile usage.
- `runtime` rows measure repeated scoring in a warmed spawned process.
- Any row with `extra_info.swap_invalidated == true` leaked into swap/pagefile; keep it as a stress outcome, but do not treat its timing as a valid in-memory runtime number.

Large reference-backend rows may need substantial swap to complete. On Linux, create temporary swap outside the test runner before the full benchmark matrix:

```bash
sudo fallocate -l 64G /swap_iso18571_bench.img
sudo chmod 600 /swap_iso18571_bench.img
sudo mkswap /swap_iso18571_bench.img
sudo swapon /swap_iso18571_bench.img
```

After benchmark collection, remove the temporary swap file when it is no longer needed:

```bash
sudo swapoff /swap_iso18571_bench.img
sudo rm /swap_iso18571_bench.img
```

Benchmark snapshot from this machine:

### Load Time, ms

| Backend | 512 | 2048 | 8192 | 32768 |
| --- | ---: | ---: | ---: | ---: |
| native | 125.77 | 143.70 | 153.15 | 603.32 |
| dtwalign | 3898.99 | 3813.41 | 4897.01 | - |
| dtw_python | 931.82 | 995.43 | 1803.67 | - |
| librosa | 1231.87 | 1274.18 | 2253.83 | 17590.40 |

### Peak RSS, MiB

| Backend | 512 | 2048 | 8192 | 32768 |
| --- | ---: | ---: | ---: | ---: |
| native | 46.07 | 46.16 | 49.63 | 99.29 |
| dtwalign | 359.48 | 450.83 | 2105.46 | - |
| dtw_python | 253.07 | 431.89 | 3278.38 | - |
| librosa | 312.04 | 385.59 | 1759.17 | 24522.72 |

### Runtime, ms

| Backend | 512 | 2048 | 8192 | 32768 |
| --- | ---: | ---: | ---: | ---: |
| native | 0.19 | 2.45 | 29.91 | 475.39 |
| dtwalign | 6.97 | 87.72 | 1177.31 | - |
| dtw_python | 7.01 | 62.88 | 936.57 | - |
| librosa | 7.02 | 77.73 | 1049.70 | 16130.18 |
