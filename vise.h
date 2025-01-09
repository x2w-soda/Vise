#ifndef VISE_H
#define VISE_H

#ifdef WIN32
# define VI_PLATFORM_WIN32
# define VK_USE_PLATFORM_WIN32_KHR
#else
# error "vise unsupported platform"
#endif

#include <vector>

// Vise uses some Vulkan structs and uses Volk as meta loader.
// Make sure Extern/volk directory is in your include path.
#include <volk.h>

#ifndef VI_API
#define VI_API extern
#endif

#define VI_DECLARE_HANDLE(HANDLE)\
	struct HANDLE ## Obj;\
	using HANDLE = HANDLE ## Obj *;

#define VI_NULL nullptr

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
VI_DECLARE_HANDLE(VICommandPool);
VI_DECLARE_HANDLE(VIFence);
VI_DECLARE_HANDLE(VISemaphore);
VI_DECLARE_HANDLE(VIQueue);

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

enum VIBackend
{
	VI_BACKEND_VULKAN,
	VI_BACKEND_OPENGL,
};

enum VIModuleType
{
	VI_MODULE_TYPE_VERTEX,
	VI_MODULE_TYPE_FRAGMENT,
	VI_MODULE_TYPE_COMPUTE,
};

enum VISetBindingType
{
	VI_SET_BINDING_TYPE_UNIFORM_BUFFER,
	VI_SET_BINDING_TYPE_STORAGE_BUFFER,
	VI_SET_BINDING_TYPE_STORAGE_IMAGE,
	VI_SET_BINDING_TYPE_COMBINED_IMAGE_SAMPLER,
};

enum VIGLSLType
{
	VI_GLSL_TYPE_FLOAT,
	VI_GLSL_TYPE_VEC2,
	VI_GLSL_TYPE_VEC3,
	VI_GLSL_TYPE_VEC4,
	VI_GLSL_TYPE_DOUBLE,
	VI_GLSL_TYPE_DVEC2,
	VI_GLSL_TYPE_DVEC3,
	VI_GLSL_TYPE_DVEC4,
	VI_GLSL_TYPE_UINT,
	VI_GLSL_TYPE_UVEC2,
	VI_GLSL_TYPE_UVEC3,
	VI_GLSL_TYPE_UVEC4,
	VI_GLSL_TYPE_INT,
	VI_GLSL_TYPE_IVEC2,
	VI_GLSL_TYPE_IVEC3,
	VI_GLSL_TYPE_IVEC4,
	VI_GLSL_TYPE_BOOL,
	VI_GLSL_TYPE_BVEC2,
	VI_GLSL_TYPE_BVEC3,
	VI_GLSL_TYPE_BVEC4,
	VI_GLSL_TYPE_MAT4,
};

enum VIBufferType
{
	VI_BUFFER_TYPE_TRANSFER,
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
	VI_FORMAT_R8,
	VI_FORMAT_RGBA8,
	VI_FORMAT_BGRA8,
	VI_FORMAT_RG16F,
	VI_FORMAT_RGB16F,
	VI_FORMAT_RGBA16F,
	VI_FORMAT_RGB32F,
	VI_FORMAT_RGBA32F,
	VI_FORMAT_D32F_S8U,
	VI_FORMAT_D24_S8U,
	VI_FORMAT_D32F,
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
	VI_FILTER_NEAREST,
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
	uint32_t swapchain_framebuffer_count;
	uint32_t max_push_constant_size;
	uint32_t max_compute_workgroup_count[3];     // vi_cmd_dispatch dimension limits
	uint32_t max_compute_workgroup_size[3];      // vise GLSL workgroup local size limits
	uint32_t max_compute_workgroup_invocations;  // vise GLSL workgroup local size product limit
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
	VIModuleType type;
	VIPipelineLayout pipeline_layout = VI_NULL;
	const char* vise_glsl = nullptr;
	const char* vise_binary = nullptr;
};

struct VISamplerInfo
{
	VIFilter filter = VI_FILTER_LINEAR;
	VIFilter mipmap_filter = VI_FILTER_LINEAR;
	VISamplerAddressMode address_mode = VI_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	float min_lod = 0.0f;
	float max_lod = 1.0f;
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
	uint32_t levels = 1;
	VISamplerInfo sampler;
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
	const VISetPoolResource* resources;
};

struct VISetLayoutInfo
{
	uint32_t binding_count;
	const VISetBinding* bindings;
};

