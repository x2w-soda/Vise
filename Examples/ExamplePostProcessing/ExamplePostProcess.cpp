#include <array>
#include <imgui.h>
#include "ExamplePostProcess.h"

static char render_vertex_src[] = R"(
#version 460
layout (location = 0) in vec3 aPosition;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec3 aAlbedo;
layout (location = 3) in vec2 aTextureUV;
layout (location = 0) out vec3 vPosition;
layout (location = 1) out vec3 vNormal;
layout (location = 2) out vec3 vAlbedo;
layout (location = 3) out vec2 vTextureUV;

layout (set = 0, binding = 0) uniform uFrameUBO
{
	mat4 view;
	mat4 proj;
} FrameUBO;

void main()
{
	gl_Position = FrameUBO.proj * FrameUBO.view * vec4(aPosition, 1.0);
	vNormal = aNormal;
	vAlbedo = aAlbedo;
	vTextureUV = aTextureUV;
	vPosition = aPosition;
}
)";

static char render_fragment_src[] = R"(
#version 460
layout (location = 0) in vec3 vPosition;
layout (location = 1) in vec3 vNormal;
layout (location = 2) in vec3 vAlbedo;
layout (location = 3) in vec2 vTextureUV;
layout (location = 0) out vec4 fColor;

void main()
{
	vec3 normal = normalize(vNormal);
	vec3 light_pos = vec3(0.0, 1.5, 0.0);
	vec3 light_dir = normalize(light_pos - vPosition);
	float diffuse_factor = max(dot(light_dir, normal), 0.0);
	float albedo_factor = min(0.2 + diffuse_factor, 1.0);

	fColor = vec4(albedo_factor * vAlbedo, 1.0);
}
)";

static char postprocess_vertex_src[] = R"(
#version 460
layout (location = 0) in vec2 aPosition;
layout (location = 1) in vec2 aTextureUV;
layout (location = 0) out vec2 vTextureUV;

void main()
{
	gl_Position = vec4(aPosition, 0.0f, 1.0f);
	vTextureUV = aTextureUV;
}
)";

static char none_fragment_src[] = R"(
#version 460
layout (location = 0) in vec2 vTextureUV;
layout (location = 0) out vec4 fColor;

layout (set = 0, binding = 1) uniform sampler2D uScene;

void main()
{
	fColor = vec4(texture(uScene, vTextureUV).rgb, 1.0);
}
)";

static char invert_fragment_src[] = R"(
#version 460
layout (location = 0) in vec2 vTextureUV;
layout (location = 0) out vec4 fColor;

layout (set = 0, binding = 1) uniform sampler2D uScene;

void main()
{
	vec4 color = texture(uScene, vTextureUV);
	fColor = vec4(1.0 - color.rgb, 1.0);
}
)";

static char grayscale_fragment_src[] = R"(
#version 460
layout (location = 0) in vec2 vTextureUV;
layout (location = 0) out vec4 fColor;

layout (set = 0, binding = 1) uniform sampler2D uScene;

void main()
{
	vec4 color = texture(uScene, vTextureUV);
	float luminance = 0.299 * color.r + 0.587 * color.g + 0.114 * color.b;
	fColor = vec4(vec3(luminance), 1.0);
}
)";

struct FrameUBO
{
	glm::mat4 ViewMat;
	glm::mat4 ProjMat;
};

