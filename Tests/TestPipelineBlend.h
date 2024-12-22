#pragma once

#include <vise.h>
#include "TestApplication.h"

// Test Graphics Pipeline Blend States
class TestPipelineBlend : public TestApplication
{
public:
	TestPipelineBlend(const TestPipelineBlend&) = delete;
	TestPipelineBlend(VIBackend backend);
	virtual ~TestPipelineBlend();

	TestPipelineBlend& operator=(const TestPipelineBlend&) = delete;

	virtual void Run() override;

	const char* Filename = nullptr;

private:
	VIModule mTestVM;
	VIModule mTestFM;
	VIPipeline mPipelineBlendDisabled;
	VIPipeline mPipelineBlendDefault;
	VIPipeline mPipelineBlendColorAdd;
	VIPipeline mPipelineBlendColorMax;
	VIPipelineLayout mTestPipelineLayout;
	VICommandPool mCmdPool;
};