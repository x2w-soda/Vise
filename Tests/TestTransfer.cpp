#include "TestTransfer.h"

#define PATTERN_SIZE 64

static const char image_vertex_src[] = R"(
#version 460

// NDC positions, Texture UV
const float vertices[24] = {
    -0.5,  0.5, 0.0, 0.0, // top left
    -0.5, -0.5, 0.0, 1.0, // bottom left
     0.5, -0.5, 1.0, 1.0, // bottom right
     0.5, -0.5, 1.0, 1.0, // bottom right
     0.5,  0.5, 1.0, 0.0, // top right
    -0.5,  0.5, 0.0, 0.0, // top left
};

layout (location = 0) out vec2 vUV;

layout (push_constant) uniform uPC
{
	vec4 ndc_offset;
} PC;

void main()
{
	vec2 pos;
	pos.x = vertices[gl_VertexIndex * 4];
	pos.y = vertices[gl_VertexIndex * 4 + 1];
	vUV.x = vertices[gl_VertexIndex * 4 + 2];
	vUV.y = vertices[gl_VertexIndex * 4 + 3];
	gl_Position = vec4(pos + PC.ndc_offset.xy, 0.0, 1.0);
}
)";

static const char image_fragment_src[] = R"(
#version 460

layout (location = 0) in vec2 vUV;
layout (location = 0) out vec4 fColor;

layout (set = 0, binding = 0) uniform sampler2D uImage;

void main()
{
	fColor = vec4(texture(uImage, vUV).rg, 0.0, 1.0);
}
)";

TestTransfer::TestTransfer(VIBackend backend)
	: TestApplication("TestTransfer", backend)
{
	VISetLayoutInfo setLayoutI;
	setLayoutI.binding_count = 1;
	mSetLayout = CreateSetLayout({
		{ VI_SET_BINDING_TYPE_COMBINED_IMAGE_SAMPLER, 0, 1 },
	});

	mSetPool = CreateSetPool(2, {
		{ VI_SET_BINDING_TYPE_COMBINED_IMAGE_SAMPLER, 2 },
	});

	VIPipelineLayoutInfo pipelineLayoutI;
	pipelineLayoutI.push_constant_size = 16;
	pipelineLayoutI.set_layout_count = 1;
	pipelineLayoutI.set_layouts = &mSetLayout;
	mPipelineLayout = vi_create_pipeline_layout(mDevice, &pipelineLayoutI);

	VIModuleInfo moduleI;
	moduleI.pipeline_layout = mPipelineLayout;
	moduleI.type = VI_MODULE_TYPE_VERTEX;
	moduleI.vise_glsl = image_vertex_src;
	mVM = vi_create_module(mDevice, &moduleI);

	moduleI.type = VI_MODULE_TYPE_FRAGMENT;
	moduleI.vise_glsl = image_fragment_src;
	mFM = vi_create_module(mDevice, &moduleI);

	VIPipelineInfo pipelineI;
	pipelineI.vertex_attribute_count = 0;
	pipelineI.vertex_binding_count = 0;
	pipelineI.vertex_module = mVM;
	pipelineI.fragment_module = mFM;
	pipelineI.pass = mScreenshotPass;
	pipelineI.layout = mPipelineLayout;
	mPipeline = vi_create_pipeline(mDevice, &pipelineI);

	// generate pixel pattern
	mPattern = new uint32_t[PATTERN_SIZE * PATTERN_SIZE];
	mPatternSize = PATTERN_SIZE * PATTERN_SIZE * 4;
	for (int y = 0; y < PATTERN_SIZE; y++)
		for (int x = 0; x < PATTERN_SIZE; x++)
		{
			int xvalue = ((float)x / PATTERN_SIZE) * 255.0f;
			int yvalue = ((float)y / PATTERN_SIZE) * 255.0f;
			int idx = y * PATTERN_SIZE + x;
			mPattern[idx] = 0;
			mPattern[idx] |= (xvalue & 255);
			mPattern[idx] |= (yvalue & 255) << 8;
		}

	uint32_t graphcis_family = vi_device_get_graphics_family_index(mDevice);
	mCmdPool = vi_create_command_pool(mDevice, graphcis_family, 0);
}

TestTransfer::~TestTransfer()
{
	vi_device_wait_idle(mDevice);

	vi_destroy_command_pool(mDevice, mCmdPool);

	vi_destroy_pipeline(mDevice, mPipeline);
	vi_destroy_module(mDevice, mFM);
	vi_destroy_module(mDevice, mVM);
	vi_destroy_pipeline_layout(mDevice, mPipelineLayout);
	vi_destroy_set_pool(mDevice, mSetPool);
	vi_destroy_set_layout(mDevice, mSetLayout);
}

void TestTransfer::Run()
{
	TestFullCopy();
	// TODO: vi_cmd_copy_image individual parameters
	// TODO: vi_cmd_copy_image_to_buffer individual parameters
	// TODO: vi_cmd_copy_buffer individual parameters
	// TODO: vi_cmd_copy_buffer_to_image individual parameters
}

