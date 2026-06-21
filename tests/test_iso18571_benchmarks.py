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
from dataclasses import dataclass, field
from multiprocessing.connection import Connection
from multiprocessing.process import BaseProcess
from pathlib import Path
from typing import Any, Protocol, cast

import numpy as np
import pytest

try:
    import resource
except ImportError:  # pragma: no cover - Windows
    resource = None  # type: ignore[assignment]

from tests.iso18571_benchmark_data import (
    BENCHMARK_BACKENDS,
    BENCHMARK_LENGTHS,
    BenchmarkSignal,
    make_mixed_signal,
)


class BenchmarkScorer(Protocol):
    def overall_rating(self, ndigits: int = 3) -> float: ...


class ScorerClass(Protocol):
    def __call__(
        self,
        reference_curve: np.ndarray,
        comparison_curve: np.ndarray,
        **kwargs: float,
    ) -> BenchmarkScorer: ...


@dataclass(frozen=True)
class WorkerResult:
    status: str
    score: float | None
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
class SetupProbe:
    backend: str
    length: int
    results: list[WorkerResult] = field(default_factory=list)

    def __call__(self) -> float:
        result = run_setup_child(self.backend, self.length)
        self.results.append(result)
        if result.status != "ok" or result.score is None:
            raise AssertionError(
                result.error or f"{self.backend} n={self.length} setup probe failed"
            )
        return result.score

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


SCORER_IMPORTS = {
    "native": ("iso18571", "ISO18571"),
    "dtwalign": ("reference.rating_dtwalign", "ISO18571"),
    "dtw_python": ("reference.rating_dtw_python", "ISO18571"),
    "librosa": ("reference.rating_librosa", "ISO18571"),
}


def max_optional(left: float | None, right: float | None) -> float | None:
    if left is None:
        return right
    if right is None:
        return left
    return max(left, right)


def scorer_class(backend: str) -> ScorerClass:
    module_name, class_name = SCORER_IMPORTS[backend]
    module = importlib.import_module(module_name)
    return cast(ScorerClass, getattr(module, class_name))


def score_case(backend: str, case: BenchmarkSignal) -> float:
    score_cls = scorer_class(backend)
    if backend == "native":
        scorer = score_cls(case.reference_curve, case.comparison_curve)
    else:
        scorer = score_cls(case.reference_curve, case.comparison_curve, dt=case.dt)
    return scorer.overall_rating(ndigits=-1)


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
    child_elapsed_ms: float | None,
    error: str | None,
) -> WorkerResult:
    memory = current_memory_snapshot()
    return WorkerResult(
        status=status,
        score=score,
        child_elapsed_ms=child_elapsed_ms,
        peak_rss_mib=memory.peak_rss_mib,
        peak_swap_mib=memory.peak_swap_mib,
        error=error,
    )


class ProcessMemoryMonitor:
    def __init__(self, pid: int, interval_seconds: float = 0.01) -> None:
        self._pid = pid
        self._interval_seconds = interval_seconds
        self._stop = threading.Event()
        self._lock = threading.Lock()
        self._snapshot = MemorySnapshot()
        self._thread = threading.Thread(
            target=self._run, name=f"memory-monitor-{pid}", daemon=True
        )

    def start(self) -> None:
        self._thread.start()

    def stop(self) -> MemorySnapshot:
        self._stop.set()
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


def apply_monitor_snapshot(
    result: WorkerResult, snapshot: MemorySnapshot
) -> WorkerResult:
    return WorkerResult(
        status=result.status,
        score=result.score,
        child_elapsed_ms=result.child_elapsed_ms,
        peak_rss_mib=max_optional(result.peak_rss_mib, snapshot.peak_rss_mib),
        peak_swap_mib=max_optional(result.peak_swap_mib, snapshot.peak_swap_mib),
        error=result.error,
    )


def setup_worker(backend: str, length: int, connection: Connection) -> None:
    try:
        start = time.perf_counter()
        case = make_mixed_signal(length)
        score = score_case(backend, case)
        child_elapsed_ms = (time.perf_counter() - start) * 1000.0
        connection.send(worker_result("ok", score, child_elapsed_ms, None))
    except BaseException:
        connection.send(worker_result("failed", None, None, traceback.format_exc()))
    finally:
        connection.close()


def runtime_worker(backend: str, length: int, connection: Connection) -> None:
    case = make_mixed_signal(length)
    try:
        _ = score_case(backend, case)
        connection.send(worker_result("ready", None, None, None))
        while True:
            message = connection.recv()
            if message == "score":
                connection.send(score_case(backend, case))
            elif message == "memory":
                connection.send(current_memory_snapshot())
            elif message == "stop":
                return
            else:
                raise ValueError(f"unknown runtime worker message {message!r}")
    except BaseException:
        connection.send(worker_result("failed", None, None, traceback.format_exc()))
    finally:
        connection.close()


def multiprocessing_context() -> multiprocessing.context.SpawnContext:
    return multiprocessing.get_context("spawn")


