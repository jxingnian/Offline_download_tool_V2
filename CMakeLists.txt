# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(Offline_download_tool_V2)

# 添加SPIFFS文件系统支持
spiffs_create_partition_image(storage0 ./spiffs FLASH_IN_PROJECT)