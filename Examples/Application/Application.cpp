#include <iostream>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_vulkan.h>
#include "Application.h"
#include "Common.h"

Application* Application::sInstance = nullptr;

VISetLayout CreateSetLayout(VIDevice device, const std::initializer_list<VIBinding>& list)
{
	VISetLayoutInfo info;
	info.binding_count = list.size();
	info.bindings = list.begin();

	return vi_create_set_layout(device, &info);
}

VISetPool CreateSetPool(VIDevice device, uint32_t max_sets, const std::initializer_list<VISetPoolResource>& list)
{
	VISetPoolInfo info;
	info.max_set_count = max_sets;
	info.resource_count = list.size();
	info.resources = list.begin();

	return vi_create_set_pool(device, &info);
}

VIPipelineLayout CreatePipelineLayout(VIDevice device, const std::initializer_list<VISetLayout>& list, uint32_t push_constant_size)
{
	VIPipelineLayoutInfo info;
	info.set_layout_count = list.size();
	info.set_layouts = list.begin();
	info.push_constant_size = push_constant_size;

	return vi_create_pipeline_layout(device, &info);
}

VIModule CreateModule(VIDevice device, VIPipelineLayout layout, VIModuleType type, const char* vise_glsl)
{
	VIModuleInfo info;
	info.pipeline_layout = layout;
	info.type = type;
	info.vise_glsl = vise_glsl;

	return vi_create_module(device, &info);
}

VIModule CreateOrLoadModule(VIDevice device, VIBackend backend, VIPipelineLayout layout, VIModuleType type, const char* vise_glsl, const char* name)
{
	Timer timer;
	timer.Start();

	std::string glsl(vise_glsl);
	std::string path(name);

	path += backend == VI_BACKEND_VULKAN ? "_vk" : "_gl";
	std::string path_to_hash = path;
	std::string path_to_binary = path;
	std::stringstream ss;

	path_to_hash += ".txt";
	path_to_binary += ".bin";

	ss << std::hash<std::string>{}(glsl);
	std::string glsl_hash = ss.str();
	bool use_disk_binary = false;

	std::ifstream hash_file(path_to_hash.c_str(), std::ios::binary | std::ios::ate);
	std::ifstream binary_file(path_to_binary.c_str(), std::ios::binary | std::ios::ate);
	std::vector<char> binary;

	if (hash_file && binary_file)
	{
		std::streampos end = hash_file.tellg();
		hash_file.seekg(0, std::ios::beg);

		size_t size = static_cast<size_t>(end - hash_file.tellg());
		std::string disk_hash;

		disk_hash.resize(size);
		if (hash_file.read(disk_hash.data(), disk_hash.size()))
			use_disk_binary = disk_hash == glsl_hash;
	}

	VIModuleInfo moduleI;
	moduleI.type = type;
	moduleI.pipeline_layout = layout;
	moduleI.vise_glsl = nullptr;

	VIModule result = VI_NULL;

	if (use_disk_binary)
	{
		std::streampos end = binary_file.tellg();
		binary_file.seekg(0, std::ios::beg);

		size_t size = static_cast<size_t>(end - binary_file.tellg());
		binary.resize(size);
		binary_file.read(binary.data(), binary.size());

		moduleI.vise_binary = binary.data();
		result = vi_create_module(device, &moduleI);
	}
	else
	{
		uint32_t binary_size;
		char* binary = vi_compile_binary(device, type, layout, vise_glsl, &binary_size);

		std::ofstream out_hash_file;
		out_hash_file.open(path_to_hash);
		out_hash_file.write(glsl_hash.data(), glsl_hash.size());
		out_hash_file.close();

		std::ofstream out_binary_file;
		out_binary_file.open(path_to_binary, std::ios::out | std::ios::binary);
		out_binary_file.write(binary, binary_size);
		out_binary_file.close();

		moduleI.vise_binary = binary;
		result = vi_create_module(device, &moduleI);
		vi_free(binary);
	}

	timer.Stop();
	std::cout << (use_disk_binary ? "loaded " : "created ") << "module " << name << " (" << timer.GetMilliSeconds() << " ms)" << std::endl;

	return result;
}

