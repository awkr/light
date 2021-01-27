cmake_minimum_required(VERSION 3.17)
project(light)
set(CMAKE_CXX_STANDARD 17)

find_package(Vulkan REQUIRED)
if (NOT Vulkan_FOUND)
    message(FATAL_ERROR "Vulkan not found")
endif ()

find_package(glfw3 REQUIRED)
if (NOT glfw3_FOUND)
    message(FATAL_ERROR "glfw3 not found")
endif ()

add_executable(light main.cc)

target_link_libraries(
    light
    PUBLIC
    Vulkan::Vulkan
    glfw
)
