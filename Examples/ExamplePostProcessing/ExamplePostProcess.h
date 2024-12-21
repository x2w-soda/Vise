#pragma once

#include <memory>
#include <vise.h>
#include "../Application/Model.h"
#include "../Application/Application.h"

class ExamplePostProcess : public Application
{
public:
	ExamplePostProcess(VIBackend type);
	~ExamplePostProcess();

	void Run() override;

private:
	static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);

private:

	// per-frame synchronization
	struct FrameData
	{
		VISet set;
		VIFramebuffer fbo;
		VIBuffer scene_ubo;
		VIImage scene_image;
		VIImage scene_depth;
		VICommand cmd;
	};

	struct Config
	{
		VIPipeline postprocess_pipeline;
	} mConfig;

	std::vector<FrameData> mFrames;
	std::vector<std::shared_ptr<Mesh>> mMeshes;
	VIPass mSceneRenderPass;
	VIPass mPostProcessPass;
	VIModule mVMRender, mFMRender;
	VIModule mVMPostProcess, mFMNone, mFMGrayscale, mFMInvert;
	VISetPool mSetPool;
	VICommandPool mCmdPool;
	VISetLayout mSetLayout;
	VIPipelineLayout mPipelineLayout;
	VIPipeline mPipelineRender;
	VIPipeline mPipelineNone, mPipelineGrayscale, mPipelineInvert;
	VIBuffer mQuadVBO;
	VIBuffer mQuadIBO;
};