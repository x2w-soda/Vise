#pragma once

#include <vector>
#include <memory>
#include <cstdint>
#include "../Application/Application.h"
#include "../Application/Model.h"

class ExampleSSAO : public Application
{
public:
	ExampleSSAO(VIBackend backend);
	~ExampleSSAO();

	virtual void Run() override;

	static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);

private:

	// per-frame synchronization
	struct FrameData
	{
		VICommand cmd;
		VIBuffer ubo;
		VIFramebuffer gbuffer;
		VIFramebuffer ssao_fbo;
		VIFramebuffer ssao_blur_fbo;
		VISet ssao_set;
		VISet ssao_blur_set;
		VISet gbuffer_set;
		VISet composition_set;
		VIImage gbuffer_diffuse;
		VIImage gbuffer_normals;
		VIImage gbuffer_positions;
		VIImage gbuffer_depth;
		VIImage ssao;
		VIImage ssao_blur;
	};

	struct Config
	{
		uint32_t show_result;
		int ssao_sample_count;
		float ssao_depth_bias;
		float ssao_kernel_radius;
		bool ssao_use_range_check;
		bool blur_ssao;
		bool use_ssao;
		bool use_normal_map;
	} mConfig;

	std::shared_ptr<GLTFModel> mSceneModel;
	std::vector<FrameData> mFrames;
	VIPass mGeometryPass;
	VIPass mColorR8Pass;
	VIImage mNoise;
	VIBuffer mQuadVBO;
	VIBuffer mKernelUBO;
	VIModule mGeometryVM;
	VIModule mGeometryFM;
	VIModule mQuadVM;
	VIModule mSSAOFM;
	VIModule mSSAOBlurFM;
	VIModule mCompositionFM;
	VISetPool mSetPool;
	VISetLayout mSetLayoutUCCC;
	VISetLayout mSetLayoutCCCC;
	VIPipelineLayout mPipelineLayoutUCCC2;
	VIPipelineLayout mPipelineLayoutCCCC;
	VIPipeline mSSAOPipeline;
	VIPipeline mSSAOBlurPipeline;
	VIPipeline mGeometryPipeline;
	VIPipeline mCompositionPipeline;
	VICommandPool mCmdPool;
};