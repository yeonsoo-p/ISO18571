from __future__ import annotations

import argparse
import json
import os
import platform
import shutil
import subprocess
import sys
import tarfile
import tempfile
import tomllib
import urllib.request
import zipfile
from dataclasses import dataclass
from pathlib import Path
from typing import Sequence, TypedDict


PROJECT_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_PYTHONS = ("3.12", "3.13", "3.14")
WINDOWS_ARCH = "win_amd64"
FORBIDDEN_ARCHIVE_PREFIXES = ("reference", "tests", "ref")
PRODUCTION_PACKAGE_FILES = frozenset(
    {
        "iso18571/__init__.py",
        "iso18571/rating.py",
        "iso18571/_core.pyi",
        "iso18571/py.typed",
    }
)
SDIST_REQUIRED_FILES = frozenset(
    {
        "CMakeLists.txt",
        "LICENSE",
        "pyproject.toml",
        "cmake/ISO18571CompilerOptions.cmake",
        "cmake/ISO18571Dispatch.cmake",
        "cmake/ISO18571Python.cmake",
        "python/iso18571/__init__.py",
        "python/iso18571/rating.py",
        "python/iso18571/_core.cpp",
        "python/iso18571/_core.pyi",
        "python/iso18571/py.typed",
        "src/engine.cpp",
    }
)
NATIVE_EXTENSION_SUFFIXES = (".so", ".pyd")


class VersionParts(TypedDict):
    major: int
    minor: int
    patch: int


class PythonDownload(TypedDict):
    key: str
    url: str
    version_parts: VersionParts


@dataclass(frozen=True)
class Toolchain:
    clang_cl: Path
    lld_link: Path
    llvm_rc: Path
    llvm_mt: Path
    objdump: Path
    uv: Path
    xwin: Path


@dataclass(frozen=True)
class WindowsPython:
    version: str
    key: str
    root: Path
    include_dir: Path
    import_lib: Path
    runtime_dll: Path

    @property
    def cp_tag(self) -> str:
        major, minor = self.version.split(".")
        return f"cp{major}{minor}"

    @property
    def wheel_tag(self) -> str:
        return f"{self.cp_tag}-{self.cp_tag}-{WINDOWS_ARCH}"

    @property
    def extension_suffix(self) -> str:
        return f".{self.cp_tag}-{WINDOWS_ARCH}.pyd"


def main(argv: Sequence[str] | None = None) -> int:
    args = parse_args(argv)
    host = platform.system().lower()
    platforms = ["linux", "windows"] if args.platform == "all" else [args.platform]
    pythons = normalize_python_versions(args.pythons)
    out_dir = args.out_dir.resolve()
    out_dir.mkdir(parents=True, exist_ok=True)

    for target in platforms:
        if target == "windows" and host == "linux":
            build_windows_cross(args, pythons, out_dir)
        elif target == "windows" and host == "windows":
            build_with_cibuildwheel(target, pythons, out_dir)
        elif target == "linux":
            build_with_cibuildwheel(target, pythons, out_dir)
        else:
            raise SystemExit(
                "Windows wheels from this host are unsupported. Use a Linux host "
                "for the clang-cl/xwin cross-build lane or a Windows host for "
                "the native MSVC/cibuildwheel lane."
            )

    validate_release_artifacts(out_dir, project_version())
    return 0


