#include <array>
#include "TestPipelineBlend.h"

const char triangle_vertex_src[] = R"(
#version 460

const float vertices[6] = {
     0.0,  0.5, // top center
    -0.5, -0.5, // bottom left
     0.5, -0.5, // bottom right
};

layout (push_constant) uniform uPC
{
	vec4 ndc_offset;
	vec4 color;
} PC;

void main()
{
	vec2 pos;
	pos.x = vertices[gl_VertexIndex * 2];
	pos.y = vertices[gl_VertexIndex * 2 + 1];
	pos += PC.ndc_offset.xy;
	gl_Position = vec4(pos, 0.0, 1.0);
}
)";

const char triangle_fragment_src[] = R"(
#version 460

layout (location = 0) out vec4 fColor;

layout (push_constant) uniform uPC
{
	vec4 ndc_offset;
	vec4 color;
} PC;

void main()
{
	fColor = PC.color;
}
)";

TestPipelineBlend::TestPipelineBlend(VIBackend backend)
	: TestApplication("TestPipelineBlend", backend)
{
	VIPipelineLayoutInfo pipelineLayoutI;
	pipelineLayoutI.push_constant_size = 32;
	pipelineLayoutI.set_layout_count = 0;
	mTestPipelineLayout = vi_create_pipeline_layout(mDevice, &pipelineLayoutI);

	VIModuleInfo moduleI;
	moduleI.pipeline_layout = mTestPipelineLayout;
	moduleI.type = VI_MODULE_TYPE_VERTEX;
	moduleI.vise_glsl = triangle_vertex_src;
	mTestVM = vi_create_module(mDevice, &moduleI);

	moduleI.type = VI_MODULE_TYPE_FRAGMENT;
	moduleI.vise_glsl = triangle_fragment_src;
	mTestFM = vi_create_module(mDevice, &moduleI);

	std::array<VIModule, 2> modules;
	modules[0] = mTestVM;
	modules[1] = mTestFM;

	// with blending disabled, the alpha channel is ignored and the last drawn color is preserved
	VIPipelineInfo pipelineI;
	pipelineI.layout = mTestPipelineLayout;
	pipelineI.vertex_attribute_count = 0;
	pipelineI.vertex_binding_count = 0;
	pipelineI.module_count = modules.size();
	pipelineI.modules = modules.data();
	pipelineI.pass = mScreenshotPass;
	pipelineI.blend_state.enabled = false;
	mPipelineBlendDisabled = vi_create_pipeline(mDevice, &pipelineI);

	// the "default" blending is usually how we expect alpha to behave, mixing src and dst color linearly
	pipelineI.blend_state.enabled = true;
	pipelineI.blend_state.src_alpha_factor = VI_BLEND_FACTOR_ONE;
	pipelineI.blend_state.dst_alpha_factor = VI_BLEND_FACTOR_ZERO;
	pipelineI.blend_state.alpha_blend_op = VI_BLEND_OP_ADD;
	pipelineI.blend_state.src_color_factor = VI_BLEND_FACTOR_SRC_ALPHA;
	pipelineI.blend_state.dst_color_factor = VI_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	pipelineI.blend_state.color_blend_op = VI_BLEND_OP_ADD;
	mPipelineBlendDefault = vi_create_pipeline(mDevice, &pipelineI);

	// additive color blending, src alpha controls how much src color is accumulated
	pipelineI.blend_state.enabled = true;
	pipelineI.blend_state.src_alpha_factor = VI_BLEND_FACTOR_ONE;
	pipelineI.blend_state.dst_alpha_factor = VI_BLEND_FACTOR_ZERO;
	pipelineI.blend_state.alpha_blend_op = VI_BLEND_OP_ADD;
	pipelineI.blend_state.src_color_factor = VI_BLEND_FACTOR_SRC_ALPHA;
	pipelineI.blend_state.dst_color_factor = VI_BLEND_FACTOR_ONE;
	pipelineI.blend_state.color_blend_op = VI_BLEND_OP_ADD;
	mPipelineBlendColorAdd = vi_create_pipeline(mDevice, &pipelineI);

	// select max color value, alpha channel not used in blending
	pipelineI.blend_state.enabled = true;
	pipelineI.blend_state.src_alpha_factor = VI_BLEND_FACTOR_ONE;
	pipelineI.blend_state.dst_alpha_factor = VI_BLEND_FACTOR_ZERO;
	pipelineI.blend_state.alpha_blend_op = VI_BLEND_OP_ADD;
	pipelineI.blend_state.src_color_factor = VI_BLEND_FACTOR_ONE;
	pipelineI.blend_state.dst_color_factor = VI_BLEND_FACTOR_ONE;
	pipelineI.blend_state.color_blend_op = VI_BLEND_OP_MAX;
	mPipelineBlendColorMax = vi_create_pipeline(mDevice, &pipelineI);

	uint32_t graphics_family = vi_device_get_graphics_family_index(mDevice);
	mCmdPool = vi_create_command_pool(mDevice, graphics_family, 0);
}

