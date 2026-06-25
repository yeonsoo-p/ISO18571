from __future__ import annotations

import ctypes
import importlib
import multiprocessing
import os
import platform
import sys
import threading
import time
import traceback
from collections.abc import Mapping
from dataclasses import dataclass, field
from multiprocessing.connection import Connection
from multiprocessing.process import BaseProcess
from pathlib import Path
from typing import Any, Final, Protocol, TypeAlias

import numpy as np
import pytest
from numpy.typing import NDArray

from tools import signals
from tools.signals import SignalGenerator

try:
    import resource
except ImportError:  # pragma: no cover - Windows
    resource = None  # type: ignore[assignment]


BENCHMARK_BACKEND: Final = "native"
BENCHMARK_LENGTHS: Final = (512, 2048, 8192, 32768)
LOAD_BENCHMARK_ROUNDS_ENV: Final = "ISO18571_BENCHMARK_LOAD_ROUNDS"
RUNTIME_BENCHMARK_ROUNDS_ENV: Final = "ISO18571_BENCHMARK_RUNTIME_ROUNDS"
POSITIVE_SPIKE_POSITIONS: Final[tuple[float, ...]] = (0.08, 0.21, 0.47, 0.63, 0.82)
NEGATIVE_SPIKE_POSITIONS: Final[tuple[float, ...]] = (0.14, 0.36, 0.57, 0.74, 0.93)
TIMING_KEYS: Final = (
    "corridor_ms",
    "phase_ms",
    "magnitude_ms",
    "slope_ms",
    "total_ms",
)
NativeTimings: TypeAlias = dict[str, float]


class NativeScorer(Protocol):
    @property
    def timings(self) -> Mapping[str, float]: ...

    def overall_rating(self, ndigits: int = 3) -> float: ...


@dataclass(frozen=True)
class BenchmarkCase:
    reference_curve: NDArray[np.float64]
    comparison_curve: NDArray[np.float64]
    dt: float


@dataclass(frozen=True)
class ScoreRun:
    score: float
    timings: NativeTimings


@dataclass(frozen=True)
class WorkerResult:
    status: str
    score: float | None
    timings: NativeTimings | None
    child_elapsed_ms: float | None
    peak_rss_mib: float | None
    peak_swap_mib: float | None
    error: str | None


@dataclass(frozen=True)
class MemorySnapshot:
    peak_rss_mib: float | None = None
    peak_swap_mib: float | None = None

    def merged(self, other: MemorySnapshot) -> MemorySnapshot:
        return MemorySnapshot(
            peak_rss_mib=max_optional(self.peak_rss_mib, other.peak_rss_mib),
            peak_swap_mib=max_optional(self.peak_swap_mib, other.peak_swap_mib),
        )


@dataclass
class LoadProbe:
    length: int
    results: list[WorkerResult] = field(default_factory=list)

    def __call__(self) -> float:
        result = run_load_child(self.length)
        self.results.append(result)
        assert (
            result.status == "ok"
            and result.score is not None
            and result.timings is not None
        ), result.error or f"{BENCHMARK_BACKEND} n={self.length} load probe failed"
        return result.timings["total_ms"]

    @property
    def peak_rss_mib(self) -> float | None:
        values = [
            result.peak_rss_mib
            for result in self.results
            if result.peak_rss_mib is not None
        ]
        return max(values) if values else None

    @property
    def peak_swap_mib(self) -> float | None:
        values = [
            result.peak_swap_mib
            for result in self.results
            if result.peak_swap_mib is not None
        ]
        return max(values) if values else None

    @property
    def child_elapsed_ms(self) -> float | None:
        values = [
            result.child_elapsed_ms
            for result in self.results
            if result.child_elapsed_ms is not None
        ]
        return values[-1] if values else None

    @property
    def timings(self) -> NativeTimings | None:
        values = [
            result.timings for result in self.results if result.timings is not None
        ]
        return values[-1] if values else None


