# If a separate compiler for testing is not set, use default
set(TARGET_COMPILER "${CMAKE_CXX_COMPILER}" CACHE STRING "Path to the Clang compiler used for testing")

function(add_ica_test)
    cmake_parse_arguments(
        ARGS
        ""
        "NAME;CHECKS"
        "FILES_PATHS"
        ${ARGN}
    )
    set(CONCAT_PATH "")
    foreach(path ${ARGS_FILES_PATHS})
        set(CONCAT_PATH "${CONCAT_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/${path}")
    endforeach(path)
    add_test(
        NAME ${ARGS_NAME}
        COMMAND sh -c "${TARGET_COMPILER} --std=c++17 ${TOOLCHAIN_ARG} -Xclang -load -Xclang $<TARGET_FILE:ICAPlugin> -Xclang -add-plugin -Xclang ica-plugin -Xclang -plugin-arg-ica-plugin -Xclang checks=${ARGS_CHECKS} -Xclang -verify ${CONCAT_PATH} -c"
    )
endfunction(add_ica_test)

add_subdirectory("shared")
add_subdirectory("internal")
