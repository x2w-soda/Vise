#include <array>
#include <imgui.h>
#include "ExampleCubemap.h"
#include "../Application/Common.h"
#include "../Application/Model.h"
#include "../../Extern/stb/stb_image.h"

static const char skybox_vm_glsl[] = R"(
#version 460

layout (location = 0) in vec3 aPosition;
layout (location = 0) out vec3 vCubemapUVW;

layout (push_constant) uniform uPC
{
	mat4 mvp;
} PC;

void main()
{
	vCubemapUVW = aPosition;
	gl_Position = PC.mvp * vec4(aPosition, 1.0f);
}
)";

static const char skybox_fm_glsl[] = R"(
#version 460

layout (location = 0) in vec3 vCubemapUVW;
layout (location = 0) out vec4 fColor;

layout (set = 0, binding = 1) uniform samplerCube uCubemap;

void main()
{
	fColor = vec4(texture(uCubemap, vCubemapUVW).rgb, 1.0);
}
)";

static const char model_vm_glsl[] = R"(
#version 460

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aUV;

layout (location = 0) out vec3 vPos;
layout (location = 1) out vec3 vNormal;
layout (location = 2) out vec2 vUV;

layout (set = 0, binding = 0) uniform Scene
{
	mat4 view;
	mat4 proj;
	vec4 cameraPos;
} uScene;

layout (push_constant) uniform PC
{
	mat4 nodeTransform;
	uint showRefraction;
	float refractiveIndex;
	float chromaticDispersion;
} uPC;

void main()
{
	vec4 worldPos = uPC.nodeTransform * vec4(aPos, 1.0);
	mat4 normalMat = transpose(inverse(uPC.nodeTransform));

	vPos = worldPos.xyz;
	vNormal = vec3(normalMat * vec4(aNormal, 1.0)); // TODO: normal mapping
	vUV = aUV;
	
	gl_Position = uScene.proj * uScene.view * worldPos;
}
)";

static const char model_fm_glsl[] = R"(
#version 460

layout (location = 0) in vec3 vPos;
layout (location = 1) in vec3 vNormal;
layout (location = 2) in vec2 vUV;

layout (location = 0) out vec4 fColor;

layout (set = 0, binding = 0) uniform Scene
{
	mat4 view;
	mat4 proj;
	vec4 cameraPos;
} uScene;

layout (set = 0, binding = 1) uniform samplerCube uCubemap;

// MATERIAL SET

layout (set = 1, binding = 0) uniform Mat
{
	uint hasColorMap;
	uint hasNormalMap;
	uint hasMetallicRoughnessMap;
	uint hasOcclusionMap;
	float metallicFactor;
	float roughnessFactor;
} uMat;

layout (set = 1, binding = 1) uniform sampler2D uMatColor;
layout (set = 1, binding = 2) uniform sampler2D uMatNormal;
layout (set = 1, binding = 3) uniform sampler2D uMatMR;

layout (push_constant) uniform PC
{
	mat4 nodeTransform;
	uint showRefraction;
	float refractiveIndex;
	float chromaticDispersion;
} uPC;

void main()
{
	float indexR = 1.0 / (uPC.refractiveIndex * uPC.chromaticDispersion);
	float indexB = 1.0 / uPC.refractiveIndex;
	float indexG = 1.0 / (uPC.refractiveIndex / uPC.chromaticDispersion);

	vec3 cameraPos = uScene.cameraPos.xyz;
	vec3 viewDir = normalize(vPos - cameraPos);
	vec3 reflectDir = reflect(viewDir, vNormal);
	vec3 refractDirR = refract(viewDir, vNormal, indexR);
	vec3 refractDirG = refract(viewDir, vNormal, indexG);
	vec3 refractDirB = refract(viewDir, vNormal, indexB);

	if (bool(uPC.showRefraction))
	{
		float colorR = texture(uCubemap, refractDirR).r;
		float colorG = texture(uCubemap, refractDirG).g;
		float colorB = texture(uCubemap, refractDirB).b;
		fColor = vec4(colorR, colorG, colorB, 1.0);
	}
	else
	{
		vec3 color = texture(uCubemap, reflectDir).rgb;
		fColor = vec4(color, 1.0);
	}
}
)";

