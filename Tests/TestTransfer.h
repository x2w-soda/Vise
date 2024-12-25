#pragma once

#include <vise.h>
#include "TestApplication.h"

// test transfer operations
// - vi_cmd_copy_buffer
// - vi_cmd_copy_buffer_to_image
// - vi_cmd_copy_image
// - vi_cmd_copy_image_to_buffer
class TestTransfer : public TestApplication
{
public:
	TestTransfer(const TestTransfer&) = delete;
	TestTransfer(VIBackend backend);
	virtual ~TestTransfer();

	TestTransfer& operator=(const TestTransfer&) = delete;

	virtual void Run() override;

	const char* Filename = nullptr;

private:
	void TestFullCopy();

private:
	uint32_t* mPattern;
	uint32_t mPatternSize;
	VIModule mVM;
	VIModule mFM;
	VISetLayout mSetLayout;
	VISetPool mSetPool;
	VIPipeline mPipeline;
	VIPipelineLayout mPipelineLayout;
	VICommandPool mCmdPool;
};