class ProcessMemoryCounters(ctypes.Structure):
    _fields_ = [
        ("cb", ctypes.c_ulong),
        ("PageFaultCount", ctypes.c_ulong),
        ("PeakWorkingSetSize", ctypes.c_size_t),
        ("WorkingSetSize", ctypes.c_size_t),
        ("QuotaPeakPagedPoolUsage", ctypes.c_size_t),
        ("QuotaPagedPoolUsage", ctypes.c_size_t),
        ("QuotaPeakNonPagedPoolUsage", ctypes.c_size_t),
        ("QuotaNonPagedPoolUsage", ctypes.c_size_t),
        ("PagefileUsage", ctypes.c_size_t),
        ("PeakPagefileUsage", ctypes.c_size_t),
    ]


class ProcessMemoryMonitor:
    def __init__(self, pid: int, interval_seconds: float = 0.01) -> None:
        self._pid = pid
        self._interval_seconds = interval_seconds
        self._stop = threading.Event()
        self._lock = threading.Lock()
        self._snapshot = MemorySnapshot()
        self._started = False
        self._thread = threading.Thread(
            target=self._run, name=f"memory-monitor-{pid}", daemon=True
        )

    def start(self) -> None:
        self._started = True
        self._thread.start()

    def stop(self) -> MemorySnapshot:
        self._stop.set()
        if self._started:
            self._thread.join(timeout=2.0)
        self.record_once()
        return self.snapshot()

    def snapshot(self) -> MemorySnapshot:
        with self._lock:
            return self._snapshot

    def record_once(self) -> None:
        snapshot = process_memory_snapshot(self._pid)
        with self._lock:
            self._snapshot = self._snapshot.merged(snapshot)

    def _run(self) -> None:
        while not self._stop.is_set():
            self.record_once()
            time.sleep(self._interval_seconds)


def max_optional(left: float | None, right: float | None) -> float | None:
    if left is None:
        return right
    if right is None:
        return left
    return max(left, right)


def record_native_timing_extra_info(
    benchmark: Any, timings: NativeTimings | None
) -> None:
    for key in TIMING_KEYS:
        benchmark.extra_info[f"native_{key}"] = (
            None if timings is None else timings[key]
        )


def positive_env_int(name: str, default: int) -> int:
    raw = os.environ.get(name)
    if raw is None:
        return default
    try:
        value = int(raw)
    except ValueError:
        value = None
    assert value is not None and value > 0, f"{name} must be a positive integer"
    return value


def make_benchmark_case(length: int) -> BenchmarkCase:
    assert length >= 9, "benchmark length must be at least 9"

    dt = 1.0 / float(length - 1)
    reference = (
        SignalGenerator(length, dt, seed=18571 + length)
        .add(signals.sine, frequency=4.0, amplitude=0.55)
        .add(signals.chirp, start_frequency=1.5, end_frequency=9.0, amplitude=0.22)
        .add(signals.ramp, slope=0.12, intercept=-0.06)
        .add(
            signals.piecewise_discontinuous,
            scale=0.18,
            breakpoints=(0.42, 0.71),
            values=(-1.0, 1.0, -0.6),
        )
        .add(
            signals.sparse_spikes,
            amplitude=0.24,
            positions=POSITIVE_SPIKE_POSITIONS,
        )
        .add(
            signals.sparse_spikes,
            amplitude=-0.20,
            positions=NEGATIVE_SPIKE_POSITIONS,
        )
        .curve()
    )
    comparison = (
        SignalGenerator(length, dt, seed=19571 + length)
        .add(signals.sine, frequency=4.0, amplitude=0.57, sample_shift=2)
        .add(
            signals.chirp,
            start_frequency=1.5,
            end_frequency=9.0,
            amplitude=0.22,
            sample_shift=2,
        )
        .add(signals.ramp, slope=0.10, intercept=-0.025)
        .add(
            signals.piecewise_discontinuous,
            scale=0.18,
            breakpoints=(0.42, 0.71),
            values=(-1.0, 1.0, -0.6),
        )
        .add(
            signals.sparse_spikes,
            amplitude=0.18,
            positions=POSITIVE_SPIKE_POSITIONS,
            sample_shift=2,
        )
        .add(
            signals.sparse_spikes,
            amplitude=-0.16,
            positions=NEGATIVE_SPIKE_POSITIONS,
            sample_shift=2,
        )
        .add(signals.gaussian_noise, std=0.006)
        .curve()
    )
    return BenchmarkCase(reference_curve=reference, comparison_curve=comparison, dt=dt)


