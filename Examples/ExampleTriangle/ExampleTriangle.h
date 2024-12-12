#pragma once

#include <vector>
#include "../Application/Application.h"

class ExampleTriangle : public Application
{
public:
	ExampleTriangle(VIBackend backend);
	~ExampleTriangle();

	virtual void Run() override;

private:
	void RecordCommands();

private:
	VIModule mVertexModule;
	VIModule mFragmentModule;
	VIPipelineLayout mPipelineLayout;
	VIPipeline mPipeline;
	VICommandPool mCmdPool;
	VIBuffer mVBO;
	std::vector<VICommand> mCommands;
};