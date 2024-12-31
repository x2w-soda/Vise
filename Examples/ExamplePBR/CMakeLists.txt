set(VISE_EXAMPLE_NAME ExamplePBR)

add_executable(${VISE_EXAMPLE_NAME}
  ExamplePBR.h
  ExamplePBR.cpp
  ExamplePBRMain.cpp
)

target_include_directories(${VISE_EXAMPLE_NAME} PRIVATE ${VISE_INCLUDE_DIRS} ${glm_SOURCE_DIR} ${imgui_SOURCE_DIR})
target_link_directories(${VISE_EXAMPLE_NAME} PRIVATE ${VISE_LINK_DIRS})
target_link_libraries(${VISE_EXAMPLE_NAME} vise vise_application)