TestPipelineBlend::~TestPipelineBlend()
{
	vi_device_wait_idle(mDevice);

	vi_destroy_command_pool(mDevice, mCmdPool);
	vi_destroy_pipeline(mDevice, mPipelineBlendColorMax);
	vi_destroy_pipeline(mDevice, mPipelineBlendColorAdd);
	vi_destroy_pipeline(mDevice, mPipelineBlendDisabled);
	vi_destroy_pipeline(mDevice, mPipelineBlendDefault);
	vi_destroy_module(mDevice, mTestFM);
	vi_destroy_module(mDevice, mTestVM);
	vi_destroy_pipeline_layout(mDevice, mTestPipelineLayout);
}

void TestPipelineBlend::Run()
{
	VICommand cmd = vi_allocate_primary_command(mDevice, mCmdPool);
	vi_command_begin(cmd, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, nullptr);

	VkClearValue clear_color = MakeClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	VIPassBeginInfo passBI;
	passBI.color_clear_value_count = 1;
	passBI.color_clear_values = &clear_color;
	passBI.depth_stencil_clear_value = nullptr;
	passBI.framebuffer = mScreenshotFBO;
	passBI.pass = mScreenshotPass;
	vi_cmd_begin_pass(cmd, &passBI);

	VIDrawInfo drawI;
	drawI.instance_count = 1;
	drawI.instance_start = 0;
	drawI.vertex_count = 3;
	drawI.vertex_start = 0;

	struct PC
	{
		glm::vec4 ndc_offset;
		glm::vec4 color;
	} pc;

	const glm::vec4 opaque_r(1.0f, 0.0f, 0.0f, 1.0f);
	const glm::vec4 opaque_g(0.0f, 1.0f, 0.0f, 1.0f);
	const glm::vec4 opaque_b(0.0f, 0.0f, 1.0f, 1.0f);
	const glm::vec4 transparent_r(1.0f, 0.0f, 0.0f, 0.5f);
	const glm::vec4 transparent_g(0.0f, 1.0f, 0.0f, 0.5f);
	const glm::vec4 transparent_b(0.0f, 0.0f, 1.0f, 0.5f);

	{
		vi_cmd_bind_graphics_pipeline(cmd, mPipelineBlendDisabled);
		vi_cmd_set_viewport(cmd, MakeViewport(TEST_WINDOW_WIDTH, TEST_WINDOW_HEIGHT));
		vi_cmd_set_scissor(cmd, MakeScissor(TEST_WINDOW_WIDTH, TEST_WINDOW_HEIGHT));

		pc.ndc_offset.x = -0.5f;
		pc.ndc_offset.y = 0.5f;
		pc.color = opaque_r;
		vi_cmd_push_constants(cmd, mTestPipelineLayout, 0, sizeof(pc), &pc);
		vi_cmd_draw(cmd, &drawI);

		pc.color = transparent_g;
		vi_cmd_push_constants(cmd, mTestPipelineLayout, 0, sizeof(pc), &pc);
		vi_cmd_draw(cmd, &drawI);
	}
	{
		vi_cmd_bind_graphics_pipeline(cmd, mPipelineBlendDefault);
		vi_cmd_set_viewport(cmd, MakeViewport(TEST_WINDOW_WIDTH, TEST_WINDOW_HEIGHT));
		vi_cmd_set_scissor(cmd, MakeScissor(TEST_WINDOW_WIDTH, TEST_WINDOW_HEIGHT));

		pc.ndc_offset.x = 0.5f;
		pc.ndc_offset.y = 0.5f;
		pc.color = opaque_r;
		vi_cmd_push_constants(cmd, mTestPipelineLayout, 0, sizeof(pc), &pc);
		vi_cmd_draw(cmd, &drawI);

		pc.color = transparent_g;
		vi_cmd_push_constants(cmd, mTestPipelineLayout, 0, sizeof(pc), &pc);
		vi_cmd_draw(cmd, &drawI);
	}
	{
		vi_cmd_bind_graphics_pipeline(cmd, mPipelineBlendColorAdd);
		vi_cmd_set_viewport(cmd, MakeViewport(TEST_WINDOW_WIDTH, TEST_WINDOW_HEIGHT));
		vi_cmd_set_scissor(cmd, MakeScissor(TEST_WINDOW_WIDTH, TEST_WINDOW_HEIGHT));

		pc.ndc_offset.x = -0.5f;
		pc.ndc_offset.y = -0.5f;
		pc.color = opaque_r;
		vi_cmd_push_constants(cmd, mTestPipelineLayout, 0, sizeof(pc), &pc);
		vi_cmd_draw(cmd, &drawI);

		pc.color = opaque_g;
		vi_cmd_push_constants(cmd, mTestPipelineLayout, 0, sizeof(pc), &pc);
		vi_cmd_draw(cmd, &drawI);
	}
	{
		vi_cmd_bind_graphics_pipeline(cmd, mPipelineBlendColorMax);
		vi_cmd_set_viewport(cmd, MakeViewport(TEST_WINDOW_WIDTH, TEST_WINDOW_HEIGHT));
		vi_cmd_set_scissor(cmd, MakeScissor(TEST_WINDOW_WIDTH, TEST_WINDOW_HEIGHT));

		pc.ndc_offset.x = 0.5f;
		pc.ndc_offset.y = -0.5f;
		pc.color = glm::vec4(0.5f, 0.2f, 0.5f, 1.0f);
		vi_cmd_push_constants(cmd, mTestPipelineLayout, 0, sizeof(pc), &pc);
		vi_cmd_draw(cmd, &drawI);

		pc.color = glm::vec4(0.2f, 0.5f, 0.2f, 1.0f);
		vi_cmd_push_constants(cmd, mTestPipelineLayout, 0, sizeof(pc), &pc);
		vi_cmd_draw(cmd, &drawI);
	}
	vi_cmd_end_pass(cmd);

	VkBufferImageCopy region = MakeBufferImageCopy2D(VK_IMAGE_ASPECT_COLOR_BIT, TEST_WINDOW_WIDTH, TEST_WINDOW_HEIGHT);
	vi_cmd_copy_image_to_buffer(cmd, mScreenshotImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, mScreenshotBuffer, 1, &region);
	vi_command_end(cmd);

	VISubmitInfo submit;
	submit.cmd_count = 1;
	submit.cmds = &cmd;
	submit.signal_count = 0;
	submit.wait_count = 0;
	submit.wait_stages = 0;
	VIQueue queue = vi_device_get_graphics_queue(mDevice);
	vi_queue_submit(queue, 1, &submit, VI_NULL);
	vi_queue_wait_idle(queue);
	vi_free_command(mDevice, cmd);

	SaveScreenshot(Filename);
}