ExamplePostProcess::ExamplePostProcess(VIBackend backend)
	: Application("Post Processing", backend)
{
	glfwSetKeyCallback(mWindow, &ExamplePostProcess::KeyCallback);

	VIPass pass = vi_device_get_swapchain_pass(mDevice);

	// layouts
	{
		mSetLayout = CreateSetLayout(mDevice, {
			{ VI_SET_BINDING_TYPE_UNIFORM_BUFFER, 0, 1 },
			{ VI_SET_BINDING_TYPE_COMBINED_IMAGE_SAMPLER, 1, 1 },
		});

		mPipelineLayout = CreatePipelineLayout(mDevice, {
			mSetLayout
		});
	}

	// scene process pass description
	{
		VkSubpassDependency render_dependency = MakeSubpassDependency(
			0,                   VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			VK_SUBPASS_EXTERNAL, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,         VK_ACCESS_SHADER_READ_BIT);

		VISubpassColorAttachment color_attachment_ref;
		color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		color_attachment_ref.index = 0;

		VISubpassDepthStencilAttachment depth_attachment_ref;
		depth_attachment_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VISubpassInfo subpass;
		subpass.color_attachment_ref_count = 1;
		subpass.color_attachment_refs = &color_attachment_ref;
		subpass.depth_stencil_attachment_ref = &depth_attachment_ref;

		VIPassColorAttachment color_attachment;
		color_attachment.color_format = VI_FORMAT_RGBA8;
		color_attachment.color_load_op = VK_ATTACHMENT_LOAD_OP_CLEAR;
		color_attachment.color_store_op = VK_ATTACHMENT_STORE_OP_STORE;
		color_attachment.initial_layout = VK_IMAGE_LAYOUT_UNDEFINED;
		color_attachment.final_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; // sampled in post process pass

		VIPassDepthStencilAttachment depth_attachment;
		depth_attachment.initial_layout = VK_IMAGE_LAYOUT_UNDEFINED;
		depth_attachment.final_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		depth_attachment.depth_stencil_format = VI_FORMAT_D32F_S8U;
		depth_attachment.depth_load_op = VK_ATTACHMENT_LOAD_OP_CLEAR;
		depth_attachment.depth_store_op = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depth_attachment.stencil_store_op = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depth_attachment.stencil_load_op = VK_ATTACHMENT_LOAD_OP_DONT_CARE;

		VIPassInfo passI;
		passI.color_attachment_count = 1;
		passI.color_attachments = &color_attachment;
		passI.depth_stencil_attachment = &depth_attachment;
		passI.depenency_count = 1;
		passI.dependencies = &render_dependency;
		passI.subpass_count = 1;
		passI.subpasses = &subpass;

		mSceneRenderPass = vi_create_pass(mDevice, &passI);
		mPostProcessPass = vi_device_get_swapchain_pass(mDevice);
	}

	mVMRender = CreateModule(mDevice, mPipelineLayout, VI_MODULE_TYPE_VERTEX, render_vertex_src);
	mFMRender = CreateModule(mDevice, mPipelineLayout, VI_MODULE_TYPE_FRAGMENT, render_fragment_src);
	mVMPostProcess = CreateModule(mDevice, mPipelineLayout, VI_MODULE_TYPE_VERTEX, postprocess_vertex_src);
	mFMGrayscale = CreateModule(mDevice, mPipelineLayout, VI_MODULE_TYPE_FRAGMENT, grayscale_fragment_src);
	mFMInvert = CreateModule(mDevice, mPipelineLayout, VI_MODULE_TYPE_FRAGMENT, invert_fragment_src);
	mFMNone = CreateModule(mDevice, mPipelineLayout, VI_MODULE_TYPE_FRAGMENT, none_fragment_src);

	VIVertexBinding vertexBinding;
	std::vector<VIVertexAttribute> vertexAttrs;
	MeshVertex::GetBindingAndAttributes(vertexBinding, vertexAttrs);

	VIVertexBinding quadVertexBinding;
	quadVertexBinding.rate = VK_VERTEX_INPUT_RATE_VERTEX;
	quadVertexBinding.stride = sizeof(float) * 4;

	std::array<VIVertexAttribute, 2> quadVertexAttrs;
	quadVertexAttrs[0].type = VI_GLSL_TYPE_VEC2; // NDC positions
	quadVertexAttrs[0].binding = 0;
	quadVertexAttrs[0].offset = 0;
	quadVertexAttrs[1].type = VI_GLSL_TYPE_VEC2; // uv
	quadVertexAttrs[1].binding = 0;
	quadVertexAttrs[1].offset = sizeof(float) * 2;

	VIPipelineInfo pipelineI;
	pipelineI.vertex_module = mVMRender;
	pipelineI.fragment_module = mFMRender;
	pipelineI.layout = mPipelineLayout;
	pipelineI.pass = mSceneRenderPass;
	pipelineI.vertex_attribute_count = vertexAttrs.size();
	pipelineI.vertex_attributes = vertexAttrs.data();
	pipelineI.vertex_binding_count = 1;
	pipelineI.vertex_bindings = &vertexBinding;
	mPipelineRender = vi_create_pipeline(mDevice, &pipelineI);

	pipelineI.vertex_module = mVMPostProcess;
	pipelineI.fragment_module = mFMGrayscale;
	pipelineI.pass = mPostProcessPass;
	pipelineI.vertex_attribute_count = quadVertexAttrs.size();
	pipelineI.vertex_attributes = quadVertexAttrs.data();
	pipelineI.vertex_binding_count = 1;
	pipelineI.vertex_bindings = &quadVertexBinding;
	mPipelineGrayscale = vi_create_pipeline(mDevice, &pipelineI);
	
	pipelineI.fragment_module = mFMInvert;
	mPipelineInvert = vi_create_pipeline(mDevice, &pipelineI);

	pipelineI.fragment_module = mFMNone;
	mPipelineNone = vi_create_pipeline(mDevice, &pipelineI);

	float quadVertices[] = {
		 -1.0f,  1.0f, 0.0f, 1.0f, // top left
		 -1.0f, -1.0f, 0.0f, 0.0f, // bottom left
		  1.0f, -1.0f, 1.0f, 0.0f, // bottom right
		  1.0f,  1.0f, 1.0f, 1.0f, // top right
	};
	VIBufferInfo vboI;
	vboI.type = VI_BUFFER_TYPE_VERTEX;
	vboI.usage = VI_BUFFER_USAGE_TRANSFER_DST_BIT;
	vboI.properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
	vboI.size = sizeof(quadVertices);
	mQuadVBO = CreateBufferStaged(mDevice, &vboI, quadVertices);

	uint32_t indices[] = { 0, 1, 2, 2, 3, 0 };

	VIBufferInfo iboI;
	iboI.type = VI_BUFFER_TYPE_INDEX;
	iboI.usage = VI_BUFFER_USAGE_TRANSFER_DST_BIT;
	iboI.properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
	iboI.size = sizeof(indices);
	mQuadIBO = CreateBufferStaged(mDevice, &iboI, indices);

	std::array<VISetPoolResource, 2> resources;
	resources[0].type = VI_SET_BINDING_TYPE_COMBINED_IMAGE_SAMPLER;
	resources[0].count = mFramesInFlight;
	resources[1].type = VI_SET_BINDING_TYPE_UNIFORM_BUFFER;
	resources[1].count = mFramesInFlight;

	VISetPoolInfo poolI;
	poolI.max_set_count = mFramesInFlight;
	poolI.resource_count = resources.size();
	poolI.resources = resources.data();
	mSetPool = vi_create_set_pool(mDevice, &poolI);

	uint32_t family = vi_device_get_graphics_family_index(mDevice);
	mCmdPool = vi_create_command_pool(mDevice, family, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

	mFrames.resize(mFramesInFlight);
	for (size_t i = 0; i < mFrames.size(); i++)
	{
		VIImageInfo imageI;
		imageI.type = VI_IMAGE_TYPE_2D;
		imageI.usage = VI_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VI_IMAGE_USAGE_SAMPLED_BIT;
		imageI.properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		imageI.format = VI_FORMAT_RGBA8;
		imageI.width = APP_WINDOW_WIDTH;
		imageI.height = APP_WINDOW_HEIGHT;
		imageI.layers = 1;
		imageI.sampler_address_mode = VI_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		imageI.sampler_filter = VI_FILTER_LINEAR;
		mFrames[i].scene_image = vi_create_image(mDevice, &imageI);

		imageI.usage = VI_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		imageI.format = VI_FORMAT_D32F_S8U; // check support ???
		mFrames[i].scene_depth = vi_create_image(mDevice, &imageI);

		VIBufferInfo bufferI;
		bufferI.type = VI_BUFFER_TYPE_UNIFORM;
		bufferI.size = sizeof(FrameUBO);
		bufferI.properties = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
		mFrames[i].scene_ubo = vi_create_buffer(mDevice, &bufferI);
		vi_buffer_map(mFrames[i].scene_ubo);
		
		VIFramebufferInfo fbI;
		fbI.color_attachment_count = 1;
		fbI.color_attachments = &mFrames[i].scene_image;
		fbI.depth_stencil_attachment = mFrames[i].scene_depth;
		fbI.width = APP_WINDOW_WIDTH;
		fbI.height = APP_WINDOW_HEIGHT;
		fbI.pass = mSceneRenderPass;

		mFrames[i].fbo = vi_create_framebuffer(mDevice, &fbI);
		mFrames[i].cmd = vi_alloc_command(mDevice, mCmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
		mFrames[i].set = AllocAndUpdateSet(mDevice, mSetPool, mSetLayout, {
			{ 0, mFrames[i].scene_ubo, VI_NULL },
			{ 1, VI_NULL, mFrames[i].scene_image },
		});
	}

	mMeshes = GenerateMeshSceneV1(mDevice);
}

ExamplePostProcess::~ExamplePostProcess()
{
	vi_device_wait_idle(mDevice);

	mMeshes.clear();

	for (size_t i = 0; i < mFrames.size(); i++)
	{
		vi_destroy_framebuffer(mDevice, mFrames[i].fbo);
		vi_buffer_unmap(mFrames[i].scene_ubo);
		vi_destroy_buffer(mDevice, mFrames[i].scene_ubo);
		vi_destroy_image(mDevice, mFrames[i].scene_image);
		vi_destroy_image(mDevice, mFrames[i].scene_depth);
		vi_free_command(mDevice, mFrames[i].cmd);
		vi_free_set(mDevice, mFrames[i].set);
	}

	vi_destroy_command_pool(mDevice, mCmdPool);
	vi_destroy_set_pool(mDevice, mSetPool);
	vi_destroy_buffer(mDevice, mQuadVBO);
	vi_destroy_buffer(mDevice, mQuadIBO);
	vi_destroy_pipeline_layout(mDevice, mPipelineLayout);
	vi_destroy_pipeline(mDevice, mPipelineInvert);
	vi_destroy_pipeline(mDevice, mPipelineGrayscale);
	vi_destroy_pipeline(mDevice, mPipelineNone);
	vi_destroy_pipeline(mDevice, mPipelineRender);
	vi_destroy_set_layout(mDevice, mSetLayout);
	vi_destroy_pass(mDevice, mSceneRenderPass);
	vi_destroy_module(mDevice, mVMRender);
	vi_destroy_module(mDevice, mFMRender);
	vi_destroy_module(mDevice, mFMInvert);
	vi_destroy_module(mDevice, mFMNone);
	vi_destroy_module(mDevice, mFMGrayscale);
	vi_destroy_module(mDevice, mVMPostProcess);
}

void ExamplePostProcess::Run()
{
	mCamera.SetPosition({ -5.0f, 1.0f, 0.0f });
	mConfig.postprocess_pipeline = mPipelineNone;

	while (!glfwWindowShouldClose(mWindow))
	{
		Application::NewFrame();
		Application::ImGuiNewFrame();
		Application::CameraUpdate();

		if (!CameraIsCaptured())
		{
			ImGui::Begin(mName);
			ImGui::Text("Select a postprocess pipeline:");
			if (ImGui::Button("No Effect"))
				mConfig.postprocess_pipeline = mPipelineNone;
			if (ImGui::Button("Grayscale"))
				mConfig.postprocess_pipeline = mPipelineGrayscale;
			if (ImGui::Button("Invert"))
				mConfig.postprocess_pipeline = mPipelineInvert;
			ImGui::End();
		}

		VISemaphore image_acquired;
		VISemaphore present_ready;
		VIFence frame_complete;
		uint32_t frame_idx = vi_device_next_frame(mDevice, &image_acquired, &present_ready, &frame_complete);
		VIFramebuffer swapchain_fb = vi_device_get_swapchain_framebuffer(mDevice, frame_idx);

		FrameData* frame = mFrames.data() + frame_idx;

		FrameUBO ubo;
		ubo.ViewMat = mCamera.GetViewMat();
		ubo.ProjMat = mCamera.GetProjMat();
		vi_buffer_map_write(frame->scene_ubo, 0, sizeof(ubo), &ubo);

		vi_reset_command(frame->cmd);
		vi_begin_command(frame->cmd, 0);

		VkClearValue clear = MakeClearColor(0.1f, 0.7f, 0.7f, 1.0f);
		VkClearValue clear_depth = MakeClearDepthStencil(1.0f, 0);
		VIPassBeginInfo beginI;
		beginI.pass = mSceneRenderPass;
		beginI.framebuffer = frame->fbo;
		beginI.color_clear_values = &clear;
		beginI.color_clear_value_count = 1;
		beginI.depth_stencil_clear_value = &clear_depth;

		vi_cmd_begin_pass(frame->cmd, &beginI);
		{
			vi_cmd_bind_pipeline(frame->cmd, mPipelineRender);
			vi_cmd_set_viewport(frame->cmd, MakeViewport(APP_WINDOW_WIDTH, APP_WINDOW_HEIGHT));
			vi_cmd_set_scissor(frame->cmd, MakeScissor(APP_WINDOW_WIDTH, APP_WINDOW_HEIGHT));

			vi_cmd_bind_set(frame->cmd, 0, frame->set, mPipelineRender);

			for (std::shared_ptr<Mesh>& mesh : mMeshes)
			{
				vi_cmd_bind_vertex_buffers(frame->cmd, 0, 1, &mesh->VBO);
				vi_cmd_bind_index_buffer(frame->cmd, mesh->IBO, VK_INDEX_TYPE_UINT32);
			
				VIDrawIndexedInfo info;
				info.index_count = mesh->IndexCount;
				info.index_start = 0;
				info.instance_count = 1;
				info.instance_start = 0;
				vi_cmd_draw_indexed(frame->cmd, &info);
			}
		}
		vi_cmd_end_pass(frame->cmd);
		
		beginI.pass = mPostProcessPass;
		beginI.framebuffer = swapchain_fb;
		beginI.color_clear_values = &clear;
		beginI.color_clear_value_count = 1;
		beginI.depth_stencil_clear_value = &clear_depth;
		vi_cmd_begin_pass(frame->cmd, &beginI);
		{
			vi_cmd_bind_pipeline(frame->cmd, mConfig.postprocess_pipeline);
			vi_cmd_set_viewport(frame->cmd, MakeViewport(APP_WINDOW_WIDTH, APP_WINDOW_HEIGHT));
			vi_cmd_set_scissor(frame->cmd, MakeScissor(APP_WINDOW_WIDTH, APP_WINDOW_HEIGHT));

			vi_cmd_bind_vertex_buffers(frame->cmd, 0, 1, &mQuadVBO);
			vi_cmd_bind_index_buffer(frame->cmd, mQuadIBO, VK_INDEX_TYPE_UINT32);
			vi_cmd_bind_set(frame->cmd, 0, frame->set, mPipelineGrayscale);

			VIDrawIndexedInfo info;
			info.index_count = 6;
			info.index_start = 0;
			info.instance_count = 1;
			info.instance_start = 0;
			vi_cmd_draw_indexed(frame->cmd, &info);

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
}

void ExamplePostProcess::KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	ExamplePostProcess* example = (ExamplePostProcess*)Application::Get();

	if (action != GLFW_PRESS)
		return;

	switch (key)
	{
	case GLFW_KEY_ESCAPE:
		example->CameraToggleCapture();
		break;
	default:
		break;
	}
}
