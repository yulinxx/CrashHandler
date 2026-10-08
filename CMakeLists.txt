cmake_minimum_required(VERSION 4.3)

add_subdirectory(CrashHandler)

if(BUILD_CRASHHANDLER_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Test/CMakeLists.txt")
    add_subdirectory(Test)
    message(STATUS "  CrashHandler tests: ENABLED")
else()
    message(STATUS "  CrashHandler tests: DISABLED")
endif()
