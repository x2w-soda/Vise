#include <vise.h>
#include <array>
#include <cstring>
#include <cstdlib>
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "ExamplePyramid.h"

ExamplePyramid* ExamplePyramid::sInstance = nullptr;

static char vertex_src[] = R"(
#version 460

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aColor;
layout (location = 0) out vec3 vColor;

layout (set = 0, binding = 0) uniform uFrameUBO
{
	mat4 view;
	mat4 proj;
} FrameUBO;

void main()
{
	gl_Position = FrameUBO.proj * FrameUBO.view * vec4(aPos, 1.0);
	vColor = aColor;
}

)";
static char fragment_src[] = R"(
#version 460

layout (location = 0) in vec3 vColor;
layout (location = 0) out vec4 fColor;

void main()
{
	fColor = vec4(vColor, 1.0);
}
)";

ExamplePyramid::ExamplePyramid(VIBackend backend)
	: Application("Example Pyramid", backend)
{
	sInstance = this;

	VIPass pass = vi_device_get_swapchain_pass(mDevice);

	// layouts
	{
		std::vector<VISetBinding> bindings(1);
		bindings[0].array_count = 1;
		bindings[0].type = VI_SET_BINDING_TYPE_UNIFORM_BUFFER;
		bindings[0].idx = 0;

		VISetLayoutInfo layout_info;
		layout_info.bindings = bindings.data();
		layout_info.binding_count = bindings.size();
		mSetLayout = vi_create_set_layout(mDevice, &layout_info);

		VIPipelineLayoutInfo pipelineLayoutI;
		pipelineLayoutI.push_constant_size = 0;
		pipelineLayoutI.set_layout_count = 1;
		pipelineLayoutI.set_layouts = &mSetLayout;
		mPipelineLayout = vi_create_pipeline_layout(mDevice, &pipelineLayoutI);
	}

	VIModuleInfo moduleInfo;
	moduleInfo.type = VI_MODULE_TYPE_VERTEX;
	moduleInfo.pipeline_layout = mPipelineLayout;
	moduleInfo.vise_glsl = vertex_src;
	mVertexModule = vi_create_module(mDevice, &moduleInfo);

	moduleInfo.type = VI_MODULE_TYPE_FRAGMENT;
	moduleInfo.pipeline_layout = mPipelineLayout;
	moduleInfo.vise_glsl = fragment_src;
	mFragmentModule = vi_create_module(mDevice, &moduleInfo);

	VIVertexBinding vertexBinding;
	vertexBinding.rate = VK_VERTEX_INPUT_RATE_VERTEX;
	vertexBinding.stride = sizeof(float) * 6;

	std::array<VIVertexAttribute, 2> vertexAttrs;
	vertexAttrs[0].type = VI_GLSL_TYPE_VEC3; // world positions
	vertexAttrs[0].binding = 0;
	vertexAttrs[0].offset = 0;
	vertexAttrs[1].type = VI_GLSL_TYPE_VEC3; // colors
	vertexAttrs[1].binding = 0;
	vertexAttrs[1].offset = sizeof(float) * 3;

	VIPipelineInfo pipelineI;
	pipelineI.vertex_module = mVertexModule;
	pipelineI.fragment_module = mFragmentModule;
	pipelineI.layout = mPipelineLayout;
	pipelineI.pass = pass;
	pipelineI.vertex_attribute_count = vertexAttrs.size();
	pipelineI.vertex_attributes = vertexAttrs.data();
	pipelineI.vertex_binding_count = 1;
	pipelineI.vertex_bindings = &vertexBinding;
	mPipeline = vi_create_pipeline(mDevice, &pipelineI);

	float vertices[] = {
		 0.0f,  0.5f, 0.0f, 1.0f, 1.0f, 1.0f,
		-0.5f, -0.5f, 0.5f, 1.0f, 0.0f, 1.0f,
		 0.5f, -0.5f, 0.5f, 0.0f, 1.0f, 1.0f,
		 0.5f, -0.5f,-0.5f, 0.0f, 0.0f, 0.0f,
		-0.5f, -0.5f,-0.5f, 1.0f, 1.0f, 0.0f,
	};

	VIBufferInfo vboI;
	vboI.type = VI_BUFFER_TYPE_VERTEX;
	vboI.usage = VI_BUFFER_USAGE_TRANSFER_DST_BIT;
	vboI.properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
	vboI.size = sizeof(vertices);
	mVBO = CreateBufferStaged(mDevice, &vboI, vertices);

	uint32_t indices[] = { 0, 1, 2, 0, 2, 3, 0, 3, 4, 0, 4, 1 };

	VIBufferInfo iboI;
	iboI.type = VI_BUFFER_TYPE_INDEX;
	iboI.usage = VI_BUFFER_USAGE_TRANSFER_DST_BIT;
	iboI.properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
	iboI.size = sizeof(indices);
	mIBO = CreateBufferStaged(mDevice, &iboI, indices);

	std::array<VISetPoolResource, 1> resources;
	resources[0].type = VI_SET_BINDING_TYPE_UNIFORM_BUFFER;
	resources[0].count = mFramesInFlight;

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
		VIBufferInfo uboI;
		uboI.properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
		uboI.size = sizeof(FrameUBO);
		uboI.type = VI_BUFFER_TYPE_UNIFORM;
		uboI.usage = 0;
		mFrames[i].ubo = vi_create_buffer(mDevice, &uboI);
		mFrames[i].set = vi_alloc_set(mDevice, mSetPool, mSetLayout);
		mFrames[i].cmd = vi_alloc_command(mDevice, mCmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
		vi_buffer_map(mFrames[i].ubo);

		VISetUpdateInfo updateI;
		updateI.binding = 0;
		updateI.image = nullptr;
		updateI.buffer = mFrames[i].ubo;
		vi_set_update(mFrames[i].set, 1, &updateI);
	}

	glfwSetKeyCallback(mWindow, &ExamplePyramid::KeyCallback);
	glfwSetCursorPosCallback(mWindow, &ExamplePyramid::CursorPosCallback);
}

