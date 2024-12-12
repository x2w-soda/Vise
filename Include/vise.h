#ifndef VISE_H
#define VISE_H

#ifdef WIN32
# define VI_PLATFORM_WIN32
# define VK_USE_PLATFORM_WIN32_KHR
#else
# error "vise unsupported platform"
#endif

#include <volk.h>
#include <vector>

#ifndef VI_API
#define VI_API extern
#endif

#define VI_DECLARE_HANDLE(HANDLE)\
	struct HANDLE ## Obj;\
	using HANDLE = HANDLE ## Obj *;

#define VI_NULL_HANDLE nullptr

VI_DECLARE_HANDLE(VIDevice);
VI_DECLARE_HANDLE(VIBuffer);
VI_DECLARE_HANDLE(VIImage);
VI_DECLARE_HANDLE(VIPass);
VI_DECLARE_HANDLE(VIModule);
VI_DECLARE_HANDLE(VISetLayout);
VI_DECLARE_HANDLE(VISetPool);
VI_DECLARE_HANDLE(VISet);
VI_DECLARE_HANDLE(VIPipelineLayout);
VI_DECLARE_HANDLE(VIPipeline);
VI_DECLARE_HANDLE(VIComputePipeline);
VI_DECLARE_HANDLE(VIFramebuffer);
VI_DECLARE_HANDLE(VICommand);

struct VISwapchainInfo;
struct VISubmitInfo;
struct VIPassInfo;
struct VIPassBeginInfo;
struct VIModuleInfo;
struct VISetPoolInfo;
struct VISetLayoutInfo;
struct VISetUpdateInfo;
struct VIPipelineInfo;
struct VIPipelineLayoutInfo;
struct VIComputePipelineInfo;
struct VIFramebufferInfo;
struct VIBufferInfo;
struct VIImageInfo;
struct VIDrawInfo;
struct VIPhysicalDevice;

// TODO: wrap
using VIQueue = VkQueue;
using VIFence = VkFence;
using VISemaphore = VkSemaphore;
using VICommandPool = VkCommandPool;

enum VIBackend
{
	VI_BACKEND_VULKAN,
	VI_BACKEND_OPENGL,
};

enum VIModuleTypeBit : uint32_t
{
	VI_MODULE_TYPE_VERTEX_BIT = 1,
	VI_MODULE_TYPE_FRAGMENT_BIT = 2,
	VI_MODULE_TYPE_COMPUTE_BIT = 4,
};
using VIModuleTypeFlags = uint32_t;

enum VISetBindingType
{
	VI_SET_BINDING_TYPE_UNIFORM_BUFFER,
	VI_SET_BINDING_TYPE_STORAGE_BUFFER,
	VI_SET_BINDING_TYPE_STORAGE_IMAGE,
	VI_SET_BINDING_TYPE_COMBINED_IMAGE_SAMPLER,
};

enum VIGLSLType
{
	VI_GLSL_TYPE_VEC2,
	VI_GLSL_TYPE_VEC3,
};

enum VIBufferType
{
	VI_BUFFER_TYPE_NONE,
	VI_BUFFER_TYPE_VERTEX,
	VI_BUFFER_TYPE_INDEX,
	VI_BUFFER_TYPE_UNIFORM,
	VI_BUFFER_TYPE_STORAGE,
};

enum VIImageType
{
	VI_IMAGE_TYPE_2D,
	VI_IMAGE_TYPE_2D_ARRAY,
	VI_IMAGE_TYPE_CUBE,
};

enum VIFormat
{
	VI_FORMAT_UNDEFINED,
	VI_FORMAT_RGBA8,
	VI_FORMAT_BGRA8,
	VI_FORMAT_D32F_S8U,
	VI_FORMAT_D24_S8U,
};

enum VIBufferUsageBit : uint32_t
{
	VI_BUFFER_USAGE_TRANSFER_SRC_BIT = 1,
	VI_BUFFER_USAGE_TRANSFER_DST_BIT = 2,
};
using VIBufferUsageFlags = uint32_t;

enum VIImageUsageBit : uint32_t
{
	VI_IMAGE_USAGE_TRANSFER_SRC_BIT = 1,
	VI_IMAGE_USAGE_TRANSFER_DST_BIT = 2,
	VI_IMAGE_USAGE_SAMPLED_BIT = 4,
	VI_IMAGE_USAGE_STORAGE_BIT = 8,
	VI_IMAGE_USAGE_COLOR_ATTACHMENT_BIT = 16,
	VI_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT = 32,
};
using VIImageUsageFlags = uint32_t;

enum VISamplerAddressMode
{
	VI_SAMPLER_ADDRESS_MODE_REPEAT,
	VI_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,
	VI_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
};

enum VIFilter
{
	VI_FILTER_LINEAR,
};

