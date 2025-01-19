#pragma once

#include <vise.h>
#include "../Application/Application.h"
#include "../Application/Model.h"

class ExampleCubemap : public Application
{
public:
	ExampleCubemap(VIBackend type);
	~ExampleCubemap();

	void Run() override;

private:

	static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);

private:

	// per-frame synchronization
	struct FrameData
	{
		VISet set;
		VICommand cmd;
		VIBuffer ubo;
	};

	struct FrameUBO
	{
		glm::mat4 view;
		glm::mat4 proj;
		glm::vec4 cameraPos;
	};

	struct Config
	{
		bool showRefraction;
		float refractiveIndex;
		float chromaticDispersion;
	} mConfig;

	std::vector<FrameData> mFrames;
	std::shared_ptr<GLTFModel> mModel, mOpenGLModel, mVulkanModel;
	VIModule mSkyboxVM;
	VIModule mSkyboxFM;
	VIModule mModelVM;
	VIModule mModelFM;
	VIImage mImageCubemap;
	VISetPool mSetPool;
	VICommandPool mCmdPool;
	VISetLayout mSetLayout;
	VISetLayout mMaterialSetLayout;
	VIPipelineLayout mPipelineLayout;
	VIPipeline mSkyboxPipeline;
	VIPipeline mModelPipeline;
	VIBuffer mCubeVBO;
};