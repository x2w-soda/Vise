#pragma once

#include <vise.h>
#include "TestApplication.h"

// Test Push Constant Ranges
// TODO: test push_constant on compute pipelines
class TestPushConstants : public TestApplication
{
public:
	TestPushConstants(const TestPushConstants&) = delete;
	TestPushConstants(VIBackend backend);
	virtual ~TestPushConstants();

	TestPushConstants& operator=(const TestPushConstants&) = delete;

	virtual void Run() override;

	const char* Filename = nullptr;

private:
	VIModule mTestVM1;
	VIModule mTestVM2;
	VIModule mTestFM;
	VIPipeline mTestPipeline1;
	VIPipeline mTestPipeline2;
	VIPipelineLayout mTestPipelineLayout;
	VICommandPool mCmdPool;
};