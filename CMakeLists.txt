cmake_minimum_required(VERSION 3.21)

project(euroncap LANGUAGES CXX)

find_package(Python COMPONENTS Interpreter Development.Module REQUIRED)
find_package(pybind11 CONFIG REQUIRED)

set(ISO18571_NATIVE_SOURCES
    src/iso18571_native/_core.cpp
    src/iso18571_native/simd_scalar.cpp
)
set(ISO18571_NATIVE_DEFINITIONS ISO18571_COMPILED_SCALAR=1)

string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" ISO18571_PROCESSOR)
if(ISO18571_PROCESSOR MATCHES "x86_64|amd64|x64")
    list(APPEND ISO18571_NATIVE_SOURCES
        src/iso18571_native/simd_sse2.cpp
        src/iso18571_native/simd_avx2.cpp
        src/iso18571_native/simd_avx2_fma.cpp
    )
    list(APPEND ISO18571_NATIVE_DEFINITIONS
        ISO18571_COMPILED_SSE2=1
        ISO18571_COMPILED_AVX2=1
        ISO18571_COMPILED_AVX2_FMA=1
    )
endif()

pybind11_add_module(_core MODULE ${ISO18571_NATIVE_SOURCES})
target_compile_features(_core PRIVATE cxx_std_17)
target_include_directories(_core PRIVATE src/iso18571_native)
target_compile_definitions(_core PRIVATE ${ISO18571_NATIVE_DEFINITIONS})

if(MSVC)
    target_compile_options(_core PRIVATE /O2 /fp:precise)
else()
    target_compile_options(_core PRIVATE -O3)
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

install(TARGETS _core
    LIBRARY DESTINATION iso18571_native
    RUNTIME DESTINATION iso18571_native
)
install(FILES iso18571_native/__init__.py DESTINATION iso18571_native)
install(FILES rating.py main.py DESTINATION .)
