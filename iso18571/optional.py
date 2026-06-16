"""Optional dependency loading helpers."""

from __future__ import annotations

from importlib import import_module
from types import ModuleType
from typing import Any


def optional_module(module_name: str, package_name: str, backend_name: str) -> ModuleType:
    try:
        return import_module(module_name)
    except ImportError as exc:
        raise ImportError(f"The '{package_name}' package is required for dtw_backend='{backend_name}'") from exc


def optional_attr(module_name: str, attr_name: str, package_name: str, backend_name: str) -> Any:
    module = optional_module(module_name, package_name, backend_name)
    return getattr(module, attr_name)
