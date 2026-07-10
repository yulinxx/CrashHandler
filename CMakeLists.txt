cmake_minimum_required(VERSION 4.3)

add_subdirectory(CrashHandler)

option(BUILD_CRASHHANDLER_TESTS "Build CrashHandler unit tests" OFF)
if(BUILD_CRASHHANDLER_TESTS)
    add_subdirectory(Test)
endif()
