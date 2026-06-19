cmake_minimum_required(VERSION 3.21)

project(iso18571 LANGUAGES CXX)

include(CheckCXXCompilerFlag)
include(CheckCXXSourceCompiles)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

check_cxx_source_compiles([=[
    #include <span>
    int main() {
        double values[2] = {1.0, 2.0};
        std::span<double> view(values);
        return view.size() == 2 ? 0 : 1;
    }
]=] ISO18571_HAS_STD_SPAN)
if(NOT ISO18571_HAS_STD_SPAN)
    message(FATAL_ERROR "iso18571 requires a C++20 compiler and standard library with std::span")
endif()

option(ISO18571_CROSS_COMPILE_PYTHON "Use target Python development artifacts without a target interpreter" OFF)
if(ISO18571_CROSS_COMPILE_PYTHON)
    find_package(Python COMPONENTS Development.Module REQUIRED)
else()
    find_package(Python COMPONENTS Interpreter Development.Module REQUIRED)
endif()
find_package(pybind11 CONFIG REQUIRED)

set(ISO18571_SOURCES
    src/iso18571/_core.cpp
    src/iso18571/dispatch.cpp
    src/iso18571/engine_v1.cpp
    src/iso18571/validation.cpp
)
set(ISO18571_DEFINITIONS ISO18571_COMPILED_X86_64_V1=1)

function(iso18571_try_add_level level source flag definition)
    if("${flag}" STREQUAL "")
        list(APPEND ISO18571_SOURCES "${source}")
        list(APPEND ISO18571_DEFINITIONS "${definition}=1")
        set(ISO18571_SOURCES "${ISO18571_SOURCES}" PARENT_SCOPE)
        set(ISO18571_DEFINITIONS "${ISO18571_DEFINITIONS}" PARENT_SCOPE)
        return()
    endif()

    string(MAKE_C_IDENTIFIER "ISO18571_HAS_${level}_${flag}" check_name)
    check_cxx_compiler_flag("${flag}" "${check_name}")
    if(${check_name})
        list(APPEND ISO18571_SOURCES "${source}")
        list(APPEND ISO18571_DEFINITIONS "${definition}=1")
        set_source_files_properties("${source}" PROPERTIES COMPILE_OPTIONS "${flag}")
    endif()
    set(ISO18571_SOURCES "${ISO18571_SOURCES}" PARENT_SCOPE)
    set(ISO18571_DEFINITIONS "${ISO18571_DEFINITIONS}" PARENT_SCOPE)
endfunction()

string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" ISO18571_PROCESSOR)
if(NOT ISO18571_PROCESSOR MATCHES "x86_64|amd64|x64")
    message(FATAL_ERROR "iso18571 currently requires an x86_64/AMD64 compiler target")
endif()

if(MSVC)
    iso18571_try_add_level("X86_64_V3" "src/iso18571/engine_v3.cpp" "/arch:AVX2" "ISO18571_COMPILED_X86_64_V3")
    iso18571_try_add_level("X86_64_V4" "src/iso18571/engine_v4.cpp" "/arch:AVX512" "ISO18571_COMPILED_X86_64_V4")
else()
    iso18571_try_add_level("X86_64_V2" "src/iso18571/engine_v2.cpp" "-march=x86-64-v2" "ISO18571_COMPILED_X86_64_V2")
    iso18571_try_add_level("X86_64_V3" "src/iso18571/engine_v3.cpp" "-march=x86-64-v3" "ISO18571_COMPILED_X86_64_V3")
    iso18571_try_add_level("X86_64_V4" "src/iso18571/engine_v4.cpp" "-march=x86-64-v4" "ISO18571_COMPILED_X86_64_V4")
endif()

pybind11_add_module(_core MODULE ${ISO18571_SOURCES})
set(ISO18571_EXTENSION_SUFFIX "" CACHE STRING "Override native extension suffix for cross-built wheels")
if(ISO18571_EXTENSION_SUFFIX)
    set_target_properties(_core PROPERTIES SUFFIX "${ISO18571_EXTENSION_SUFFIX}")
endif()
target_compile_features(_core PRIVATE cxx_std_20)
target_include_directories(_core PRIVATE src/iso18571)
target_compile_definitions(_core PRIVATE ${ISO18571_DEFINITIONS})

if(MSVC)
    target_compile_options(_core PRIVATE /O2 /fp:precise /W4 /permissive-)
else()
    target_compile_options(_core PRIVATE -O3 -Wall -Wextra -Wpedantic)
    set_source_files_properties(src/iso18571/_core.cpp PROPERTIES COMPILE_OPTIONS "-Wno-pedantic")
endif()

install(TARGETS _core
    LIBRARY DESTINATION iso18571
    RUNTIME DESTINATION iso18571
)
install(FILES iso18571/__init__.py iso18571/rating.py iso18571/_core.pyi iso18571/py.typed DESTINATION iso18571)
