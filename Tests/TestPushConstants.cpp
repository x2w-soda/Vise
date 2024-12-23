#include <filesystem>
#include "TestPushConstants.h"


const char test_vertex_src1[] =  R"(
// Vise NDC positions, CCW
const float vertices[6] = {
     0.0,  0.5, // top center
    -0.5, -0.5, // bottom left
     0.5, -0.5, // bottom right
};

layout (location = 0) out vec4 vColor;

layout (push_constant) uniform uPC
{
	mat4 pad;
	vec4 ndc_offset;
	vec4 color;
} PC;

void main()
{
	vec2 pos;
	pos.x = vertices[gl_VertexIndex * 2];
	pos.y = vertices[gl_VertexIndex * 2 + 1];

	pos = pos + PC.ndc_offset.xy;

	gl_Position = vec4(pos, 0.0, 1.0);
	vColor = PC.color;
}
)";

const char test_vertex_src2[] = R"(
// Vise NDC positions, CCW
const float vertices[6] = {
     0.0,  0.5, // top center
    -0.5, -0.5, // bottom left
     0.5, -0.5, // bottom right
};

layout (location = 0) out vec4 vColor;

// NOTE: Any member of a push constant block that is declared as an array
//       must only be accessed with dynamically uniform indices.
layout (push_constant) uniform uPC
{
	mat4 pad;
	vec4 ndc_offset;
	vec4 colors[3];
} PC;

void main()
{
	vec2 pos;
	pos.x = vertices[gl_VertexIndex * 2];
	pos.y = vertices[gl_VertexIndex * 2 + 1];

	pos = pos + PC.ndc_offset.xy;

	gl_Position = vec4(pos, 0.0, 1.0);

	// Dirty hack to convert non dynamically-uniform indices into constant-integral indices.
	// Using vColor[gl_VertexIndex] is not valid.
	switch (gl_VertexIndex)
	{
	case 0: vColor = PC.colors[0]; break;
	case 1: vColor = PC.colors[1]; break;
	case 2: vColor = PC.colors[2]; break;
	}
}
)";

const char test_fragment_src[] = R"(
layout (location = 0) in vec4 vColor;
layout (location = 0) out vec4 fColor;

void main()
{
	fColor = vColor;
}
)";

TestPushConstants::TestPushConstants(VIBackend backend)
	: TestApplication("TestPushConstants", backend)
{
	// Push Constant Layout is described with byte size,
	// bounded by VIDeviceLimits::max_push_constant_size.
	VIPipelineLayoutInfo layoutI;
	layoutI.set_layout_count = 0;
	layoutI.set_layouts = nullptr;
	layoutI.push_constant_size = 128;
	mTestPipelineLayout = vi_create_pipeline_layout(mDevice, &layoutI);

	VIModuleInfo moduleI;
	moduleI.pipeline_layout = mTestPipelineLayout;
	moduleI.type = VI_MODULE_TYPE_VERTEX_BIT;
	moduleI.vise_glsl = test_vertex_src1;
	mTestVM1 = vi_create_module(mDevice, &moduleI);
	
	moduleI.vise_glsl = test_vertex_src2;
	mTestVM2 = vi_create_module(mDevice, &moduleI);

	moduleI.type = VI_MODULE_TYPE_FRAGMENT_BIT;
	moduleI.vise_glsl = test_fragment_src;
	mTestFM = vi_create_module(mDevice, &moduleI);

	VIPipelineInfo pipelineI;
	pipelineI.vertex_attribute_count = 0;
	pipelineI.vertex_binding_count = 0;
	pipelineI.vertex_module = mTestVM1;
	pipelineI.fragment_module = mTestFM;
	pipelineI.pass = mScreenshotPass;
	pipelineI.layout = mTestPipelineLayout;
	mTestPipeline1 = vi_create_pipeline(mDevice, &pipelineI);

	pipelineI.vertex_module = mTestVM2;
	mTestPipeline2 = vi_create_pipeline(mDevice, &pipelineI);

	uint32_t graphics_family = vi_device_get_graphics_family_index(mDevice);
	mCmdPool = vi_create_command_pool(mDevice, graphics_family, 0);
}

TestPushConstants::~TestPushConstants()
{
	vi_device_wait_idle(mDevice);

	vi_destroy_command_pool(mDevice, mCmdPool);
	vi_destroy_pipeline(mDevice, mTestPipeline2);
	vi_destroy_pipeline(mDevice, mTestPipeline1);
	vi_destroy_module(mDevice, mTestVM2);
	vi_destroy_module(mDevice, mTestVM1);
	vi_destroy_module(mDevice, mTestFM);
	vi_destroy_pipeline_layout(mDevice, mTestPipelineLayout);
}

