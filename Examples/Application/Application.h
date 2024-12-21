#pragma once

#include <algorithm>
#include <vise.h>
#include <glm/gtc/matrix_transform.hpp>
#include <GLFW/glfw3.h>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

#define APP_DESIRED_FRAMES_IN_FLIGHT   2
#define APP_WINDOW_WIDTH               1280
#define APP_WINDOW_HEIGHT              720
#define APP_WINDOW_ASPECT_RATIO        ((float)APP_WINDOW_WIDTH / (float)APP_WINDOW_HEIGHT)

#define ARRAY_LEN(A) (sizeof(A) / sizeof(*(A)))

class Camera
{
public:

	glm::vec3 GetPosition()
	{
		return mPosition;
	}
	
	void SetPosition(const glm::vec3 pos)
	{
		mPosition = pos;
	}

	void MoveLocalForward(float forward)
	{
		mPosition += mDirection * forward;
	}

	void MoveLocalUp(float up)
	{
		mPosition += mLocalUp * up;
	}

	void MoveLocalRight(float right)
	{
		mPosition += mLocalRight * right;
	}

	void RotateLocal(float pitch, float yaw)
	{
		mPitch = std::clamp(mPitch + pitch, -89.0f, 89.0f);
		mYaw += yaw;

		if (mYaw > 360.0f)
			mYaw -= 360.0f;
		if (mYaw < 0.0f)
			mYaw += 360.0f;
	}

	void Update()
	{
		glm::vec3 direction;
		direction.x = std::cos(glm::radians(mYaw)) * std::cos(glm::radians(mPitch));
		direction.y = std::sin(glm::radians(mPitch));
		direction.z = std::sin(glm::radians(mYaw)) * std::cos(glm::radians(mPitch));

		mDirection = glm::normalize(direction);
		mLocalRight = glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), mDirection);
		mLocalUp = glm::cross(mDirection, mLocalRight);

		glm::vec3 target = mPosition + mDirection;

		mView = glm::lookAt(mPosition, target, glm::vec3(0.0f, 1.0f, 0.0f));
		mProj = glm::perspective(glm::radians(fov), aspect, 0.1f, 100.0f);
	}

	glm::vec3 GetDirection()
	{
		return mDirection;
	}

	glm::mat4 GetViewMat()
	{
		return mView;
	}

	glm::mat4 GetProjMat()
	{
		return mProj;
	}

	float fov = 30.0f;
	float aspect = APP_WINDOW_ASPECT_RATIO;

private:
	glm::vec3 mDirection;
	glm::vec3 mLocalRight;
	glm::vec3 mLocalUp;
	glm::vec3 mPosition;
	glm::mat4 mView;
	glm::mat4 mProj;
	float mPitch = 0.0f;
	float mYaw = 0.0f;
};

class Application
{
public:
	Application() = delete;
	Application(const Application&) = delete;
	Application(const char* name, VIBackend backend, bool create_visible = true);
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

	// helper to reduce set layout creation verbosity
	VISetLayout CreateSetLayout(const std::initializer_list<VISetBinding>& list);

	// helper to reduce set pool creation verbosity
	VISetPool CreateSetPool(uint32_t max_sets, const std::initializer_list<VISetPoolResource>& list);

	// helper to reduce pipeline layout creation verbosity
	VIPipelineLayout CreatePipelineLayout(const std::initializer_list<VISetLayout>& list, uint32_t push_constant_size = 0);

	// helper to reduce shader module creation verbosity
	VIModule CreateModule(VIPipelineLayout layout, VIModuleTypeBit type, const char* vise_glsl);

	// helper to reduce set allocation verbosity
	VISet AllocAndUpdateSet(VISetPool pool, VISetLayout layout, const std::initializer_list<VISetUpdateInfo>& updates);

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

	// helper to reduce render pass color attachment verbosity
	VIPassColorAttachment MakePassColorAttachment(VIFormat format, VkAttachmentLoadOp load_op, VkAttachmentStoreOp store_op,
		VkImageLayout initial_layout, VkImageLayout final_layout);

	// helper to reduce subpass dependency verbosity
	VkSubpassDependency MakeSubpassDependency(uint32_t src_subpass, VkPipelineStageFlags src_stages, VkAccessFlags src_access,
		uint32_t dst_subpass, VkPipelineStageFlags dst_stages, VkAccessFlags dst_access);

	// helper to reduce transfer verbosity
	VkBufferImageCopy MakeBufferImageCopy2D(VkImageAspectFlags aspect, uint32_t width, uint32_t height);

protected:
	bool mIsFirstFrame = false;
	int mFramesInFlight;
	double mFrameTimeDelta;
	double mFrameTimeThisFrame;
	double mFrameTimePrevFrame;
	const char* mName;
	GLFWwindow* mWindow;
	VIDevice mDevice;
	VIDeviceLimits mDeviceLimits;
	VIBackend mBackend;
	Camera mCamera;

private:
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