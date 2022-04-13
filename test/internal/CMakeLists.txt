add_ica_test(
    NAME BadRandTest
    CHECKS bad-rand
    FILES_PATHS test_bad_rand.cpp
)

add_ica_test(
    NAME ConstCastMemberTest
    CHECKS const-cast-member
    FILES_PATHS test_const_cast_member.cpp
)

add_ica_test(
    NAME ForRangeConstTest
    CHECKS for-range-const,const-param
    FILES_PATHS test_for_range_const.cpp test_const_param_silence.cpp
)

add_ica_test(
    NAME ImproperMoveTest
    CHECKS improper-move,const-param
    FILES_PATHS test_improper_move.cpp
)

add_ica_test(
    NAME InitFieldInBodyTest
    CHECKS init-members
    FILES_PATHS test_init_field_in_body.cpp
)

add_ica_test(
    NAME ReturnValueTest
    CHECKS return-value-type
    FILES_PATHS test_return_value.cpp
)

add_ica_test(
    NAME StaticKeywordTest
    CHECKS static-keyword
    FILES_PATHS test_static_keyword.cpp
)

add_ica_test(
    NAME TemporaryInCtorTest
    CHECKS temporary-in-ctor
    FILES_PATHS test_temporary_in_ctor.cpp
)