def score_native_case(case: BenchmarkCase) -> ScoreRun:
    module = importlib.import_module("iso18571")
    scorer_class = getattr(module, "ISO18571")
    scorer: NativeScorer = scorer_class(case.reference_curve, case.comparison_curve)
    return ScoreRun(
        score=float(scorer.overall_rating(ndigits=-1)),
        timings=dict(scorer.timings),
    )


def linux_process_memory_snapshot(pid: int) -> MemorySnapshot:
    try:
        lines = (
            (Path("/proc") / str(pid) / "status")
            .read_text(encoding="utf-8")
            .splitlines()
        )
    except OSError:
        return MemorySnapshot()

    peak_rss_mib = None
    peak_swap_mib = None
    for line in lines:
        if line.startswith("VmHWM:") or line.startswith("VmRSS:"):
            parts = line.split()
            if len(parts) >= 2:
                peak_rss_mib = max_optional(peak_rss_mib, float(parts[1]) / 1024.0)
        elif line.startswith("VmSwap:"):
            parts = line.split()
            if len(parts) >= 2:
                peak_swap_mib = max_optional(peak_swap_mib, float(parts[1]) / 1024.0)
    return MemorySnapshot(peak_rss_mib=peak_rss_mib, peak_swap_mib=peak_swap_mib)


def linux_current_memory_snapshot() -> MemorySnapshot:
    snapshot = linux_process_memory_snapshot(os.getpid())
    if snapshot.peak_rss_mib is not None or resource is None:
        return snapshot
    return MemorySnapshot(
        peak_rss_mib=float(resource.getrusage(resource.RUSAGE_SELF).ru_maxrss) / 1024.0,
        peak_swap_mib=snapshot.peak_swap_mib,
    )


def windows_memory_snapshot_for_handle(process_handle: int) -> MemorySnapshot:
    counters = ProcessMemoryCounters()
    counters.cb = ctypes.sizeof(ProcessMemoryCounters)
    windll: Any = getattr(ctypes, "windll")
    ok = windll.psapi.GetProcessMemoryInfo(
        process_handle,
        ctypes.byref(counters),
        counters.cb,
    )
    if not ok:
        return MemorySnapshot()
    return MemorySnapshot(
        peak_rss_mib=float(counters.PeakWorkingSetSize) / (1024.0 * 1024.0),
        peak_swap_mib=float(counters.PeakPagefileUsage) / (1024.0 * 1024.0),
    )


def windows_process_memory_snapshot(pid: int) -> MemorySnapshot:
    windll: Any = getattr(ctypes, "windll")
    process_query_limited_information = 0x1000
    process_vm_read = 0x0010
    handle = windll.kernel32.OpenProcess(
        process_query_limited_information | process_vm_read,
        False,
        pid,
    )
    if not handle:
        return MemorySnapshot()
    try:
        return windows_memory_snapshot_for_handle(handle)
    finally:
        windll.kernel32.CloseHandle(handle)


def windows_current_memory_snapshot() -> MemorySnapshot:
    windll: Any = getattr(ctypes, "windll")
    return windows_memory_snapshot_for_handle(windll.kernel32.GetCurrentProcess())


def process_memory_snapshot(pid: int) -> MemorySnapshot:
    if sys.platform.startswith("linux"):
        return linux_process_memory_snapshot(pid)
    if sys.platform == "win32":
        return windows_process_memory_snapshot(pid)
    return MemorySnapshot()


def current_memory_snapshot() -> MemorySnapshot:
    if sys.platform.startswith("linux"):
        return linux_current_memory_snapshot()
    if sys.platform == "win32":
        return windows_current_memory_snapshot()
    if resource is None:
        return MemorySnapshot()
    value = float(resource.getrusage(resource.RUSAGE_SELF).ru_maxrss)
    if sys.platform == "darwin":
        return MemorySnapshot(peak_rss_mib=value / (1024.0 * 1024.0))
    return MemorySnapshot(peak_rss_mib=value / 1024.0)


def worker_result(
    status: str,
    score: float | None,
    timings: NativeTimings | None,
    child_elapsed_ms: float | None,
    error: str | None,
) -> WorkerResult:
    memory = current_memory_snapshot()
    return WorkerResult(
        status=status,
        score=score,
        timings=timings,
        child_elapsed_ms=child_elapsed_ms,
        peak_rss_mib=memory.peak_rss_mib,
        peak_swap_mib=memory.peak_swap_mib,
        error=error,
    )


