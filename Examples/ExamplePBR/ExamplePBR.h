#pragma once

#include <memory>
#include <glm/mat4x4.hpp>
#include <vise.h>
#include "../Application/Application.h"
#include "../Application/Model.h"

class ExamplePBR : public Application
{
public:
	ExamplePBR(VIBackend backend);
	~ExamplePBR();

	void Run() override;

private:

	// per-frame synchronization
	struct FrameData
	{
		VICommand cmd;
		VIBuffer scene_ubo;
		VISet scene_set;
	};

	struct SceneUBO
	{
		glm::mat4 view;
		glm::mat4 proj;
		glm::vec4 camera_pos;
		uint32_t show_channel;
		uint32_t metallic_state;
		float clamp_max_roughness;
	} mSceneUBO;

	struct Config
	{
		VISet show_background_skybox = VI_NULL;
		float show_prefilter_roughness = -1.0f;
	} mConfig;

	struct CubemapPushConstant
	{
		glm::mat4 mvp;
		float delta_phi;
		float delta_theta;
		float roughness;
		uint32_t sample_count;
	};

	struct SkyboxPushConstant
	{
		glm::mat4 mvp;
		float prefilter_roughness = -1.0f;
	};

	static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);

	void ImGuiUpdate();
	void Bake();
	void BakeCubemap(VIImage targetCubemap, uint32_t cubemapDim, VIPipeline pipeline, VIPipelineLayout layout, VISet imageSet, uint32_t mipCount, CubemapPushConstant* constants);
	void BakeBRDFLUT();
	void UpdateUBO();

private: // resources for baking

	VIModule mCubemapFaceVM;
	VIModule mHDRI2CubeFM;
	VIModule mIrradianceFM;
	VIModule mPrefilterFM;
	VIModule mBRDFLUTVM;
	VIModule mBRDFLUTFM;
	VIPass mBRDFLUTPass;
	VIPass mCubemapPass;
	VIImage mCubemap;
	VIImage mOffscreenImage;
	VIImage mHDRI;
	VIImage mIrradiance;
	VIImage mPrefilter;
	VIImage mBRDFLUT;
	VISet mHDRISet;
	VISet mCubemapSet;
	VISet mPrefilterSet;
	VISet mIrradianceSet;
	VISet mBRDFLUTSet;
	VIFramebuffer mOffscreenFBO;
	VIFramebuffer mBRDFLUTFBO;
	VIPipeline mBRDFLUTPipeline;
	VIPipeline mHDRI2CubePipeline;
	VIPipeline mIrradiancePipeline;
	VIPipeline mPrefilterPipeline;

private: // runtime resources

	std::shared_ptr<GLTFModel> mModel;
	std::vector<FrameData> mFrames;
	VICommandPool mCmdPool;
	VIBuffer mSkyboxVBO;
	VISetPool mSetPool;
	VIModule mSkyboxVM;
	VIModule mSkyboxFM;
	VIModule mPBRVM;
	VIModule mPBRFM;
	VIPipeline mSkyboxPipeline;
	VIPipeline mPBRPipeline;
	VISetLayout mSetLayoutSingleImage;
	VISetLayout mSetLayoutScene;
	VISetLayout mSetLayoutMaterial;
	VIPipelineLayout mPipelineLayoutSingleImage;
	VIPipelineLayout mPipelineLayoutPBR;

	uint64_t mImGuiHDRI;
	uint64_t mImGuiCubemap;
	uint64_t mImGuiBRDFLUT;
};