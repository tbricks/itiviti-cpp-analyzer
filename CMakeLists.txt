cmake_minimum_required(VERSION 3.12)

project(ICA LANGUAGES C CXX)

set(ICA_VERSION 0.0.1)
set(BOOST_VERSION 1.77.0)
set(LLVM_VERSION 10.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Always export compile_commands.json
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Project local modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Minimum compiler requirements

include(CheckCompilerSupported)

check_min_version("GNU" 7.3)
check_min_version("Clang" 6.0)
check_min_version("AppleClang" 7.0)
check_compiler_supported()

#
# Target itself and its compilation flags
#

file(GLOB_RECURSE ica_sources CONFIGURE_DEPENDS src/*.cpp)

add_library(ICAPlugin SHARED ${ica_sources})
add_library(ICA::ICAPlugin ALIAS ICAPlugin)

set_target_properties(ICAPlugin PROPERTIES OUTPUT_NAME "ica-plugin")

target_include_directories(ICAPlugin PRIVATE include)

# GCC toolchain. Can also be provided in CMAKE_CXX_FLAGS
set(GCC_TOOLCHAIN "" CACHE PATH "Ask Clang to use GCC toolchain from specified directory")

if (EXISTS ${GCC_TOOLCHAIN})
    set(TOOLCHAIN_ARG "--gcc-toolchain=${GCC_TOOLCHAIN}")
    if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        message(STATUS "Using GCC toolchain: ${GCC_TOOLCHAIN}")
        target_compile_options(ICAPlugin PRIVATE "${TOOLCHAIN_ARG}")
        target_link_options(ICAPlugin PRIVATE "${TOOLCHAIN_ARG}")
    endif()
endif()

# Warnings
target_compile_options(ICAPlugin PRIVATE
    -Wall
    -Wold-style-cast
#   $<$<CXX_COMPILER_ID:GNU>:-Wsuggest-final-types>
    $<$<CXX_COMPILER_ID:GNU>:-Wsuggest-override>
    $<$<CXX_COMPILER_ID:GNU>:-Wuseless-cast>
    $<$<CXX_COMPILER_ID:Clang>:-Wno-unknown-warning-option>
#   $<$<CXX_COMPILER_ID:Clang>:-Wno-unused-command-line-argument>
)

# Feature flags
target_compile_options(ICAPlugin PRIVATE -ftemplate-backtrace-limit=0)

# Linker flags
if(CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
    target_link_options(ICAPlugin PRIVATE -undefined dynamic_lookup)
endif()

# Interface

add_library(ICAChecks INTERFACE)
add_dependencies(ICAChecks ICAPlugin)

target_compile_options(ICAChecks INTERFACE 
    "SHELL:-Xclang -load -Xclang $<TARGET_FILE:ICA::ICAPlugin>"
    "SHELL:-Xclang -add-plugin -Xclang ica-plugin"
)

function(add_ica_options)
    foreach(opt IN LISTS ARGN)
        add_compile_options("SHELL:-Xclang -plugin-arg-ica-plugin -Xclang ${opt}")
    endforeach()
endfunction()

function(target_ica_options TARGET VISIBILITY)
    foreach(opt IN LISTS ARGN)
        target_compile_options(${TARGET} ${VISIBILITY} "SHELL:-Xclang -plugin-arg-ica-plugin -Xclang ${opt}")
    endforeach()
endfunction()

function(add_ica_checks)
    list(JOIN ARGN "," ICA_CHECKS)
    link_libraries(ICAChecks)
    add_ica_options("checks=${ICA_CHECKS}")
endfunction()

function(target_ica_checks TARGET VISIBILITY)
    list(JOIN ARGN "," ICA_CHECKS)
    target_link_libraries(${TARGET} ${VISIBILITY} ICAChecks)
    target_ica_options(${TARGET} ${VISIBILITY} "checks=${ICA_CHECKS}")
endfunction()

function(ica_no_url)
    add_ica_options("no-url")
endfunction()

function(target_ica_no_url TARGET VISIBILITY)
    target_ica_options(${TARGET} ${VISIBILITY} "no-url")
endfunction()

#
# Third-party
#

include(BoostFromInternet)

find_package(Boost ${BOOST_VERSION} REQUIRED)
find_package(LLVMHeaders ${LLVM_VERSION} REQUIRED)

target_link_libraries(ICAPlugin PRIVATE LLVMHeaders Boost::headers)

#
# Packaging
#

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/ICAConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/ICAConfig.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/ICA"
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/ICAConfigVersion.cmake"
    VERSION "${ICA_VERSION}"
    COMPATIBILITY SameMajorVersion  
)

install(TARGETS ICAPlugin ICAChecks EXPORT ICATargets LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(EXPORT ICATargets NAMESPACE ICA:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ICA)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/ICAConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/ICAConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ICA
)

#
# Tests
#

enable_testing()
add_subdirectory(test)
