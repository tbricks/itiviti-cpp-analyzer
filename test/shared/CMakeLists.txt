add_ica_test(
    NAME CharInCTypePredTest
    CHECKS char-in-ctype-pred
    FILES_PATHS test_char_in_ctype_pred.cpp
)

add_ica_test(
    NAME EmplaceDefaultValueTest
    CHECKS emplace-default-value
    FILES_PATHS test_emplace_default_value.cpp
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
    NAME InlineInMethodDeclsTest
    CHECKS inline-methods-in-class
    FILES_PATHS test_method_decl_inline.cpp test_inline_demo.cpp
)

add_ica_test(
    NAME ReleaseGuardTest
    CHECKS release-lock
    FILES_PATHS test_lock_guard_release.cpp
)

add_ica_test(
    NAME MoveStringStreamTest
    CHECKS move-string-stream
    FILES_PATHS test_move_string_stream.cpp
)

add_ica_test(
    NAME RedundantNoexcept
    CHECKS redundant-noexcept
    FILES_PATHS test_noexcept.cpp
)

add_ica_test(
    NAME RemoveCStrVisitorTest
    CHECKS remove-c_str
    FILES_PATHS test_remove_c_str.cpp
)

add_ica_test(
    NAME TryEmplaceTest
    CHECKS try_emplace-instead-emplace
    FILES_PATHS test_try_emplace.cpp
)
