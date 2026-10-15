# CrashHandler 模块单元测试配置
# 挂载点：内层 CrashHandler/CrashHandler/CMakeLists.txt（BUILD_CRASHHANDLER_TESTS=ON 时），
# 根工程与独立构建都经由内层进来，此处不得被外层重复 add_subdirectory。

# 查找 GTest 包（独立构建下由外层 sanyi_find_gtest() 预先降级过开关）
find_package(GTest CONFIG REQUIRED)

# 包含 GoogleTest 模块以使用 gtest_discover_tests
include(GoogleTest)

# 收集测试源文件
file(GLOB_RECURSE CRASHHANDLER_TEST_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cxx"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cc"
)

# 创建测试可执行文件
add_executable(CrashHandlerTests ${CRASHHANDLER_TEST_SOURCES})

target_compile_features(CrashHandlerTests PRIVATE cxx_std_17)

# CrashHandler 的导出头通过链接 target 自动带入（PUBLIC Include 目录）
target_link_libraries(CrashHandlerTests PRIVATE
    CrashHandler
    GTest::gtest
    GTest::gtest_main
)

# MSVC 特定配置
if(MSVC)
    target_compile_options(CrashHandlerTests PRIVATE /utf-8)
endif()

# 将测试添加到 CTest：
# Windows 上手动 add_test 并注入 PATH —— 测试进程要加载 vcpkg 的 breakpad DLL，
# 只有把 vcpkg 的 bin 目录写进环境变量，测试发现和运行时才能找到它们。
if(WIN32)
    set(VCPKG_BIN_DIR "${VCPKG_DIR}/installed/x64-windows/bin")
    set(VCPKG_DEBUG_BIN_DIR "${VCPKG_DIR}/installed/x64-windows/debug/bin")
    set(CH_TESTS_PATH "${VCPKG_DEBUG_BIN_DIR};${VCPKG_BIN_DIR};$ENV{PATH}")

    add_test(NAME CrashHandlerTests
        COMMAND "${CMAKE_BINARY_DIR}/bin_Qt${QT_VERSION_MAJOR}/$<CONFIG>/CrashHandlerTests.exe"
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin_Qt${QT_VERSION_MAJOR}/$<CONFIG>"
    )
    set_tests_properties(CrashHandlerTests PROPERTIES
        ENVIRONMENT "PATH=${CH_TESTS_PATH}"
    )
else()
    gtest_discover_tests(CrashHandlerTests)
endif()

# 测试目标归入 Tests 文件夹
set_property(TARGET CrashHandlerTests PROPERTY FOLDER "Tests")