struct VIDeviceInfo
{
	void* window; // GLFWwindow* handle

	int desired_swapchain_framebuffer_count;

	struct
	{
		bool enable_validation_layers = true;
		
		// vulkan physical device selection policy
		int (*select_physical_device)(int pdevice_count, VIPhysicalDevice* pdevices);

		// vulkan swapchain configuration policy
		void (*configure_swapchain)(const VIPhysicalDevice* pdevice, void* window, VISwapchainInfo* out_info);
	} vulkan;
};

struct VIDeviceLimits
{
	int swapchain_framebuffer_count;
	int max_compute_workgroup_count[3];     // vi_cmd_dispatch dimension limits
	int max_compute_workgroup_size[3];      // vise GLSL workgroup local size limits
	int max_compute_workgroup_invocations;  // vise GLSL workgroup local size product limit
};

struct VIPhysicalDevice
{
	VkPhysicalDevice handle;
	VkPhysicalDeviceProperties device_props;
	VkSurfaceKHR surface;
	VkSurfaceCapabilitiesKHR surface_caps;
	VkPhysicalDeviceFeatures2 features;
	std::vector<VkFormat> depth_stencil_formats; // supported depth stencil formats with VK_IMAGE_TILING_OPTIMAL
	std::vector<VkQueueFamilyProperties> family_props;
	std::vector<VkExtensionProperties> ext_props;
	std::vector<VkSurfaceFormatKHR> surface_formats;
	std::vector<VkPresentModeKHR> present_modes;
};

struct VIPassBeginInfo
{
	VIPass pass;
	VIFramebuffer framebuffer;
	uint32_t color_clear_value_count;
	VkClearValue* color_clear_values;
	VkClearValue* depth_stencil_clear_value;
};

struct VISubmitInfo
{
	uint32_t cmd_count;
	uint32_t wait_count;
	uint32_t signal_count;
	VICommand* cmds;
	VISemaphore* signals;
	VISemaphore* waits;
	VkPipelineStageFlags* wait_stages;
};

struct VISwapchainInfo
{
	VkExtent2D image_extent;
	VkFormat image_format;
	VkFormat depth_stencil_format;
	VkPresentModeKHR present_mode;
	VkColorSpaceKHR image_color_space;
};

struct VIModuleInfo
{
	VIModuleTypeBit type;
	VIPipelineLayout pipeline_layout = nullptr;
	const char* vise_glsl = nullptr;
};

struct VIImageInfo
{
	VIImageType type;
	VIImageUsageFlags usage;
	VIFormat format;
	VkMemoryPropertyFlags properties;
	uint32_t width;
	uint32_t height;
	uint32_t layers = 1;
	VIFilter sampler_filter = VI_FILTER_LINEAR;
	VISamplerAddressMode sampler_address_mode = VI_SAMPLER_ADDRESS_MODE_REPEAT;
};

struct VIBufferInfo
{
	VIBufferType type;
	VIBufferUsageFlags usage;
	size_t size;
	VkMemoryPropertyFlags properties;
};

struct VISetBinding
{
	VISetBindingType type;
	uint32_t idx;
	uint32_t array_count;
};

struct VISetPoolResource
{
	VISetBindingType type;
	uint32_t count;
};

struct VISetPoolInfo
{
	uint32_t max_set_count;
	uint32_t resource_count;
	VISetPoolResource* resources;
};

struct VISetLayoutInfo
{
	uint32_t binding_count;
	const VISetBinding* bindings;
};

struct VISetUpdateInfo
{
	uint32_t binding;
	VIBuffer buffer = nullptr;
	VIImage image = nullptr;
};

struct VIVertexAttribute
{
	VIGLSLType type;       // attribute data type
	uint32_t offset;       // offset from start of vertex
	uint32_t binding;      // corresponding VIVertexBinding
};

struct VIVertexBinding
{
	VkVertexInputRate rate;  // vertex poll rate
	uint32_t stride;         // vertex stride
};

struct VIPipelineLayoutInfo
{
	uint32_t set_layout_count;
	const VISetLayout* set_layouts;
};

struct VIPipelineInfo
{
	uint32_t vertex_binding_count;
	uint32_t vertex_attribute_count;
	VIVertexAttribute* vertex_attributes;
	VIVertexBinding* vertex_bindings;
	VIPipelineLayout layout;
	VIModule vertex_module;
	VIModule fragment_module;
	VIPass pass;
};

struct VIComputePipelineInfo
{
	VIPipelineLayout layout;
	VIModule compute_module;
};

struct VISubpassColorAttachment
{
	uint32_t index; // references VIPassInfo::color_attachments[index]
	VkImageLayout layout;
};

// references VIPassInfo::depth_stencil_attachment
struct VISubpassDepthStencilAttachment
{
	VkImageLayout layout;
};

