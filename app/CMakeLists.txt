# 负责生成最终烧录的 .elf/.bin/.hex，链接所有底层库。

file(GLOB _app_sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
target_sources(${CMAKE_PROJECT_NAME} PRIVATE ${_app_sources})
#  指定头文件搜索路径 (这样你的代码里才能 #include "shoot_motor.hpp")
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include)