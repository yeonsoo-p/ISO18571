from pathlib import Path

from pybind11.setup_helpers import Pybind11Extension, build_ext
from setuptools import setup
import platform
import sys


def is_x86_64() -> bool:
    machine = platform.machine().lower()
    return machine in {"x86_64", "amd64", "x64"}


extra_compile_args = ["/O2", "/fp:precise"] if sys.platform == "win32" else ["-O3"]
sources = [
    "src/iso18571_native/_core.cpp",
    "src/iso18571_native/simd_scalar.cpp",
]
define_macros = [("ISO18571_COMPILED_SCALAR", "1")]

if is_x86_64():
    sources.extend(
        [
            "src/iso18571_native/simd_sse2.cpp",
            "src/iso18571_native/simd_avx2.cpp",
            "src/iso18571_native/simd_avx2_fma.cpp",
        ]
    )
    define_macros.extend(
        [
            ("ISO18571_COMPILED_SSE2", "1"),
            ("ISO18571_COMPILED_AVX2", "1"),
            ("ISO18571_COMPILED_AVX2_FMA", "1"),
        ]
    )


def simd_compile_args(source: str, compiler_type: str) -> list[str]:
    name = Path(source).name
    if compiler_type == "msvc":
        if name in {"simd_avx2.cpp", "simd_avx2_fma.cpp"}:
            return ["/arch:AVX2"]
        return []
    if name == "simd_sse2.cpp":
        return ["-msse2"]
    if name == "simd_avx2.cpp":
        return ["-mavx2"]
    if name == "simd_avx2_fma.cpp":
        return ["-mavx2", "-mfma"]
    return []


class IsoBuildExt(build_ext):
    def build_extensions(self):
        compiler_type = self.compiler.compiler_type
        original_compile = self.compiler._compile

        def compile_with_simd(obj, src, ext, cc_args, extra_postargs, pp_opts):
            postargs = list(extra_postargs or [])
            postargs.extend(simd_compile_args(src, compiler_type))
            return original_compile(obj, src, ext, cc_args, postargs, pp_opts)

        self.compiler._compile = compile_with_simd
        try:
            super().build_extensions()
        finally:
            self.compiler._compile = original_compile


ext_modules = [
    Pybind11Extension(
        "iso18571_native._core",
        sources,
        cxx_std=17,
        define_macros=define_macros,
        extra_compile_args=extra_compile_args,
    ),
]


setup(
    ext_modules=ext_modules,
    cmdclass={"build_ext": IsoBuildExt},
)