def parse_args(argv: Sequence[str] | None) -> argparse.Namespace:
    cache_default = Path.home() / ".cache" / "iso18571-wheel-build"
    xwin_default = Path(
        os.environ.get("XWIN_ROOT", Path.home() / ".cache" / "iso18571-xwin")
    )
    parser = argparse.ArgumentParser(
        description="Build iso18571 release wheels for host-specific target lanes."
    )
    parser.add_argument(
        "--platform",
        choices=("all", "linux", "windows"),
        default="all",
        help="Target platform lane to build.",
    )
    parser.add_argument(
        "--python",
        dest="pythons",
        nargs="+",
        default=list(DEFAULT_PYTHONS),
        help="CPython minor versions to build, for example: 3.12 3.13 3.14.",
    )
    parser.add_argument(
        "--out-dir",
        type=Path,
        default=Path("dist"),
        help="Directory for built wheels. Defaults to dist.",
    )
    parser.add_argument(
        "--cache-dir",
        type=Path,
        default=cache_default,
        help="Cache directory for cross-build tools and target Python artifacts.",
    )
    parser.add_argument(
        "--xwin-root",
        type=Path,
        default=xwin_default,
        help="xwin SDK/CRT root. Defaults to XWIN_ROOT or ~/.cache/iso18571-xwin.",
    )
    parser.add_argument(
        "--no-accept-ms-license",
        dest="accept_ms_license",
        action="store_false",
        help="Do not let xwin download Microsoft SDK/CRT files if the cache is missing.",
    )
    parser.set_defaults(accept_ms_license=True)
    return parser.parse_args(argv)


def normalize_python_versions(raw_versions: Sequence[str]) -> list[str]:
    versions: list[str] = []
    for raw in raw_versions:
        version = raw.removeprefix("cp")
        if len(version) == 3 and version.isdecimal():
            version = f"{version[0]}.{version[1:]}"
        if version not in DEFAULT_PYTHONS:
            allowed = ", ".join(DEFAULT_PYTHONS)
            raise SystemExit(f"Unsupported Python target {raw!r}; expected {allowed}.")
        if version not in versions:
            versions.append(version)
    return versions


def build_with_cibuildwheel(target: str, pythons: Sequence[str], out_dir: Path) -> None:
    env = os.environ.copy()
    env["CIBW_BUILD"] = " ".join(f"{cp_tag(v)}-*" for v in pythons)
    run(
        [
            sys.executable,
            "-m",
            "cibuildwheel",
            "--platform",
            target,
            "--output-dir",
            str(out_dir),
        ],
        env=env,
    )


def build_windows_cross(
    args: argparse.Namespace, pythons: Sequence[str], out_dir: Path
) -> None:
    tools = find_toolchain()
    cache_dir = args.cache_dir.expanduser().resolve()
    xwin_root = args.xwin_root.expanduser().resolve()
    ensure_xwin_root(tools.xwin, xwin_root, args.accept_ms_license)
    ar_wrapper = ensure_lld_lib_wrapper(cache_dir / "tools", tools.lld_link)

    include_dirs = [
        xwin_root / "crt" / "include",
        xwin_root / "sdk" / "include" / "ucrt",
        xwin_root / "sdk" / "include" / "um",
        xwin_root / "sdk" / "include" / "shared",
    ]
    lib_dirs = [
        xwin_root / "crt" / "lib" / "x86_64",
        xwin_root / "sdk" / "lib" / "ucrt" / "x86_64",
        xwin_root / "sdk" / "lib" / "um" / "x86_64",
    ]
    require_dirs([*include_dirs, *lib_dirs])

    for version in pythons:
        target_python = ensure_windows_python(
            tools.uv, version, cache_dir / "windows-python"
        )
        build_one_windows_wheel(
            tools=tools,
            ar_wrapper=ar_wrapper,
            include_dirs=include_dirs,
            lib_dirs=lib_dirs,
            target_python=target_python,
            out_dir=out_dir,
        )


def find_toolchain() -> Toolchain:
    return Toolchain(
        clang_cl=require_tool("clang-cl"),
        lld_link=require_tool("lld-link"),
        llvm_rc=require_tool("llvm-rc", extra_globs=("/usr/lib/llvm-*/bin/llvm-rc",)),
        llvm_mt=require_tool("llvm-mt", extra_globs=("/usr/lib/llvm-*/bin/llvm-mt",)),
        objdump=require_tool("objdump"),
        uv=require_tool("uv"),
        xwin=require_tool("xwin"),
    )


