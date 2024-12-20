# fetch GLM
# - not shipped with Vise

FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 0.9.9.8
)
FetchContent_MakeAvailable(glm)

# example application framework to use Vise
# - uses GLFW as window backend

add_library(vise_application
	Application/Application.h
	Application/Application.cpp
	Application/Model.h
	Application/Model.cpp
)

target_include_directories(vise_application PRIVATE ${VISE_INCLUDE_DIRS} ${glm_SOURCE_DIR})
target_link_libraries(vise_application PRIVATE vise glfw)

# build examples

add_subdirectory(ExampleTriangle)
add_subdirectory(ExamplePyramid)
add_subdirectory(ExamplePostProcessing)
add_subdirectory(ExampleDeferred)