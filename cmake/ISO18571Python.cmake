include_guard(GLOBAL)

option(
    ISO18571_CROSS_COMPILE_PYTHON
    "Use target Python development artifacts without a target interpreter"
    OFF
)

function(iso18571_add_python_module)
    if(ISO18571_CROSS_COMPILE_PYTHON)
        find_package(Python COMPONENTS Development.Module REQUIRED)
    else()
        find_package(Python COMPONENTS Interpreter Development.Module REQUIRED)
    endif()
    find_package(pybind11 CONFIG REQUIRED)

    pybind11_add_module(_core MODULE
        "${CMAKE_CURRENT_SOURCE_DIR}/python/iso18571/_core.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/float16.cpp"
    )
    target_link_libraries(_core PRIVATE iso18571_engine)
    iso18571_configure_native_target(_core)

    set(ISO18571_EXTENSION_SUFFIX "" CACHE STRING "Override native extension suffix for cross-built wheels")
    if(ISO18571_EXTENSION_SUFFIX)
        set_target_properties(_core PROPERTIES SUFFIX "${ISO18571_EXTENSION_SUFFIX}")
    endif()

    if(NOT MSVC)
        set_source_files_properties(
            "${CMAKE_CURRENT_SOURCE_DIR}/python/iso18571/_core.cpp"
            PROPERTIES COMPILE_OPTIONS "-Wno-pedantic"
        )
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
endfunction()
