#include <iostream>
#include <filesystem>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_vulkan.h>
#include "Application.h"

Application* Application::sInstance = nullptr;

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
	deviceI.vulkan.enable_validation_layers = true; // TODO: disable in release build

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

VISetLayout Application::CreateSetLayout(const std::initializer_list<VISetBinding>& list)
{
	VISetLayoutInfo info;
	info.binding_count = list.size();
	info.bindings = list.begin();

	return vi_create_set_layout(mDevice, &info);
}

VISetPool Application::CreateSetPool(uint32_t max_sets, const std::initializer_list<VISetPoolResource>& list)
{
	VISetPoolInfo info;
	info.max_set_count = max_sets;
	info.resource_count = list.size();
	info.resources = list.begin();

	return vi_create_set_pool(mDevice, &info);
}

VIPipelineLayout Application::CreatePipelineLayout(const std::initializer_list<VISetLayout>& list, uint32_t push_constant_size)
{
	VIPipelineLayoutInfo info;
	info.set_layout_count = list.size();
	info.set_layouts = list.begin();
	info.push_constant_size = push_constant_size;

	return vi_create_pipeline_layout(mDevice, &info);
}

VIModule Application::CreateModule(VIPipelineLayout layout, VIModuleTypeBit type, const char* vise_glsl)
{
	VIModuleInfo info;
	info.pipeline_layout = layout;
	info.type = type;
	info.vise_glsl = vise_glsl;

	return vi_create_module(mDevice, &info);
}

VISet Application::AllocAndUpdateSet(VISetPool pool, VISetLayout layout, const std::initializer_list<VISetUpdateInfo>& updates)
{
	VISet set = vi_alloc_set(mDevice, pool, layout);
	vi_set_update(set, updates.size(), updates.begin());

	return set;
}

VkViewport Application::MakeViewport(float width, float height)
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

VkRect2D Application::MakeScissor(uint32_t width, uint32_t height)
{
	VkRect2D scissor;
	scissor.offset.x = 0;
	scissor.offset.y = 0;
	scissor.extent.width = width;
	scissor.extent.height = height;

	return scissor;
}

VkClearValue Application::MakeClearDepthStencil(float depth, uint32_t stencil)
{
	VkClearValue value;
	value.depthStencil.depth = depth;
	value.depthStencil.stencil = stencil;

	return value;
}

VkClearValue Application::MakeClearColor(float r, float g, float b, float a)
{
	VkClearValue value;
	value.color.float32[0] = r;
	value.color.float32[1] = g;
	value.color.float32[2] = b;
	value.color.float32[3] = a;

	return value;
}

VIImageInfo Application::MakeImageInfo2D(VIFormat format, uint32_t width, uint32_t height, VkMemoryPropertyFlags properties)
{
	VIImageInfo imageI;
	imageI.type = VI_IMAGE_TYPE_2D;
	imageI.usage = 0;
	imageI.layers = 1;
	imageI.format = format;
	imageI.width = width;
	imageI.height = height;
	imageI.properties = properties;
	imageI.sampler_address_mode = VI_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	imageI.sampler_filter = VI_FILTER_LINEAR;

	return imageI;
}

VIPassColorAttachment Application::MakePassColorAttachment(VIFormat format, VkAttachmentLoadOp load_op, VkAttachmentStoreOp store_op, VkImageLayout initial_layout, VkImageLayout final_layout)
{
	VIPassColorAttachment pass_color_attachment;
	pass_color_attachment.color_format = format;
	pass_color_attachment.color_load_op = load_op;
	pass_color_attachment.color_store_op = store_op;
	pass_color_attachment.initial_layout = initial_layout;
	pass_color_attachment.final_layout = final_layout;

	return pass_color_attachment;
}

VkSubpassDependency Application::MakeSubpassDependency(
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

VkBufferImageCopy Application::MakeBufferImageCopy2D(VkImageAspectFlags aspect, uint32_t width, uint32_t height)
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

void Application::PrintDeviceLimits(const VIDeviceLimits& limits)
{
	printf("== vise device limits (%s):\n", mBackend == VI_BACKEND_VULKAN ? "Vulkan" : "OpenGL");
	printf(" - swapchain framebuffer count %d\n", (int)limits.swapchain_framebuffer_count);
	printf(" - max push constant size %d\n", (int)limits.max_push_constant_size);
	printf(" - max compute workgroup count (%d, %d, %d)\n", (int)limits.max_compute_workgroup_count[0], (int)limits.max_compute_workgroup_count[1], (int)limits.max_compute_workgroup_count[2]);
	printf(" - max compute workgroup size  (%d, %d, %d)\n", (int)limits.max_compute_workgroup_size[0], (int)limits.max_compute_workgroup_size[1], (int)limits.max_compute_workgroup_size[2]);
	printf(" - max compute workgroup invocations %d\n", (int)limits.max_compute_workgroup_invocations);
}