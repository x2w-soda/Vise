set(VISE_EXAMPLE_NAME ExamplePyramid)

add_executable(${VISE_EXAMPLE_NAME}
  ExamplePyramid.h
  ExamplePyramid.cpp
  ExamplePyramidMain.cpp
)

target_include_directories(${VISE_EXAMPLE_NAME} PRIVATE ${VISE_INCLUDE_DIRS} ${glm_SOURCE_DIR})
target_link_directories(${VISE_EXAMPLE_NAME} PRIVATE ${VISE_LINK_DIRS})
target_link_libraries(${VISE_EXAMPLE_NAME} vise vise_application)