VISet AllocAndUpdateSet(VIDevice device, VISetPool pool, VISetLayout layout, const std::initializer_list<VISetUpdateInfo>& updates)
{
	VISet set = vi_allocate_set(device, pool, layout);
	vi_set_update(set, updates.size(), updates.begin());

	return set;
}

VkViewport MakeViewport(float width, float height)
{
	VkViewport viewport;
	viewport.x = 0;
	viewport.y = 0;
	viewport.width = width;
	viewport.height = height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	return viewport;
}

VkRect2D MakeScissor(uint32_t width, uint32_t height)
{
	VkRect2D scissor;
	scissor.offset.x = 0;
	scissor.offset.y = 0;
	scissor.extent.width = width;
	scissor.extent.height = height;

	return scissor;
}

VkClearValue MakeClearDepthStencil(float depth, uint32_t stencil)
{
	VkClearValue value;
	value.depthStencil.depth = depth;
	value.depthStencil.stencil = stencil;

	return value;
}

VkClearValue MakeClearColor(float r, float g, float b, float a)
{
	VkClearValue value;
	value.color.float32[0] = r;
	value.color.float32[1] = g;
	value.color.float32[2] = b;
	value.color.float32[3] = a;

	return value;
}

VIImageInfo MakeImageInfo2D(VIFormat format, uint32_t width, uint32_t height, VkMemoryPropertyFlags properties)
{
	VIImageInfo imageI{};
	imageI.type = VI_IMAGE_TYPE_2D;
	imageI.usage = 0;
	imageI.layers = 1;
	imageI.levels = 1;
	imageI.format = format;
	imageI.width = width;
	imageI.height = height;
	imageI.properties = properties;

	return imageI;
}

VIImageInfo MakeImageInfoCube(VIFormat format, uint32_t dim, VkMemoryPropertyFlags properties)
{
	VIImageInfo imageI{};
	imageI.type = VI_IMAGE_TYPE_CUBE;
	imageI.usage = 0;
	imageI.layers = 6;
	imageI.levels = 1;
	imageI.format = format;
	imageI.width = dim;
	imageI.height = dim;
	imageI.properties = properties;

	return imageI;
}

VIPassColorAttachment MakePassColorAttachment(VIFormat format, VkAttachmentLoadOp load_op, VkAttachmentStoreOp store_op, VkImageLayout initial_layout, VkImageLayout final_layout)
{
	VIPassColorAttachment pass_color_attachment;
	pass_color_attachment.color_format = format;
	pass_color_attachment.color_load_op = load_op;
	pass_color_attachment.color_store_op = store_op;
	pass_color_attachment.initial_layout = initial_layout;
	pass_color_attachment.final_layout = final_layout;

	return pass_color_attachment;
}

VIPassDepthStencilAttachment MakePassDepthAttachment(VIFormat depth_format, VkAttachmentLoadOp depth_load_op, VkAttachmentStoreOp depth_store_op, VkImageLayout initial_layout, VkImageLayout final_layout)
{
	VIPassDepthStencilAttachment pass_depth_attachment{};
	pass_depth_attachment.depth_stencil_format = depth_format;
	pass_depth_attachment.depth_load_op = depth_load_op;
	pass_depth_attachment.depth_store_op = depth_store_op;
	pass_depth_attachment.stencil_load_op = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	pass_depth_attachment.stencil_store_op = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	pass_depth_attachment.initial_layout = initial_layout;
	pass_depth_attachment.final_layout = final_layout;

	return pass_depth_attachment;
}

VkSubpassDependency MakeSubpassDependency(
	uint32_t src_subpass, VkPipelineStageFlags src_stages, VkAccessFlags src_access,
	uint32_t dst_subpass, VkPipelineStageFlags dst_stages, VkAccessFlags dst_access)
{
	VkSubpassDependency dependency;
	dependency.srcSubpass = src_subpass;
	dependency.dstSubpass = dst_subpass;
	dependency.srcAccessMask = src_access;
	dependency.dstAccessMask = dst_access;
	dependency.srcStageMask = src_stages;
	dependency.dstStageMask = dst_stages;
	dependency.dependencyFlags = 0;

	return dependency;
}

