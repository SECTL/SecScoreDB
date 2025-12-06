cmake_minimum_required(VERSION 3.28)
project(SecScoreDB
    VERSION 1.0.0
    DESCRIPTION "SecScoreDB - A secure score database system"
    LANGUAGES CXX
)

# ============================================================
# C++23 标准配置
# ============================================================
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ============================================================
# 编译器版本检查
# ============================================================
if(MSVC)
    # MSVC v143+ (Visual Studio 2022 17.0+)
    if(MSVC_VERSION LESS 1930)
        message(FATAL_ERROR "MSVC v143+ (VS 2022) or later is required. Current: ${MSVC_VERSION}")
    endif()
    message(STATUS "Using MSVC ${MSVC_VERSION}")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    # GCC 14+
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 14.0)
        message(FATAL_ERROR "GCC 14+ is required. Current: ${CMAKE_CXX_COMPILER_VERSION}")
    endif()
    message(STATUS "Using GCC ${CMAKE_CXX_COMPILER_VERSION}")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    # Clang 17+
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 17.0)
        message(FATAL_ERROR "Clang 17+ is required. Current: ${CMAKE_CXX_COMPILER_VERSION}")
    endif()
    message(STATUS "Using Clang ${CMAKE_CXX_COMPILER_VERSION}")
endif()

# ============================================================
# 编译器警告和选项
# ============================================================
if(MSVC)
    add_compile_options(
        /utf-8          # UTF-8 源文件编码
        /W4             # 高警告级别
        /permissive-    # 严格标准一致性
        /Zc:__cplusplus # 正确报告 __cplusplus 宏
        /Zc:preprocessor # 使用符合标准的预处理器
        /wd4819         # 代码页警告（跨平台源文件）
    )
    # Debug 模式下启用更多检查
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(/RTC1 /GS)
    endif()
else()
    # GCC/Clang 警告
    add_compile_options(
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wsign-conversion
        -Wnon-virtual-dtor
        -Wold-style-cast
        -Woverloaded-virtual
    )
    # Debug 模式下启用 sanitizers
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
        add_link_options(-fsanitize=address,undefined)
    endif()
endif()

# ============================================================
# 启用 CTest
# ============================================================
include(CTest)
enable_testing()

# ============================================================
# 查找依赖包
# ============================================================
find_package(nlohmann_json CONFIG REQUIRED)
find_package(cereal CONFIG REQUIRED)
find_package(ixwebsocket CONFIG REQUIRED)
find_package(GTest CONFIG REQUIRED)

# ============================================================
# 核心静态库
# ============================================================
add_library(SecScoreDB STATIC
    src/FileHelper.cpp
    src/SecScoreDB.cpp
    src/UserManager.cpp
)

# 设置库的 C++ 标准（确保传播给使用者）
target_compile_features(SecScoreDB PUBLIC cxx_std_23)

# 设置包含目录
target_include_directories(SecScoreDB PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:include>
)

# 链接依赖库
target_link_libraries(SecScoreDB PUBLIC
    nlohmann_json::nlohmann_json
    cereal::cereal
)


# ============================================================
# WebSocket 服务端 Wrapper
# ============================================================
if(EXISTS "${PROJECT_SOURCE_DIR}/wrappers/websockets/main.cpp")
    add_executable(SecScoreDB-Websockets
        wrappers/websockets/main.cpp
        wrappers/websockets/Protocol.cpp
        wrappers/websockets/Handlers.cpp
        wrappers/websockets/JsonUtils.cpp
        wrappers/websockets/Errors.cpp
    )
    target_link_libraries(SecScoreDB-Websockets PRIVATE
        SecScoreDB
        ixwebsocket::ixwebsocket
    )
    # Windows: mbedtls 依赖 bcrypt
    if(WIN32)
        target_link_libraries(SecScoreDB-Websockets PRIVATE bcrypt)
    endif()
    target_include_directories(SecScoreDB-Websockets PRIVATE
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/wrappers/websockets
    )
endif()

# ============================================================
# GoogleTest 单元测试
# ============================================================
file(GLOB_RECURSE UNIT_TEST_SOURCES CONFIGURE_DEPENDS
    ${PROJECT_SOURCE_DIR}/tests/unit/*.cpp
)

if(UNIT_TEST_SOURCES)
    # 添加 WebSocket 模块源文件用于测试
    add_executable(SecScoreDB_UnitTests
        ${UNIT_TEST_SOURCES}
        wrappers/websockets/Protocol.cpp
        wrappers/websockets/Handlers.cpp
        wrappers/websockets/JsonUtils.cpp
        wrappers/websockets/Errors.cpp
    )

    target_link_libraries(SecScoreDB_UnitTests PRIVATE
        SecScoreDB
        GTest::gtest
        GTest::gtest_main
        ixwebsocket::ixwebsocket
    )

    # Windows: mbedtls 依赖 bcrypt
    if(WIN32)
        target_link_libraries(SecScoreDB_UnitTests PRIVATE bcrypt)
    endif()

    target_include_directories(SecScoreDB_UnitTests PRIVATE
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/wrappers/websockets
    )

    # 注册到 CTest
    include(GoogleTest)
    gtest_discover_tests(SecScoreDB_UnitTests
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        PROPERTIES
            LABELS "unit"
    )
endif()

