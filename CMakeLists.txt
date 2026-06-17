cmake_minimum_required(VERSION 3.21)

project(iso18571 LANGUAGES CXX)

include(CheckCXXCompilerFlag)

find_package(Python COMPONENTS Interpreter Development.Module REQUIRED)
find_package(pybind11 CONFIG REQUIRED)

set(ISO18571_SOURCES
    src/iso18571/_core.cpp
    src/iso18571/dispatch.cpp
    src/iso18571/scorer_v1.cpp
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
    iso18571_try_add_level("X86_64_V3" "src/iso18571/scorer_v3.cpp" "/arch:AVX2" "ISO18571_COMPILED_X86_64_V3")
    iso18571_try_add_level("X86_64_V4" "src/iso18571/scorer_v4.cpp" "/arch:AVX512" "ISO18571_COMPILED_X86_64_V4")
else()
    iso18571_try_add_level("X86_64_V2" "src/iso18571/scorer_v2.cpp" "-march=x86-64-v2" "ISO18571_COMPILED_X86_64_V2")
    iso18571_try_add_level("X86_64_V3" "src/iso18571/scorer_v3.cpp" "-march=x86-64-v3" "ISO18571_COMPILED_X86_64_V3")
    iso18571_try_add_level("X86_64_V4" "src/iso18571/scorer_v4.cpp" "-march=x86-64-v4" "ISO18571_COMPILED_X86_64_V4")
endif()

pybind11_add_module(_core MODULE ${ISO18571_SOURCES})
target_compile_features(_core PRIVATE cxx_std_17)
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