VkBufferImageCopy MakeBufferImageCopy2D(VkImageAspectFlags aspect, uint32_t width, uint32_t height)
{
	VkBufferImageCopy region;
	region.bufferImageHeight = 0;
	region.bufferOffset = 0;
	region.bufferRowLength = 0;
	region.imageExtent = { width, height, 1 };
	region.imageOffset = { 0, 0, 0 };
	region.imageSubresource.aspectMask = aspect;
	region.imageSubresource.layerCount = 1;
	region.imageSubresource.baseArrayLayer = 0;
	region.imageSubresource.mipLevel = 0;

	return region;
}

VIBuffer CreateBufferStaged(VIDevice device, const VIBufferInfo* info, const void* data)
{
	assert(info->usage & VI_BUFFER_USAGE_TRANSFER_DST_BIT);
	assert(info->properties & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	VIBufferInfo stagingBufferI;
	stagingBufferI.type = info->type;
	stagingBufferI.size = info->size;
	stagingBufferI.usage = VI_BUFFER_USAGE_TRANSFER_SRC_BIT;
	stagingBufferI.properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

	VIBuffer srcBuffer = vi_create_buffer(device, &stagingBufferI);
	VIBuffer dstBuffer = vi_create_buffer(device, info);

	vi_buffer_map(srcBuffer);
	vi_buffer_map_write(srcBuffer, 0, info->size, data);
	vi_buffer_unmap(srcBuffer);

	uint32_t family = vi_device_get_graphics_family_index(device);
	VICommandPool pool = vi_create_command_pool(device, family, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT);
	VICommand cmd = vi_allocate_primary_command(device, pool);
	vi_begin_command(cmd, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
	{
		VkBufferCopy region;
		region.size = info->size;
		region.srcOffset = 0;
		region.dstOffset = 0;
		vi_cmd_copy_buffer(cmd, srcBuffer, dstBuffer, 1, &region);
	}
	vi_end_command(cmd);

	VISubmitInfo submitI{};
	submitI.cmd_count = 1;
	submitI.cmds = &cmd;
	VIQueue queue = vi_device_get_graphics_queue(device);
	vi_queue_submit(queue, 1, &submitI, VI_NULL);
	vi_queue_wait_idle(queue);
	vi_free_command(device, cmd);
	vi_destroy_command_pool(device, pool);

	vi_destroy_buffer(device, srcBuffer);

	return dstBuffer;
}

VIImage CreateImageStaged(VIDevice device, const VIImageInfo* info, const void* data, VkImageLayout image_layout)
{
	//assert(info->format == VI_FORMAT_RGBA8);
	assert(info->usage & VI_IMAGE_USAGE_TRANSFER_DST_BIT);
	assert(info->properties & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	size_t texelSize = info->format == VI_FORMAT_RGBA8 ? 4 : 16; // TODO: query
	size_t imageSize = info->width * info->height * texelSize * info->layers;

	VIBufferInfo stagingBufferI;
	stagingBufferI.type = VI_BUFFER_TYPE_TRANSFER;
	stagingBufferI.size = imageSize;
	stagingBufferI.usage = VI_BUFFER_USAGE_TRANSFER_SRC_BIT;
	stagingBufferI.properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

	VIBuffer srcBuffer = vi_create_buffer(device, &stagingBufferI);
	VIImage dstImage = vi_create_image(device, info);

	vi_buffer_map(srcBuffer);
	vi_buffer_map_write(srcBuffer, 0, imageSize, data);
	vi_buffer_unmap(srcBuffer);

	uint32_t family = vi_device_get_graphics_family_index(device);
	VICommandPool pool = vi_create_command_pool(device, family, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT);
	VICommand cmd = vi_allocate_primary_command(device, pool);
	vi_begin_command(cmd, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
	{
		CmdImageLayoutTransition(cmd, dstImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, info->layers);

		VkBufferImageCopy region = MakeBufferImageCopy2D(VK_IMAGE_ASPECT_COLOR_BIT, info->width, info->height);
		region.imageSubresource.layerCount = info->layers;
		vi_cmd_copy_buffer_to_image(cmd, srcBuffer, dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

		CmdImageLayoutTransition(cmd, dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, image_layout, info->layers);
	}
	vi_end_command(cmd);

	VISubmitInfo submitI{};
	submitI.cmds = &cmd;
	submitI.cmd_count = 1;
	VIQueue queue = vi_device_get_graphics_queue(device);
	vi_queue_submit(queue, 1, &submitI, VI_NULL);
	vi_queue_wait_idle(queue);
	vi_free_command(device, cmd);
	vi_destroy_command_pool(device, pool);

	vi_destroy_buffer(device, srcBuffer);

	return dstImage;
}

void CmdImageLayoutTransition(VICommand cmd, VIImage image, VkImageLayout old_layout, VkImageLayout new_layout, uint32_t layers, uint32_t levels)
{
	// TODO: image aspect + mipmap level + array layers
	VIImageMemoryBarrier barrier{};
	barrier.old_layout = old_layout;
	barrier.new_layout = new_layout;
	barrier.src_family_index = VK_QUEUE_FAMILY_IGNORED;
	barrier.dst_family_index = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;
	barrier.subresource_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresource_range.baseMipLevel = 0;
	barrier.subresource_range.levelCount = levels;
	barrier.subresource_range.baseArrayLayer = 0;
	barrier.subresource_range.layerCount = layers;

	VkPipelineStageFlags src_stages = 0;
	VkPipelineStageFlags dst_stages = 0;

	if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
	{
		src_stages = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		dst_stages = VK_PIPELINE_STAGE_TRANSFER_BIT;
		barrier.src_access = 0;
		barrier.dst_access = VK_ACCESS_TRANSFER_WRITE_BIT;
	}
	else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
	{
		src_stages = VK_PIPELINE_STAGE_TRANSFER_BIT;
		dst_stages = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		barrier.src_access = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dst_access = VK_ACCESS_SHADER_READ_BIT;
	}
	else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL && new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
	{
		src_stages = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		dst_stages = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		barrier.src_access = 0;
		barrier.dst_access = VK_ACCESS_SHADER_READ_BIT;
	}
	else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && new_layout == VK_IMAGE_LAYOUT_GENERAL)
	{
		src_stages = VK_PIPELINE_STAGE_TRANSFER_BIT;
		dst_stages = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		barrier.src_access = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dst_access = 0;
	}
	else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && new_layout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
	{
		src_stages = VK_PIPELINE_STAGE_TRANSFER_BIT;
		dst_stages = VK_PIPELINE_STAGE_TRANSFER_BIT;
		barrier.src_access = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dst_access = VK_ACCESS_TRANSFER_READ_BIT;
	}
	else assert(0 && "unable to derive image memory barrier from new and old image layouts");

	vi_cmd_pipeline_barrier_image_memory(cmd, src_stages, dst_stages, 0, 1, &barrier);
}

Application::Application(const char* name, VIBackend backend, bool create_visible)
	: mName(name), mBackend(backend)
{
	sInstance = this;

	glfwInit();
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
	glfwWindowHint(GLFW_VISIBLE, create_visible);

	if (backend == VI_BACKEND_OPENGL)
	{
		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
		glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	}

	mWindow = glfwCreateWindow(APP_WINDOW_WIDTH, APP_WINDOW_HEIGHT, mName, nullptr, nullptr);
	glfwMakeContextCurrent(mWindow);
	glfwSwapInterval(1); // only relevant on OpenGL backend

	std::cout << "application:  " << name << std::endl;
	std::cout << "current path: " << std::filesystem::current_path() << std::endl;

	VIDeviceInfo deviceI;
	deviceI.window = (void*)mWindow;
	deviceI.desired_swapchain_framebuffer_count = APP_DESIRED_FRAMES_IN_FLIGHT;
	deviceI.vulkan.configure_swapchain = nullptr;
	deviceI.vulkan.select_physical_device = nullptr;
#if !defined(NDEBUG)
	deviceI.vulkan.enable_validation_layers = true;
#else
	deviceI.vulkan.enable_validation_layers = false;
#endif

	if (backend == VI_BACKEND_VULKAN)
	{
		mDevice = vi_create_device_vk(&deviceI, &mDeviceLimits);
		ImGuiVulkanInit();
	}
	else
	{
		mDevice = vi_create_device_gl(&deviceI, &mDeviceLimits);
		ImGuiOpenGLInit();
	}

	// the actual hardware supported frames in flight may be different from what we asked for.
	mFramesInFlight = mDeviceLimits.swapchain_framebuffer_count;
}

Application::~Application()
{
	if (mBackend == VI_BACKEND_VULKAN)
		ImGuiVulkanShutdown();
	else
		ImGuiOpenGLShutdown();

	vi_destroy_device(mDevice);

	glfwDestroyWindow(mWindow);
	glfwTerminate();
}

void Application::NewFrame()
{
	if (mIsFirstFrame)
	{
		mFrameTimePrevFrame = glfwGetTime();
		mIsFirstFrame = false;
	}

	mFrameTimeThisFrame = glfwGetTime();
	mFrameTimeDelta = mFrameTimeThisFrame - mFrameTimePrevFrame;
	mFrameTimePrevFrame = mFrameTimeThisFrame;

	glfwPollEvents();
}

void Application::CameraUpdate()
{
	static double xpos_prev;
	static double ypos_prev;

	double xpos, ypos;
	glfwGetCursorPos(mWindow, &xpos, &ypos);

	if (!mIsCameraCaptured)
	{
		xpos_prev = xpos;
		ypos_prev = ypos;
		mCamera.Update();
		return;
	}

	float dt = (float)mFrameTimeDelta;
	float speed = 3.0f;

	float xoffset = xpos - xpos_prev;
	float yoffset = ypos - ypos_prev;
	xpos_prev = xpos;
	ypos_prev = ypos;

	const float sensitivity = 0.1f;
	xoffset *= sensitivity;
	yoffset *= sensitivity;
	mCamera.RotateLocal(-yoffset, xoffset);

	if (glfwGetKey(mWindow, GLFW_KEY_W))
		mCamera.MoveLocalForward(speed * dt);
	else if (glfwGetKey(mWindow, GLFW_KEY_S))
		mCamera.MoveLocalForward(-speed * dt);

	if (glfwGetKey(mWindow, GLFW_KEY_A))
		mCamera.MoveLocalRight(speed * dt);
	else if (glfwGetKey(mWindow, GLFW_KEY_D))
		mCamera.MoveLocalRight(-speed * dt);

	if (glfwGetKey(mWindow, GLFW_KEY_Q))
		mCamera.MoveLocalUp(-speed * dt);
	else if (glfwGetKey(mWindow, GLFW_KEY_E))
		mCamera.MoveLocalUp(speed * dt);

	mCamera.Update();
}

void Application::CameraToggleCapture()
{
	if (mIsCameraCaptured)
	{
		mIsCameraCaptured = false;
		glfwSetInputMode(mWindow, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
	}
	else
	{
		mIsCameraCaptured = true;
		glfwSetInputMode(mWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	}
}

bool Application::CameraIsCaptured()
{
	return mIsCameraCaptured;
}

void Application::ImGuiNewFrame()
{
	if (mBackend == VI_BACKEND_OPENGL)
		ImGuiOpenGLNewFrame();
	else
		ImGuiVulkanNewFrame();
}

void Application::ImGuiRender(VICommand cmd)
{
	if (mBackend == VI_BACKEND_OPENGL)
		ImGuiOpenGLRender(cmd);
	else
		ImGuiVulkanRender(cmd);
}

uint64_t Application::ImGuiAddImage(VIImage image, VkImageLayout image_layout)
{
	uint64_t imgui_image;

	if (mBackend == VI_BACKEND_VULKAN)
	{
		VkImageView view = vi_image_unwrap_view(image);
		VkSampler sampler = vi_image_unwrap_sampler(image);
		VkDescriptorSet set = ImGui_ImplVulkan_AddTexture(sampler, view, image_layout);

		// cast the vulkan descriptor set (pointer type) into 64 bit handle for Dear ImGui
		imgui_image = (uint64_t)set;
	}
	else
	{
		// OpenGL GLuint handles are 32-bit, Dear ImGui uses 64 bit handles
		imgui_image = (uint64_t)vi_image_unwrap_gl(image);
	}

	return imgui_image;
}

void Application::ImGuiRemoveImage(uint64_t imgui_image)
{
	if (mBackend == VI_BACKEND_VULKAN)
	{
		// notify Dear ImGui to destroy the descriptor set
		ImGui_ImplVulkan_RemoveTexture((VkDescriptorSet)imgui_image);
	}
}

void Application::ImGuiOpenGLInit()
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();
	ImGui_ImplGlfw_InitForOpenGL(mWindow, true);
	ImGui_ImplOpenGL3_Init("#version 460");
}

void Application::ImGuiOpenGLShutdown()
{
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
}

void Application::ImGuiOpenGLNewFrame()
{
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
}

void Application::ImGuiOpenGLRender(VICommand cmd)
{
	vi_cmd_opengl_callback(cmd, [](void* data) {
		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
	}, nullptr);
}

void Application::ImGuiVulkanInit()
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();
	ImGui_ImplGlfw_InitForVulkan(mWindow, true);

	ImGui_ImplVulkan_InitInfo initI{};
	initI.Instance = vi_device_unwrap_instance(mDevice);
	initI.PhysicalDevice = vi_device_unwrap_physical(mDevice);
	initI.Device = vi_device_unwrap(mDevice);
	initI.QueueFamily = vi_device_get_graphics_family_index(mDevice);
	initI.Queue = vi_queue_unwrap(vi_device_get_graphics_queue(mDevice));
	initI.PipelineCache = VK_NULL_HANDLE;
	initI.DescriptorPool = VK_NULL_HANDLE;
	initI.DescriptorPoolSize = 256;
	initI.Allocator = nullptr;
	initI.MinImageCount = mDeviceLimits.swapchain_framebuffer_count;
	initI.ImageCount = mDeviceLimits.swapchain_framebuffer_count;
	initI.CheckVkResultFn = nullptr;
	initI.RenderPass = vi_pass_unwrap(vi_device_get_swapchain_pass(mDevice));
	ImGui_ImplVulkan_Init(&initI);
}

void Application::ImGuiVulkanShutdown()
{
	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
}

void Application::ImGuiVulkanNewFrame()
{
	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
}

void Application::ImGuiVulkanRender(VICommand cmd)
{
	ImGui::Render();
	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), vi_command_unwrap(cmd));
}

void Application::PrintDeviceLimits(const VIDeviceLimits& limits)
{
	printf("== vise device limits (%s):\n", mBackend == VI_BACKEND_VULKAN ? "Vulkan" : "OpenGL");
	printf(" - swapchain framebuffer count %d\n", (int)limits.swapchain_framebuffer_count);
	printf(" - max push constant size %d\n", (int)limits.max_push_constant_size);
	printf(" - max compute workgroup count (%d, %d, %d)\n", (int)limits.max_compute_workgroup_count[0], (int)limits.max_compute_workgroup_count[1], (int)limits.max_compute_workgroup_count[2]);
	printf(" - max compute workgroup size  (%d, %d, %d)\n", (int)limits.max_compute_workgroup_size[0], (int)limits.max_compute_workgroup_size[1], (int)limits.max_compute_workgroup_size[2]);
	printf(" - max compute workgroup invocations %d\n", (int)limits.max_compute_workgroup_invocations);
}
