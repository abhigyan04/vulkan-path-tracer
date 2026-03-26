cmake_minimum_required(VERSION 3.20)
project(VULKAN_RT)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#Tell CMake where Vulkan SDK is
if(NOT DEFINED ENV{VULKAN_SDK})
    message(FATAL_ERROR "VULKAN_SDK environment variable not set")
endif()

set(VULKAN_SDK $ENV{VULKAN_SDK})

message(STATUS "Using Vulkan SDK at: ${VULKAN_SDK}")

include(FetchContent)

FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.3.8
)

FetchContent_MakeAvailable(glfw)

add_executable(VULKAN_RT
    src/main.cpp
    src/loader/objLoader.cpp
    src/loader/objLoader.hpp
    src/renderer/acceleration.cpp
    src/renderer/acceleration.hpp
    src/renderer/camera.hpp
    src/renderer/commands.cpp
    src/renderer/commands.hpp
    src/renderer/descriptors.cpp
    src/renderer/descriptors.hpp
    src/renderer/gpuScene.cpp
    src/renderer/gpuScene.hpp
    src/renderer/pipeline.cpp
    src/renderer/pipeline.hpp
    src/renderer/sbt.cpp
    src/renderer/sbt.hpp
    src/renderer/scene.hpp
)

target_include_directories(VULKAN_RT PRIVATE
    "${VULKAN_SDK}/Include",
    include
)

target_link_libraries(VULKAN_RT PRIVATE
    "${VULKAN_SDK}/Lib/vulkan-1.lib"
    glfw
)

target_compile_definitions(VULKAN_RT PRIVATE GLFW_INCLUDE_NONE)
