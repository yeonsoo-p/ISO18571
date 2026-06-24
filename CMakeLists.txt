cmake_minimum_required(VERSION 3.21)

project(iso18571 LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(CheckCXXSourceCompiles)
include(ISO18571CompilerOptions)
include(ISO18571Dispatch)
include(ISO18571Python)

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

iso18571_configure_engine_dispatch()

add_library(iso18571_engine STATIC
    src/dispatch.cpp
    src/validation.cpp
    ${ISO18571_ENGINE_OBJECTS}
)
target_include_directories(iso18571_engine PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_features(iso18571_engine PUBLIC cxx_std_23)
target_compile_definitions(iso18571_engine PRIVATE ${ISO18571_ENGINE_DEFINITIONS})
iso18571_configure_native_target(iso18571_engine)
iso18571_configure_x86_64_v1_target(iso18571_engine)

option(ISO18571_BUILD_PYTHON "Build the iso18571 Python extension module" ON)
if(ISO18571_BUILD_PYTHON)
    iso18571_add_python_module()
endif()
