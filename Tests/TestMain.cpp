#include <vise.h>
#include "TestBuiltins.h"
#include "../Examples/Application/Application.h"
#include "../Examples/Application/stb_image.h"
#include "../Examples/Application/stb_image_write.h"

class TestDriver : public Application
{
public:
	TestDriver(VIBackend backend);
	~TestDriver();

	double TestMSE(const char* path1, const char* path2);

	virtual void Run() override;

	const char* Path1;
	const char* Path2;

private:
	uint32_t mGraphicsFamily;
	VIQueue mGraphicsQueue;
	VISetPool mMSESetPool;
	VISetLayout mMSESetLayout;
	VIPipelineLayout mMSEPipelineLayout;
	VIModule mMSEModule;
	VIComputePipeline mMSEPipeline;
	VICommandPool mCommandPool;
};

const char* compute_src = R"(
#version 460

layout (local_size_x = 32, local_size_y = 32, local_size_z = 1) in;

layout (set = 0, binding = 0) buffer uResult
{
	uint error[];
} Result;

layout (set = 0, binding = 1, rgba8) uniform readonly image2D uImage1;
layout (set = 0, binding = 2, rgba8) uniform readonly image2D uImage2;

void main()
{
	vec3 pixel1 = imageLoad(uImage1, ivec2(gl_GlobalInvocationID.xy)).rgb;
	vec3 pixel2 = imageLoad(uImage2, ivec2(gl_GlobalInvocationID.xy)).rgb;

	vec3 dist = pixel1 - pixel2;
	uint dist_squared = uint(dot(dist, dist) * 10000.0);

	uint workgroup = gl_WorkGroupID.y * gl_NumWorkGroups.x + gl_WorkGroupID.x;
	atomicAdd(Result.error[workgroup], dist_squared); 
}
)";

TestDriver::TestDriver(VIBackend backend)
	: Application("Test Driver", backend, false)
{
	mMSESetLayout = CreateSetLayout({
		{ VI_SET_BINDING_TYPE_STORAGE_BUFFER, 0, 1 },
		{ VI_SET_BINDING_TYPE_STORAGE_IMAGE, 1, 1 },
		{ VI_SET_BINDING_TYPE_STORAGE_IMAGE, 2, 1 },
	});
	mMSEPipelineLayout = CreatePipelineLayout({
		mMSESetLayout
	});

	VIModuleInfo moduleI;
	moduleI.type = VI_MODULE_TYPE_COMPUTE_BIT;
	moduleI.vise_glsl = compute_src;
	moduleI.pipeline_layout = mMSEPipelineLayout;
	mMSEModule = vi_create_module(mDevice, &moduleI);

	VIComputePipelineInfo pipelineI;
	pipelineI.compute_module = mMSEModule;
	pipelineI.layout = mMSEPipelineLayout;
	mMSEPipeline = vi_create_compute_pipeline(mDevice, &pipelineI);

	constexpr int max_mse_comparisons = 64;
	VISetPoolResource resources[2];
	resources[0].type = VI_SET_BINDING_TYPE_STORAGE_BUFFER;
	resources[0].count = max_mse_comparisons;
	resources[1].type = VI_SET_BINDING_TYPE_STORAGE_IMAGE;
	resources[1].count = max_mse_comparisons * 2;
	VISetPoolInfo poolI;
	poolI.max_set_count = max_mse_comparisons;
	poolI.resource_count = 2;
	poolI.resources = resources;
	mMSESetPool = vi_create_set_pool(mDevice, &poolI);

	mGraphicsQueue = vi_device_get_graphics_queue(mDevice); // TODO:
	mGraphicsFamily = vi_device_get_graphics_family_index(mDevice); // TODO:
	mCommandPool = vi_create_command_pool(mDevice, mGraphicsFamily, 0);
}

TestDriver::~TestDriver()
{
	vi_destroy_command_pool(mDevice, mCommandPool);
	vi_destroy_set_pool(mDevice, mMSESetPool);
	vi_destroy_compute_pipeline(mDevice, mMSEPipeline);
	vi_destroy_module(mDevice, mMSEModule);
	vi_destroy_pipeline_layout(mDevice, mMSEPipelineLayout);
	vi_destroy_set_layout(mDevice, mMSESetLayout);
}