def require_tool(name: str, extra_globs: Sequence[str] = ()) -> Path:
    found = shutil.which(name)
    if found:
        return Path(found)
    for pattern in extra_globs:
        matches = sorted(Path("/").glob(pattern.removeprefix("/")), reverse=True)
        for match in matches:
            if match.is_file() and os.access(match, os.X_OK):
                return match
    raise SystemExit(f"Required tool not found on PATH: {name}")


def ensure_xwin_root(xwin: Path, xwin_root: Path, accept_license: bool) -> None:
    required = [
        xwin_root / "crt" / "include",
        xwin_root / "crt" / "lib" / "x86_64",
        xwin_root / "sdk" / "include" / "ucrt",
        xwin_root / "sdk" / "lib" / "ucrt" / "x86_64",
    ]
    if all(path.exists() for path in required):
        return
    if not accept_license:
        raise SystemExit(
            f"xwin SDK/CRT cache is missing at {xwin_root}. Re-run with "
            "the default license-accepting behavior, or provision XWIN_ROOT manually."
        )
    xwin_root.parent.mkdir(parents=True, exist_ok=True)
    run(
        [
            str(xwin),
            "--arch",
            "x86_64",
            "--accept-license",
            "splat",
            "--output",
            str(xwin_root),
        ]
    )


def ensure_lld_lib_wrapper(tools_dir: Path, lld_link: Path) -> Path:
    tools_dir.mkdir(parents=True, exist_ok=True)
    wrapper = tools_dir / "llvm-lib-wrapper.sh"
    content = f'#!/bin/sh\nexec "{lld_link}" /lib "$@"\n'
    if not wrapper.exists() or wrapper.read_text() != content:
        wrapper.write_text(content)
        wrapper.chmod(0o755)
    return wrapper


def ensure_windows_python(uv: Path, version: str, cache_dir: Path) -> WindowsPython:
    catalog = query_uv_windows_python(uv, version)
    key = catalog["key"]
    url = catalog["url"]
    archive = cache_dir / f"{key}.tar.gz"
    root = cache_dir / key
    if not has_python_artifacts(root, version):
        cache_dir.mkdir(parents=True, exist_ok=True)
        download(url, archive)
        if root.exists():
            shutil.rmtree(root)
        root.mkdir(parents=True)
        safe_extract_tar(archive, root)
    return locate_windows_python(root, key, version)


def query_uv_windows_python(uv: Path, version: str) -> PythonDownload:
    proc = subprocess.run(
        [
            str(uv),
            "python",
            "list",
            "--all-platforms",
            "--only-downloads",
            "--output-format",
            "json",
            version,
        ],
        check=True,
        stdout=subprocess.PIPE,
        text=True,
    )
    data: object = json.loads(proc.stdout)
    if not isinstance(data, list):
        raise SystemExit("uv returned an unexpected Python catalog payload.")
    major, minor = (int(part) for part in version.split("."))
    candidates: list[PythonDownload] = []
    for item in data:
        candidate = parse_windows_python_download(item)
        if (
            candidate is not None
            and candidate["version_parts"]["major"] == major
            and candidate["version_parts"]["minor"] == minor
        ):
            candidates.append(candidate)
    if not candidates:
        raise SystemExit(f"uv did not list a Windows x86_64 CPython for {version}.")
    candidates.sort(key=lambda item: item["version_parts"]["patch"], reverse=True)
    return candidates[0]


def parse_windows_python_download(item: object) -> PythonDownload | None:
    if not isinstance(item, dict):
        return None
    if item.get("implementation") != "cpython":
        return None
    if item.get("os") != "windows":
        return None
    if item.get("arch") != "x86_64":
        return None
    if item.get("libc") != "none":
        return None
    key = item.get("key")
    url = item.get("url")
    version_parts = item.get("version_parts")
    if not isinstance(key, str) or not isinstance(url, str):
        return None
    if not isinstance(version_parts, dict):
        return None
    major = version_parts.get("major")
    minor = version_parts.get("minor")
    patch = version_parts.get("patch")
    if not isinstance(major, int) or not isinstance(minor, int):
        return None
    if not isinstance(patch, int):
        return None
    return {
        "key": key,
        "url": url,
        "version_parts": {"major": major, "minor": minor, "patch": patch},
    }


