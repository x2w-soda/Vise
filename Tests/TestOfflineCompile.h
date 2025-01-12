#pragma once

#include <vise.h>
#include "TestApplication.h"

// Test offline compilation of shader modules
class TestOfflineCompile : public TestApplication
{
public:
	TestOfflineCompile(const TestOfflineCompile&) = delete;
	TestOfflineCompile(VIBackend backend);
	virtual ~TestOfflineCompile();

	TestOfflineCompile& operator=(const TestOfflineCompile&) = delete;

	virtual void Run() override;

	const char* Filename = nullptr;

private:
	char* mTestBinaryVM;
	char* mTestBinaryFM;
	VIModule mTestVM;
	VIModule mTestFM;
	VIPipeline mTestPipeline;
	VIPipelineLayout mTestPipelineLayout;
	VICommandPool mCmdPool;
};