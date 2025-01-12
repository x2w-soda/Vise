#include <array>
#include <vise.h>
#include <stb_image.h>
#include <stb_image_write.h>
#include "TestOfflineCompile.h"
#include "TestBuiltins.h"
#include "TestTransfer.h"
#include "TestPushConstants.h"
#include "TestPipelineBlend.h"
#include "../Examples/Application/Application.h"

#define TEST_MSE_THRESHOLD 0.01

class TestDriver : public Application
{
public:
	TestDriver(VIBackend backend);
	~TestDriver();

	virtual void Run() override;

	void AddMSETest(const char* path1, const char* path2);

private:
	struct MSETest
	{
		void Init(VIDevice device, VISetPool set_pool, VISetLayout set_layout);
		void Shutdown(VIDevice device);

		const char* Path1;
		const char* Path2;
		VIImage Image1;
		VIImage Image2;
		VIBuffer WGPartialSum;
		VISet MSESet;
		double ResultMSE;
	};

	std::vector<MSETest> mTests;
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
	mMSESetLayout = CreateSetLayout(mDevice, {
		{ VI_BINDING_TYPE_STORAGE_BUFFER, 0, 1 },
		{ VI_BINDING_TYPE_STORAGE_IMAGE, 1, 1 },
		{ VI_BINDING_TYPE_STORAGE_IMAGE, 2, 1 },
	});
	mMSEPipelineLayout = CreatePipelineLayout(mDevice, {
		mMSESetLayout
	});

	VIModuleInfo moduleI;
	moduleI.type = VI_MODULE_TYPE_COMPUTE;
	moduleI.vise_glsl = compute_src;
	moduleI.pipeline_layout = mMSEPipelineLayout;
	mMSEModule = vi_create_module(mDevice, &moduleI);

	VIComputePipelineInfo pipelineI;
	pipelineI.compute_module = mMSEModule;
	pipelineI.layout = mMSEPipelineLayout;
	mMSEPipeline = vi_create_compute_pipeline(mDevice, &pipelineI);

	mGraphicsQueue = vi_device_get_graphics_queue(mDevice); // TODO:
	mGraphicsFamily = vi_device_get_graphics_family_index(mDevice); // TODO:
	mCommandPool = vi_create_command_pool(mDevice, mGraphicsFamily, 0);
}

TestDriver::~TestDriver()
{
	vi_destroy_command_pool(mDevice, mCommandPool);
	vi_destroy_compute_pipeline(mDevice, mMSEPipeline);
	vi_destroy_module(mDevice, mMSEModule);
	vi_destroy_pipeline_layout(mDevice, mMSEPipelineLayout);
	vi_destroy_set_layout(mDevice, mMSESetLayout);
}

