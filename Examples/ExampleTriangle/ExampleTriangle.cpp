#include <vise.h>
#include <cstring>
#include <array>
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <cstdlib>
#include "ExampleTriangle.h"

static char vertex_src[] = R"(
#version 460

layout (location = 0) in vec2 aPos;
layout (location = 1) in vec3 aColor;
layout (location = 0) out vec4 vColor;

void main()
{
	gl_Position = vec4(aPos, 0.0, 1.0);
	vColor = vec4(aColor, 1.0);
}
)";

static char fragment_src[] = R"(
#version 460

layout (location = 0) in vec4 vColor;
layout (location = 0) out vec4 fColor;

void main()
{
	fColor = vColor;
}
)";

ExampleTriangle::ExampleTriangle(VIBackend backend)
	: Application("Triangle", backend)
{
	VIPass pass = vi_device_get_swapchain_pass(mDevice);

	VIPipelineLayoutInfo pipelineLayoutI;
	pipelineLayoutI.set_layout_count = 0;
	pipelineLayoutI.push_constant_size = 0;
	mPipelineLayout = vi_create_pipeline_layout(mDevice, &pipelineLayoutI);

	VIModuleInfo moduleInfo;
	moduleInfo.type = VI_MODULE_TYPE_VERTEX;
	moduleInfo.pipeline_layout = mPipelineLayout;
	moduleInfo.vise_glsl = vertex_src;
	mVertexModule = vi_create_module(mDevice, &moduleInfo);

	moduleInfo.type = VI_MODULE_TYPE_FRAGMENT;
	moduleInfo.pipeline_layout = mPipelineLayout;
	moduleInfo.vise_glsl = fragment_src;
	mFragmentModule = vi_create_module(mDevice, &moduleInfo);

	VIVertexBinding vertex_binding;
	vertex_binding.rate = VK_VERTEX_INPUT_RATE_VERTEX;
	vertex_binding.stride = sizeof(float) * 5;

	std::array<VIVertexAttribute, 2> vertex_attrs;
	vertex_attrs[0].type = VI_GLSL_TYPE_VEC2; // NDC position
	vertex_attrs[0].binding = 0;
	vertex_attrs[0].offset = 0;
	vertex_attrs[1].type = VI_GLSL_TYPE_VEC3; // colors
	vertex_attrs[1].binding = 0;
	vertex_attrs[1].offset = sizeof(float) * 2;

	std::array<VIModule, 2> modules;
	modules[0] = mVertexModule;
	modules[1] = mFragmentModule;

	VIPipelineInfo pipelineI;
	pipelineI.pass = pass;
	pipelineI.layout = mPipelineLayout;
	pipelineI.module_count = modules.size();
	pipelineI.modules = modules.data();
	pipelineI.primitive_topology = VI_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	pipelineI.vertex_attribute_count = vertex_attrs.size();
	pipelineI.vertex_attributes = vertex_attrs.data();
	pipelineI.vertex_binding_count = 1;
	pipelineI.vertex_bindings = &vertex_binding;
	mPipeline = vi_create_pipeline(mDevice, &pipelineI);

	// XY position in NDC + RGB colors
	float vertices[] = {
		0.0f,   0.5f, 0.0f, 1.0f, 1.0f,
		-0.5f, -0.5f, 1.0f, 0.0f, 1.0f,
		0.5f,  -0.5f, 1.0f, 1.0f, 0.0f,
	};

	VIBufferInfo vboI;
	vboI.type = VI_BUFFER_TYPE_VERTEX;
	vboI.usage = VI_BUFFER_USAGE_TRANSFER_DST_BIT;
	vboI.properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
	vboI.size = sizeof(vertices);
	mVBO = CreateBufferStaged(mDevice, &vboI, vertices);

	uint32_t family = vi_device_get_graphics_family_index(mDevice);
	mCmdPool = vi_create_command_pool(mDevice, family, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

	mCommands.resize(mFramesInFlight);
	for (size_t i = 0; i < mFramesInFlight; i++)
	{
		mCommands[i] = vi_allocate_primary_command(mDevice, mCmdPool);
	}

	RecordCommands();
}

ExampleTriangle::~ExampleTriangle()
{
	vi_device_wait_idle(mDevice);

	for (size_t i = 0; i < mCommands.size(); i++)
	{
		vi_free_command(mDevice, mCommands[i]);
	}

	vi_destroy_command_pool(mDevice, mCmdPool);
	vi_destroy_buffer(mDevice, mVBO);
	vi_destroy_pipeline_layout(mDevice, mPipelineLayout);
	vi_destroy_pipeline(mDevice, mPipeline);
	vi_destroy_module(mDevice, mVertexModule);
	vi_destroy_module(mDevice, mFragmentModule);
}


// Commands don't differ between frames, record once and for all.
// The i'th command buffer renders to the i'th swapchain framebuffer.
void ExampleTriangle::RecordCommands()
{
	for (uint32_t i = 0; i < mCommands.size(); i++)
	{
		VICommand cmd = mCommands[i];

		vi_command_begin(cmd, 0, nullptr);

		VkClearValue color_clear = MakeClearColor(0.0f, 0.0f, 0.4f, 1.0f);
		VkClearValue depth_clear = MakeClearDepthStencil(1.0f, 0);
		VIPassBeginInfo beginI;
		beginI.pass = vi_device_get_swapchain_pass(mDevice);
		beginI.framebuffer = vi_device_get_swapchain_framebuffer(mDevice, i);
		beginI.color_clear_values = &color_clear;
		beginI.color_clear_value_count = 1;
		beginI.depth_stencil_clear_value = &depth_clear;

		vi_cmd_begin_pass(cmd, &beginI);
		{
			vi_cmd_bind_graphics_pipeline(cmd, mPipeline);
			vi_cmd_set_viewport(cmd, MakeViewport(APP_WINDOW_WIDTH, APP_WINDOW_HEIGHT));
			vi_cmd_set_scissor(cmd, MakeScissor(APP_WINDOW_WIDTH, APP_WINDOW_HEIGHT));

			vi_cmd_bind_vertex_buffers(cmd, 0, 1, &mVBO);

			VIDrawInfo info;
			info.vertex_count = 3;
			info.vertex_start = 0;
			info.instance_count = 1;
			info.instance_start = 0;
			vi_cmd_draw(cmd, &info);
		}
		vi_cmd_end_pass(cmd);
		vi_command_end(cmd);
	}
}

void ExampleTriangle::Run()
{
	while (!glfwWindowShouldClose(mWindow))
	{
		Application::NewFrame();

		VISemaphore image_acquired;
		VISemaphore present_ready;
		VIFence frame_complete;

		uint32_t index = vi_device_next_frame(mDevice, &image_acquired, &present_ready, &frame_complete);
		VICommand cmd = mCommands[index];

		// bare minimum synchronization:
		// - first render command must wait for image_acquired semaphore
		// - presentation engine (vi_device_present_frame) waits for the present_ready semaphore to be signaled
		// - must signal frame_complete fence so that vi_device_next_frame does not block after FRAMES_IN_FLIGHT frames
		VkPipelineStageFlags stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		VISubmitInfo submitI;
		submitI.wait_count = 1;
		submitI.wait_stages = &stage;
		submitI.waits = &image_acquired;
		submitI.signal_count = 1;
		submitI.signals = &present_ready;
		submitI.cmd_count = 1;
		submitI.cmds = &cmd;

		VIQueue graphics_queue = vi_device_get_graphics_queue(mDevice);
		vi_queue_submit(graphics_queue, 1, &submitI, frame_complete);

		vi_device_present_frame(mDevice);
	}
}
