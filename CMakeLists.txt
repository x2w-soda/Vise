cmake_minimum_required(VERSION 3.20)

project(Vise LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# disable VS warning "Prefer enum class over enum to prevent pollution in the global namespace."
if(WIN32)
  add_definitions("/wd26812")
endif(WIN32)

find_package(Vulkan REQUIRED SPIRV-Tools glslang)

include(FetchContent)

# FETCH GLFW
# - currently Vise only supports GLFW window backend

FetchContent_Declare(
  glfw
  GIT_REPOSITORY https://github.com/glfw/glfw.git
  GIT_TAG 3.3.8
)
if(NOT glfw_POPULATED)
  set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
  set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  FetchContent_Populate(glfw)
endif()
add_subdirectory(${glfw_SOURCE_DIR})

# FETCH VMA
# - bundled along Vise

FetchContent_Declare(
  vma
  GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
  GIT_TAG v3.0.1
)
FetchContent_MakeAvailable(vma)

set(VISE_VULKAN_SDK_LIBS 
    debug OSDependentd optimized OSDependent
    debug MachineIndependentd optimized MachineIndependent
    debug GenericCodeGend optimized GenericCodeGen
    debug glslangd optimized glslang
    debug SPIRVd optimized SPIRV
    debug SPIRV-Toolsd optimized SPIRV-Tools
    debug SPIRV-Tools-optd optimized SPIRV-Tools-opt
    debug glslang-default-resource-limitsd optimized glslang-default-resource-limits
    debug spirv-cross-cored optimized spirv-cross-core
    debug spirv-cross-glsld optimized spirv-cross-glsl
    debug spirv-cross-reflectd optimized spirv-cross-reflect)
if (GLSLANG_LIB)
set(VISE_VULKAN_SDK_LIBS 
    debug OSDependentd optimized OSDependent
    debug MachineIndependentd optimized MachineIndependent
    debug GenericCodeGend optimized GenericCodeGen
    debug glslangd optimized glslang
    debug SPIRVd optimized SPIRV
    debug SPIRV-Toolsd optimized SPIRV-Tools
    debug SPIRV-Tools-optd optimized SPIRV-Tools-opt
    debug glslang-default-resource-limitsd optimized glslang-default-resource-limits
    debug spirv-cross-cored optimized spirv-cross-core
    debug spirv-cross-glsld optimized spirv-cross-glsl
    debug spirv-cross-reflectd optimized spirv-cross-reflect)
endif()

set(VISE_LIB
  ${CMAKE_CURRENT_SOURCE_DIR}/Include/vise.h
  ${CMAKE_CURRENT_SOURCE_DIR}/Lib/vise.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/Lib/volk.c
  ${CMAKE_CURRENT_SOURCE_DIR}/Lib/glad.c
)

set(VISE_INCLUDE_DIRS
  ${CMAKE_CURRENT_SOURCE_DIR}/include/glad
  ${CMAKE_CURRENT_SOURCE_DIR}/include/volk
  ${CMAKE_CURRENT_SOURCE_DIR}/include/
  ${Vulkan_INCLUDE_DIR}
  ${GLFW_SOURCE_DIR}/include
  ${vma_SOURCE_DIR}/include
)

set(VISE_LINK_DIRS
  ${Vulkan_INCLUDE_DIR}/../lib
)

# TODO: Linux
set(VISE_COMPILE_DEFINITIONS VK_USE_PLATFORM_WIN32_KHR)

add_library(vise ${VISE_LIB})
target_include_directories(vise PRIVATE ${VISE_INCLUDE_DIRS})
target_link_libraries(vise ${VISE_VULKAN_SDK_LIBS} glfw)
add_compile_definitions(${VISE_COMPILE_DEFINITIONS})

add_subdirectory(examples)
add_subdirectory(tests)