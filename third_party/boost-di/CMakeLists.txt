# 固定源码副本，不在配置或构建阶段访问网络。
file(SHA256 "${CMAKE_CURRENT_SOURCE_DIR}/include/boost/di.hpp" boost_di_header_sha256)
if(NOT boost_di_header_sha256 STREQUAL "1680ca33ffa04457edcea5c14346ad6e3138d8bb34d4fe074a59e13e34b54827")
    message(FATAL_ERROR "Boost.Ext.DI v1.3.2 头文件校验失败")
endif()
add_library(CaliburnBoostDI INTERFACE)
add_library(Caliburn::BoostDI ALIAS CaliburnBoostDI)
target_include_directories(CaliburnBoostDI SYSTEM INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include")
