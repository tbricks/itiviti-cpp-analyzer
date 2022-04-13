set(LLVM_ROOT CACHE PATH "Path to LLVM installation")

# Based on https://llvm.org/docs/CMake.html#embedding-llvm-in-your-project
find_package(LLVM ${LLVMHeaders_FIND_VERSION} CONFIG REQUIRED)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

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
