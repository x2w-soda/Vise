#include <filesystem>
#include "TestBuiltins.h"

// test vertex pulling with gl_VertexIndex from 2 to 7
const char test_gl_VertexIndex_src[] = R"(
// Vise NDC positions, CCW
const float quadVertices[16] = {
     0.0,  0.0, // NULL
     0.0,  0.0, // NULL
    -1.0,  1.0, // top left
    -1.0, -1.0, // bottom left
     1.0, -1.0, // bottom right
     1.0, -1.0, // bottom right
     1.0,  1.0, // top right
    -1.0,  1.0, // top left
};

void main()
{
	vec2 pos;
	pos.x = quadVertices[2 * gl_VertexIndex];
	pos.y = quadVertices[2 * gl_VertexIndex + 1];
	gl_Position = vec4(pos, 0.0, 1.0);
}
)";

// test gl_FragCorod origin is top left
const char test_gl_FragCoord_src[] = R"(
layout (location = 0) out vec4 fColor;

void main()
{
	fColor = vec4(gl_FragCoord.x / 256.0, gl_FragCoord.y / 256.0, 0.0f, 1.0);
}
)";
static_assert(TEST_WINDOW_WIDTH == 512); // remember to update shaders above

// test vertex pulling with gl_InstanceIndex 2, gl_VertexIndex from 0 to 2
// - OpenGL does *not* add the instance offset to gl_InstanceID.
//   when using OpenGL backend, SPIRV transforms gl_InstanceIndex into (gl_InstanceID + SPIRV_Cross_BaseInstance).
const char test_gl_InstanceIndex_src[] = R"(
layout (location = 0) out vec3 vColor;

// Vise NDC positions, RGB colors, CCW
const float triangleVertices[25] = {
     0.0,  0.0, 0.0, 0.0, 0.0,  // NULL
     0.0,  0.0, 0.0, 0.0, 0.0,  // NULL
     0.0,  1.0, 0.9, 0.1, 0.1,  // top center, red
    -1.0, -1.0, 0.1, 0.9, 0.1,  // bottom left, blue
     1.0, -1.0, 0.1, 0.1, 0.9,  // bottom right, green
};

void main()
{
	vec2 pos;
	pos.x = triangleVertices[5 * (gl_InstanceIndex + gl_VertexIndex)];
	pos.y = triangleVertices[5 * (gl_InstanceIndex + gl_VertexIndex) + 1];
	vec3 color;
	color.r = triangleVertices[5 * (gl_InstanceIndex + gl_VertexIndex) + 2];
	color.g = triangleVertices[5 * (gl_InstanceIndex + gl_VertexIndex) + 3];
	color.b = triangleVertices[5 * (gl_InstanceIndex + gl_VertexIndex) + 4];

	gl_Position = vec4(pos, 0.0, 1.0);
	vColor = color;
}
)";

const char fragment_color_src[] = R"(
layout (location = 0) in vec3 vColor;
layout (location = 0) out vec4 fColor;

void main()
{
	fColor = vec4(vColor, 1.0);
}
)";


TestBuiltins::TestBuiltins(VIBackend backend)
	: TestApplication("TestBuiltins", backend)
{
	VIPipelineLayoutInfo layoutI;
	layoutI.push_constant_size = 0;
	layoutI.set_layout_count = 0;
	layoutI.set_layouts = nullptr;
	mTestPipelineLayout = vi_create_pipeline_layout(mDevice, &layoutI);

	VIModuleInfo moduleI;
	moduleI.pipeline_layout = mTestPipelineLayout;
	moduleI.type = VI_MODULE_TYPE_VERTEX;
	moduleI.vise_glsl = test_gl_VertexIndex_src;
	mTestVertexIndexVM = vi_create_module(mDevice, &moduleI);
	moduleI.vise_glsl = test_gl_InstanceIndex_src;
	mTestInstanceIndexVM = vi_create_module(mDevice, &moduleI);

	moduleI.type = VI_MODULE_TYPE_FRAGMENT;
	moduleI.vise_glsl = test_gl_FragCoord_src;
	mTestFragCoordFM = vi_create_module(mDevice, &moduleI);
	moduleI.vise_glsl = fragment_color_src;
	mFragmentColorFM = vi_create_module(mDevice, &moduleI);

	VIPipelineInfo pipelineI;
	pipelineI.vertex_attribute_count = 0;
	pipelineI.vertex_binding_count = 0;
	pipelineI.vertex_module = mTestVertexIndexVM;
	pipelineI.fragment_module = mTestFragCoordFM;
	pipelineI.pass = mScreenshotPass;
	pipelineI.layout = mTestPipelineLayout;
	mTestPipeline1 = vi_create_pipeline(mDevice, &pipelineI);
	pipelineI.vertex_module = mTestInstanceIndexVM;
	pipelineI.fragment_module = mFragmentColorFM;
	mTestPipeline2 = vi_create_pipeline(mDevice, &pipelineI);

	uint32_t graphics_family = vi_device_get_graphics_family_index(mDevice);
	mCmdPool = vi_create_command_pool(mDevice, graphics_family, 0);
}

TestBuiltins::~TestBuiltins()
{
	vi_device_wait_idle(mDevice);

	vi_destroy_command_pool(mDevice, mCmdPool);
	vi_destroy_pipeline(mDevice, mTestPipeline2);
	vi_destroy_pipeline(mDevice, mTestPipeline1);
	vi_destroy_module(mDevice, mTestFragCoordFM);
	vi_destroy_module(mDevice, mTestVertexIndexVM);
	vi_destroy_module(mDevice, mTestInstanceIndexVM);
	vi_destroy_module(mDevice, mFragmentColorFM);
	vi_destroy_pipeline_layout(mDevice, mTestPipelineLayout);
}

void TestBuiltins::Run()
{
	// render to color attachment and copy the results to buffer
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
		VkViewport area;
		area.x = 0;
		area.y = 0;
		area.minDepth = 0.0f;
		area.maxDepth = 1.0f;
		area.width = TEST_WINDOW_WIDTH / 2.0f;
		area.height = TEST_WINDOW_HEIGHT / 2.0f;
		vi_cmd_bind_graphics_pipeline(cmd, mTestPipeline1);
		vi_cmd_set_viewport(cmd, area);
		vi_cmd_set_scissor(cmd, MakeScissor(TEST_WINDOW_WIDTH, TEST_WINDOW_HEIGHT));

		// gl_VertexIndex 2 to 7
		VIDrawInfo drawI;
		drawI.instance_count = 1;
		drawI.instance_start = 0;
		drawI.vertex_count = 6;
		drawI.vertex_start = 2;
		vi_cmd_draw(cmd, &drawI);

		area.x = TEST_WINDOW_WIDTH / 2.0f;
		area.y = TEST_WINDOW_HEIGHT / 2.0f;
		vi_cmd_bind_graphics_pipeline(cmd, mTestPipeline2);
		vi_cmd_set_viewport(cmd, area);
		vi_cmd_set_scissor(cmd, MakeScissor(TEST_WINDOW_WIDTH, TEST_WINDOW_HEIGHT));

		// gl_InstanceIndex 2, gl_VertexIndex 0 to 2
		drawI.instance_count = 1;
		drawI.instance_start = 2;
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