void TestDriver::Run()
{
	uint32_t workgroup_x = TEST_WINDOW_WIDTH / 32;
	uint32_t workgroup_y = TEST_WINDOW_HEIGHT / 32;
	uint32_t storage_size = sizeof(uint32_t) * workgroup_x * workgroup_y;

	size_t mse_test_count = mTests.size();
	VISetPoolResource resources[2];
	resources[0].type = VI_BINDING_TYPE_STORAGE_BUFFER;
	resources[0].count = mse_test_count;
	resources[1].type = VI_BINDING_TYPE_STORAGE_IMAGE;
	resources[1].count = mse_test_count * 2;
	VISetPoolInfo poolI;
	poolI.max_set_count = mse_test_count;
	poolI.resource_count = 2;
	poolI.resources = resources;
	mMSESetPool = vi_create_set_pool(mDevice, &poolI);

	for (MSETest& test : mTests)
		test.Init(mDevice, mMSESetPool, mMSESetLayout);

	VICommand cmd = vi_allocate_primary_command(mDevice, mCommandPool);
	vi_begin_command(cmd, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
	vi_cmd_bind_compute_pipeline(cmd, mMSEPipeline);
	for (MSETest& test : mTests)
	{
		vi_cmd_bind_compute_set(cmd, mMSEPipelineLayout, 0, test.MSESet);
		vi_cmd_dispatch(cmd, workgroup_x, workgroup_y, 1);
	}
	vi_end_command(cmd);

	VISubmitInfo submitI;
	submitI.cmd_count = 1;
	submitI.cmds = &cmd;
	submitI.signal_count = 0;
	submitI.wait_count = 0;
	vi_queue_submit(mGraphicsQueue, 1, &submitI, nullptr);
	vi_queue_wait_idle(mGraphicsQueue);
	vi_free_command(mDevice, cmd);

	// calculate MSE from workgroup partial sums
	for (MSETest& test : mTests)
	{
		test.ResultMSE = 0.0;
		double squared_errors = 0.0;

		vi_buffer_map(test.WGPartialSum);
		uint32_t* storage_data = (uint32_t*)vi_buffer_map_read(test.WGPartialSum, 0, storage_size);

		for (uint32_t i = 0; i < workgroup_x * workgroup_y; i++)
			squared_errors += (storage_data[i] / 1e4);

		vi_buffer_unmap(test.WGPartialSum);
		test.ResultMSE = squared_errors / (TEST_WINDOW_WIDTH * TEST_WINDOW_HEIGHT);

		printf("Test [%s] [%s] ", test.Path1, test.Path2);
		printf("MSE %.4f %s\n", test.ResultMSE, (test.ResultMSE < TEST_MSE_THRESHOLD) ? "OK" : "FAILED");
	}

	for (MSETest& test : mTests)
		test.Shutdown(mDevice);

	vi_destroy_set_pool(mDevice, mMSESetPool);
}

void TestDriver::AddMSETest(const char* path1, const char* path2)
{
	MSETest test;
	test.Path1 = path1;
	test.Path2 = path2;

	mTests.push_back(test);
}

void TestDriver::MSETest::Init(VIDevice device, VISetPool set_pool, VISetLayout set_layout)
{
	uint32_t workgroup_x = TEST_WINDOW_WIDTH / 32;
	uint32_t workgroup_y = TEST_WINDOW_HEIGHT / 32;
	uint32_t storage_size = sizeof(uint32_t) * workgroup_x * workgroup_y;

	int width1, height1, ch1;
	int width2, height2, ch2;
	stbi_uc* data1 = stbi_load(Path1, &width1, &height1, &ch1, STBI_rgb_alpha);
	stbi_uc* data2 = stbi_load(Path2, &width2, &height2, &ch2, STBI_rgb_alpha);

	VIImageInfo imageI{};
	imageI.type = VI_IMAGE_TYPE_2D;
	imageI.format = VI_FORMAT_RGBA8;
	imageI.usage = VI_IMAGE_USAGE_STORAGE_BIT | VI_IMAGE_USAGE_TRANSFER_DST_BIT | VI_IMAGE_USAGE_TRANSFER_SRC_BIT;
	imageI.width = (uint32_t)TEST_WINDOW_WIDTH;
	imageI.height = (uint32_t)TEST_WINDOW_HEIGHT;
	imageI.properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
	Image1 = CreateImageStaged(device, &imageI, data1, VK_IMAGE_LAYOUT_GENERAL);
	Image2 = CreateImageStaged(device, &imageI, data2, VK_IMAGE_LAYOUT_GENERAL);

	stbi_image_free(data1);
	stbi_image_free(data2);

	VIBufferInfo bufferI;
	bufferI.type = VI_BUFFER_TYPE_STORAGE; // TODO: buffer type none and usage type storage?
	bufferI.usage = VI_BUFFER_USAGE_TRANSFER_DST_BIT;
	bufferI.properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
	bufferI.size = storage_size;
	WGPartialSum = vi_create_buffer(device, &bufferI);

	std::vector<uint32_t> storage_data(workgroup_x * workgroup_y);
	for (size_t i = 0; i < storage_data.size(); i++)
		storage_data[i] = 0;

	vi_buffer_map(WGPartialSum);
	vi_buffer_map_write(WGPartialSum, 0, storage_size, storage_data.data());
	vi_buffer_unmap(WGPartialSum);

	MSESet = vi_allocate_set(device, set_pool, set_layout);
	std::array<VISetUpdateInfo, 3> set_updates;
	set_updates[0] = { 0, WGPartialSum, VI_NULL };
	set_updates[1] = { 1, VI_NULL, Image1 };
	set_updates[2] = { 2, VI_NULL, Image2 };
	vi_set_update(MSESet, set_updates.size(), set_updates.data());
}

void TestDriver::MSETest::Shutdown(VIDevice device)
{
	vi_free_set(device, MSESet);
	vi_destroy_buffer(device, WGPartialSum);
	vi_destroy_image(device, Image2);
	vi_destroy_image(device, Image1);
}

int main(int argc, char** argv)
{
	{
		TestOfflineCompile test_offline_compile(VI_BACKEND_VULKAN);
		test_offline_compile.Filename = "offline_compile_vk.png";
		test_offline_compile.Run();
	}
	{
		TestOfflineCompile test_offline_compile(VI_BACKEND_OPENGL);
		test_offline_compile.Filename = "offline_compile_gl.png";
		test_offline_compile.Run();
	}
	{
		TestBuiltins test_builtins(VI_BACKEND_VULKAN);
		test_builtins.Filename = "glsl_builtins_vk.png";
		test_builtins.Run();
	}
	{
		TestBuiltins test_builtins(VI_BACKEND_OPENGL);
		test_builtins.Filename = "glsl_builtins_gl.png";
		test_builtins.Run();
	}
	{
		TestTransfer test_transfer(VI_BACKEND_VULKAN);
		test_transfer.Filename = "transfer_vk.png";
		test_transfer.Run();
	}
	{
		TestTransfer test_transfer(VI_BACKEND_OPENGL);
		test_transfer.Filename = "transfer_gl.png";
		test_transfer.Run();
	}
	{
		TestPushConstants test_push_constants(VI_BACKEND_VULKAN);
		test_push_constants.Filename = "push_constant_vk.png";
		test_push_constants.Run();
	}
	{
		TestPushConstants test_push_constants(VI_BACKEND_OPENGL);
		test_push_constants.Filename = "push_constant_gl.png";
		test_push_constants.Run();
	}
	{
		TestPipelineBlend test_pipeline_blend(VI_BACKEND_VULKAN);
		test_pipeline_blend.Filename = "pipeline_blend_vk.png";
		test_pipeline_blend.Run();
	}
	{
		TestPipelineBlend test_pipeline_blend(VI_BACKEND_OPENGL);
		test_pipeline_blend.Filename = "pipeline_blend_gl.png";
		test_pipeline_blend.Run();
	}

	// the MSE test driver can be done in either backend
	// NOTE: without golden images, it is possible that both backends are incorrect but identical renders
	TestDriver testDriver(VI_BACKEND_VULKAN);
	testDriver.AddMSETest("offline_compile_vk.png", "offline_compile_gl.png");
	testDriver.AddMSETest("glsl_builtins_vk.png", "glsl_builtins_gl.png");
	testDriver.AddMSETest("transfer_vk.png", "transfer_gl.png");
	testDriver.AddMSETest("push_constant_vk.png", "push_constant_gl.png");
	testDriver.AddMSETest("pipeline_blend_vk.png", "pipeline_blend_gl.png");
	testDriver.Run();

	return 0;
}