ExamplePyramid::~ExamplePyramid()
{
	vi_device_wait_idle(mDevice);

	for (size_t i = 0; i < mFrames.size(); i++)
	{
		vi_free_command(mDevice, mFrames[i].cmd);
		vi_free_set(mDevice, mFrames[i].set);
		vi_buffer_unmap(mFrames[i].ubo);
		vi_destroy_buffer(mDevice, mFrames[i].ubo);
	}

	vi_destroy_command_pool(mDevice, mCmdPool);
	vi_destroy_set_pool(mDevice, mSetPool);
	vi_destroy_buffer(mDevice, mIBO);
	vi_destroy_buffer(mDevice, mVBO);
	vi_destroy_pipeline(mDevice, mPipeline);
	vi_destroy_pipeline_layout(mDevice, mPipelineLayout);
	vi_destroy_set_layout(mDevice, mSetLayout);
	vi_destroy_module(mDevice, mVertexModule);
	vi_destroy_module(mDevice, mFragmentModule);
}

void ExamplePyramid::Run()
{
	mCamera.SetPosition({ -3.0f, 0.0f, 0.0f });

	while (!glfwWindowShouldClose(mWindow))
	{
		Application::NewFrame();
		Application::CameraUpdate();

		VISemaphore image_acquired;
		VISemaphore present_ready;
		VIFence frame_complete;
		uint32_t frame_idx = vi_device_next_frame(mDevice, &image_acquired, &present_ready, &frame_complete);
		VIPass pass = vi_device_get_swapchain_pass(mDevice);
		VIFramebuffer fb = vi_device_get_swapchain_framebuffer(mDevice, frame_idx);

		FrameData* frame = mFrames.data() + frame_idx;

		FrameUBO uboData;
		uboData.view = mCamera.GetViewMat();
		uboData.proj = mCamera.GetProjMat();
		vi_buffer_map_write(frame->ubo, 0, sizeof(uboData), &uboData);

		vi_reset_command(frame->cmd);

		VkClearValue color_clear = MakeClearColor(0.1f, 0.1f, 0.1f, 1.0f);
		VkClearValue depth_clear = MakeClearDepthStencil(1.0f, 0);
		VIPassBeginInfo beginI;
		beginI.pass = pass;
		beginI.framebuffer = fb;
		beginI.color_clear_values = &color_clear;
		beginI.color_clear_value_count = 1;
		beginI.depth_stencil_clear_value = &depth_clear;

		vi_begin_command(frame->cmd, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
		vi_cmd_begin_pass(frame->cmd, &beginI);
		{
			vi_cmd_bind_graphics_pipeline(frame->cmd, mPipeline);
			vi_cmd_set_viewport(frame->cmd, MakeViewport(APP_WINDOW_WIDTH, APP_WINDOW_HEIGHT));
			vi_cmd_set_scissor(frame->cmd, MakeScissor(APP_WINDOW_WIDTH, APP_WINDOW_HEIGHT));

			vi_cmd_bind_graphics_set(frame->cmd, mPipelineLayout, 0, frame->set);
			vi_cmd_bind_vertex_buffers(frame->cmd, 0, 1, &mVBO);
			vi_cmd_bind_index_buffer(frame->cmd, mIBO, VK_INDEX_TYPE_UINT32);

			VIDrawIndexedInfo info;
			info.index_count = 12;
			info.index_start = 0;
			info.instance_count = 1;
			info.instance_start = 0;
			vi_cmd_draw_indexed(frame->cmd, &info);
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

void ExamplePyramid::KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	ExamplePyramid* example = ExamplePyramid::Get();

	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
	{
		example->CameraToggleCapture();
		puts("OK");
	}
}

void ExamplePyramid::CursorPosCallback(GLFWwindow* window, double xpos, double ypos)
{
	ExamplePyramid* example = ExamplePyramid::Get();

	static double xpos_prev;
	static double ypos_prev;
	static bool s_first_frame = true;

	if (s_first_frame)
	{
		s_first_frame = false;
		xpos_prev = xpos;
		ypos_prev = ypos;
	}

	float xoffset = xpos - xpos_prev;
	float yoffset = ypos - ypos_prev;
	xpos_prev = xpos;
	ypos_prev = ypos;

	const float sensitivity = 0.1f;
	xoffset *= sensitivity;
	yoffset *= sensitivity;

	if (example->mIsCameraCaptured)
	{
		example->mCamera.RotateLocal(-yoffset, xoffset);
	}
}
