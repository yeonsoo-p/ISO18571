include_guard(GLOBAL)

include(CheckCXXCompilerFlag)

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

function(iso18571_add_engine_dispatch target_name)
    add_library("${target_name}" OBJECT "${CMAKE_CURRENT_SOURCE_DIR}/src/engine.cpp")
    target_include_directories("${target_name}" PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_compile_definitions(
        "${target_name}"
        PRIVATE ISO18571_ENGINE_DISPATCH=1 ${ISO18571_ENGINE_DEFINITIONS}
    )
    iso18571_configure_native_target("${target_name}")

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

function(iso18571_configure_engine_dispatch)
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

    iso18571_add_engine_dispatch("iso18571_engine_dispatch_obj")

    set(ISO18571_ENGINE_OBJECTS "${ISO18571_ENGINE_OBJECTS}" PARENT_SCOPE)
    set(ISO18571_ENGINE_DEFINITIONS "${ISO18571_ENGINE_DEFINITIONS}" PARENT_SCOPE)
endfunction()