void TestPushConstants::Run()
{
	VICommand cmd = vi_alloc_command(mDevice, mCmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	vi_begin_command(cmd, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

	VkClearValue clear_color = MakeClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	VIPassBeginInfo passBI;
	passBI.color_clear_value_count = 1;
	passBI.color_clear_values = &clear_color;
	passBI.depth_stencil_clear_value = nullptr;
	passBI.framebuffer = mScreenshotFBO;
	passBI.pass = mScreenshotPass;
	vi_cmd_begin_pass(cmd, &passBI);
	{
		struct PCLayout1
		{
			glm::vec4 ndc_offset;
			glm::vec4 color;
		} pc1;

		struct PCLayout2
		{
			glm::vec4 ndc_offset;
			glm::vec4 colors[3];
		} pc2;

		VIDrawInfo drawI;
		drawI.instance_count = 1;
		drawI.instance_start = 0;
		drawI.vertex_count = 3;
		drawI.vertex_start = 0;

		constexpr uint32_t offset = sizeof(float) * 16;

		vi_cmd_bind_pipeline(cmd, mTestPipeline1);
		vi_cmd_set_viewport(cmd, MakeViewport(TEST_WINDOW_WIDTH, TEST_WINDOW_HEIGHT));
		vi_cmd_set_scissor(cmd, MakeScissor(TEST_WINDOW_WIDTH, TEST_WINDOW_HEIGHT));

		pc1.ndc_offset.x = 0.5f;
		pc1.ndc_offset.y = 0.5f;
		pc1.color = glm::vec4(0.9f, 0.1f, 0.1f, 1.0f);
		vi_cmd_push_constants(cmd, mTestPipelineLayout, offset, sizeof(pc1), &pc1);
		vi_cmd_draw(cmd, &drawI);

		pc1.ndc_offset.x = -0.5f;
		pc1.ndc_offset.y = 0.5f;
		pc1.color = glm::vec4(0.1f, 0.9f, 0.1f, 1.0f);
		vi_cmd_push_constants(cmd, mTestPipelineLayout, offset, sizeof(pc1), &pc1);
		vi_cmd_draw(cmd, &drawI);

		// Vise push constant layout is characterized only by number of bytes.
		// Even though Pipeline2 uses a different push_constant block compared to Pipeline1,
		// they are both under 128 bytes and therefore share the same push constant layout.

		vi_cmd_bind_pipeline(cmd, mTestPipeline2);
		vi_cmd_set_viewport(cmd, MakeViewport(TEST_WINDOW_WIDTH, TEST_WINDOW_HEIGHT));
		vi_cmd_set_scissor(cmd, MakeScissor(TEST_WINDOW_WIDTH, TEST_WINDOW_HEIGHT));

		pc2.colors[0] = glm::vec4(0.1f, 0.9f, 0.9f, 1.0f);
		pc2.colors[1] = glm::vec4(0.9f, 0.1f, 0.9f, 1.0f);
		pc2.colors[2] = glm::vec4(0.9f, 0.9f, 0.1f, 1.0f);

		pc2.ndc_offset.x = 0.5f;
		pc2.ndc_offset.y = -0.5f;
		vi_cmd_push_constants(cmd, mTestPipelineLayout, offset, sizeof(pc2), &pc2);
		vi_cmd_draw(cmd, &drawI);

		pc2.ndc_offset.x = -0.5f;
		pc2.ndc_offset.y = -0.5f;
		vi_cmd_push_constants(cmd, mTestPipelineLayout, offset, sizeof(pc2), &pc2);
		vi_cmd_draw(cmd, &drawI);
	}
	vi_cmd_end_pass(cmd);

	VkBufferImageCopy region = MakeBufferImageCopy2D(VK_IMAGE_ASPECT_COLOR_BIT, TEST_WINDOW_WIDTH, TEST_WINDOW_HEIGHT);
	vi_cmd_copy_image_to_buffer(cmd, mScreenshotImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, mScreenshotBuffer, 1, &region);
	vi_end_command(cmd);

	VISubmitInfo submit;
	submit.cmd_count = 1;
	submit.cmds = &cmd;
	submit.signal_count = 0;
	submit.wait_count = 0;
	submit.wait_stages = 0;
	VIQueue queue = vi_device_get_graphics_queue(mDevice);
	vi_queue_submit(queue, 1, &submit, VI_NULL_HANDLE);

	vi_device_wait_idle(mDevice);
	vi_free_command(mDevice, cmd);

	SaveScreenshot(Filename);
}