// Full copy between Buffers and Images, no offsets
// - store pixel pattern in Image1
// - Image1 -> Buffer1
// - Buffer1 -> Buffer2
// - Buffer2 -> Image2
// - Image2 -> Image3
void TestTransfer::TestFullCopy()
{
	VIImage image1, image2, image3;
	VIBuffer buffer1, buffer2;

	VIImageInfo imageI = MakeImageInfo2D(VI_FORMAT_RGBA8, PATTERN_SIZE, PATTERN_SIZE, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	imageI.usage = VI_IMAGE_USAGE_TRANSFER_SRC_BIT | VI_IMAGE_USAGE_TRANSFER_DST_BIT | VI_IMAGE_USAGE_STORAGE_BIT;
	imageI.sampler_filter = VI_FILTER_NEAREST;
	image1 = vi_util_create_image_staged(mDevice, &imageI, mPattern, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

	imageI.usage = VI_IMAGE_USAGE_TRANSFER_SRC_BIT | VI_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	image2 = vi_create_image(mDevice, &imageI);

	imageI.usage = VI_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	image3 = vi_create_image(mDevice, &imageI);

	VIBufferInfo bufferI;
	bufferI.properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
	bufferI.size = PATTERN_SIZE * PATTERN_SIZE * 4;
	bufferI.type = VI_BUFFER_TYPE_NONE;
	bufferI.usage = VI_BUFFER_USAGE_TRANSFER_SRC_BIT | VI_BUFFER_USAGE_TRANSFER_DST_BIT;
	buffer1 = vi_create_buffer(mDevice, &bufferI);
	buffer2 = vi_create_buffer(mDevice, &bufferI);

	VISet setImage2 = AllocAndUpdateSet(mSetPool, mSetLayout, { {0, VI_NULL, image2 } });
	VISet setImage3 = AllocAndUpdateSet(mSetPool, mSetLayout, { {0, VI_NULL, image3 } });

	VICommand cmd = vi_alloc_command(mDevice, mCmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	vi_begin_command(cmd, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

	// TODO: this assumes transfer queue == graphics queue

	{
		// transfer from Image1 to Buffer1
		VkBufferImageCopy bufferImageRegion = MakeBufferImageCopy2D(VK_IMAGE_ASPECT_COLOR_BIT, PATTERN_SIZE, PATTERN_SIZE);
		vi_cmd_copy_image_to_buffer(cmd, image1, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buffer1, 1, &bufferImageRegion);

		// transfer from Buffer1 to Buffer2
		VIMemoryBarrier barrier;
		barrier.src_access = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dst_access = VK_ACCESS_TRANSFER_READ_BIT;
		vi_cmd_pipeline_barrier_memory(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_DEPENDENCY_BY_REGION_BIT, 1, &barrier);

		VkBufferCopy bufferRegion;
		bufferRegion.dstOffset = 0;
		bufferRegion.srcOffset = 0;
		bufferRegion.size = mPatternSize;
		vi_cmd_copy_buffer(cmd, buffer1, buffer2, 1, &bufferRegion);

		// transfer from Buffer2 to Image2
		vi_cmd_pipeline_barrier_memory(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_DEPENDENCY_BY_REGION_BIT, 1, &barrier);
		vi_util_cmd_image_layout_transition(cmd, image2, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
		vi_cmd_copy_buffer_to_image(cmd, buffer2, image2, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bufferImageRegion);

		VkImageCopy imageRegion{};
		imageRegion.extent.width = PATTERN_SIZE;
		imageRegion.extent.height = PATTERN_SIZE;
		imageRegion.extent.depth = 1;
		imageRegion.srcSubresource.aspectMask = imageRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageRegion.srcSubresource.layerCount = imageRegion.dstSubresource.layerCount = 1;

		// transfer from Image2 to Image3
		vi_cmd_pipeline_barrier_memory(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_DEPENDENCY_BY_REGION_BIT, 1, &barrier);
		vi_util_cmd_image_layout_transition(cmd, image2, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
		vi_util_cmd_image_layout_transition(cmd, image3, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
		vi_cmd_copy_image(cmd, image2, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image3, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &imageRegion);

		vi_util_cmd_image_layout_transition(cmd, image2, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		vi_util_cmd_image_layout_transition(cmd, image3, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

		// render Image2 and Image3
		VkClearValue clear_color = MakeClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		VIPassBeginInfo passBI;
		passBI.color_clear_value_count = 1;
		passBI.color_clear_values = &clear_color;
		passBI.depth_stencil_clear_value = nullptr;
		passBI.framebuffer = mScreenshotFBO;
		passBI.pass = mScreenshotPass;
		vi_cmd_begin_pass(cmd, &passBI);

		barrier.dst_access = VK_ACCESS_SHADER_READ_BIT;
		vi_cmd_bind_pipeline(cmd, mPipeline);
		vi_cmd_set_viewport(cmd, MakeViewport(TEST_WINDOW_WIDTH, TEST_WINDOW_HEIGHT));
		vi_cmd_set_scissor(cmd, MakeScissor(TEST_WINDOW_WIDTH, TEST_WINDOW_HEIGHT));

		vi_cmd_bind_set(cmd, 0, setImage2, mPipeline);

		glm::vec4 pc(-0.5f, 0.5f, 0.0f, 0.0f);
		vi_cmd_push_constants(cmd, mPipelineLayout, 0, sizeof(pc), &pc);

		VIDrawInfo drawI;
		drawI.vertex_count = 6;
		drawI.vertex_start = 0;
		drawI.instance_count = 1;
		drawI.instance_start = 0;
		vi_cmd_draw(cmd, &drawI);

		vi_cmd_bind_set(cmd, 0, setImage3, mPipeline);

		pc = glm::vec4(0.5f, 0.5f, 0.0f, 0.0f);
		vi_cmd_push_constants(cmd, mPipelineLayout, 0, sizeof(pc), &pc);
		vi_cmd_draw(cmd, &drawI);

		vi_cmd_end_pass(cmd);
	}

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
	vi_queue_wait_idle(queue);

	vi_free_set(mDevice, setImage3);
	vi_free_set(mDevice, setImage2);
	vi_free_command(mDevice, cmd);

	SaveScreenshot(Filename);

	vi_destroy_image(mDevice, image3);
	vi_destroy_image(mDevice, image2);
	vi_destroy_image(mDevice, image1);
	vi_destroy_buffer(mDevice, buffer2);
	vi_destroy_buffer(mDevice, buffer1);
}
