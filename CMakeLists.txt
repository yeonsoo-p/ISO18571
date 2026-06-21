cmake_minimum_required(VERSION 3.21)

project(iso18571 LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(CheckCXXSourceCompiles)
include(ISO18571CompilerOptions)
include(ISO18571Dispatch)
include(ISO18571Python)

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

iso18571_configure_engine_dispatch()

add_library(iso18571_engine STATIC
    src/dispatch.cpp
    src/validation.cpp
    ${ISO18571_ENGINE_OBJECTS}
)
target_include_directories(iso18571_engine PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_features(iso18571_engine PUBLIC cxx_std_20)
target_compile_definitions(iso18571_engine PRIVATE ${ISO18571_ENGINE_DEFINITIONS})
iso18571_configure_native_target(iso18571_engine)

option(ISO18571_BUILD_PYTHON "Build the iso18571 Python extension module" ON)
if(ISO18571_BUILD_PYTHON)
    iso18571_add_python_module()
endif()
