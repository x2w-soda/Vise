#pragma once

#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>
#include <GLFW/glfw3.h>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <vise.h>

#include "Common.h"

#define APP_DESIRED_FRAMES_IN_FLIGHT   2
#define APP_WINDOW_WIDTH               1600
#define APP_WINDOW_HEIGHT              900
#define APP_WINDOW_ASPECT_RATIO        ((float)APP_WINDOW_WIDTH / (float)APP_WINDOW_HEIGHT)

#ifdef WIN32
# define APP_PATH "../"
#else
# define APP_PATH ""
#endif

#define ARRAY_LEN(A) (sizeof(A) / sizeof(*(A)))

// helper to reduce set layout creation verbosity
VISetLayout CreateSetLayout(VIDevice device, const std::initializer_list<VIBinding>& list);

// helper to reduce set pool creation verbosity
VISetPool CreateSetPool(VIDevice device, uint32_t max_sets, const std::initializer_list<VISetPoolResource>& list);

// helper to reduce pipeline layout creation verbosity
VIPipelineLayout CreatePipelineLayout(VIDevice device, const std::initializer_list<VISetLayout>& list, uint32_t push_constant_size = 0);

// helper to reduce shader module creation verbosity
VIModule CreateModule(VIDevice device, VIPipelineLayout layout, VIModuleType type, const char* vise_glsl);

// helper to cache module on disk
VIModule CreateOrLoadModule(VIDevice device, VIBackend backend, VIPipelineLayout layout, VIModuleType type, const char* vise_glsl, const char* name);

// helper to reduce set allocation verbosity
VISet AllocAndUpdateSet(VIDevice device, VISetPool pool, VISetLayout layout, const std::initializer_list<VISetUpdateInfo>& updates);

// helper to reduce viewport verbosity
VkViewport MakeViewport(float width, float height);

// helper to reduce scissor verbosity
VkRect2D MakeScissor(uint32_t width, uint32_t height);

// helper to reduce clear depth stencil verbosity
VkClearValue MakeClearDepthStencil(float depth, uint32_t stencil);

// helper to reduce clear color verbosity
VkClearValue MakeClearColor(float r, float g, float b, float a);

// helper to reduce image creation verbosity
VIImageInfo MakeImageInfo2D(VIFormat format, uint32_t width, uint32_t height, VkMemoryPropertyFlags properties);
VIImageInfo MakeImageInfoCube(VIFormat format, uint32_t dim, VkMemoryPropertyFlags properties);

// helper to reduce render pass color attachment verbosity
VIPassColorAttachment MakePassColorAttachment(VIFormat format, VkAttachmentLoadOp load_op, VkAttachmentStoreOp store_op,
	VkImageLayout initial_layout, VkImageLayout final_layout);

// helper to reduce render pass depth attachment verbosity
VIPassDepthStencilAttachment MakePassDepthAttachment(VIFormat depth_format, VkAttachmentLoadOp depth_load_op, VkAttachmentStoreOp depth_store_op,
	VkImageLayout initial_layout, VkImageLayout final_layout);

// helper to reduce subpass dependency verbosity
VkSubpassDependency MakeSubpassDependency(uint32_t src_subpass, VkPipelineStageFlags src_stages, VkAccessFlags src_access,
	uint32_t dst_subpass, VkPipelineStageFlags dst_stages, VkAccessFlags dst_access);

// helper to reduce transfer verbosity
VkBufferImageCopy MakeBufferImageCopy2D(VkImageAspectFlags aspect, uint32_t width, uint32_t height);

VIBuffer CreateBufferStaged(VIDevice device, const VIBufferInfo* info, const void* data);

VIImage CreateImageStaged(VIDevice device, const VIImageInfo* info, const void* data, VkImageLayout image_layout);

// image layout transition via image memory barrier
void CmdImageLayoutTransition(VICommand cmd, VIImage image, VkImageLayout old_layout, VkImageLayout new_layout, uint32_t layers = 1, uint32_t levels = 1);

class VMAAllocator;

class Application
{
public:
	Application() = delete;
	Application(const Application&) = delete;
	Application(const char* name, VIBackend backend, bool visible = true, bool resizable = true);
	virtual ~Application();

	Application& operator=(const Application&) = delete;

	virtual void Run() = 0;

	static Application* Get()
	{
		return sInstance;
	}

protected:

	void NewFrame();
	void CameraUpdate();
	void CameraToggleCapture();
	bool CameraIsCaptured();
	void PrintDeviceLimits(const VIDeviceLimits& limits);

	// example on how to integrate Dear ImGui with both backends of Vise
	void ImGuiNewFrame();
	void ImGuiRender(VICommand cmd);
	uint64_t ImGuiAddImage(VIImage image, VkImageLayout image_layout);
	void ImGuiRemoveImage(uint64_t imgui_image);

	void ImGuiDeviceProfile();

protected:
	bool mIsFirstFrame = true;
	int mFramesInFlight;
	double mFrameTimeDelta;
	double mFrameTimeThisFrame;
	double mFrameTimePrevFrame;
	const char* mName;
	int mWindowWidth;
	int mWindowHeight;
	bool mWindowIsMinimized = false;
	GLFWwindow* mWindow;
	VIDevice mDevice;
	VIDeviceLimits mDeviceLimits;
	VIBackend mBackend;
	Camera mCamera;
	VMAAllocator* mVMAAllocator = nullptr;

private:
	static void WindowSizeCallback(GLFWwindow* window, int width, int height);

	void ImGuiOpenGLInit();
	void ImGuiOpenGLShutdown();
	void ImGuiOpenGLNewFrame();
	void ImGuiOpenGLRender(VICommand cmd);

	void ImGuiVulkanInit();
	void ImGuiVulkanShutdown();
	void ImGuiVulkanNewFrame();
	void ImGuiVulkanRender(VICommand cmd);

private:
	static Application* sInstance;
	bool mIsCameraCaptured = false;
};