struct VISubpassInfo
{
	uint32_t color_attachment_ref_count;
	VISubpassColorAttachment* color_attachment_refs;
	VISubpassDepthStencilAttachment* depth_stencil_attachment_ref;
};

struct VIPassColorAttachment
{
	VIFormat color_format;
	VkAttachmentLoadOp color_load_op;
	VkAttachmentStoreOp color_store_op;
	VkImageLayout initial_layout;
	VkImageLayout final_layout;
};

struct VIPassDepthStencilAttachment
{
	VIFormat depth_stencil_format;
	VkAttachmentLoadOp depth_load_op;
	VkAttachmentStoreOp depth_store_op;
	VkAttachmentLoadOp stencil_load_op;
	VkAttachmentStoreOp stencil_store_op;
	VkImageLayout initial_layout;
	VkImageLayout final_layout;
};

struct VIPassInfo
{
	uint32_t color_attachment_count;
	uint32_t depenency_count;
	uint32_t subpass_count;
	VIPassColorAttachment* color_attachments;
	VIPassDepthStencilAttachment* depth_stencil_attachment;
	VkSubpassDependency* dependencies;
	VISubpassInfo* subpasses;
};

struct VIFramebufferInfo
{
	uint32_t width;
	uint32_t height;
	uint32_t color_attachment_count;
	VIImage* color_attachments;
	VIImage depth_stencil_attachment;
	VIPass pass;
};

struct VIImageMemoryBarrier
{
	VIImage image;
	VkAccessFlags src_access;
	VkAccessFlags dst_access;
	VkImageLayout old_layout;
	VkImageLayout new_layout;
	uint32_t src_family_index;
	uint32_t dst_family_index;
	VkImageSubresourceRange subresource_range;
};

struct VIDrawInfo
{
	uint32_t vertex_count;
	uint32_t vertex_start;
	uint32_t instance_count;
	uint32_t instance_start;
};

struct VIDrawIndexedInfo
{
	uint32_t index_count;
	uint32_t index_start;
	uint32_t instance_count;
	uint32_t instance_start;
};

VI_API VIDevice vi_create_device_vk(const VIDeviceInfo* info, VIDeviceLimits* limits);
VI_API VIDevice vi_create_device_gl(const VIDeviceInfo* info, VIDeviceLimits* limits);
VI_API void vi_destroy_device(VIDevice device);
VI_API VIPass vi_create_pass(VIDevice device, const VIPassInfo* info);
VI_API void vi_destroy_pass(VIDevice device, VIPass pass);
VI_API VIModule vi_create_module(VIDevice device, const VIModuleInfo* info);
VI_API void vi_destroy_module(VIDevice device, VIModule module);
VI_API VIBuffer vi_create_buffer(VIDevice device, const VIBufferInfo* info);
VI_API void vi_destroy_buffer(VIDevice device, VIBuffer buffer);
VI_API VIImage vi_create_image(VIDevice device, const VIImageInfo* info);
VI_API void vi_destroy_image(VIDevice device, VIImage image);
VI_API VISetPool vi_create_set_pool(VIDevice device, const VISetPoolInfo* info);
VI_API void vi_destroy_set_pool(VIDevice device, VISetPool pool);
VI_API VISetLayout vi_create_set_layout(VIDevice device, const VISetLayoutInfo* info);
VI_API void vi_destroy_set_layout(VIDevice device, VISetLayout layout);
VI_API VISet vi_alloc_set(VIDevice device, VISetPool pool, VISetLayout layout);
VI_API void vi_free_set(VIDevice device, VISet set);
VI_API VIPipelineLayout vi_create_pipeline_layout(VIDevice device, const VIPipelineLayoutInfo* info);
VI_API void vi_destroy_pipeline_layout(VIDevice device, VIPipelineLayout layout);
VI_API VIPipeline vi_create_pipeline(VIDevice device, const VIPipelineInfo* info);
VI_API void vi_destroy_pipeline(VIDevice device, VIPipeline pipeline);
VI_API VIComputePipeline vi_create_compute_pipeline(VIDevice device, const VIComputePipelineInfo* info);
VI_API void vi_destroy_compute_pipeline(VIDevice device, VIComputePipeline pipeline);
VI_API VIFramebuffer vi_create_framebuffer(VIDevice device, const VIFramebufferInfo* info);
VI_API void vi_destroy_framebuffer(VIDevice device, VIFramebuffer framebuffer);
VI_API VICommandPool vi_create_command_pool(VIDevice device, uint32_t family_idx, VkCommandPoolCreateFlags flags);
VI_API void vi_destroy_command_pool(VIDevice device, VICommandPool pool);
VI_API VICommand vi_alloc_command(VIDevice device, VICommandPool pool, VkCommandBufferLevel level);
VI_API void vi_free_command(VIDevice device, VICommand cmd);

