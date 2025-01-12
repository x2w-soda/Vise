#include "TestOfflineCompile.h"

static const char test_vertex_glsl[] = R"(
#version 460

const float vertices[6] = {
     0.0,  0.5, // top center
    -0.5, -0.5, // bottom left
     0.5, -0.5, // bottom right
};

void main()
{
	vec2 pos;
	pos.x = vertices[gl_VertexIndex * 2];
	pos.y = vertices[gl_VertexIndex * 2 + 1];
	gl_Position = vec4(pos, 0.0, 1.0);
}
)";

static const char test_fragment_glsl[] = R"(
#version 460

layout (location = 0) out vec4 fColor;

layout (push_constant) uniform uPC
{
	vec4 color;
} PC;

void main()
{
	fColor = PC.color;
}
)";

TestOfflineCompile::TestOfflineCompile(VIBackend backend)
	: TestApplication("TestOfflineCompile", backend)
{
	// offline compilation

	VIPipelineLayoutData pipelineLD;
	pipelineLD.push_constant_size = sizeof(glm::vec4);
	pipelineLD.set_layouts = nullptr; // TODO: test
	pipelineLD.set_layout_count = 0;

	mTestBinaryVM = vi_compile_binary_offline(backend, VI_MODULE_TYPE_VERTEX, &pipelineLD, test_vertex_glsl, nullptr);
	mTestBinaryFM = vi_compile_binary_offline(backend, VI_MODULE_TYPE_FRAGMENT, &pipelineLD, test_fragment_glsl, nullptr);

	// runtime resources

	VIPipelineLayoutInfo pipelineLI;
	pipelineLI.push_constant_size = pipelineLD.push_constant_size;
	pipelineLI.set_layout_count = pipelineLD.set_layout_count;
	mTestPipelineLayout = vi_create_pipeline_layout(mDevice, &pipelineLI);

	VIModuleInfo moduleI;
	moduleI.pipeline_layout = mTestPipelineLayout;
	moduleI.type = VI_MODULE_TYPE_VERTEX;
	moduleI.vise_glsl = nullptr;
	moduleI.vise_binary = mTestBinaryVM;
	mTestVM = vi_create_module(mDevice, &moduleI);

	moduleI.type = VI_MODULE_TYPE_FRAGMENT;
	moduleI.vise_binary = mTestBinaryFM;
	mTestFM = vi_create_module(mDevice, &moduleI);

	VIPipelineInfo pipelineI;
	pipelineI.layout = mTestPipelineLayout;
	pipelineI.pass = mScreenshotPass;
	pipelineI.vertex_module = mTestVM;
	pipelineI.fragment_module = mTestFM;
	pipelineI.vertex_attribute_count = 0;
	pipelineI.vertex_binding_count = 0;
	mTestPipeline = vi_create_pipeline(mDevice, &pipelineI);

	vi_free(mTestBinaryFM);
	vi_free(mTestBinaryVM);

	uint32_t graphics_family = vi_device_get_graphics_family_index(mDevice);
	mCmdPool = vi_create_command_pool(mDevice, graphics_family, 0);
}

TestOfflineCompile::~TestOfflineCompile()
{
	vi_destroy_command_pool(mDevice, mCmdPool);
	vi_destroy_pipeline(mDevice, mTestPipeline);
	vi_destroy_module(mDevice, mTestFM);
	vi_destroy_module(mDevice, mTestVM);
	vi_destroy_pipeline_layout(mDevice, mTestPipelineLayout);
}

void TestOfflineCompile::Run()
{
	VICommand cmd = vi_allocate_primary_command(mDevice, mCmdPool);

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
		vi_cmd_bind_graphics_pipeline(cmd, mTestPipeline);
		vi_cmd_set_viewport(cmd, MakeViewport(TEST_WINDOW_WIDTH, TEST_WINDOW_HEIGHT));
		vi_cmd_set_scissor(cmd, MakeScissor(TEST_WINDOW_WIDTH, TEST_WINDOW_HEIGHT));

		glm::vec4 color(0.1f, 0.9f, 0.1f, 1.0f);
		vi_cmd_push_constants(cmd, mTestPipelineLayout, 0, sizeof(color), &color);

		VIDrawInfo drawI;
		drawI.instance_count = 1;
		drawI.instance_start = 0;
		drawI.vertex_count = 3;
		drawI.vertex_start = 0;
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
	vi_queue_submit(queue, 1, &submit, VI_NULL);

	vi_device_wait_idle(mDevice);
	vi_free_command(mDevice, cmd);

	SaveScreenshot(Filename);
}
