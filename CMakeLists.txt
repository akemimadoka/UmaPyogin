cmake_minimum_required(VERSION 3.20)

project(UmaPyogin CXX)

if(CONAN_EXPORTED)
    include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
    conan_basic_setup(TARGETS)
else()
    if(NOT EXISTS "${CMAKE_BINARY_DIR}/conan.cmake")
        message(STATUS "Downloading conan.cmake")
        file(DOWNLOAD "https://raw.githubusercontent.com/akemimadoka/cmake-conan/develop/conan.cmake"
                        "${CMAKE_BINARY_DIR}/conan.cmake" SHOW_PROGRESS
                        STATUS _download_status)
        list(GET _download_status 0 _download_status_code)
        list(GET _download_status 1 _download_status_msg)
        if(NOT _download_status_code EQUAL 0)
            file(REMOVE "${CMAKE_BINARY_DIR}/conan.cmake")
            message(FATAL_ERROR "Failed to download conan.cmake, status code is ${_download_status_code}, msg is ${_download_status_msg}")
        endif()
    endif()

    include(${CMAKE_BINARY_DIR}/conan.cmake)

    conan_cmake_run(
        CONANFILE conanfile.py
        BASIC_SETUP CMAKE_TARGETS
        BUILD missing
        PROFILE_AUTO all
        ENV CC=${CMAKE_C_COMPILER} CXX=${CMAKE_CXX_COMPILER}
    )
endif()

file(GLOB_RECURSE SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h
)

add_library(UmaPyogin STATIC ${SOURCES})

target_compile_features(UmaPyogin PUBLIC cxx_std_20)
target_link_libraries(UmaPyogin PRIVATE ${CONAN_TARGETS})

install(TARGETS UmaPyogin)

install(DIRECTORY src/
    TYPE INCLUDE
    FILES_MATCHING PATTERN "*.h")
