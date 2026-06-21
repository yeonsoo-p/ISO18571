include_guard(GLOBAL)

function(iso18571_configure_native_target target_name)
    target_compile_features("${target_name}" PRIVATE cxx_std_20)
    set_target_properties("${target_name}" PROPERTIES POSITION_INDEPENDENT_CODE ON)

    if(MSVC)
        target_compile_options("${target_name}" PRIVATE /O2 /fp:precise /W4 /permissive-)
    else()
        target_compile_options("${target_name}" PRIVATE -O3 -Wall -Wextra -Wpedantic)
    endif()
endfunction()
