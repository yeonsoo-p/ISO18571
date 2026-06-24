include_guard(GLOBAL)

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
