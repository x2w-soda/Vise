# fetch GLM
# - not shipped with Vise

FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 0.9.9.8
)
FetchContent_MakeAvailable(glm)

add_library(stb STATIC
	${CMAKE_CURRENT_SOURCE_DIR}/../Extern/stb/stb.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/../Extern/stb/stb_image.h
	${CMAKE_CURRENT_SOURCE_DIR}/../Extern/stb/stb_image_write.h
)

# example application framework to use Vise
# - uses GLFW as window backend

add_library(vise_application
	${CMAKE_CURRENT_SOURCE_DIR}/Application/Application.h
	${CMAKE_CURRENT_SOURCE_DIR}/Application/Application.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/Application/Model.h
	${CMAKE_CURRENT_SOURCE_DIR}/Application/Model.cpp
)

target_include_directories(vise_application PRIVATE ${VISE_INCLUDE_DIRS} ${glm_SOURCE_DIR})
target_link_libraries(vise_application PRIVATE vise stb glfw)

# build examples

add_subdirectory(ExampleTriangle)
add_subdirectory(ExamplePyramid)
add_subdirectory(ExamplePostProcessing)
add_subdirectory(ExampleDeferred)