struct VISetUpdateInfo
{
	uint32_t binding;
	VIBuffer buffer = VI_NULL;
	VIImage image = VI_NULL;
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
	uint32_t push_constant_size;
	uint32_t set_layout_count;
	const VISetLayout* set_layouts;
};

struct VIPipelineLayoutData
{
	uint32_t push_constant_size;
	uint32_t set_layout_count;
	const VISetLayoutInfo* set_layouts;
};

enum VIBlendFactor
{
	VI_BLEND_FACTOR_ZERO,
	VI_BLEND_FACTOR_ONE,
	VI_BLEND_FACTOR_SRC_ALPHA,
	VI_BLEND_FACTOR_DST_ALPHA,
	VI_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
	VI_BLEND_FACTOR_ONE_MINUS_DST_ALPHA,
};

enum VIBlendOp
{
	VI_BLEND_OP_ADD,
	VI_BLEND_OP_SUBTRACT,
	VI_BLEND_OP_REVERSE_SUBTRACT,
	VI_BLEND_OP_MIN,
	VI_BLEND_OP_MAX,
};

struct VIPipelineBlendStateInfo
{
	bool enabled = false;
	VIBlendFactor src_color_factor;
	VIBlendFactor dst_color_factor;
	VIBlendFactor src_alpha_factor;
	VIBlendFactor dst_alpha_factor;
	VIBlendOp color_blend_op;
	VIBlendOp alpha_blend_op;
};

enum VICompareOp
{
	VI_COMPARE_OP_NEVER,
	VI_COMPARE_OP_LESS,
	VI_COMPARE_OP_EQUAL,
	VI_COMPARE_OP_LESS_OR_EQUAL,
	VI_COMPARE_OP_GREATER,
	VI_COMPARE_OP_NOT_EQUAL,
	VI_COMPARE_OP_GREATER_OR_EQUAL,
	VI_COMPARE_OP_ALWAYS,
};

struct VIPipelineDepthStencilStateInfo
{
	bool depth_test_enabled = true;
	bool depth_write_enabled = true;
	VICompareOp depth_compare_op = VI_COMPARE_OP_LESS;
};

