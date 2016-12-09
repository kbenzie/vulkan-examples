add_executable(triangle
  ${CMAKE_CURRENT_SOURCE_DIR}/triangle.cpp)

target_include_directories(triangle PRIVATE ${Vulkan_INCLUDE_DIRS})

target_compile_definitions(triangle PRIVATE
  GLFW_INCLUDE_VULKAN
  SHADER_PATH="${CMAKE_CURRENT_SOURCE_DIR}/"
  $<$<STREQUAL:ON,${ENABLE_LAYERS}>:ENABLE_LAYERS>)

target_compile_options(triangle PRIVATE
  $<$<PLATFORM_ID:Linux>:-std=c++11 -Werror -Wall>
  $<$<CXX_COMPILER_ID:Clang>:-fcolor-diagnostics>)

target_link_libraries(triangle PRIVATE ${Vulkan_LIBRARIES} glfw)
