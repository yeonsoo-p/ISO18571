"""ISO/TS 18571 scoring package."""

from importlib.metadata import PackageNotFoundError, version

from ._core import backend_info as _native_backend_info
from .rating import ISO18571, ScoreComponents

__all__ = ["ISO18571", "backend_info", "ScoreComponents"]


def backend_info() -> dict[str, str]:
    try:
        package_version = version("iso18571")
    except PackageNotFoundError:
        package_version = "0+unknown"
    native_info = _native_backend_info()
    return {
        "name": "iso18571",
        "implementation": str(native_info["implementation"]),
        "version": package_version,
        "optimization": str(native_info["optimization"]),
    }
