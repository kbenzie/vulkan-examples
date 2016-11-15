add_executable(vector_add
  ${CMAKE_CURRENT_SOURCE_DIR}/vector_add.cpp)

target_include_directories(vector_add PRIVATE ${Vulkan_INCLUDE_DIRS})

target_compile_definitions(vector_add PRIVATE
  SHADER_PATH="${CMAKE_CURRENT_SOURCE_DIR}/"
  $<$<STREQUAL:ON,${ENABLE_LAYERS}>:ENABLE_LAYERS>)

target_compile_options(vector_add PRIVATE
  $<$<PLATFORM_ID:Linux>:-std=c++11 -Werror -Wall>
  $<$<CXX_COMPILER_ID:Clang>:-fcolor-diagnostics>)

target_link_libraries(vector_add PRIVATE ${Vulkan_LIBRARIES})
