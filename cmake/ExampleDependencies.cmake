include_guard(GLOBAL)
include(FetchContent)

# 新版 CMake 使用解压时间，旧版继续兼容 3.21。
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

function(caliburn_fetch_example_dependencies)
    # 用局部变量配置第三方项目，不改动父工程的构建选项。
    set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
    set(BUILD_SHARED_LIBS OFF)
    set(SPDLOG_MASTER_PROJECT OFF)
    set(SPDLOG_BUILD_SHARED OFF)
    set(SPDLOG_BUILD_PIC ON)
    set(SPDLOG_BUILD_ALL OFF)
    set(SPDLOG_BUILD_EXAMPLE OFF)
    set(SPDLOG_BUILD_EXAMPLE_HO OFF)
    set(SPDLOG_BUILD_TESTS OFF)
    set(SPDLOG_BUILD_TESTS_HO OFF)
    set(SPDLOG_BUILD_BENCH OFF)
    set(SPDLOG_INSTALL OFF)
    set(SPDLOG_FMT_EXTERNAL OFF)
    set(SPDLOG_FMT_EXTERNAL_HO OFF)
    set(SPDLOG_USE_STD_FORMAT OFF)
    set(SPDLOG_SYSTEM_INCLUDES ON)

    FetchContent_Declare(spdlog
        URL https://codeload.github.com/gabime/spdlog/tar.gz/refs/tags/v1.17.0
        URL_HASH SHA256=d8862955c6d74e5846b3f580b1605d2428b11d97a410d86e2fb13e857cd3a744
        TLS_VERIFY ON
    )
    FetchContent_Declare(boost_di
        URL https://raw.githubusercontent.com/boost-ext/di/v1.3.2/include/boost/di.hpp
        URL_HASH SHA256=1680ca33ffa04457edcea5c14346ad6e3138d8bb34d4fe074a59e13e34b54827
        DOWNLOAD_NO_EXTRACT TRUE
        TLS_VERIFY ON
    )
    FetchContent_MakeAvailable(spdlog boost_di)

    # 保证从随仓库提供的头文件迁移后，实际使用的内容不变。
    file(SHA256 "${boost_di_SOURCE_DIR}/di.hpp" boost_di_header_sha256)
    if(NOT boost_di_header_sha256 STREQUAL "1680ca33ffa04457edcea5c14346ad6e3138d8bb34d4fe074a59e13e34b54827")
        message(FATAL_ERROR "Boost.Ext.DI v1.3.2 头文件校验失败")
    endif()
    # 单头文件不执行上游 CMake，将公开 include 路径整理到构建目录。
    configure_file("${boost_di_SOURCE_DIR}/di.hpp"
        "${boost_di_BINARY_DIR}/include/boost/di.hpp" COPYONLY)
    add_library(CaliburnBoostDI INTERFACE)
    add_library(Caliburn::BoostDI ALIAS CaliburnBoostDI)
    target_include_directories(CaliburnBoostDI SYSTEM INTERFACE "${boost_di_BINARY_DIR}/include")
endfunction()

caliburn_fetch_example_dependencies()
