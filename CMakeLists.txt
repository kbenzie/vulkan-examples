cmake_minimum_required(VERSION 2.8.12)
project(vulkan_compute_example)

option(ENABLE_LAYERS "Enable validation layers." OFF)

option(GLFW_BUILD_EXAMPLES "Build the GLFW example programs" OFF)
option(GLFW_BUILD_TESTS "Build the GLFW test programs" OFF)
option(GLFW_BUILD_DOCS "Build the GLFW documentation" OFF)
option(GLFW_INSTALL "Generate installation target" OFF)
add_subdirectory(${PROJECT_SOURCE_DIR}/external/glfw)

if(${CMAKE_VERSION} VERSION_LESS 3.7)
  set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)
endif()

find_package(Vulkan REQUIRED)

add_subdirectory(vector_add)
add_subdirectory(triangle)
