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

set(glm_DIR third-party/glm/cmake/glm)
find_package(glm REQUIRED)
if (NOT glm_FOUND)
    message(FATAL_ERROR "glm not found")
else ()
    include_directories(${GLM_INCLUDE_DIRS})
endif ()

include_directories(third-party/glslang/include)
link_directories(third-party/glslang/lib)
set(GLSLANG_LIBS
    GenericCodeGen
    glslang
    HLSL
    MachineIndependent
    OGLCompiler
    OSDependent
    SPIRV
    SPIRV-Tools
    SPIRV-Tools-link
    SPIRV-Tools-opt
    SPIRV-Tools-reduce
    SPIRV-Tools-shared
    SPVRemapper)

add_executable(light main.cc)

target_link_libraries(
    light
    PUBLIC
    Vulkan::Vulkan
    glfw
    glm::glm
    ${GLSLANG_LIBS}
)

target_compile_definitions(light PUBLIC VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1)
