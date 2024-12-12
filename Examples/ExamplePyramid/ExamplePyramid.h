#pragma once

#include <vise.h>
#include "../Application/Application.h"

class ExamplePyramid : public Application
{
public:
	ExamplePyramid(VIBackend type);
	~ExamplePyramid();

	void Run() override;

private:

	static ExamplePyramid* Get()
	{
		return sInstance;
	}

	static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
	static void CursorPosCallback(GLFWwindow* window, double xpos, double ypos);

private:

	struct FrameUBO
	{
		glm::mat4 view;
		glm::mat4 proj;
	};

	// per-frame synchronization
	struct FrameData
	{
		VISet set;
		VIBuffer ubo;
		VICommand cmd;
		void* ubo_map;
	};

	static ExamplePyramid* sInstance;

	bool mIsCameraCaptured = false;
	std::vector<FrameData> mFrames;

	VIModule mVertexModule;
	VIModule mFragmentModule;
	VISetPool mSetPool;
	VICommandPool mCmdPool;
	VISetLayout mSetLayout;
	VIPipelineLayout mPipelineLayout;
	VIPipeline mPipeline;
	VIBuffer mVBO;
	VIBuffer mIBO;
};