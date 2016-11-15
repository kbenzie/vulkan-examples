cmake_minimum_required(VERSION 2.8.12)
project(vulkan_compute_example)

option(ENABLE_LAYERS "Enable validation layers." ON)

if(${CMAKE_VERSION} VERSION_LESS 3.7)
  set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)
endif()

find_package(Vulkan REQUIRED)

add_subdirectory(vector_add)
