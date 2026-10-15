# CrashHandler 使用示例（独立构建专属）
# 由外层 CrashHandler/CMakeLists.txt 在 BUILD_CRASHHANDLER_EXAMPLE=ON 时挂载，
# 根工程不编译示例。

add_executable(SyCrashHandlerExample
    main.cpp
)

target_compile_features(SyCrashHandlerExample PRIVATE cxx_std_17)

# CrashHandler 的导出头通过链接 target 自动带入（PUBLIC Include 目录）
target_link_libraries(SyCrashHandlerExample PRIVATE
    CrashHandler
)

if(MSVC)
    target_compile_options(SyCrashHandlerExample PRIVATE /utf-8)
endif()

set_target_properties(SyCrashHandlerExample PROPERTIES FOLDER "Examples")

message(STATUS "  Example: SyCrashHandlerExample")