ExampleCubemap::ExampleCubemap(VIBackend backend)
	: Application("Cubemap", backend)
{
	VIPass pass = vi_device_get_swapchain_pass(mDevice);

	mSetLayout = CreateSetLayout(mDevice, {
		{ VI_BINDING_TYPE_UNIFORM_BUFFER, 0, 1 },
		{ VI_BINDING_TYPE_COMBINED_IMAGE_SAMPLER, 1, 1 },
	});

	mMaterialSetLayout = GLTFMaterial::CreateSetLayout(mDevice);

	mPipelineLayout = CreatePipelineLayout(mDevice, {
		mSetLayout,
		mMaterialSetLayout
	}, 128);

	mSkyboxVM = CreateOrLoadModule(mDevice, mBackend, mPipelineLayout, VI_MODULE_TYPE_VERTEX, skybox_vm_glsl, "skybox_vm");
	mSkyboxFM = CreateOrLoadModule(mDevice, mBackend, mPipelineLayout, VI_MODULE_TYPE_FRAGMENT, skybox_fm_glsl, "skybox_fm");
	mModelVM = CreateOrLoadModule(mDevice, mBackend, mPipelineLayout, VI_MODULE_TYPE_VERTEX, model_vm_glsl, "model_vm");
	mModelFM = CreateOrLoadModule(mDevice, mBackend, mPipelineLayout, VI_MODULE_TYPE_FRAGMENT, model_fm_glsl, "model_fm");

	uint32_t size;
	std::vector<VIVertexAttribute> vertexAttr;
	std::vector<VIVertexBinding> vertexBinding;
	const float* vertices = GetSkyboxVertices(nullptr, &size, &vertexAttr, &vertexBinding);

	VIBufferInfo vboI;
	vboI.type = VI_BUFFER_TYPE_VERTEX;
	vboI.usage = VI_BUFFER_USAGE_TRANSFER_DST_BIT;
	vboI.properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
	vboI.size = size;
	mCubeVBO = CreateBufferStaged(mDevice, &vboI, vertices);

	std::array<VIModule, 2> modules;
	modules[0] = mSkyboxVM;
	modules[1] = mSkyboxFM;

	VIPipelineInfo pipelineI;
	pipelineI.module_count = modules.size();
	pipelineI.modules = modules.data();
	pipelineI.layout = mPipelineLayout;
	pipelineI.pass = pass;
	pipelineI.vertex_attribute_count = vertexAttr.size();
	pipelineI.vertex_attributes = vertexAttr.data();
	pipelineI.vertex_binding_count = vertexBinding.size();
	pipelineI.vertex_bindings = vertexBinding.data();
	pipelineI.depth_stencil_state.depth_test_enabled = false;
	mSkyboxPipeline = vi_create_pipeline(mDevice, &pipelineI);

	modules[0] = mModelVM;
	modules[1] = mModelFM;
	vertexBinding.resize(1);
	GLTFVertex::GetBindingAndAttributes(vertexBinding[0], vertexAttr);
	pipelineI.vertex_attribute_count = vertexAttr.size();
	pipelineI.vertex_attributes = vertexAttr.data();
	pipelineI.vertex_binding_count = vertexBinding.size();
	pipelineI.vertex_bindings = vertexBinding.data();
	pipelineI.depth_stencil_state.depth_test_enabled = true;
	mModelPipeline = vi_create_pipeline(mDevice, &pipelineI);

	std::array<VISetPoolResource, 2> resources;
	resources[0].type = VI_BINDING_TYPE_COMBINED_IMAGE_SAMPLER;
	resources[0].count = mFramesInFlight;
	resources[1].type = VI_BINDING_TYPE_UNIFORM_BUFFER;
	resources[1].count = mFramesInFlight;

	VISetPoolInfo poolI;
	poolI.max_set_count = mFramesInFlight;
	poolI.resource_count = resources.size();
	poolI.resources = resources.data();
	mSetPool = vi_create_set_pool(mDevice, &poolI);

	uint32_t family = vi_device_get_graphics_family_index(mDevice);
	mCmdPool = vi_create_command_pool(mDevice, family, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

	int width, height, ch;
	stbi_uc* face_pixels[6];
	face_pixels[0] = stbi_load(APP_PATH "../../Assets/cubemaps/goegap_road_2k/px.jpg", &width, &height, &ch, STBI_rgb_alpha);
	face_pixels[1] = stbi_load(APP_PATH "../../Assets/cubemaps/goegap_road_2k/nx.jpg", &width, &height, &ch, STBI_rgb_alpha);
	face_pixels[2] = stbi_load(APP_PATH "../../Assets/cubemaps/goegap_road_2k/py.jpg", &width, &height, &ch, STBI_rgb_alpha);
	face_pixels[3] = stbi_load(APP_PATH "../../Assets/cubemaps/goegap_road_2k/ny.jpg", &width, &height, &ch, STBI_rgb_alpha);
	face_pixels[4] = stbi_load(APP_PATH "../../Assets/cubemaps/goegap_road_2k/pz.jpg", &width, &height, &ch, STBI_rgb_alpha);
	face_pixels[5] = stbi_load(APP_PATH "../../Assets/cubemaps/goegap_road_2k/nz.jpg", &width, &height, &ch, STBI_rgb_alpha);
	size_t face_size = width * height * 4;
	unsigned char* pixels = new unsigned char[face_size * 6];

	for (int i = 0; i < 6; i++)
	{
		uint32_t offset = i * face_size;
		memcpy(pixels + offset, face_pixels[i], face_size);
	}

	VIImageInfo imageI{};
	imageI.type = VI_IMAGE_TYPE_CUBE;
	imageI.usage = VI_IMAGE_USAGE_SAMPLED_BIT | VI_IMAGE_USAGE_TRANSFER_DST_BIT;
	imageI.format = VI_FORMAT_RGBA8;
	imageI.width = (uint32_t)width;
	imageI.height = (uint32_t)height;
	imageI.layers = 6;
	imageI.properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
	imageI.sampler.filter = VI_FILTER_LINEAR;
	mImageCubemap = CreateImageStaged(mDevice, &imageI, pixels, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	
	for (int i = 0; i < 6; i++)
		stbi_image_free(face_pixels[i]);

	delete[] pixels;

	mFrames.resize(mFramesInFlight);
	for (size_t i = 0; i < mFrames.size(); i++)
	{
		VIBufferInfo uboI;
		uboI.type = VI_BUFFER_TYPE_UNIFORM;
		uboI.properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
		uboI.size = sizeof(FrameUBO);
		mFrames[i].ubo = vi_create_buffer(mDevice, &uboI);
		vi_buffer_map(mFrames[i].ubo);

		mFrames[i].cmd = vi_allocate_primary_command(mDevice, mCmdPool);
		mFrames[i].set = AllocAndUpdateSet(mDevice, mSetPool, mSetLayout, {
			{ 0, mFrames[i].ubo, VI_NULL},
			{ 1, VI_NULL, mImageCubemap }
		});
	}

	glfwSetKeyCallback(mWindow, &ExampleCubemap::KeyCallback);
}

ExampleCubemap::~ExampleCubemap()
{
	vi_device_wait_idle(mDevice);

	for (size_t i = 0; i < mFrames.size(); i++)
	{
		vi_free_command(mDevice, mFrames[i].cmd);
		vi_free_set(mDevice, mFrames[i].set);
		vi_buffer_unmap(mFrames[i].ubo);
		vi_destroy_buffer(mDevice, mFrames[i].ubo);
	}

	vi_destroy_image(mDevice, mImageCubemap);
	vi_destroy_command_pool(mDevice, mCmdPool);
	vi_destroy_set_pool(mDevice, mSetPool);
	vi_destroy_buffer(mDevice, mCubeVBO);
	vi_destroy_pipeline_layout(mDevice, mPipelineLayout);
	vi_destroy_pipeline(mDevice, mSkyboxPipeline);
	vi_destroy_pipeline(mDevice, mModelPipeline);
	vi_destroy_set_layout(mDevice, mMaterialSetLayout);
	vi_destroy_set_layout(mDevice, mSetLayout);
	vi_destroy_module(mDevice, mSkyboxVM);
	vi_destroy_module(mDevice, mSkyboxFM);
	vi_destroy_module(mDevice, mModelVM);
	vi_destroy_module(mDevice, mModelFM);
}

void ExampleCubemap::KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	ExampleCubemap* example = (ExampleCubemap*)Application::Get();

	if (action != GLFW_PRESS)
		return;

	if (key == GLFW_KEY_ESCAPE)
	{
		example->CameraToggleCapture();
	}
}

void ExampleCubemap::Run()
{
	mCamera.SetPosition({ -3.0f, 0.0f, 0.0f });
	mConfig.showRefraction = false;
	mConfig.refractiveIndex = 1.52f;
	mConfig.chromaticDispersion = 1.005f;

	mVulkanModel = GLTFModel::LoadFromFile(APP_PATH "../../Assets/gltf/vulkan_logo/scene.gltf", mDevice, mMaterialSetLayout);
	mOpenGLModel = GLTFModel::LoadFromFile(APP_PATH "../../Assets/gltf/opengl_logo/scene.gltf", mDevice, mMaterialSetLayout);
	mModel = GLTFModel::LoadFromFile(APP_PATH "../../Assets/gltf/DamagedHelmet/glTF/DamagedHelmet.gltf", mDevice, mMaterialSetLayout);
	std::shared_ptr<GLTFModel> renderModel = mModel;

	while (!glfwWindowShouldClose(mWindow))
	{
		Application::NewFrame();
		Application::ImGuiNewFrame();
		Application::CameraUpdate();

		if (!CameraIsCaptured())
		{
			ImGui::Begin(mName);
			ImGuiDeviceProfile();

			if (ImGui::CollapsingHeader("Settings"))
			{
				ImGui::Text("GLTF Model");
				if (ImGui::Selectable("- Damaged Helmet", renderModel == mModel))
					renderModel = mModel;
				if (ImGui::Selectable("- Vulkan Logo", renderModel == mVulkanModel))
					renderModel = mVulkanModel;
				if (ImGui::Selectable("- OpenGL Logo", renderModel == mOpenGLModel))
					renderModel = mOpenGLModel;

				if (ImGui::Button("Reflective Environment Mapping"))
					mConfig.showRefraction = false;

				if (ImGui::Button("Refractive Environment Mapping"))
					mConfig.showRefraction = true;

				ImGui::SliderFloat("Refractive Index", &mConfig.refractiveIndex, 1.0f, 3.0f);
				ImGui::SliderFloat("Chromatic Dispersion", &mConfig.chromaticDispersion, 1.0f, 1.02f);
			}

			ImGui::End();
		}

		VISemaphore image_acquired;
		VISemaphore present_ready;
		VIFence frame_complete;
		uint32_t frame_idx = vi_device_next_frame(mDevice, &image_acquired, &present_ready, &frame_complete);
		VIFramebuffer fb = vi_device_get_swapchain_framebuffer(mDevice, frame_idx);
		FrameData* frame = mFrames.data() + frame_idx;

		FrameUBO frameUBO;
		frameUBO.view = mCamera.GetViewMat();
		frameUBO.proj = mCamera.GetProjMat();
		frameUBO.cameraPos = glm::vec4(mCamera.GetPosition(), 1.0f);
		vi_buffer_map_write(frame->ubo, 0, sizeof(frameUBO), &frameUBO);

		vi_begin_command(frame->cmd, 0);

		VkClearValue clear[2];
		clear[0] = MakeClearDepthStencil(1.0f, 0.0f);
		clear[1] = MakeClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		VIPassBeginInfo beginI;
		beginI.pass = vi_device_get_swapchain_pass(mDevice);
		beginI.framebuffer = fb;
		beginI.color_clear_values = clear + 1;
		beginI.color_clear_value_count = 1;
		beginI.depth_stencil_clear_value = clear;

		vi_cmd_begin_pass(frame->cmd, &beginI);
		{
			vi_cmd_bind_graphics_pipeline(frame->cmd, mSkyboxPipeline);
			vi_cmd_set_viewport(frame->cmd, MakeViewport(APP_WINDOW_WIDTH, APP_WINDOW_HEIGHT));
			vi_cmd_set_scissor(frame->cmd, MakeScissor(APP_WINDOW_WIDTH, APP_WINDOW_HEIGHT));

			vi_cmd_bind_vertex_buffers(frame->cmd, 0, 1, &mCubeVBO);
			vi_cmd_bind_graphics_set(frame->cmd, mPipelineLayout, 0, frame->set);

			glm::mat4 mvp = mCamera.GetProjMat() * glm::mat4(glm::mat3(mCamera.GetViewMat()));
			vi_cmd_push_constants(frame->cmd, mPipelineLayout, 0, sizeof(mvp), &mvp);

			VIDrawInfo info;
			info.vertex_count = 36;
			info.vertex_start = 0;
			info.instance_count = 1;
			info.instance_start = 0;
			vi_cmd_draw(frame->cmd, &info);

			vi_cmd_bind_graphics_pipeline(frame->cmd, mModelPipeline);
			vi_cmd_set_viewport(frame->cmd, MakeViewport(APP_WINDOW_WIDTH, APP_WINDOW_HEIGHT));
			vi_cmd_set_scissor(frame->cmd, MakeScissor(APP_WINDOW_WIDTH, APP_WINDOW_HEIGHT));

			vi_cmd_bind_graphics_set(frame->cmd, mPipelineLayout, 0, frame->set);

			struct ModelPushConstant
			{
				uint32_t showRefraction;
				float refractiveIndex;
				float chromaticDispersion;
			} modelPC;
			modelPC.showRefraction = (uint32_t)mConfig.showRefraction;
			modelPC.refractiveIndex = mConfig.refractiveIndex;
			modelPC.chromaticDispersion = mConfig.chromaticDispersion;

			vi_cmd_push_constants(frame->cmd, mPipelineLayout, sizeof(glm::mat4), sizeof(modelPC), &modelPC);

			uint32_t materialSetIndex = 1;
			renderModel->Draw(frame->cmd, mPipelineLayout, materialSetIndex);

			Application::ImGuiRender(frame->cmd);
		}
		vi_cmd_end_pass(frame->cmd);
		vi_end_command(frame->cmd);
		
		VkPipelineStageFlags stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		VISubmitInfo submitI;
		submitI.wait_count = 1;
		submitI.wait_stages = &stage;
		submitI.waits = &image_acquired;
		submitI.signal_count = 1;
		submitI.signals = &present_ready;
		submitI.cmd_count = 1;
		submitI.cmds = &frame->cmd;

		VIQueue graphics_queue = vi_device_get_graphics_queue(mDevice);
		vi_queue_submit(graphics_queue, 1, &submitI, frame_complete);

		vi_device_present_frame(mDevice);
	}

	vi_device_wait_idle(mDevice);
	mVulkanModel = nullptr;
	mOpenGLModel = nullptr;
	mModel = nullptr;
}
