cmake_minimum_required(VERSION 3.4.3)

set(ICA_PLUGIN_LLVM_CONFIG "llvm-config" CACHE STRING "LLVM config executable to use")

project(ICAPlugin)

function(check_compiler COMPILER version)
    if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "${COMPILER}")
        if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS ${version})
            message(FATAL_ERROR "${COMPILER} version must be at least ${version}!")
        endif()

        set(HAVE_COMPILER 1 PARENT_SCOPE)
    endif()
endfunction(check_compiler)

check_compiler("GNU" 7.3)
check_compiler("Clang" 6.0)
check_compiler("AppleClang" 7.0)

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    if(EXISTS "${GCC_TOOLCHAIN}")
        message(STATUS "gcc toolchain: ${GCC_TOOLCHAIN}")
        add_definitions("--gcc-toolchain=${GCC_TOOLCHAIN}")
    else()
        message(FATAL_ERROR "GCC_TOOLCHAIN = '${GCC_TOOLCHAIN}' doesn't exist. Needed gcc-toolchain when clang is used")
    endif()
endif()

if(NOT HAVE_COMPILER)
    message(WARNING "You are using an unsupported compiler! Compilation has only been tested with Clang and GCC.")
endif()

# Find LLVM Config binary for LLVM
# If you're on debian, go to http://llvm.org/apt/ and install the development
# release of LLVM (all packages).
find_program(LLVM_CONFIG_PATH "${ICA_PLUGIN_LLVM_CONFIG}")
if(NOT LLVM_CONFIG_PATH)
    message(FATAL_ERROR "llvm-config not found -- ${LLVM_CONFIG_PATH}: ${ICA_PLUGIN_LLVM_CONFIG}")
else()
    message(STATUS "Found LLVM_CONFIG_PATH: ${LLVM_CONFIG_PATH}")
endif()

# This function saves the output of the llvm-config command with the given
# switch to the variable named VARNAME.
#
# Example usage: llvm_config(LLVM_CXXFLAGS "--cxxflags")
function(llvm_config VARNAME switch)
    set(CONFIG_COMMAND "${LLVM_CONFIG_PATH}" "${switch}")

    execute_process(
        COMMAND ${CONFIG_COMMAND} ${LIB_TYPE}
        RESULT_VARIABLE HAD_ERROR
        OUTPUT_VARIABLE CONFIG_OUTPUT
    )

    if (HAD_ERROR)
        string(REPLACE ";" " " CONFIG_COMMAND_STR "${CONFIG_COMMAND}")
        message(STATUS "${CONFIG_COMMAND_STR}")
        message(FATAL_ERROR "llvm-config failed with status ${HAD_ERROR}")
    endif()

    # replace linebreaks with semicolon
    string(REGEX REPLACE
        "[ \t]*[\r\n]+[ \t]*" ";"
        CONFIG_OUTPUT ${CONFIG_OUTPUT})

    # make all includes system include to prevent the compiler to warn about issues in LLVM/Clang
    string(REGEX REPLACE "-I" "-isystem" CONFIG_OUTPUT "${CONFIG_OUTPUT}")

    # remove certain options clang doesn't like
    if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
        string(REGEX REPLACE "-Wl,--no-keep-files-mapped" "" CONFIG_OUTPUT "${CONFIG_OUTPUT}")
        string(REGEX REPLACE "-Wl,--no-map-whole-files" "" CONFIG_OUTPUT "${CONFIG_OUTPUT}")
        string(REGEX REPLACE "-fuse-ld=gold" "" CONFIG_OUTPUT "${CONFIG_OUTPUT}")
    endif()

    # make result available outside
    set(${VARNAME} ${CONFIG_OUTPUT} PARENT_SCOPE)

    # Optionally output the configured value
    message(STATUS "llvm_config(${VARNAME})=>${CONFIG_OUTPUT}")

    # cleanup
    unset(CONFIG_COMMAND)
endfunction(llvm_config)

llvm_config(LLVM_CXXFLAGS "--cxxflags")
llvm_config(LLVM_LIBDIR "--libdir")
llvm_config(LLVM_INCLUDE_DIR "--includedir")
llvm_config(LLVM_PACKAGE_VERSION "--version")

# always generate the compile commands
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_definitions(${LLVM_CXXFLAGS})
add_definitions(-std=c++1z)
add_definitions(-Wall)

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    add_definitions(-Wsuggest-override)
    #        add_definitions(-Wsuggest-final-types)
    add_definitions(-Wuseless-cast)
elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    # -Wno-maybe-uninitialized is a g++ option which is (currently) unknown to clang
    add_definitions(-Wno-unknown-warning-option)
    #add_definitions(-Wnounused-command-line-argument)
elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "AppleClang")
    set(CMAKE_SHARED_LIBRARY_CREATE_CXX_FLAGS "${CMAKE_SHARED_LIBRARY_CREATE_CXX_FLAGS} -undefined dynamic_lookup")
endif()
add_definitions(-Wold-style-cast)
add_definitions(-ftemplate-backtrace-limit=0)

find_package(Boost 1.68.0)
if (NOT Boost_FOUND)
    message(FATAL "Couldn't find boost packages")
else()
    message(STATUS "Boost found")
endif()

message(STATUS "Boost includes: ${Boost_INCLUDE_DIRS}")
include_directories(SYSTEM ${Boost_INCLUDE_DIRS})

include_directories(${LLVM_INCLUDE_DIR} ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/cpp-course-checks)
link_directories(${LLVM_LIBDIR})

set(ICA_PLUGIN_MIN_LLVM_VERSION 9.0)

# minimum clang version
if (${LLVM_PACKAGE_VERSION} VERSION_LESS ${ICA_PLUGIN_MIN_LLVM_VERSION})
    message(FATAL_ERROR "LLVM version ${ICA_PLUGIN_MIN_LLVM_VERSION} or higher required. Current version is: ${LLVM_PACKAGE_VERSION}.")
endif()

file(GLOB src ${CMAKE_SOURCE_DIR}/ICA*.cpp ${CMAKE_SOURCE_DIR}/cpp-course-checks/CC*.cpp)
add_library(ica-plugin SHARED ${src})


message(STATUS "Using CXX: ${CMAKE_CXX_COMPILER}")

enable_testing()
add_subdirectory(test)
