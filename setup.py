from pybind11.setup_helpers import Pybind11Extension, build_ext
from setuptools import setup
import sys


extra_compile_args = ["/O2"] if sys.platform == "win32" else ["-O3"]


ext_modules = [
    Pybind11Extension(
        "iso18571_native._core",
        ["src/iso18571_native/_core.cpp"],
        cxx_std=17,
        extra_compile_args=extra_compile_args,
    ),
]


setup(
    ext_modules=ext_modules,
    cmdclass={"build_ext": build_ext},
)
