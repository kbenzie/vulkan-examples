cmake_minimum_required(VERSION 2.8.12)
project(vulkan_compute_example)

include(${CMAKE_CURRENT_SOURCE_DIR}/FindVulkan.cmake)

add_executable(example
  ${CMAKE_CURRENT_SOURCE_DIR}/example.cpp)
target_include_directories(example PRIVATE ${Vulkan_INCLUDE_DIRS})
target_compile_definitions(example PRIVATE ENABLE_LAYERS)
target_compile_options(example PRIVATE
  $<$<PLATFORM_ID:Linux>:-std=c++11 -Werror -Wall>
  $<$<CXX_COMPILER_ID:Clang>:-fcolor-diagnostics>)
target_link_libraries(example PRIVATE ${Vulkan_LIBRARIES})