VI_API void vi_device_wait_idle(VIDevice device);
VI_API const VIPhysicalDevice* vi_device_get_physical_device(VIDevice device); // TODO: remove? common limits?
VI_API uint32_t vi_device_get_graphics_family_index(VIDevice device); // TODO: remove?
VI_API VIQueue vi_device_get_graphics_queue(VIDevice device); // TODO: remove?
VI_API bool vi_device_has_depth_stencil_format(VIDevice device, VIFormat format, VkImageTiling tiling);
VI_API VIPass vi_device_get_swapchain_pass(VIDevice device);
VI_API VIFramebuffer vi_device_get_swapchain_framebuffer(VIDevice device, uint32_t index);
VI_API uint32_t vi_device_next_frame(VIDevice device, VISemaphore* image_acquired, VISemaphore* present_ready, VIFence* frame_complete);
VI_API void vi_device_present_frame(VIDevice device);
VI_API void vi_queue_wait_idle(VIQueue queue);
VI_API void vi_queue_submit(VIQueue queue, uint32_t submit_count, VISubmitInfo* submits, VIFence fence);
VI_API void vi_set_update(VISet set, uint32_t update_count, const VISetUpdateInfo* updates);

VI_API void* vi_buffer_map(VIBuffer buffer);
VI_API void vi_buffer_unmap(VIBuffer buffer);
VI_API void vi_buffer_flush_map(VIBuffer buffer);

VI_API void vi_reset_command(VICommand cmd);
VI_API void vi_cmd_begin_record(VICommand cmd, VkCommandBufferUsageFlags flags);
VI_API void vi_cmd_end_record(VICommand cmd);
VI_API void vi_cmd_copy_buffer(VICommand cmd, VIBuffer src, VIBuffer dst, uint32_t region_count, VkBufferCopy* regions);
VI_API void vi_cmd_copy_buffer_to_image(VICommand cmd, VIBuffer buffer, VIImage image, VkImageLayout layout, uint32_t region_count, VkBufferImageCopy* regions);
VI_API void vi_cmd_copy_image_to_buffer(VICommand cmd, VIImage image, VkImageLayout layout, VIBuffer buffer, uint32_t region_count, VkBufferImageCopy* regions);
// VI_API void vi_cmd_copy_color_attachment_to_buffer(VICommand cmd, VIFramebuffer framebuffer, VkImageLayout layout, uint32_t index, VIBuffer buffer);
// VI_API void vi_cmd_copy_depth_stencil_attachment_to_buffer(VICommand cmd, VIFramebuffer framebuffer, VIBuffer buffer);
VI_API void vi_cmd_begin_pass(VICommand cmd, const VIPassBeginInfo* info);
VI_API void vi_cmd_end_pass(VICommand cmd);
VI_API void vi_cmd_bind_pipeline(VICommand cmd, VIPipeline pipeline);
VI_API void vi_cmd_bind_compute_pipeline(VICommand cmd, VIComputePipeline pipeline);
VI_API void vi_cmd_dispatch(VICommand cmd, uint32_t workgroup_x, uint32_t workgroup_y, uint32_t workgroup_z);
VI_API void vi_cmd_bind_vertex_buffers(VICommand cmd, uint32_t first_binding, uint32_t binding_count, VIBuffer* buffers);
VI_API void vi_cmd_bind_index_buffer(VICommand cmd, VIBuffer buffer, VkIndexType index_type);
VI_API void vi_cmd_bind_set(VICommand cmd, uint32_t set_idx, VISet set, VIPipeline pipeline);
VI_API void vi_cmd_bind_set(VICommand cmd, uint32_t set_idx, VISet set, VIComputePipeline pipeline);
VI_API void vi_cmd_set_viewport(VICommand cmd, VkViewport viewport);
VI_API void vi_cmd_set_scissor(VICommand cmd, VkRect2D scissor);
VI_API void vi_cmd_draw(VICommand cmd, const VIDrawInfo* info);
VI_API void vi_cmd_draw_indexed(VICommand cmd, const VIDrawIndexedInfo* info);
VI_API void vi_cmd_pipeline_barrier_image_memory(VICommand cmd, VkPipelineStageFlags src_stages, VkPipelineStageFlags dst_stages, VkDependencyFlags deps, uint32_t barrier_count, VIImageMemoryBarrier* barriers);

// TODO: this should probably be done by user
VI_API VIBuffer vi_util_create_buffer_staged(VIDevice device, VIBufferInfo* info, void* data);
VI_API VIImage vi_util_create_image_staged(VIDevice device, VIImageInfo* info, void* data, VkImageLayout image_layout);
VI_API void vi_util_cmd_image_layout_transition(VICommand cmd, VIImage image, VkImageLayout old_layout, VkImageLayout new_layout);

#endif // VISE_H