cmake_minimum_required(VERSION 3.21)

project(iso18571 LANGUAGES CXX)

include(CheckCXXCompilerFlag)
include(CheckCXXSourceCompiles)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

check_cxx_source_compiles([=[
    #include <array>
    #include <ranges>

    constexpr bool has_stride_view() {
        constexpr std::array<int, 6> values = {0, 1, 2, 3, 4, 5};
        auto view = values | std::views::stride(2);
        auto it = view.begin();
        return *it == 0 && *++it == 2 && *++it == 4;
    }

    static_assert(has_stride_view());

    int main() {
        return 0;
    }
]=] ISO18571_HAS_STD_VIEWS_STRIDE)
if(NOT ISO18571_HAS_STD_VIEWS_STRIDE)
    message(
        FATAL_ERROR
        "iso18571 requires a C++23 compiler and standard library with std::views::stride"
    )
endif()

function(iso18571_configure_native_target target_name)
    target_compile_features("${target_name}" PRIVATE cxx_std_23)
    set_target_properties("${target_name}" PROPERTIES POSITION_INDEPENDENT_CODE ON)

    if(MSVC)
        target_compile_options("${target_name}" PRIVATE /O2 /fp:precise /W4 /permissive-)
    else()
        target_compile_options("${target_name}" PRIVATE -O3 -Wall -Wextra -Wpedantic)
    endif()
endfunction()

function(iso18571_configure_x86_64_v1_target target_name)
    if(MSVC)
        return()
    endif()

    # GCC does not accept -march=x86-64-v1; the x86-64 baseline is the v1-equivalent target.
    target_compile_options("${target_name}" PRIVATE -march=x86-64)
endfunction()

function(iso18571_add_engine_variant target_name suffix flag)
    add_library("${target_name}" OBJECT "${CMAKE_CURRENT_SOURCE_DIR}/src/engine.cpp")
    target_include_directories("${target_name}" PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_compile_definitions("${target_name}" PRIVATE "IMPL_SUFFIX=${suffix}")
    iso18571_configure_native_target("${target_name}")

    if(NOT "${flag}" STREQUAL "")
        target_compile_options("${target_name}" PRIVATE "${flag}")
    endif()

    list(APPEND ISO18571_ENGINE_OBJECTS "$<TARGET_OBJECTS:${target_name}>")
    set(ISO18571_ENGINE_OBJECTS "${ISO18571_ENGINE_OBJECTS}" PARENT_SCOPE)
endfunction()

function(iso18571_try_add_engine_level level target_name suffix flag definition)
    string(MAKE_C_IDENTIFIER "ISO18571_HAS_${level}_${flag}" check_name)
    check_cxx_compiler_flag("${flag}" "${check_name}")
    if(${check_name})
        iso18571_add_engine_variant("${target_name}" "${suffix}" "${flag}")
        list(APPEND ISO18571_ENGINE_DEFINITIONS "${definition}=1")
    endif()
    set(ISO18571_ENGINE_OBJECTS "${ISO18571_ENGINE_OBJECTS}" PARENT_SCOPE)
    set(ISO18571_ENGINE_DEFINITIONS "${ISO18571_ENGINE_DEFINITIONS}" PARENT_SCOPE)
endfunction()

string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" ISO18571_PROCESSOR)
if(NOT ISO18571_PROCESSOR MATCHES "x86_64|amd64|x64")
    message(FATAL_ERROR "iso18571 currently requires an x86_64/AMD64 compiler target")
endif()

set(ISO18571_ENGINE_OBJECTS)
set(ISO18571_ENGINE_DEFINITIONS ISO18571_COMPILED_X86_64_V1=1)

iso18571_add_engine_variant("iso18571_engine_v1_obj" "_v1" "")

if(MSVC)
    iso18571_try_add_engine_level(
        "ENGINE_X86_64_V3" "iso18571_engine_v3_obj" "_v3" "/arch:AVX2"
        "ISO18571_COMPILED_X86_64_V3")
    iso18571_try_add_engine_level(
        "ENGINE_X86_64_V4" "iso18571_engine_v4_obj" "_v4" "/arch:AVX512"
        "ISO18571_COMPILED_X86_64_V4")
else()
    iso18571_try_add_engine_level(
        "ENGINE_X86_64_V2" "iso18571_engine_v2_obj" "_v2" "-march=x86-64-v2"
        "ISO18571_COMPILED_X86_64_V2")
    iso18571_try_add_engine_level(
        "ENGINE_X86_64_V3" "iso18571_engine_v3_obj" "_v3" "-march=x86-64-v3"
        "ISO18571_COMPILED_X86_64_V3")
    iso18571_try_add_engine_level(
        "ENGINE_X86_64_V4" "iso18571_engine_v4_obj" "_v4" "-march=x86-64-v4"
        "ISO18571_COMPILED_X86_64_V4")
endif()

add_library(iso18571_engine STATIC
    src/dispatch.cpp
    ${ISO18571_ENGINE_OBJECTS}
)
target_include_directories(iso18571_engine PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_features(iso18571_engine PUBLIC cxx_std_23)
target_compile_definitions(iso18571_engine PRIVATE ${ISO18571_ENGINE_DEFINITIONS})
iso18571_configure_native_target(iso18571_engine)
iso18571_configure_x86_64_v1_target(iso18571_engine)

option(ISO18571_BUILD_PYTHON "Build the iso18571 Python extension module" ON)
option(
    ISO18571_CROSS_COMPILE_PYTHON
    "Use target Python development artifacts without a target interpreter"
    OFF
)
if(ISO18571_BUILD_PYTHON)
    if(ISO18571_CROSS_COMPILE_PYTHON)
        find_package(Python COMPONENTS Development.Module REQUIRED)
    else()
        find_package(Python COMPONENTS Interpreter Development.Module REQUIRED)
    endif()
    find_package(pybind11 CONFIG REQUIRED)

    pybind11_add_module(_core MODULE "${CMAKE_CURRENT_SOURCE_DIR}/python/iso18571/_core.cpp")
    target_link_libraries(_core PRIVATE iso18571_engine)
    iso18571_configure_native_target(_core)
    iso18571_configure_x86_64_v1_target(_core)

    set(ISO18571_EXTENSION_SUFFIX "" CACHE STRING "Override native extension suffix for cross-built wheels")
    if(ISO18571_EXTENSION_SUFFIX)
        set_target_properties(_core PROPERTIES SUFFIX "${ISO18571_EXTENSION_SUFFIX}")
    endif()

    install(TARGETS _core
        LIBRARY DESTINATION iso18571
        RUNTIME DESTINATION iso18571
    )
    install(FILES
        python/iso18571/__init__.py
        python/iso18571/rating.py
        python/iso18571/_core.pyi
        python/iso18571/py.typed
        DESTINATION iso18571
    )
endif()
