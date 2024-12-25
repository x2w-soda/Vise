#include <filesystem>
#include <stb_image.h>
#include <stb_image_write.h>
#include "TestApplication.h"

TestApplication::TestApplication(const char* name, VIBackend backend)
	: Application(name, backend, false)
{
	// right after the screenshot pass we will copy the color attachment to host visible buffer
	VkSubpassDependency dep;
	dep.dependencyFlags = 0;
	dep.srcSubpass = 0;
	dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dep.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	dep.dstSubpass = VK_SUBPASS_EXTERNAL;
	dep.dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
	dep.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	VIPassColorAttachment color_atch;
	color_atch.color_format = VI_FORMAT_RGBA8;
	color_atch.color_load_op = VK_ATTACHMENT_LOAD_OP_CLEAR;
	color_atch.color_store_op = VK_ATTACHMENT_STORE_OP_STORE;
	color_atch.initial_layout = VK_IMAGE_LAYOUT_UNDEFINED;
	color_atch.final_layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL; // prepare to copy to screenshot buffer
	VISubpassColorAttachment color_atch_ref;
	color_atch_ref.index = 0;
	color_atch_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	VISubpassInfo subpassI;
	subpassI.depth_stencil_attachment_ref = nullptr;
	subpassI.color_attachment_refs = &color_atch_ref;
	subpassI.color_attachment_ref_count = 1;
	VIPassInfo passI;
	passI.depenency_count = 1;
	passI.dependencies = &dep;
	passI.color_attachment_count = 1;
	passI.color_attachments = &color_atch;
	passI.depth_stencil_attachment = nullptr;
	passI.subpass_count = 1;
	passI.subpasses = &subpassI;
	mScreenshotPass = vi_create_pass(mDevice, &passI);

	VIImageInfo imageI;
	imageI.type = VI_IMAGE_TYPE_2D;
	imageI.properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
	imageI.format = VI_FORMAT_RGBA8;
	imageI.usage = VI_IMAGE_USAGE_TRANSFER_SRC_BIT | VI_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	imageI.width = TEST_WINDOW_WIDTH;
	imageI.height = TEST_WINDOW_HEIGHT;
	mScreenshotImage = vi_create_image(mDevice, &imageI);

	VIFramebufferInfo fboI;
	fboI.pass = mScreenshotPass;
	fboI.width = TEST_WINDOW_WIDTH;
	fboI.height = TEST_WINDOW_HEIGHT;
	fboI.color_attachment_count = 1;
	fboI.color_attachments = &mScreenshotImage;
	fboI.depth_stencil_attachment = nullptr;
	mScreenshotFBO = vi_create_framebuffer(mDevice, &fboI);

	VIBufferInfo bufferI;
	bufferI.type = VI_BUFFER_TYPE_TRANSFER;
	bufferI.size = TEST_WINDOW_WIDTH * TEST_WINDOW_HEIGHT * 4;
	bufferI.usage = VI_BUFFER_USAGE_TRANSFER_DST_BIT;
	bufferI.properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
	mScreenshotBuffer = vi_create_buffer(mDevice, &bufferI);
}

TestApplication::~TestApplication()
{
	vi_destroy_buffer(mDevice, mScreenshotBuffer);
	vi_destroy_framebuffer(mDevice, mScreenshotFBO);
	vi_destroy_image(mDevice, mScreenshotImage);
	vi_destroy_pass(mDevice, mScreenshotPass);
}

void TestApplication::SaveScreenshot(const char* name)
{
	vi_buffer_map(mScreenshotBuffer);
	void* readback = vi_buffer_map_read(mScreenshotBuffer, 0, TEST_WINDOW_WIDTH * TEST_WINDOW_HEIGHT * 4);
	stbi_write_png(name, TEST_WINDOW_WIDTH, TEST_WINDOW_HEIGHT, 4, readback, TEST_WINDOW_WIDTH * 4);
	vi_buffer_unmap(mScreenshotBuffer);

	std::filesystem::path local_path(name);
	std::filesystem::path current_path = std::filesystem::current_path();

	printf("saved screenshot to [%s\\%s]\n", current_path.u8string().c_str(), local_path.u8string().c_str());
}