struct VIPipelineInfo
{
	uint32_t vertex_binding_count;
	uint32_t vertex_attribute_count;
	VIVertexAttribute* vertex_attributes;
	VIVertexBinding* vertex_bindings;
	VIPipelineLayout layout;
	VIPipelineBlendStateInfo blend_state;
	VIPipelineDepthStencilStateInfo depth_stencil_state;
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

struct VIMemoryBarrier
{
	VkAccessFlags src_access;
	VkAccessFlags dst_access;
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

struct VIBufferMemoryBarrier
{
	VIBuffer buffer;
	VkAccessFlags src_access;
	VkAccessFlags dst_access;
	uint32_t src_family_index;
	uint32_t dst_family_index;
	uint32_t offset;
	uint32_t size;
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
VI_API VIFence vi_create_fence(VIDevice device, VkFenceCreateFlags flags);
VI_API void vi_wait_for_fences(VIDevice device, uint32_t fence_count, VIFence* fences, bool wait_all, uint64_t timeout);
VI_API void vi_destroy_fence(VIDevice device, VIFence fence);
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

VI_API void vi_buffer_map(VIBuffer buffer);
VI_API void* vi_buffer_map_read(VIBuffer buffer, uint32_t offset, uint32_t size);
VI_API void vi_buffer_map_write(VIBuffer buffer, uint32_t offset, uint32_t size, const void* write);
VI_API void vi_buffer_map_flush(VIBuffer buffer, uint32_t offset, uint32_t size);
VI_API void vi_buffer_map_invalidate(VIBuffer buffer, uint32_t offset, uint32_t size);
VI_API void vi_buffer_unmap(VIBuffer buffer);

VI_API void vi_reset_command(VICommand cmd);
VI_API void vi_begin_command(VICommand cmd, VkCommandBufferUsageFlags flags);
VI_API void vi_end_command(VICommand cmd);
VI_API void vi_cmd_opengl_callback(VICommand cmd, void (*callback)(void* data), void* data);
VI_API void vi_cmd_copy_buffer(VICommand cmd, VIBuffer src, VIBuffer dst, uint32_t region_count, const VkBufferCopy* regions);
VI_API void vi_cmd_copy_buffer_to_image(VICommand cmd, VIBuffer buffer, VIImage image, VkImageLayout layout, uint32_t region_count, const VkBufferImageCopy* regions);
VI_API void vi_cmd_copy_image(VICommand cmd, VIImage src, VkImageLayout src_layout, VIImage dst, VkImageLayout dst_layout, uint32_t region_count, const VkImageCopy* regions);
VI_API void vi_cmd_copy_image_to_buffer(VICommand cmd, VIImage image, VkImageLayout layout, VIBuffer buffer, uint32_t region_count, const VkBufferImageCopy* regions);
// VI_API void vi_cmd_copy_color_attachment_to_buffer(VICommand cmd, VIFramebuffer framebuffer, VkImageLayout layout, uint32_t index, VIBuffer buffer);
// VI_API void vi_cmd_copy_depth_stencil_attachment_to_buffer(VICommand cmd, VIFramebuffer framebuffer, VIBuffer buffer);
VI_API void vi_cmd_begin_pass(VICommand cmd, const VIPassBeginInfo* info);
VI_API void vi_cmd_end_pass(VICommand cmd);
VI_API void vi_cmd_bind_graphics_pipeline(VICommand cmd, VIPipeline pipeline);
VI_API void vi_cmd_bind_compute_pipeline(VICommand cmd, VIComputePipeline pipeline);
VI_API void vi_cmd_dispatch(VICommand cmd, uint32_t workgroup_x, uint32_t workgroup_y, uint32_t workgroup_z);
VI_API void vi_cmd_bind_vertex_buffers(VICommand cmd, uint32_t first_binding, uint32_t binding_count, VIBuffer* buffers);
VI_API void vi_cmd_bind_index_buffer(VICommand cmd, VIBuffer buffer, VkIndexType index_type);
VI_API void vi_cmd_bind_graphics_set(VICommand cmd, VIPipelineLayout layout, uint32_t set_idx, VISet set);
VI_API void vi_cmd_bind_compute_set(VICommand cmd, VIPipelineLayout layout, uint32_t set_idx, VISet set);
VI_API void vi_cmd_push_constants(VICommand cmd, VIPipelineLayout layout, uint32_t offset, uint32_t size, const void* value);
VI_API void vi_cmd_set_viewport(VICommand cmd, VkViewport viewport);
VI_API void vi_cmd_set_scissor(VICommand cmd, VkRect2D scissor);
VI_API void vi_cmd_draw(VICommand cmd, const VIDrawInfo* info);
VI_API void vi_cmd_draw_indexed(VICommand cmd, const VIDrawIndexedInfo* info);
VI_API void vi_cmd_pipeline_barrier_memory(VICommand cmd, VkPipelineStageFlags src_stages, VkPipelineStageFlags dst_stages, VkDependencyFlags deps, uint32_t barrier_count, const VIMemoryBarrier* barriers);
VI_API void vi_cmd_pipeline_barrier_image_memory(VICommand cmd, VkPipelineStageFlags src_stages, VkPipelineStageFlags dst_stages, VkDependencyFlags deps, uint32_t barrier_count, const VIImageMemoryBarrier* barriers);
VI_API void vi_cmd_pipeline_barrier_buffer_memory(VICommand cmd, VkPipelineStageFlags src_stages, VkPipelineStageFlags dst_stages, VkDependencyFlags deps, uint32_t barrier_count, const VIBufferMemoryBarrier* barriers);

VI_API char* vi_compile_binary_offline(VIBackend backend, VIModuleType type, const VIPipelineLayoutData* pipeline_layout, const char* vise_glsl, uint32_t* binary_size);
VI_API char* vi_compile_binary(VIDevice device, VIModuleType type, VIPipelineLayout pipeline_layout, const char* vise_glsl, uint32_t* binary_size);
VI_API void vi_free(void* data);

VI_API VkInstance vi_device_unwrap_instance(VIDevice device);
VI_API VkDevice vi_device_unwrap(VIDevice device);
VI_API VkPhysicalDevice vi_device_unwrap_physical(VIDevice device);
VI_API VkRenderPass vi_pass_unwrap(VIPass pass);
VI_API VkSemaphore vi_semaphore_unwrap(VISemaphore semaphore);
VI_API VkQueue vi_queue_unwrap(VIQueue queue);
VI_API VkCommandBuffer vi_command_unwrap(VICommand command);
VI_API VkBuffer vi_buffer_unwrap(VIBuffer buffer);
VI_API VkImage vi_image_unwrap(VIImage image);
VI_API VkImageView vi_image_unwrap_view(VIImage image);
VI_API VkSampler vi_image_unwrap_sampler(VIImage image);
VI_API uint32_t vi_image_unwrap_gl(VIImage image);

#endif // VISE_H