double TestDriver::TestMSE(const char* path1, const char* path2)
{
	int width1, height1, ch1;
	int width2, height2, ch2;
	stbi_uc* data1 = stbi_load(path1, &width1, &height1, &ch1, STBI_rgb_alpha);
	stbi_uc* data2 = stbi_load(path2, &width2, &height2, &ch2, STBI_rgb_alpha);

	VIImageInfo imageI;
	imageI.type = VI_IMAGE_TYPE_2D;
	imageI.format = VI_FORMAT_RGBA8;
	imageI.usage = VI_IMAGE_USAGE_STORAGE_BIT | VI_IMAGE_USAGE_TRANSFER_DST_BIT | VI_IMAGE_USAGE_TRANSFER_SRC_BIT;
	imageI.width = (uint32_t)width1;
	imageI.height = (uint32_t)height1;
	imageI.sampler_filter = VI_FILTER_LINEAR;
	imageI.sampler_address_mode = VI_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	imageI.properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
	VIImage image1 = vi_util_create_image_staged(mDevice, &imageI, data1, VK_IMAGE_LAYOUT_GENERAL);
	VIImage image2 = vi_util_create_image_staged(mDevice, &imageI, data2, VK_IMAGE_LAYOUT_GENERAL);

	stbi_image_free(data1);
	stbi_image_free(data2);

	uint32_t workgroup_x = width1 / 32;
	uint32_t workgroup_y = height1 / 32;
	uint32_t storage_size = sizeof(uint32_t) * workgroup_x * workgroup_y;

	VIBufferInfo bufferI;
	bufferI.type = VI_BUFFER_TYPE_STORAGE; // TODO: buffer type none and usage type storage?
	bufferI.usage = VI_BUFFER_USAGE_TRANSFER_DST_BIT;
	bufferI.properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
	bufferI.size = storage_size;
	VIBuffer storage_buffer = vi_create_buffer(mDevice, &bufferI);

	std::vector<uint32_t> storage_data(workgroup_x * workgroup_y);
	for (size_t i = 0; i < storage_data.size(); i++)
		storage_data[i] = 0;

	vi_buffer_map(storage_buffer);
	vi_buffer_map_write(storage_buffer, 0, storage_size, storage_data.data());
	vi_buffer_unmap(storage_buffer);

	VISet MSESet = AllocAndUpdateSet(mMSESetPool, mMSESetLayout, {
		{ 0, storage_buffer, VI_NULL_HANDLE },
		{ 1, VI_NULL_HANDLE, image1 },
		{ 2, VI_NULL_HANDLE, image2 },
	});

	VICommand cmd = vi_alloc_command(mDevice, mCommandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	vi_cmd_begin_record(cmd, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
	{
		vi_cmd_bind_compute_pipeline(cmd, mMSEPipeline);
		vi_cmd_bind_set(cmd, 0, MSESet, mMSEPipeline); // TODO: pipeline layout + bind point
		vi_cmd_dispatch(cmd, workgroup_x, workgroup_y, 1);
	}
	vi_cmd_end_record(cmd);

	VISubmitInfo submitI;
	submitI.cmd_count = 1;
	submitI.cmds = &cmd;
	submitI.signal_count = 0;
	submitI.wait_count = 0;
	vi_queue_submit(mGraphicsQueue, 1, &submitI, nullptr);
	vi_queue_wait_idle(mGraphicsQueue); // or wait on Fence?
	vi_free_command(mDevice, cmd);

	double mse = 0.0;

	// results
	{
		double squared_errors = 0.0;

		vi_buffer_map(storage_buffer);
		uint32_t* storage_data = (uint32_t*)vi_buffer_map_read(storage_buffer, 0, storage_size);

		for (uint32_t i = 0; i < workgroup_x * workgroup_y; i++)
		{
			squared_errors += (storage_data[i] / 1e4);
		}

		vi_buffer_unmap(storage_buffer);
		mse = squared_errors / (width1 * height1);
	}
	printf("MSE: %.4f\n", mse);
	
	vi_free_set(mDevice, MSESet);

	// test screenshot
	VIBuffer transferDst;
	uint32_t imageSize = width1 * height1 * 4;
	//unsigned char* readback = new unsigned char[imageSize];
	{
		VIBufferInfo transferI;
		transferI.properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
		transferI.type = VI_BUFFER_TYPE_NONE;
		transferI.size = imageSize;
		transferI.usage = VI_BUFFER_USAGE_TRANSFER_DST_BIT;
		transferDst = vi_create_buffer(mDevice, &transferI);

		VICommand cmd = vi_alloc_command(mDevice, mCommandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
		vi_cmd_begin_record(cmd, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
		{
			VkBufferImageCopy region{};
			region.imageExtent.width = width1;
			region.imageExtent.height = height1;
			region.imageExtent.depth = 1;
			region.imageSubresource.baseArrayLayer = 0;
			region.imageSubresource.layerCount = 1;
			region.imageSubresource.mipLevel = 0;
			region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			vi_cmd_copy_image_to_buffer(cmd, image2, VK_IMAGE_LAYOUT_GENERAL, transferDst, 1, &region);
		}
		vi_cmd_end_record(cmd);

		VISubmitInfo submitI;
		submitI.cmd_count = 1;
		submitI.cmds = &cmd;
		submitI.signal_count = 0;
		submitI.wait_count = 0;
		vi_queue_submit(mGraphicsQueue, 1, &submitI, nullptr);
		vi_queue_wait_idle(mGraphicsQueue);
		vi_free_command(mDevice, cmd);

		//void* readback_data = vi_buffer_map(transferDst);
		//memcpy(readback, readback_data, imageSize);
		//vi_buffer_unmap(transferDst);
		vi_destroy_buffer(mDevice, transferDst);
	}
	//stbi_write_png(mBackend == VI_BACKEND_VULKAN ? "./readback_vk.png" : "./readback_gl.png", width1, height1, 4, readback, width1 * 4);
	//delete[]readback;

	vi_destroy_buffer(mDevice, storage_buffer);
	vi_destroy_image(mDevice, image2);
	vi_destroy_image(mDevice, image1);

	return 0.0;
}

void TestDriver::Run()
{
	TestMSE(Path1, Path2);
}

int main(int argc, char** argv)
{
	{
		TestBuiltins test_builtins(VI_BACKEND_VULKAN);
		test_builtins.Run();
	}
	{
		TestBuiltins test_builtins(VI_BACKEND_OPENGL);
		test_builtins.Run();
	}

	// the MSE test driver can be done in either backend
	TestDriver testDriver(VI_BACKEND_VULKAN);
	testDriver.Path1 = "gl_fragcoord_vk.png";
	testDriver.Path2 = "gl_fragcoord_gl.png";
	testDriver.Run();

	return 0;
}