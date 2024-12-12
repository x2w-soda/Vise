#pragma once

#include <vise.h>
#include "TestApplication.h"

// Test NDC coordinates and VISE GLSL builtins
// - gl_FragCoord top left origin
// - gl_VertexIndex
// - gl_InstanceIndex (adds instance base offset even on OpenGL backend)
class TestBuiltins : public TestApplication
{
public:
	TestBuiltins(const TestBuiltins&) = delete;
	TestBuiltins(VIBackend backend);
	virtual ~TestBuiltins();

	TestBuiltins& operator=(const TestBuiltins&) = delete;

	virtual void Run() override;

private:
	VIModule mTestVertexIndexVM;
	VIModule mTestInstanceIndexVM;
	VIModule mTestFragCoordFM;
	VIModule mFragmentColorFM;
	VIPipeline mTestPipeline1;
	VIPipeline mTestPipeline2;
	VIPipelineLayout mTestPipelineLayout;
	VICommandPool mCmdPool;
};