def has_python_artifacts(root: Path, version: str) -> bool:
    if not root.exists():
        return False
    try:
        locate_windows_python(root, root.name, version)
    except SystemExit:
        return False
    return True


def locate_windows_python(root: Path, key: str, version: str) -> WindowsPython:
    tag = cp_tag(version)
    include = first_match(root, "Python.h").parent
    import_lib = first_match(root, f"python{tag[2:]}.lib")
    runtime_dll = first_match(root, f"python{tag[2:]}.dll")
    return WindowsPython(
        version=version,
        key=key,
        root=root,
        include_dir=include,
        import_lib=import_lib,
        runtime_dll=runtime_dll,
    )


def first_match(root: Path, name: str) -> Path:
    for path in root.rglob("*"):
        if path.is_file() and path.name.lower() == name.lower():
            return path
    raise SystemExit(f"Could not find {name} under {root}")


def download(url: str, dest: Path) -> None:
    if dest.exists():
        return
    print(f"Downloading {url}")
    tmp = dest.with_suffix(dest.suffix + ".tmp")
    request = urllib.request.Request(
        url,
        headers={"User-Agent": "iso18571-wheel-builder/1.0"},
    )
    with (
        urllib.request.urlopen(request, timeout=120) as response,
        tmp.open("wb") as out,
    ):
        shutil.copyfileobj(response, out)
    tmp.replace(dest)


def safe_extract_tar(archive: Path, dest: Path) -> None:
    dest_resolved = dest.resolve()
    with tarfile.open(archive) as tar:
        for member in tar.getmembers():
            target = (dest_resolved / member.name).resolve()
            if target != dest_resolved and dest_resolved not in target.parents:
                raise SystemExit(f"Unsafe path in archive {archive}: {member.name}")
        tar.extractall(dest)


def build_one_windows_wheel(
    *,
    tools: Toolchain,
    ar_wrapper: Path,
    include_dirs: Sequence[Path],
    lib_dirs: Sequence[Path],
    target_python: WindowsPython,
    out_dir: Path,
) -> None:
    tag = target_python.wheel_tag
    before = set(out_dir.glob(f"iso18571-*-{tag}.whl"))
    env = os.environ.copy()
    env["INCLUDE"] = os.pathsep.join(str(path) for path in include_dirs)
    env["LIB"] = os.pathsep.join(str(path) for path in lib_dirs)
    env["ISO18571_CROSS_WINDOWS_TAG"] = tag

    include_flags = " ".join(f"/imsvc{path}" for path in include_dirs)
    lib_flags = " ".join(f"/libpath:{path}" for path in lib_dirs)
    build_dir = PROJECT_ROOT / "build" / "cross-windows" / tag
    run(
        [
            str(tools.uv),
            "build",
            "--wheel",
            str(PROJECT_ROOT),
            "--out-dir",
            str(out_dir),
            "--python",
            target_python.version,
            f"-Cbuild-dir={build_dir}",
            "-Ccmake.build-type=Release",
            "-Ccmake.define.CMAKE_SYSTEM_NAME=Windows",
            "-Ccmake.define.CMAKE_SYSTEM_PROCESSOR=x86_64",
            f"-Ccmake.define.CMAKE_CXX_COMPILER={tools.clang_cl}",
            f"-Ccmake.define.CMAKE_LINKER={tools.lld_link}",
            f"-Ccmake.define.CMAKE_AR={ar_wrapper}",
            f"-Ccmake.define.CMAKE_RC_COMPILER={tools.llvm_rc}",
            f"-Ccmake.define.CMAKE_MT={tools.llvm_mt}",
            "-Ccmake.define.CMAKE_TRY_COMPILE_CONFIGURATION=Release",
            "-Ccmake.define.CMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL",
            f"-Ccmake.define.CMAKE_CXX_FLAGS_INIT={include_flags} /EHsc",
            f"-Ccmake.define.CMAKE_EXE_LINKER_FLAGS_INIT={lib_flags} /machine:x64",
            f"-Ccmake.define.CMAKE_SHARED_LINKER_FLAGS_INIT={lib_flags} /machine:x64",
            f"-Ccmake.define.CMAKE_MODULE_LINKER_FLAGS_INIT={lib_flags} /machine:x64",
            "-Ccmake.define.ISO18571_CROSS_COMPILE_PYTHON=ON",
            f"-Ccmake.define.Python_INCLUDE_DIR={target_python.include_dir}",
            f"-Ccmake.define.Python_LIBRARY={target_python.import_lib}",
            f"-Ccmake.define.Python_RUNTIME_LIBRARY={target_python.runtime_dll}",
            f"-Ccmake.define.Python_SOABI={target_python.cp_tag}-{WINDOWS_ARCH}",
            f"-Ccmake.define.ISO18571_EXTENSION_SUFFIX={target_python.extension_suffix}",
        ],
        env=env,
    )
    wheel = select_built_wheel(out_dir, tag, before)
    validate_windows_wheel(tools.objdump, wheel, target_python)


