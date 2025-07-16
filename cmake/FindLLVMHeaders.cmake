set(LLVM_ROOT CACHE PATH "Path to LLVM installation")

# Based on https://llvm.org/docs/CMake.html#embedding-llvm-in-your-project
find_package(LLVM ${LLVMHeaders_FIND_VERSION} CONFIG)

if(LLVM_FOUND)
    message(STATUS "Found LLVMConfig.cmake in: ${LLVM_DIR}")
else()
    # If we're here, that means LLVM distribution doesn't provide LLVMConfig.cmake
    # (distribution is custom-built or intentionally stripped). Unlucky!
    # Since we only need headers to build ICA, try to simply find_path() them.
    set(_llvm_config_header "llvm/Config/llvm-config.h")
    find_path(LLVM_INCLUDE_DIR NAMES ${_llvm_config_header} PATH_SUFFIXES include REQUIRED)
    message(STATUS "Found ${_llvm_config_header} in: ${LLVM_INCLUDE_DIR}")

    # Inspired by https://gitlab.kitware.com/cmake/cmake/blob/master/Modules/FindZLIB.cmake
    file(
        STRINGS "${LLVM_INCLUDE_DIR}/${_llvm_config_header}" LLVM_CONFIG_H
        REGEX "^#define LLVM_VERSION_STRING \"[^\"]*\"$"
    )
    if(LLVM_CONFIG_H MATCHES "LLVM_VERSION_STRING \"([0-9\\.]+)")
        set(LLVM_PACKAGE_VERSION "${CMAKE_MATCH_1}")
    else()
        set(LLVM_PACKAGE_VERSION "")
    endif()

    set(LLVM_INCLUDE_DIRS ${LLVM_INCLUDE_DIR})
    set(LLVM_DEFINITIONS "")
endif()

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVM includes in: ${LLVM_INCLUDE_DIRS}")

add_library(LLVMHeaders IMPORTED INTERFACE)

target_include_directories(LLVMHeaders INTERFACE ${LLVM_INCLUDE_DIRS})
separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
target_compile_definitions(LLVMHeaders INTERFACE ${LLVM_DEFINITIONS_LIST})

target_compile_options(LLVMHeaders INTERFACE -fno-rtti -fno-exceptions)


include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LLVMHeaders
    REQUIRED_VARS LLVM_INCLUDE_DIRS
    VERSION_VAR LLVM_PACKAGE_VERSION
)
