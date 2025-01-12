add_executable(vise_tests
	TestMain.cpp
	TestApplication.h
	TestApplication.cpp
	TestOfflineCompile.h
	TestOfflineCompile.cpp
	TestPushConstants.h
	TestPushConstants.cpp
	TestBuiltins.h
	TestBuiltins.cpp
	TestTransfer.h
	TestTransfer.cpp
	TestPipelineBlend.h
	TestPipelineBlend.cpp
)

target_include_directories(vise_tests PRIVATE ${VISE_INCLUDE_DIRS} ${glm_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/Extern/stb)
target_link_directories(vise_tests PRIVATE ${VISE_LINK_DIRS})
target_link_libraries(vise_tests PRIVATE ${VISE_VULKAN_SDK_LIBS} vise vise_application glfw stb)