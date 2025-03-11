cmake_minimum_required(VERSION 3.12)

include(ExternalProject)

project(replay-agent)

option(ENABLE_GUI      "Enable GUI" OFF)
option(ENABLE_UHD      "Enable UHD" ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


# UHD
if(ENABLE_UHD)
  find_package(UHD)
  if(UHD_FOUND)
    include_directories(${UHD_INCLUDE_DIRS})
    link_directories(${UHD_LIBRARY_DIRS})
  endif()
endif()

add_compile_definitions(SPDLOG_FMT_EXTERNAL)
find_package(spdlog REQUIRED)
add_compile_definitions(SPDLOG_ACTIVE_LEVEL=1)

include_directories(include)
include_directories(lib/lib/include)

add_subdirectory(lib)
add_subdirectory(src)
