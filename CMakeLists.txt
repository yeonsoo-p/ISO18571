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
    src/iso18571/validation.cpp
)
set(ISO18571_VARIANT_OBJECTS)
set(ISO18571_DEFINITIONS
    ISO18571_COMPILED_X86_64_V1=1
)

function(iso18571_add_variant_object target_name source suffix flag)
    add_library("${target_name}" OBJECT "${source}")
    target_compile_features("${target_name}" PRIVATE cxx_std_20)
    target_include_directories("${target_name}" PRIVATE src/iso18571)
    target_compile_definitions("${target_name}" PRIVATE "IMPL_SUFFIX=${suffix}")
    set_target_properties("${target_name}" PROPERTIES POSITION_INDEPENDENT_CODE ON)
    if(MSVC)
        target_compile_options("${target_name}" PRIVATE /O2 /fp:precise /W4 /permissive-)
    else()
        target_compile_options("${target_name}" PRIVATE -O3 -Wall -Wextra -Wpedantic)
    endif()
    if(NOT "${flag}" STREQUAL "")
        target_compile_options("${target_name}" PRIVATE "${flag}")
    endif()
    list(APPEND ISO18571_VARIANT_OBJECTS "$<TARGET_OBJECTS:${target_name}>")
    set(ISO18571_VARIANT_OBJECTS "${ISO18571_VARIANT_OBJECTS}" PARENT_SCOPE)
endfunction()

iso18571_add_variant_object("iso18571_engine_v1" "src/iso18571/engine.cpp" "_v1" "")

function(iso18571_add_dispatch_object target_name source definition)
    add_library("${target_name}" OBJECT "${source}")
    target_compile_features("${target_name}" PRIVATE cxx_std_20)
    target_include_directories("${target_name}" PRIVATE src/iso18571)
    target_compile_definitions("${target_name}" PRIVATE "${definition}" ${ISO18571_DEFINITIONS})
    set_target_properties("${target_name}" PROPERTIES POSITION_INDEPENDENT_CODE ON)
    if(MSVC)
        target_compile_options("${target_name}" PRIVATE /O2 /fp:precise /W4 /permissive-)
    else()
        target_compile_options("${target_name}" PRIVATE -O3 -Wall -Wextra -Wpedantic)
    endif()
    list(APPEND ISO18571_VARIANT_OBJECTS "$<TARGET_OBJECTS:${target_name}>")
    set(ISO18571_VARIANT_OBJECTS "${ISO18571_VARIANT_OBJECTS}" PARENT_SCOPE)
endfunction()

function(iso18571_try_add_level_source level target_name source suffix flag definition)
    string(MAKE_C_IDENTIFIER "ISO18571_HAS_${level}_${source}_${flag}" check_name)
    check_cxx_compiler_flag("${flag}" "${check_name}")
    if(${check_name})
        iso18571_add_variant_object("${target_name}" "${source}" "${suffix}" "${flag}")
        list(APPEND ISO18571_DEFINITIONS "${definition}=1")
    endif()
    set(ISO18571_VARIANT_OBJECTS "${ISO18571_VARIANT_OBJECTS}" PARENT_SCOPE)
    set(ISO18571_DEFINITIONS "${ISO18571_DEFINITIONS}" PARENT_SCOPE)
endfunction()

string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" ISO18571_PROCESSOR)
if(NOT ISO18571_PROCESSOR MATCHES "x86_64|amd64|x64")
    message(FATAL_ERROR "iso18571 currently requires an x86_64/AMD64 compiler target")
endif()

if(MSVC)
    iso18571_try_add_level_source(
        "ENGINE_X86_64_V3" "iso18571_engine_v3" "src/iso18571/engine.cpp" "_v3" "/arch:AVX2"
        "ISO18571_COMPILED_X86_64_V3")
    iso18571_try_add_level_source(
        "ENGINE_X86_64_V4" "iso18571_engine_v4" "src/iso18571/engine.cpp" "_v4" "/arch:AVX512"
        "ISO18571_COMPILED_X86_64_V4")
else()
    iso18571_try_add_level_source(
        "ENGINE_X86_64_V2" "iso18571_engine_v2" "src/iso18571/engine.cpp" "_v2" "-march=x86-64-v2"
        "ISO18571_COMPILED_X86_64_V2")
    iso18571_try_add_level_source(
        "ENGINE_X86_64_V3" "iso18571_engine_v3" "src/iso18571/engine.cpp" "_v3" "-march=x86-64-v3"
        "ISO18571_COMPILED_X86_64_V3")
    iso18571_try_add_level_source(
        "ENGINE_X86_64_V4" "iso18571_engine_v4" "src/iso18571/engine.cpp" "_v4" "-march=x86-64-v4"
        "ISO18571_COMPILED_X86_64_V4")
endif()

iso18571_add_dispatch_object("iso18571_engine_dispatch" "src/iso18571/engine.cpp" "ISO18571_ENGINE_DISPATCH=1")

pybind11_add_module(_core MODULE ${ISO18571_SOURCES} ${ISO18571_VARIANT_OBJECTS})
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
