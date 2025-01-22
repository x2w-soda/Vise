# fetch GLM
# - not shipped with Vise

FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 0.9.9.8
)
FetchContent_MakeAvailable(glm)

# fetch Dear ImGui
# - not shipped with Vise
# - the example Application shows how to integrate ImGui with Vise

FetchContent_Declare(
	imgui
	GIT_REPOSITORY https://github.com/ocornut/imgui
	GIT_TAG v1.91.6
)
FetchContent_MakeAvailable(imgui)

add_library(stb STATIC
	${CMAKE_CURRENT_SOURCE_DIR}/../Extern/stb/stb.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/../Extern/stb/stb_image.h
	${CMAKE_CURRENT_SOURCE_DIR}/../Extern/stb/stb_image_write.h
)

add_library(tinygltf STATIC
	${CMAKE_CURRENT_SOURCE_DIR}/../Extern/tinygltf/tiny_gltf.cc
	${CMAKE_CURRENT_SOURCE_DIR}/../Extern/tinygltf/tiny_gltf.h
	${CMAKE_CURRENT_SOURCE_DIR}/../Extern/tinygltf/json.hpp
)

target_include_directories(tinygltf PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/../Extern/stb
)

add_library(imgui STATIC
	${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
	${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.h
	${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.h
	${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
	${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3_loader.h
	${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.h
	${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
	${imgui_SOURCE_DIR}/imgui.h
	${imgui_SOURCE_DIR}/imgui.cpp
	${imgui_SOURCE_DIR}/imgui_demo.cpp
	${imgui_SOURCE_DIR}/imgui_draw.cpp
	${imgui_SOURCE_DIR}/imgui_tables.cpp
	${imgui_SOURCE_DIR}/imgui_widgets.cpp
)

target_include_directories(imgui PRIVATE
	${imgui_SOURCE_DIR}
	${imgui_SOURCE_DIR}/backends
	${glfw_SOURCE_DIR}/include
	${Vulkan_INCLUDE_DIR}
)

# example application framework to use Vise
# - uses GLFW as window backend
# - uses stb for image loading and saving
# - uses Dear ImGui for example demo

add_library(vise_application
	${CMAKE_CURRENT_SOURCE_DIR}/Application/Application.h
	${CMAKE_CURRENT_SOURCE_DIR}/Application/Application.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/Application/Model.h
	${CMAKE_CURRENT_SOURCE_DIR}/Application/Model.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/Application/Common.h
	${CMAKE_CURRENT_SOURCE_DIR}/Application/Common.cpp
)

target_include_directories(vise_application PRIVATE
	${VISE_INCLUDE_DIRS}
	${glm_SOURCE_DIR}
	${imgui_SOURCE_DIR}
	${imgui_SOURCE_DIR}/backends
	${CMAKE_CURRENT_SOURCE_DIR}/../Extern/tinygltf
	${CMAKE_CURRENT_SOURCE_DIR}/../Extern/stb
)

target_link_libraries(vise_application PRIVATE vise stb tinygltf imgui glfw)

# build examples

add_subdirectory(ExampleTriangle)
add_subdirectory(ExamplePyramid)
add_subdirectory(ExamplePostProcessing)
add_subdirectory(ExampleSSAO)
add_subdirectory(ExampleCubemap)
add_subdirectory(ExamplePBR)