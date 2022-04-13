function(check_min_version COMPILER COMPILER_MIN_VERSION)
    if(CMAKE_CXX_COMPILER_ID STREQUAL COMPILER)
        if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS COMPILER_MIN_VERSION)
            message(FATAL_ERROR
                    "Your CXX compiler is too old.\n"
                    "Current version: ${CMAKE_CXX_COMPILER_VERSION}\n"
                    "Minimum version: ${COMPILER_MIN_VERSION}\n"
                    "Please upgrade or select different compiler via -DCMAKE_CXX_COMPILER=...")
        endif()

        set(_COMPILER_SUPPORTED ON PARENT_SCOPE)
    endif()
endfunction(check_min_version)

function(check_compiler_supported)
    if(NOT _COMPILER_SUPPORTED)
        message(WARNING
                "Your CXX compiler (${CMAKE_CXX_COMPILER}) was not tested properly with this project.\n"
                "You can still try to build it, but if anything goes wrong, you might want to switch to GCC or Clang.")
    endif()
endfunction(check_compiler_supported)
