cmake_minimum_required(VERSION 4.3)

add_subdirectory(CrashHandler)

if(BUILD_CRASHHANDLER_TESTS)
    add_subdirectory(Test)
    message(STATUS "  CrashHandler tests: ENABLED")
else()
    message(STATUS "  CrashHandler tests: DISABLED")
endif()