def select_built_wheel(out_dir: Path, tag: str, before: set[Path]) -> Path:
    candidates = set(out_dir.glob(f"iso18571-*-{tag}.whl")) - before
    if not candidates:
        candidates = set(out_dir.glob(f"iso18571-*-{tag}.whl"))
    if not candidates:
        raise SystemExit(f"No wheel was produced for {tag}.")
    return max(candidates, key=lambda path: path.stat().st_mtime)


def validate_windows_wheel(
    objdump: Path, wheel: Path, target_python: WindowsPython
) -> None:
    validate_release_archive(wheel)
    expected_pyd = f"iso18571/_core{target_python.extension_suffix}"
    with zipfile.ZipFile(wheel) as wheel_file:
        names = set(wheel_file.namelist())
        if expected_pyd not in names:
            raise SystemExit(f"{wheel} does not contain {expected_pyd}.")
        wrong_extensions = [
            name
            for name in names
            if name.startswith("iso18571/_core")
            and name.endswith((".so", ".dll", ".dylib"))
        ]
        if wrong_extensions:
            raise SystemExit(
                f"{wheel} contains non-Windows extension names: {wrong_extensions}"
            )
        with tempfile.TemporaryDirectory() as tmp:
            extracted = Path(tmp) / Path(expected_pyd).name
            extracted.write_bytes(wheel_file.read(expected_pyd))
            proc = subprocess.run(
                [str(objdump), "-p", str(extracted)],
                check=True,
                stdout=subprocess.PIPE,
                text=True,
            )
    imports = {
        line.split("DLL Name:", 1)[1].strip()
        for line in proc.stdout.splitlines()
        if "DLL Name:" in line
    }
    required = {
        target_python.runtime_dll.name,
        "MSVCP140.dll",
        "VCRUNTIME140.dll",
    }
    missing = required - imports
    if missing:
        raise SystemExit(f"{wheel} is missing expected DLL imports: {sorted(missing)}")
    print(f"Validated {wheel.name}: {', '.join(sorted(required))}")


def project_version() -> str:
    with (PROJECT_ROOT / "pyproject.toml").open("rb") as file:
        data = tomllib.load(file)
    project = data.get("project")
    if not isinstance(project, dict):
        raise SystemExit("pyproject.toml is missing [project] metadata.")
    version = project.get("version")
    if not isinstance(version, str):
        raise SystemExit("pyproject.toml is missing project.version.")
    return version


