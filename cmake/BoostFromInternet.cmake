set(BOOST_FROM_INTERNET OFF CACHE BOOL "Download Boost headers from Internet")

string(REPLACE "." "_" BOOST_DIRNAME ${BOOST_VERSION})
set(BOOST_DIRNAME boost_${BOOST_DIRNAME})

set(BOOST_URL "https://boostorg.jfrog.io/artifactory/main/release/${BOOST_VERSION}/source/${BOOST_DIRNAME}.tar.gz")

if (BOOST_FROM_INTERNET)
    set(Boost_INCLUDE_DIR ${CMAKE_CURRENT_BINARY_DIR}/${BOOST_DIRNAME})

    if(NOT EXISTS ${Boost_INCLUDE_DIR})
        message(STATUS "BOOST_FROM_INTERNET is set, downloading Boost headers v${BOOST_VERSION} from Boost artifactory")

        find_program(CURL curl REQUIRED)
        find_program(TAR NAMES tar gtar REQUIRED)

        execute_process(
            COMMAND ${CURL} -L ${BOOST_URL}
            COMMAND ${TAR} xzf - ${BOOST_DIRNAME}/boost
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
            RESULT_VARIABLE HAD_ERROR
        )

        if (HAD_ERROR)
            message(FATAL_ERROR "Boost fetch failed with status ${HAD_ERROR}")
        endif()
    endif()
endif()