def receive_worker_result(
    connection: Connection, process: BaseProcess, backend: str, length: int
) -> WorkerResult:
    try:
        result = connection.recv()
        assert isinstance(result, WorkerResult)
        return result
    except EOFError:
        process.join(timeout=1.0)
    if process.exitcode is not None:
        return WorkerResult(
            status="failed",
            score=None,
            child_elapsed_ms=None,
            peak_rss_mib=None,
            peak_swap_mib=None,
            error=f"{backend} n={length} child exited with code {process.exitcode}",
        )
    return WorkerResult(
        status="failed",
        score=None,
        child_elapsed_ms=None,
        peak_rss_mib=None,
        peak_swap_mib=None,
        error=f"{backend} n={length} child did not report readiness",
    )


def run_setup_child(backend: str, length: int) -> WorkerResult:
    context = multiprocessing_context()
    parent, child = context.Pipe(duplex=False)
    process = context.Process(target=setup_worker, args=(backend, length, child))
    process.start()
    child.close()
    assert process.pid is not None
    monitor = ProcessMemoryMonitor(process.pid)
    monitor.start()
    process.join()
    monitor_snapshot = monitor.stop()
    if parent.poll():
        result = parent.recv()
        assert isinstance(result, WorkerResult)
    else:
        result = WorkerResult(
            status="failed",
            score=None,
            child_elapsed_ms=None,
            peak_rss_mib=None,
            peak_swap_mib=None,
            error=f"{backend} n={length} setup child exited with code {process.exitcode}",
        )
    parent.close()
    return apply_monitor_snapshot(result, monitor_snapshot)


def start_runtime_child(
    backend: str, length: int
) -> tuple[BaseProcess, Connection, WorkerResult]:
    context = multiprocessing_context()
    parent, child = context.Pipe(duplex=True)
    process = context.Process(target=runtime_worker, args=(backend, length, child))
    process.start()
    child.close()
    result = receive_worker_result(parent, process, backend, length)
    if result.status != "ready":
        process.join(timeout=1.0)
        parent.close()
        raise AssertionError(
            result.error or f"{backend} n={length} runtime worker failed"
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
@pytest.mark.parametrize("backend", BENCHMARK_BACKENDS)
@pytest.mark.parametrize("length", BENCHMARK_LENGTHS)
def test_mixed_signal_load_memory_benchmark(
    benchmark: Any, backend: str, length: int
) -> None:
    probe = SetupProbe(backend, length)
    result = benchmark.pedantic(probe, rounds=1, iterations=1)
    benchmark.extra_info["metric"] = "load_memory"
    benchmark.extra_info["backend"] = backend
    benchmark.extra_info["length"] = length
    benchmark.extra_info["platform"] = platform.platform()
    benchmark.extra_info["peak_rss_mib"] = probe.peak_rss_mib
    benchmark.extra_info["peak_swap_mib"] = probe.peak_swap_mib
    benchmark.extra_info["swap_invalidated"] = (probe.peak_swap_mib or 0.0) > 0.0
    benchmark.extra_info["child_elapsed_ms"] = probe.child_elapsed_ms
    assert isinstance(result, float)


@pytest.mark.benchmark
@pytest.mark.parametrize("backend", BENCHMARK_BACKENDS)
@pytest.mark.parametrize("length", BENCHMARK_LENGTHS)
def test_mixed_signal_runtime_benchmark(
    benchmark: Any, backend: str, length: int
) -> None:
    process, connection, ready = start_runtime_child(backend, length)
    assert process.pid is not None
    monitor = ProcessMemoryMonitor(process.pid)

    def score_in_worker() -> float:
        connection.send("score")
        score = connection.recv()
        assert isinstance(score, float)
        return score

    try:
        monitor.start()
        result = benchmark.pedantic(score_in_worker, rounds=3, iterations=1)
        connection.send("memory")
        worker_memory = connection.recv()
    finally:
        monitor_snapshot = monitor.stop()
        stop_runtime_child(process, connection)

    if isinstance(worker_memory, MemorySnapshot):
        monitor_snapshot = monitor_snapshot.merged(worker_memory)

    benchmark.extra_info["metric"] = "runtime"
    benchmark.extra_info["backend"] = backend
    benchmark.extra_info["length"] = length
    benchmark.extra_info["platform"] = platform.platform()
    benchmark.extra_info["initial_peak_rss_mib"] = ready.peak_rss_mib
    benchmark.extra_info["peak_rss_mib"] = max_optional(
        ready.peak_rss_mib, monitor_snapshot.peak_rss_mib
    )
    benchmark.extra_info["peak_swap_mib"] = max_optional(
        ready.peak_swap_mib, monitor_snapshot.peak_swap_mib
    )
    benchmark.extra_info["swap_invalidated"] = (
        benchmark.extra_info["peak_swap_mib"] or 0.0
    ) > 0.0
    assert isinstance(result, float)