def apply_monitor_snapshot(
    result: WorkerResult, snapshot: MemorySnapshot
) -> WorkerResult:
    return WorkerResult(
        status=result.status,
        score=result.score,
        timings=result.timings,
        child_elapsed_ms=result.child_elapsed_ms,
        peak_rss_mib=max_optional(result.peak_rss_mib, snapshot.peak_rss_mib),
        peak_swap_mib=max_optional(result.peak_swap_mib, snapshot.peak_swap_mib),
        error=result.error,
    )


def load_worker(length: int, connection: Connection) -> None:
    try:
        start = time.perf_counter()
        case = make_benchmark_case(length)
        score_run = score_native_case(case)
        child_elapsed_ms = (time.perf_counter() - start) * 1000.0
        connection.send(
            worker_result(
                "ok", score_run.score, score_run.timings, child_elapsed_ms, None
            )
        )
    except BaseException:
        connection.send(
            worker_result("failed", None, None, None, traceback.format_exc())
        )
    finally:
        connection.close()


def runtime_worker(length: int, connection: Connection) -> None:
    try:
        case = make_benchmark_case(length)
        _ = score_native_case(case)
        connection.send(worker_result("ready", None, None, None, None))
        while True:
            message = connection.recv()
            if message == "score":
                connection.send(score_native_case(case))
            elif message == "memory":
                connection.send(current_memory_snapshot())
            elif message == "stop":
                return
            else:
                assert False, f"unknown runtime worker message {message!r}"
    except BaseException:
        connection.send(
            worker_result("failed", None, None, None, traceback.format_exc())
        )
    finally:
        connection.close()


def multiprocessing_context() -> multiprocessing.context.SpawnContext:
    return multiprocessing.get_context("spawn")


def receive_worker_result(
    connection: Connection, process: BaseProcess, length: int
) -> WorkerResult:
    try:
        result = connection.recv()
        if isinstance(result, WorkerResult):
            return result
        return WorkerResult(
            status="failed",
            score=None,
            timings=None,
            child_elapsed_ms=None,
            peak_rss_mib=None,
            peak_swap_mib=None,
            error=f"{BENCHMARK_BACKEND} n={length} child returned {type(result)!r}",
        )
    except EOFError:
        process.join(timeout=1.0)

    if process.exitcode is not None:
        return WorkerResult(
            status="failed",
            score=None,
            timings=None,
            child_elapsed_ms=None,
            peak_rss_mib=None,
            peak_swap_mib=None,
            error=f"{BENCHMARK_BACKEND} n={length} child exited with code "
            f"{process.exitcode}",
        )
    return WorkerResult(
        status="failed",
        score=None,
        timings=None,
        child_elapsed_ms=None,
        peak_rss_mib=None,
        peak_swap_mib=None,
        error=f"{BENCHMARK_BACKEND} n={length} child did not report readiness",
    )


def run_load_child(length: int) -> WorkerResult:
    context = multiprocessing_context()
    parent, child = context.Pipe(duplex=False)
    process = context.Process(target=load_worker, args=(length, child))
    process.start()
    child.close()
    pid = process.pid
    assert pid is not None, "load child process has no pid"

    monitor = ProcessMemoryMonitor(pid)
    monitor.start()
    process.join()
    monitor_snapshot = monitor.stop()
    if parent.poll():
        result = parent.recv()
        if not isinstance(result, WorkerResult):
            result = WorkerResult(
                status="failed",
                score=None,
                timings=None,
                child_elapsed_ms=None,
                peak_rss_mib=None,
                peak_swap_mib=None,
                error=f"{BENCHMARK_BACKEND} n={length} child returned {type(result)!r}",
            )
    else:
        result = WorkerResult(
            status="failed",
            score=None,
            timings=None,
            child_elapsed_ms=None,
            peak_rss_mib=None,
            peak_swap_mib=None,
            error=f"{BENCHMARK_BACKEND} n={length} load child exited with code "
            f"{process.exitcode}",
        )
    parent.close()
    return apply_monitor_snapshot(result, monitor_snapshot)


