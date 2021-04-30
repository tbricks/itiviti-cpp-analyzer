SET(GCC_TOOLCHAIN
    "-isystem /opt/gcc-8.2.0/include/c++/8.2.0 -isystem /opt/gcc-8.2.0/include/c++/8.2.0/x86_64-unknown-linux -isystem /opt/gcc-8.2.0/include/c++/8.2.0/backward  -isystem /usr/local/include -isystem /opt/llvm-10/lib64/clang/10.0.1/include -isystem /usr/include")

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
        set(CONCAT_PATH "${CONCAT_PATH} ../../test/${path}")
    endforeach(path)
    add_test(
        NAME ${ARGS_NAME}
        COMMAND sh -c "/opt/llvm-10/bin/clang++ --std=c++17 --gcc-toolchain=/opt/gcc-8.2.0 -Xclang -load -Xclang $<TARGET_FILE:ica-plugin> -Xclang -add-plugin -Xclang ica-plugin -Xclang -plugin-arg-ica-plugin -Xclang checks=${ARGS_CHECKS} ${GCC_TOOLCHAIN} -Xclang -verify ${CONCAT_PATH} -c"
    )
endfunction(add_ica_test)

add_ica_test(
    NAME RemoveCStrVisitorTest
    CHECKS remove-c_str
    FILES_PATHS test_remove_c_str.cpp
)

add_ica_test(
    NAME CharInCTypePredTest
    CHECKS char-in-ctype-pred
    FILES_PATHS test_char_in_ctype_pred.cpp
)

add_ica_test(
    NAME EraseInLoopTest
    CHECKS erase-in-loop
    FILES_PATHS test_erase_in_loop.cpp
)

add_ica_test(
    NAME FindEmplaceTest
    CHECKS find-emplace
    FILES_PATHS test_find_emplace.cpp test_map.cpp test_map_insert.cpp
)

add_ica_test(
    NAME EmplaceDefaultValueTest
    CHECKS emplace-default-value
    FILES_PATHS test_emplace_default_value.cpp
)

add_ica_test(
    NAME RedundantNoexcept
    CHECKS redundant-noexcept
    FILES_PATHS test_noexcept.cpp
)

# cpp-course-checks
add_ica_test(
    NAME CCForRangeConstTest
    CHECKS for-range-const,const-param
    FILES_PATHS cpp-course/test_for_range_const.cpp
)

add_ica_test(
    NAME CCBadRandTest
    CHECKS bad-rand
    FILES_PATHS cpp-course/test_bad_rand.cpp
)

add_ica_test(
    NAME CCItreatorVisitorTest
    CHECKS correct-iterator
    FILES_PATHS cpp-course/test_correct_iterator.cpp
)

add_ica_test(
    NAME CCImproperMoveTest
    CHECKS improper-move,const-param
    FILES_PATHS cpp-course/test_improper_move.cpp
)

add_ica_test(
    NAME CCReturnValueTest
    CHECKS return-value-type
    FILES_PATHS cpp-course/test_return_value.cpp
)

add_ica_test(
    NAME CCConstCastMemberTest
    CHECKS const-cast-member
    FILES_PATHS cpp-course/test_const_cast_member.cpp
)

add_ica_test(
    NAME CCTemporaryInCtorTest
    CHECKS temporary-in-ctor
    FILES_PATHS cpp-course/test_temporary_in_ctor.cpp
)

add_ica_test(
    NAME CCStaticKeywordTest
    CHECKS static-keyword
    FILES_PATHS cpp-course/test_static_keyword.cpp
)

add_ica_test(
    NAME CCInitFieldInBodyTest
    CHECKS init-members
    FILES_PATHS cpp-course/test_init_field_in_body.cpp
)
