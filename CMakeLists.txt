cmake_minimum_required(VERSION 3.21)

project(euroncap LANGUAGES CXX)

find_package(Python COMPONENTS Interpreter Development.Module REQUIRED)
find_package(pybind11 CONFIG REQUIRED)

set(ISO18571_NATIVE_SOURCES
    src/iso18571_native/_core.cpp
    src/iso18571_native/simd_scalar.cpp
)

string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" ISO18571_PROCESSOR)
if(ISO18571_PROCESSOR MATCHES "x86_64|amd64|x64")
    list(APPEND ISO18571_NATIVE_SOURCES
        src/iso18571_native/simd_sse2.cpp
        src/iso18571_native/simd_avx2.cpp
        src/iso18571_native/simd_avx2_fma.cpp
    )
else()
    message(FATAL_ERROR "iso18571_native currently requires an x86_64/AMD64 compiler target with SSE2, AVX2, and FMA translation-unit support")
endif()

pybind11_add_module(_core MODULE ${ISO18571_NATIVE_SOURCES})
target_compile_features(_core PRIVATE cxx_std_17)
target_include_directories(_core PRIVATE src/iso18571_native)

if(MSVC)
    target_compile_options(_core PRIVATE /O2 /fp:precise /W4 /permissive-)
else()
    target_compile_options(_core PRIVATE -O3 -Wall -Wextra -Wpedantic)
endif()

if(ISO18571_PROCESSOR MATCHES "x86_64|amd64|x64")
    if(MSVC)
        set_source_files_properties(
            src/iso18571_native/simd_avx2.cpp
            src/iso18571_native/simd_avx2_fma.cpp
            PROPERTIES COMPILE_OPTIONS "/arch:AVX2"
        )
    else()
        set_source_files_properties(src/iso18571_native/simd_sse2.cpp PROPERTIES COMPILE_OPTIONS "-msse2")
        set_source_files_properties(src/iso18571_native/simd_avx2.cpp PROPERTIES COMPILE_OPTIONS "-mavx2")
        set_source_files_properties(src/iso18571_native/simd_avx2_fma.cpp PROPERTIES COMPILE_OPTIONS "-mavx2;-mfma")
    endif()
endif()

if(NOT MSVC)
    set_source_files_properties(src/iso18571_native/_core.cpp PROPERTIES COMPILE_OPTIONS "-Wno-pedantic")
endif()

install(TARGETS _core
    LIBRARY DESTINATION iso18571_native
    RUNTIME DESTINATION iso18571_native
)
install(FILES iso18571/__init__.py iso18571/optional.py iso18571/rating.py DESTINATION iso18571)
install(FILES iso18571_native/__init__.py DESTINATION iso18571_native)
install(FILES main.py DESTINATION .)