def archive_member_names(archive: Path) -> set[str]:
    if archive.suffix == ".whl":
        with zipfile.ZipFile(archive) as wheel_file:
            raw_names = wheel_file.namelist()
    elif archive.name.endswith(".tar.gz"):
        with tarfile.open(archive) as sdist_file:
            raw_names = sdist_file.getnames()
    else:
        raise SystemExit(f"Unsupported release artifact type: {archive}")
    return {
        normalized_archive_member(name)
        for name in raw_names
        if normalized_archive_member(name)
    }


def normalized_archive_member(name: str) -> str:
    parts = tuple(part for part in name.replace("\\", "/").split("/") if part)
    if not parts:
        return ""
    if len(parts) > 1 and parts[0].startswith("iso18571-"):
        parts = parts[1:]
    return "/".join(parts).rstrip("/")


def validate_no_forbidden_members(archive: Path, names: set[str]) -> None:
    forbidden = [
        name
        for name in sorted(names)
        for prefix in FORBIDDEN_ARCHIVE_PREFIXES
        if name == prefix or name.startswith(f"{prefix}/")
    ]
    if forbidden:
        raise SystemExit(
            f"{archive} contains non-production archive members: {forbidden[:10]}"
        )


def validate_wheel_archive(archive: Path, names: set[str]) -> None:
    missing = sorted(PRODUCTION_PACKAGE_FILES - names)
    if missing:
        raise SystemExit(f"{archive} is missing production package files: {missing}")
    native_extensions = sorted(
        name
        for name in names
        if name.startswith("iso18571/_core.")
        and name.endswith(NATIVE_EXTENSION_SUFFIXES)
    )
    if len(native_extensions) != 1:
        raise SystemExit(
            f"{archive} must contain exactly one native extension, found {native_extensions}"
        )


def validate_sdist_archive(archive: Path, names: set[str]) -> None:
    missing = sorted(SDIST_REQUIRED_FILES - names)
    if missing:
        raise SystemExit(f"{archive} is missing source distribution files: {missing}")
    native_extensions = sorted(
        name for name in names if name.endswith(NATIVE_EXTENSION_SUFFIXES)
    )
    if native_extensions:
        raise SystemExit(
            f"{archive} source distribution contains built extensions: {native_extensions}"
        )


def validate_release_archive(archive: Path) -> None:
    names = archive_member_names(archive)
    validate_no_forbidden_members(archive, names)
    if archive.suffix == ".whl":
        validate_wheel_archive(archive, names)
    elif archive.name.endswith(".tar.gz"):
        validate_sdist_archive(archive, names)
    else:
        raise SystemExit(f"Unsupported release artifact type: {archive}")


def validate_release_artifacts(out_dir: Path, version: str) -> None:
    forbidden_sdists = sorted(out_dir.glob(f"iso18571-{version}.tar.gz"))
    if forbidden_sdists:
        names = ", ".join(path.name for path in forbidden_sdists)
        raise SystemExit(
            "Release artifacts must be wheel-only; remove current-version "
            f"source distributions before publishing: {names}"
        )

    artifacts = sorted(out_dir.glob(f"iso18571-{version}-*.whl"))
    if not artifacts:
        raise SystemExit(f"No iso18571 {version} wheel artifacts found in {out_dir}.")
    for artifact in artifacts:
        validate_release_archive(artifact)
    print(f"Validated {len(artifacts)} iso18571 {version} wheel artifact(s).")


def cp_tag(version: str) -> str:
    major, minor = version.split(".")
    return f"cp{major}{minor}"


def require_dirs(paths: Sequence[Path]) -> None:
    missing = [path for path in paths if not path.is_dir()]
    if missing:
        formatted = "\n".join(str(path) for path in missing)
        raise SystemExit(f"Missing required xwin directories:\n{formatted}")


def run(
    cmd: Sequence[str],
    *,
    env: dict[str, str] | None = None,
    cwd: Path = PROJECT_ROOT,
) -> None:
    print("$ " + " ".join(cmd))
    subprocess.run(cmd, cwd=cwd, env=env, check=True)


if __name__ == "__main__":
    raise SystemExit(main())