def start_runtime_child(length: int) -> tuple[BaseProcess, Connection, WorkerResult]:
    context = multiprocessing_context()
    parent, child = context.Pipe(duplex=True)
    process = context.Process(target=runtime_worker, args=(length, child))
    process.start()
    child.close()
    result = receive_worker_result(parent, process, length)
    if result.status != "ready":
        stop_runtime_child(process, parent)
    assert result.status == "ready", (
        result.error or f"{BENCHMARK_BACKEND} n={length} runtime worker failed"
    )
    return process, parent, result


def stop_runtime_child(process: BaseProcess, connection: Connection) -> None:
    if process.is_alive():
        try:
            connection.send("stop")
        except (BrokenPipeError, EOFError, OSError):
            pass
        process.join(timeout=5.0)
    if process.is_alive():
        process.terminate()
        process.join(timeout=5.0)
    connection.close()


@pytest.mark.benchmark
@pytest.mark.parametrize("length", BENCHMARK_LENGTHS)
def test_native_mixed_signal_load_memory_benchmark(benchmark: Any, length: int) -> None:
    probe = LoadProbe(length)
    rounds = positive_env_int(LOAD_BENCHMARK_ROUNDS_ENV, 1)
    result = benchmark.pedantic(probe, rounds=rounds, iterations=1)
    benchmark.extra_info["metric"] = "load_memory"
    benchmark.extra_info["backend"] = BENCHMARK_BACKEND
    benchmark.extra_info["length"] = length
    benchmark.extra_info["platform"] = platform.platform()
    benchmark.extra_info["rounds"] = rounds
    benchmark.extra_info["peak_rss_mib"] = probe.peak_rss_mib
    benchmark.extra_info["peak_swap_mib"] = probe.peak_swap_mib
    benchmark.extra_info["swap_invalidated"] = (probe.peak_swap_mib or 0.0) > 0.0
    benchmark.extra_info["child_elapsed_ms"] = probe.child_elapsed_ms
    record_native_timing_extra_info(benchmark, probe.timings)
    assert isinstance(result, float)


@pytest.mark.benchmark
@pytest.mark.parametrize("length", BENCHMARK_LENGTHS)
def test_native_mixed_signal_runtime_benchmark(benchmark: Any, length: int) -> None:
    process, connection, ready = start_runtime_child(length)
    pid = process.pid
    assert pid is not None, "runtime child process has no pid"
    monitor = ProcessMemoryMonitor(pid)
    worker_memory: MemorySnapshot | None = None
    runtime_timings: list[NativeTimings] = []

    def score_in_worker() -> float:
        connection.send("score")
        score_run = connection.recv()
        assert isinstance(score_run, ScoreRun), (
            f"{BENCHMARK_BACKEND} n={length} runtime child returned {type(score_run)!r}"
        )
        runtime_timings.append(score_run.timings)
        return score_run.timings["total_ms"]

    rounds = positive_env_int(RUNTIME_BENCHMARK_ROUNDS_ENV, 3)
    try:
        monitor.start()
        result = benchmark.pedantic(score_in_worker, rounds=rounds, iterations=1)
        connection.send("memory")
        memory_message = connection.recv()
        if isinstance(memory_message, MemorySnapshot):
            worker_memory = memory_message
    finally:
        monitor_snapshot = monitor.stop()
        stop_runtime_child(process, connection)

    if worker_memory is not None:
        monitor_snapshot = monitor_snapshot.merged(worker_memory)

    peak_rss_mib = max_optional(ready.peak_rss_mib, monitor_snapshot.peak_rss_mib)
    peak_swap_mib = max_optional(ready.peak_swap_mib, monitor_snapshot.peak_swap_mib)
    benchmark.extra_info["metric"] = "runtime"
    benchmark.extra_info["backend"] = BENCHMARK_BACKEND
    benchmark.extra_info["length"] = length
    benchmark.extra_info["platform"] = platform.platform()
    benchmark.extra_info["rounds"] = rounds
    benchmark.extra_info["initial_peak_rss_mib"] = ready.peak_rss_mib
    benchmark.extra_info["peak_rss_mib"] = peak_rss_mib
    benchmark.extra_info["peak_swap_mib"] = peak_swap_mib
    benchmark.extra_info["swap_invalidated"] = (peak_swap_mib or 0.0) > 0.0
    record_native_timing_extra_info(
        benchmark, runtime_timings[-1] if runtime_timings else None
    )
    assert isinstance(result, float)
