#include <limits>
#include <iostream>
#include <vector>
#include <optional>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstdint>

#include <glslang/Public/ResourceLimits.h>
#include <glslang/SPIRV/GlslangToSpv.h>
#include <spirv_cross/spirv_cross.hpp>
#include <spirv_cross/spirv_glsl.hpp>

#include <vise.h>
#include <volk.h>
#include <glad/glad.h>

#if !defined(VI_BUILD_RELEASE) && !defined(NDEBUG)
# define VI_BUILD_DEBUG
#else
# define VI_BUILD_RELEASE
#endif

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#ifdef VI_PLATFORM_WIN32
 #define GLFW_EXPOSE_NATIVE_WIN32
 #include <GLFW/glfw3.h>
 #include <GLFW/glfw3native.h>
#else
# include <GLFW/glfw3.h>
#endif

#ifdef VI_BUILD_DEBUG
# ifdef VI_PLATFORM_WIN32
#  include <debugapi.h>
#  define VI_DEBUG_BREAK   DebugBreak()
# endif
#else
# define VI_DEBUG_BREAK
#endif

#ifdef VI_BUILD_RELEASE
# define VI_ASSERT
#else
# define VI_ASSERT(EXPR) do {\
	if (!(EXPR))\
	{\
		puts("VI_ASSERT FAILURE:\n" # EXPR);\
		VI_DEBUG_BREAK;\
		exit(EXIT_FAILURE);\
	}\
} while (0)
#endif

#define VI_UNREACHABLE do {\
	puts("VI_UNREACHABLE");\
	VI_DEBUG_BREAK;\
	exit(EXIT_FAILURE);\
} while (0)

#ifdef VI_BUILD_RELEASE
# define VK_CHECK(EXPR)   EXPR
#else
# define VK_CHECK(EXPR) do {\
	VkResult result_ = EXPR;\
    if (result_ != VK_SUCCESS)\
	{\
		printf("VkResult %d\n", result_);\
        VI_ASSERT(0);\
	}\
} while (0)
#endif

#ifdef VI_BUILD_RELEASE
# define GL_CHECK(EXPR)   EXPR
#else
# define GL_CHECK(EXPR) do {\
	EXPR;\
	GLenum err_ = glGetError();\
	if (err_ != 0)\
	{\
		printf("GLenum error %d\n", err_);\
		VI_ASSERT(0);\
	}\
} while (0)
#endif

#define VI_ARR_SIZE(ARR) (sizeof(ARR) / sizeof(*ARR))

#define VI_VK_API_VERSION             VK_API_VERSION_1_2
#define VI_VK_GLSLANG_VERSION         glslang::EShTargetVulkan_1_2
#define VI_SHADER_GLSL_VERSION        460
#define VI_SHADER_ENTRY_POINT         "main"
#define VI_GL_COMMAND_LIST_CAPACITY   16

// Normalize NDC Handedness:
//   OpenGL NDC is left-handed while Vulkan NDC is right-handed,
//   this is often described as the Vulkan positive Y-axis pointing
//   downwards instead of upwards. Vise uses OpenGL left-handed NDC:
//   Positive Y pointing upwards, Positive Z pointing into the screen.
// Normalize Depth (NDC Z-Axis)
//   glClipControl
// Normalize Screen Space Origin:
//   Vise uses top-left as screen space origin, same as vulkan.
//   In OpenGL, glViewport and glScissor parameters are processed.
// Normalize Texture UV Origin:
//   Vise uses top-left as texture uv origin, same as vulkan.
//   In OpneGL, uv origin is bottom left and textures appear flipped

struct VIVulkan;
struct VIFrame;
struct VIOpenGL;
struct GLPushConstant;
struct HostMalloc;
struct VICompileResult;
struct VIBinaryHeader;

// TODO: user dependency injection
void* vi_malloc(size_t size);
void vi_free(void* ptr);

enum VIImageFlagBits
{
	VI_IMAGE_FLAG_CREATED_IMAGE_BIT = 1,
	VI_IMAGE_FLAG_CREATED_IMAGE_VIEW_BIT = 2,
	VI_IMAGE_FLAG_CREATED_SAMPLER_BIT = 4,
};

struct VIObject
{
	VIObject() = default;
	~VIObject() = default;

	VIDevice device;
};

struct VIPassObj : VIObject
{
	std::optional<VIPassDepthStencilAttachment> depth_stencil_attachment;
	std::vector<VIPassColorAttachment> color_attachments;

	union
	{
		struct
		{
			VkRenderPass handle;
		} vk;
	};
};

struct VIModuleObj : VIObject
{
	VIModuleType type;

	union
	{
		struct
		{
			VkShaderModule handle;
		} vk;

		struct
		{
			uint32_t push_constant_count;
			GLPushConstant* push_constants;
			GLuint shader;
		} gl;
	};
};

struct GLCommand;

struct VICommandObj : VIObject
{
	VICommandPool pool;

	union
	{
		struct
		{
			VkCommandBuffer handle;
		} vk;

		struct
		{
			uint32_t list_capacity;
			uint32_t list_size;
			GLCommand* list;
			VIPipeline active_pipeline; // during recording
		} gl;
	};
};

struct VICommandPoolObj : VIObject
{
	VkCommandPool vk_handle;
};

struct VIFenceObj : VIObject
{
	VkFence vk_handle;
	bool gl_signal;
};

struct VISemaphoreObj : VIObject
{
	VkSemaphore vk_handle;
	bool gl_signal;
};

struct VIQueueObj : VIObject
{
	VkQueue vk_handle;
};

struct VIBufferObj : VIObject
{
	VIBufferType type;
	VIBufferUsageFlags usage;
	VkMemoryPropertyFlags properties;
	size_t size;
	uint8_t* map;
	bool is_mapped;

	union
	{
		struct
		{
			VkBuffer handle;
			VmaAllocation alloc;
		} vk;

		struct
		{
			GLuint handle;
			GLenum target;
		} gl;
	};
};

struct VIImageObj : VIObject
{
	VIImageObj() {}

	VIImageInfo info;
	uint32_t flags;

	union
	{
		struct
		{
			VkImage handle;
			VkImageView view_handle;
			VkSampler sampler_handle;
			VmaAllocation alloc;
		} vk;

		struct
		{
			GLuint handle;
			GLenum target;
			GLenum internal_format;
			GLenum data_format;
			GLenum data_type;
		} gl;
	};
};

struct VISetPoolObj : VIObject
{
	union
	{
		struct
		{
			VkDescriptorPool handle;
			VkDescriptorPoolCreateFlags flags;
		} vk;
	};
};

struct VISetLayoutObj : VIObject
{
	std::vector<VIBinding> bindings;

	union
	{
		struct
		{
			VkDescriptorSetLayout handle;
		} vk;
	};
};

struct GLRemap
{
	VIBindingType type;
	int vk_set_binding; // set 1 binding 3 -> 103
	int gl_binding;
};

struct GLPushConstant
{
	uint32_t size;
	uint32_t offset;
	uint32_t uniform_arr_size;
	VIGLSLType uniform_glsl_type;
	std::string uniform_name;

	size_t GetSerialSize() const
	{
		return sizeof(uint32_t) * 5 + uniform_name.size();
	}
};

struct VIPipelineLayoutObj : VIObject
{
	std::vector<VISetLayout> set_layouts;
	uint32_t push_constant_size;

	union
	{
		struct
		{
			VkPipelineLayout handle;
		} vk;

		struct
		{
			uint32_t remap_count;
			GLRemap* remaps;
		} gl;
	};
};

struct VISetObj : VIObject
{
	VISetPool pool;
	VISetLayout layout;

	union
	{
		struct
		{
			VkDescriptorSet handle;
		} vk;

		struct
		{
			void** binding_sites;
		} gl;
	};
};

struct VIPipelineObj : VIObject
{
	std::vector<VIVertexBinding> vertex_bindings;
	std::vector<VIVertexAttribute> vertex_attributes;
	VIPipelineLayout layout;
	VIPipelineBlendStateInfo blend_state;
	VIPipelineDepthStencilStateInfo depth_stencil_state;
	VIPipelineRasterizationStateInfo rasterization_state;
	VIModule vertex_module;
	VIModule fragment_module;

	union
	{
		struct
		{
			VkPipeline handle;
			VkFrontFace front_face;
		} vk;

		struct
		{
			GLuint program;
			GLuint vao;
		} gl;
	};
};

struct VIComputePipelineObj : VIObject
{
	VIPipelineLayout layout;
	VIModule compute_module;

	union
	{
		struct
		{
			VkPipeline handle;
		} vk;

		struct
		{
			GLuint program;
		} gl;
	};
};

struct VIFramebufferObj : VIObject
{
	VkExtent2D extent;
	std::vector<VIImage> color_attachments;
	VIImage depth_stencil_attachment;

	union
	{
		struct
		{
			VkFramebuffer handle;
		} vk;

		struct
		{
			GLuint handle;
		} gl;
	};
};

struct VIFrame
{
	struct
	{
		VIFenceObj frame_complete;
	} fence;

	struct
	{
		VISemaphoreObj image_acquired; // swapchain framebuffer of the current frame is ready for rendering
		VISemaphoreObj present_ready; // swapchain texture of the current frame is ready for presentation
	} semaphore;
};

struct GLSubmitInfo
{
	GLSubmitInfo(uint32_t cmd_count, uint32_t wait_count, uint32_t signal_count)
		: cmds(cmd_count), waits(wait_count), signals(signal_count) {}

	std::vector<VICommand> cmds;
	std::vector<VISemaphore> waits;
	std::vector<VISemaphore> signals;
};

// Vise OpenGL Context
struct VIOpenGL
{
	VIDevice vi_device;
	GLenum index_type;
	size_t index_size;
	GLuint active_program;  // during execution
	VIModule active_module; // during execution
	VIFramebuffer active_framebuffer;
	VIFrame frame;
	std::vector<GLSubmitInfo> submits;
};

// Vise Vulkan Context
struct VIVulkan
{
	VIDevice vi_device;
	VkDevice device;
	VmaAllocator vma;
	VIFrame* frames;
	uint32_t frame_idx;
	uint32_t frames_in_flight;
	uint32_t family_idx_graphics;
	uint32_t family_idx_transfer;
	uint32_t family_idx_present;
	std::vector<VkLayerProperties> layer_props;
	std::vector<VkExtensionProperties> ext_props;
	std::vector<VIPhysicalDevice> pdevices;
	VIPhysicalDevice* pdevice_chosen;
	VkInstance instance;
	VkSurfaceKHR surface;
	VkPhysicalDevice pdevice;
	VICommandPoolObj cmd_pool_graphics;
	bool pass_uses_swapchain_framebuffer;

	struct
	{
		VkSwapchainKHR handle;
		VkExtent2D image_extent;
		uint32_t image_idx;
		VkFormat image_format;
		VkFormat depth_stencil_format;
		std::vector<VkImage> image_handles;
		std::vector<VIImageObj> images;
		std::vector<VIImageObj> depth_stencils;
	} swapchain;
};

struct VICompileResult
{
	bool success;
	std::string error;
	std::string gl_patched;
	std::vector<GLPushConstant> gl_push_constants;
	std::vector<uint32_t> vk_spirv;
};

struct VIBinaryHeader
{
	uint32_t payload_size;  // byte size of payload, the payload is serialized as a byte stream
	uint32_t header_size;   // byte offset from header to payload
	uint32_t backend_type;  // VI_BACKEND_VULKAN or VI_BACKEND_OPENGL
	uint32_t module_type;   // VI_MODULE_TYPE_VERTEX, VI_MODULE_TYPE_FRAGMENT, or VI_MODULE_TYPE_COMPUTE
	uint32_t glpc_count;    // number of GLPushConstant entries before payload
};

static_assert(sizeof(VIBinaryHeader) == 20);

enum GLCommandType
{
	GL_COMMAND_TYPE_OPENGL_CALLBACK = 0,
	GL_COMMAND_TYPE_SET_VIEWPORT,
	GL_COMMAND_TYPE_SET_SCISSOR,
	GL_COMMAND_TYPE_DRAW,
	GL_COMMAND_TYPE_DRAW_INDEXED,
	GL_COMMAND_TYPE_PUSH_CONSTANTS,
	GL_COMMAND_TYPE_BIND_SET,
	GL_COMMAND_TYPE_BIND_PIPELINE,
	GL_COMMAND_TYPE_BIND_COMPUTE_PIPELINE,
	GL_COMMAND_TYPE_BIND_VERTEX_BUFFERS,
	GL_COMMAND_TYPE_BIND_INDEX_BUFFER,
	GL_COMMAND_TYPE_BEGIN_PASS,
	GL_COMMAND_TYPE_END_PASS,
	GL_COMMAND_TYPE_COPY_BUFFER,
	GL_COMMAND_TYPE_COPY_BUFFER_TO_IMAGE,
	GL_COMMAND_TYPE_COPY_IMAGE,
	GL_COMMAND_TYPE_COPY_IMAGE_TO_BUFFER,
	GL_COMMAND_TYPE_DISPATCH,
	GL_COMMAND_TYPE_ENUM_COUNT,
};

struct GLCommandOpenGLCallback
{
	void* data;
	void (*callback)(void* data);
};

struct GLCommandPushConstants
{
	GLCommandPushConstants(uint32_t offset_, uint32_t size_)
		: offset(offset_), size(size_)
	{
		value = (uint8_t*)vi_malloc(size);
	}

	~GLCommandPushConstants()
	{
		vi_free((void*)value);
	}

	uint32_t offset;
	uint32_t size;
	uint8_t* value;
};

struct GLCommandBindSet
{
	VISet set;
	uint32_t set_index;
	VIPipelineLayout pipeline_layout;
};

struct GLCommandBindVertexBuffers
{
	std::vector<VIBuffer> buffers;
	uint32_t first_binding;
	VIPipeline pipeline;
};

struct GLCommandBindIndexBuffer
{
	VIBuffer buffer;
	VkIndexType index_type;
};

struct GLCommandBeginPass
{
	VIPass pass;
	VIFramebuffer framebuffer;
	std::vector<VkClearValue> color_clear_values;
	std::optional<VkClearValue> depth_stencil_clear_value;
};

struct GLCommandCopyBuffer
{
	VIBuffer src;
	VIBuffer dst;
	std::vector<VkBufferCopy> regions;
};

struct GLCommandCopyImage
{
	VIImage src;
	VIImage dst;
	std::vector<VkImageCopy> regions;
};

struct GLCommandCopyImageToBuffer
{
	VIImage image;
	VIBuffer buffer;
	std::vector<VkBufferImageCopy> regions;
};

using GLCommandCopyBufferToImage = GLCommandCopyImageToBuffer;

struct GLCommandDispatch
{
	GLuint group_count_x;
	GLuint group_count_y;
	GLuint group_count_z;
};

// We can only store VIObject handles when recording GLCommands
// values such as VkClearValues must be copied and preserved until GLCommand execution
struct GLCommand
{
	GLCommandType type;

	union
	{
		GLCommandOpenGLCallback opengl_callback;
		VkViewport set_viewport;
		VkRect2D set_scissor;
		VIDrawInfo draw;
		VIDrawIndexedInfo draw_indexed;
		GLCommandPushConstants push_constants;
		GLCommandBindSet bind_set;
		VIPipeline bind_pipeline;
		VIComputePipeline bind_compute_pipeline;
		GLCommandBindVertexBuffers bind_vertex_buffers;
		GLCommandBindIndexBuffer bind_index_buffer;
		GLCommandBeginPass begin_pass;
		GLCommandCopyBuffer copy_buffer;
		GLCommandCopyBufferToImage copy_buffer_to_image;
		GLCommandCopyImage copy_image;
		GLCommandCopyImageToBuffer copy_image_to_buffer;
		GLCommandDispatch dispatch;
	};
};

struct VIDeviceObj
{
	VIDeviceObj() {};
	VIDeviceObj(const VIDeviceObj&) = delete;
	~VIDeviceObj() {};

	VIDeviceObj& operator=(const VIDeviceObj&) = delete;

	VIBackend backend;
	VIQueueObj queue_graphics;
	VIQueueObj queue_transfer;
	VIQueueObj queue_present;
	VIPass swapchain_pass;
	VIFramebuffer swapchain_framebuffers;
	VIPipeline active_pipeline;
	VIComputePipeline active_compute_pipeline;
	VIDeviceLimits limits;

	// NOTE: currently the vise device encapsulates the whole backend context,
	//       and only one device may be created.
	union
	{
		VIVulkan vk;
		VIOpenGL gl;
	};
};

struct HostMalloc
{
	size_t size;
};

static void vk_create_instance(VIVulkan* vk, bool enable_validation);
static void vk_destroy_instance(VIVulkan* vk);
static void vk_create_surface(VIVulkan* vk);
static void vk_destroy_surface(VIVulkan* vk);
static void vk_create_device(VIVulkan* vk, VIDevice device, const VIDeviceInfo* info);
static void vk_destroy_device(VIVulkan* vk);
static void vk_create_swapchain(VIVulkan* vk, const VISwapchainInfo* info, uint32_t min_image_count);
static void vk_destroy_swapchain(VIVulkan* vk);
static void vk_create_image(VIVulkan* vk, VIImage image, const VkImageCreateInfo* info, const VkMemoryPropertyFlags& properties);
static void vk_destroy_image(VIVulkan* vk, VIImage image);
static void vk_create_image_view(VIVulkan* vk, VIImage image, const VkImageViewCreateInfo* info);
static void vk_destroy_image_view(VIVulkan* vk, VIImage image);
static void vk_create_sampler(VIVulkan* vk, VIImage, const VkSamplerCreateInfo* info);
static void vk_destroy_sampler(VIVulkan* vk, VIImage);
static void vk_create_framebuffer(VIVulkan* vk, VIFramebuffer fb, VIPass pass, VkExtent2D extent, uint32_t atch_count, VIImage* atchs);
static void vk_destroy_framebuffer(VIVulkan* vk, VIFramebuffer fb);
static void vk_alloc_cmd_buffer(VIVulkan* vk, VICommand cmd, VkCommandPool pool, VkCommandBufferLevel level);
static void vk_free_cmd_buffer(VIVulkan* vk, VICommand cmd);
static bool vk_has_format_features(VIVulkan* vk, VkFormat format, VkImageTiling tiling, VkFormatFeatureFlags features);
static void vk_default_configure_swapchain(const VIPhysicalDevice* device, void* window, VISwapchainInfo* out_info);

static void gl_device_present_frame(VIDevice device);
static void gl_device_append_submission(VIDevice device, const VISubmitInfo* submit);
static int gl_device_flush_submission(VIDevice device);
static void gl_create_module(VIDevice device, VIModule module, const VIModuleInfo* info);
static void gl_destroy_module(VIDevice device, VIModule module);
static void gl_create_pipeline_layout(VIDevice device, VIPipelineLayout layout, const VIPipelineLayoutInfo* info);
static void gl_remap(std::vector<GLRemap>& remaps, uint32_t set_count, uint32_t* binding_counts, const VIBinding** bindings);
static void gl_destroy_pipeline_layout(VIDevice device, VIPipelineLayout layout);
static void gl_create_pipeline(VIDevice device, VIPipeline pipeline, VIModule vm, VIModule fm);
static void gl_destroy_pipeline(VIDevice device, VIPipeline pipeline);
static void gl_create_compute_pipeline(VIDevice device, VIComputePipeline pipeline, VIModule compute_module);
static void gl_destroy_compute_pipeline(VIDevice device, VIComputePipeline pipeline);
static void gl_create_buffer(VIDevice device, VIBuffer buffer, const VIBufferInfo* info);
static void gl_destroy_buffer(VIDevice device, VIBuffer buffer);
static void gl_create_image(VIOpenGL* gl, VIImage image, const VIImageInfo* info);
static void gl_destroy_image(VIOpenGL* gl, VIImage image);
static void gl_create_framebuffer(VIOpenGL* gl, VIFramebuffer fb, const VIFramebufferInfo* info);
static void gl_destroy_framebuffer(VIOpenGL* gl, VIFramebuffer fb);
static void gl_create_swapchain_framebuffer(VIOpenGL* gl, VIFramebuffer fb);
static void gl_create_swapchain_pass(VIOpenGL* gl, VIPass pass);
static void gl_alloc_cmd_buffer(VIDevice device, VICommand cmd);
static void gl_free_command(VIDevice device, VICommand cmd);
static void gl_alloc_set(VIDevice device, VISet set);
static void gl_free_set(VIDevice device, VISet set);
static void gl_set_update(VISet set, uint32_t update_count, const VISetUpdateInfo* updates);
static void gl_pipeline_layout_get_remapped_binding(VIPipelineLayout layout, uint32_t set_index, uint32_t binding_idx, uint32_t* remapped_binding);
static void gl_copy_buffer(VIBuffer src, VIBuffer dst, uint32_t src_offset, uint32_t dst_offset, uint32_t size);
static void gl_copy_buffer_to_image(VIBuffer buffer, VIImage image, uint32_t buffer_offset, const VkOffset3D& image_offset, const VkExtent3D& image_extent,
	const VkImageSubresourceLayers& image_subresource);
static void gl_copy_image(VIImage src, VIImage dst, const VkOffset3D& src_offset, const VkOffset3D& dst_offset, const VkExtent3D& extent,
	const VkImageSubresourceLayers& src_subresource, const VkImageSubresourceLayers& dst_subresource);
static void gl_copy_image_to_buffer(VIImage image, VIBuffer buffer, uint32_t buffer_offset, const VkOffset3D& image_offset, const VkExtent3D& image_extent,
	const VkImageSubresourceLayers& image_subresource);
static GLCommand* gl_append_command(VICommand cmd, GLCommandType type);
static void gl_reset_command(VIDevice device, VICommand cmd);
static void gl_cmd_execute(VIDevice device, VICommand cmd);
static void gl_cmd_execute_opengl_callback(VIDevice device, GLCommand* glcmd);
static void gl_cmd_execute_set_viewport(VIDevice device, GLCommand* glcmd);
static void gl_cmd_execute_set_scissor(VIDevice device, GLCommand* glcmd);
static void gl_cmd_execute_draw(VIDevice device, GLCommand* glcmd);
static void gl_cmd_execute_draw_indexed(VIDevice device, GLCommand* glcmd);
static void gl_cmd_execute_push_constants(VIDevice device, GLCommand* glcmd);
static void gl_cmd_execute_bind_set(VIDevice device, GLCommand* glcmd);
static void gl_cmd_execute_bind_pipeline(VIDevice device, GLCommand* glcmd);
static void gl_cmd_execute_bind_compute_pipeline(VIDevice device, GLCommand* glcmd);
static void gl_cmd_execute_bind_vertex_buffers(VIDevice device, GLCommand* glcmd);
static void gl_cmd_execute_bind_index_buffer(VIDevice device, GLCommand* glcmd);
static void gl_cmd_execute_begin_pass(VIDevice device, GLCommand* glcmd);
static void gl_cmd_execute_end_pass(VIDevice device, GLCommand* glcmd);
static void gl_cmd_execute_copy_buffer(VIDevice device, GLCommand* glcmd);
static void gl_cmd_execute_copy_buffer_to_image(VIDevice device, GLCommand* glcmd);
static void gl_cmd_execute_copy_image(VIDevice device, GLCommand* glcmd);
static void gl_cmd_execute_copy_image_to_buffer(VIDevice device, GLCommand* glcmd);
static void gl_cmd_execute_dispatch(VIDevice device, GLCommand* glcmd);

static void compile_vk(VICompileResult& result, EShLanguage stage, const char* vise_glsl);
static void compile_gl(VICompileResult& result, EShLanguage stage, const char* vise_glsl, uint32_t remap_count, const GLRemap* remaps);
static void flip_image_data(uint8_t* data, uint32_t image_width, uint32_t image_height, uint32_t texel_size);

static void debug_print_compilation(const spirv_cross::CompilerGLSL& compiler, EShLanguage stage);

static void cast_compare_op_vk(VICompareOp in_op, VkCompareOp* out_op);
static void cast_compare_op_gl(VICompareOp in_op, GLenum* out_op);
static void cast_module_type_vk(VIModuleType in_type, VkShaderStageFlags* out_stages);
static void cast_module_type_glslang(VIModuleType in_type, EShLanguage* out_type);
static void cast_module_type_gl(VIModuleType in_type, GLenum* out_type);
static void cast_index_type(VkIndexType in_type, GLenum* out_type, size_t* out_size);
static void cast_buffer_usages(VIBufferType in_type, VIBufferUsageFlags in_usages, VkBufferUsageFlags* out_usages);
static void cast_buffer_type(VIBufferType in_type, GLenum* out_type);
static void cast_image_usages(VIImageUsageFlags in_usages, VkImageUsageFlags* out_usages);
static void cast_image_type(VIImageType in_type, VkImageType* out_type, VkImageViewType* out_view_type);
static void cast_image_type(VIImageType in_type, GLenum* out_type);
static void cast_filter_vk(const VISamplerInfo& in_sampler, VkFilter* out_filter, VkSamplerMipmapMode* out_mipmap_mode);
static void cast_filter_gl(const VISamplerInfo& in_sampler, GLenum* out_min_filter, GLenum* out_mag_filter);
static void cast_sampler_address_mode_vk(VISamplerAddressMode in_address_mode, VkSamplerAddressMode* out_address_mode);
static void cast_sampler_address_mode_gl(VISamplerAddressMode in_address_mode, GLenum* out_address_mode);
static void cast_blend_factor_vk(VIBlendFactor in_factor, VkBlendFactor* out_factor);
static void cast_blend_factor_gl(VIBlendFactor in_factor, GLenum* out_factor);
static void cast_blend_op_vk(VIBlendOp in_op, VkBlendOp* out_op);
static void cast_blend_op_gl(VIBlendOp in_op, GLenum* out_op);
static void cast_stencil_op_vk(VIStencilOp in_op, VkStencilOp* out_op);
static void cast_stencil_op_gl(VIStencilOp in_op, GLenum* out_op);
static void cast_polygon_mode_vk(VIPolygonMode in_mode, VkPolygonMode* out_mode);
static void cast_polygon_mode_gl(VIPolygonMode in_mode, GLenum* out_mode);
static void cast_cull_mode_vk(VICullMode in_mode, VkCullModeFlags* out_mode);
static void cast_cull_mode_gl(VICullMode in_mode, GLenum* out_mode);
static void cast_stencil_op_state_vk(const VIStencilOpStateInfo& in_state, VkStencilOpState* out_state);
static void cast_format_vk(VIFormat in_format, VkFormat* out_format, VkImageAspectFlags* out_aspects);
static void cast_format_vk(VkFormat in_format, VIFormat* out_format);
static void cast_format_gl(VIFormat in_format, GLenum* out_internal_format, GLenum* out_data_format, GLenum* out_data_type, uint32_t* out_texel_size);
static void cast_format_attachment_gl(VIFormat in_format, GLenum* out_attachment);
static void cast_set_pool_resources(uint32_t in_res_count, const VISetPoolResource* in_res, std::vector<VkDescriptorPoolSize>& out_sizes);
static void cast_binding(const VIBinding* in_binding, VkDescriptorSetLayoutBinding* out_binding);
static void cast_binding_type(VIBindingType in_type, VkDescriptorType* out_type);
static void cast_glsl_type_vk(VIGLSLType in_type, VkFormat* out_format);
static void cast_glsl_type_gl(VIGLSLType in_type, GLint* out_component_count, GLenum* out_component_type);
static void cast_glsl_type_spirv(const spirv_cross::SPIRType& in_type, VIGLSLType* out_type);
static void cast_pipeline_vertex_input(uint32_t attr_count, VIVertexAttribute* attrs, uint32_t binding_count, VIVertexBinding* bindings,
	std::vector<VkVertexInputAttributeDescription>& out_attrs, std::vector<VkVertexInputBindingDescription>& out_bindings);
static void cast_memory_barrier(const VIMemoryBarrier& in_barrier, VkMemoryBarrier* out_barrier);
static void cast_image_memory_barrier(const VIImageMemoryBarrier& in_barrier, VkImageMemoryBarrier* out_barrier);
static void cast_buffer_memory_barrier(const VIBufferMemoryBarrier& in_barrier, VkBufferMemoryBarrier* out_barrier);
static void cast_subpass_info(const VIPassInfo& in_pass_info, const VISubpassInfo& in_subpass_info,
	std::vector<VkAttachmentReference>* out_color_refs,
	std::optional<VkAttachmentReference>* out_depth_stencil_ref);
static void cast_pass_color_attachment(const VIPassColorAttachment& in_atch, VkAttachmentDescription* out_atch);
static void cast_pass_depth_stencil_attachment(const VIPassDepthStencilAttachment& in_atch, VkAttachmentDescription* out_atch);

static bool has_glslang_initialized;
static size_t host_malloc_usage;
static size_t host_malloc_peak;

static void (*gl_cmd_execute_table[GL_COMMAND_TYPE_ENUM_COUNT])(VIDevice, GLCommand*) = {
	gl_cmd_execute_opengl_callback,
	gl_cmd_execute_set_viewport,
	gl_cmd_execute_set_scissor,
	gl_cmd_execute_draw,
	gl_cmd_execute_draw_indexed,
	gl_cmd_execute_push_constants,
	gl_cmd_execute_bind_set,
	gl_cmd_execute_bind_pipeline,
	gl_cmd_execute_bind_compute_pipeline,
	gl_cmd_execute_bind_vertex_buffers,
	gl_cmd_execute_bind_index_buffer,
	gl_cmd_execute_begin_pass,
	gl_cmd_execute_end_pass,
	gl_cmd_execute_copy_buffer,
	gl_cmd_execute_copy_buffer_to_image,
	gl_cmd_execute_copy_image,
	gl_cmd_execute_copy_image_to_buffer,
	gl_cmd_execute_dispatch,
};

struct VIModuleTypeEntry
{
	VIModuleType vi_type;
	VkShaderStageFlagBits vk_type;
	EShLanguage glslang_type;
	GLenum gl_type;
};

static const VIModuleTypeEntry vi_module_type_table[3] = {
	{ VI_MODULE_TYPE_VERTEX,   VK_SHADER_STAGE_VERTEX_BIT,   EShLangVertex,   GL_VERTEX_SHADER },
	{ VI_MODULE_TYPE_FRAGMENT, VK_SHADER_STAGE_FRAGMENT_BIT, EShLangFragment, GL_FRAGMENT_SHADER },
	{ VI_MODULE_TYPE_COMPUTE,  VK_SHADER_STAGE_COMPUTE_BIT,  EShLangCompute,  GL_COMPUTE_SHADER },
};

struct VIGLSLTypeEntry
{
	VIGLSLType glsl_type;
	VkFormat vk_vertex_format;
	uint32_t gl_component_count;
	GLenum gl_component_type;
};

static const VIGLSLTypeEntry vi_glsl_type_table[21] = {
	{ VI_GLSL_TYPE_FLOAT,  VK_FORMAT_R32_SFLOAT,          1,  GL_FLOAT },
	{ VI_GLSL_TYPE_VEC2,   VK_FORMAT_R32G32_SFLOAT,       2,  GL_FLOAT },
	{ VI_GLSL_TYPE_VEC3,   VK_FORMAT_R32G32B32_SFLOAT,    3,  GL_FLOAT },
	{ VI_GLSL_TYPE_VEC4,   VK_FORMAT_R32G32B32A32_SFLOAT, 4,  GL_FLOAT },
	{ VI_GLSL_TYPE_DOUBLE, VK_FORMAT_R64_SFLOAT,          1,  GL_DOUBLE },
	{ VI_GLSL_TYPE_DVEC2,  VK_FORMAT_R64G64_SFLOAT,       2,  GL_DOUBLE },
	{ VI_GLSL_TYPE_DVEC3,  VK_FORMAT_R64G64B64_SFLOAT,    3,  GL_DOUBLE },
	{ VI_GLSL_TYPE_DVEC4,  VK_FORMAT_R64G64B64A64_SFLOAT, 4,  GL_DOUBLE },
	{ VI_GLSL_TYPE_UINT,   VK_FORMAT_R32_UINT,            1,  GL_UNSIGNED_INT },
	{ VI_GLSL_TYPE_UVEC2,  VK_FORMAT_R32G32_UINT,         2,  GL_UNSIGNED_INT },
	{ VI_GLSL_TYPE_UVEC3,  VK_FORMAT_R32G32B32_UINT,      3,  GL_UNSIGNED_INT },
	{ VI_GLSL_TYPE_UVEC4,  VK_FORMAT_R32G32B32A32_UINT,   4,  GL_UNSIGNED_INT },
	{ VI_GLSL_TYPE_INT,    VK_FORMAT_R32_SINT,            1,  GL_INT },
	{ VI_GLSL_TYPE_IVEC2,  VK_FORMAT_R32G32_SINT,         2,  GL_INT },
	{ VI_GLSL_TYPE_IVEC3,  VK_FORMAT_R32G32B32_SINT,      3,  GL_INT },
	{ VI_GLSL_TYPE_IVEC4,  VK_FORMAT_R32G32B32A32_SINT,   4,  GL_INT },
	{ VI_GLSL_TYPE_BOOL,   VK_FORMAT_UNDEFINED,           1,  GL_BOOL },
	{ VI_GLSL_TYPE_BVEC2,  VK_FORMAT_UNDEFINED,           2,  GL_BOOL },
	{ VI_GLSL_TYPE_BVEC3,  VK_FORMAT_UNDEFINED,           3,  GL_BOOL },
	{ VI_GLSL_TYPE_BVEC4,  VK_FORMAT_UNDEFINED,           4,  GL_BOOL },
	{ VI_GLSL_TYPE_MAT4,   VK_FORMAT_UNDEFINED,           16, GL_FLOAT },
};

struct VIFilterEntry
{
	VIFilter vi_filter;
	VkFilter vk_filter;
	VkSamplerMipmapMode vk_mipmap_mode;
	GLenum gl_filter;
};

static const VIFilterEntry vi_filter_table[2] = {
	{ VI_FILTER_LINEAR,  VK_FILTER_LINEAR,  VK_SAMPLER_MIPMAP_MODE_LINEAR,  GL_LINEAR },
	{ VI_FILTER_NEAREST, VK_FILTER_NEAREST, VK_SAMPLER_MIPMAP_MODE_NEAREST, GL_NEAREST },
};

struct VICompareOpEntry
{
	VICompareOp vi_compare_op;
	VkCompareOp vk_compare_op;
	GLenum gl_compare_op;
};

static const VICompareOpEntry vi_compare_op_table[8] = {
	{ VI_COMPARE_OP_NEVER,            VK_COMPARE_OP_NEVER,            GL_NEVER },
	{ VI_COMPARE_OP_LESS,             VK_COMPARE_OP_LESS,             GL_LESS },
	{ VI_COMPARE_OP_EQUAL,            VK_COMPARE_OP_EQUAL,            GL_EQUAL },
	{ VI_COMPARE_OP_LESS_OR_EQUAL,    VK_COMPARE_OP_LESS_OR_EQUAL,    GL_LEQUAL },
	{ VI_COMPARE_OP_GREATER,          VK_COMPARE_OP_GREATER,          GL_GREATER },
	{ VI_COMPARE_OP_NOT_EQUAL,        VK_COMPARE_OP_NOT_EQUAL,        GL_NOTEQUAL },
	{ VI_COMPARE_OP_GREATER_OR_EQUAL, VK_COMPARE_OP_GREATER_OR_EQUAL, GL_GEQUAL },
	{ VI_COMPARE_OP_ALWAYS,           VK_COMPARE_OP_ALWAYS,           GL_ALWAYS },
};

struct VISamplerAddressModeEntry
{
	VISamplerAddressMode vi_address_mode;
	VkSamplerAddressMode vk_address_mode;
	GLenum gl_address_mode;
};

static const VISamplerAddressModeEntry vi_sampler_address_mode_table[3] = {
	{ VI_SAMPLER_ADDRESS_MODE_REPEAT,           VK_SAMPLER_ADDRESS_MODE_REPEAT,           GL_REPEAT },
	{ VI_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,  VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,  GL_MIRRORED_REPEAT },
	{ VI_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,    VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,    GL_CLAMP_TO_EDGE },
};

struct VIBlendFactorEntry
{
	VIBlendFactor vi_blend_factor;
	VkBlendFactor vk_blend_factor;
	GLenum gl_blend_factor;
};

static const VIBlendFactorEntry vi_blend_factor_table[6] = {
	{ VI_BLEND_FACTOR_ZERO,                VK_BLEND_FACTOR_ZERO,                GL_ZERO },
	{ VI_BLEND_FACTOR_ONE,                 VK_BLEND_FACTOR_ONE,                 GL_ONE },
	{ VI_BLEND_FACTOR_SRC_ALPHA,           VK_BLEND_FACTOR_SRC_ALPHA,           GL_SRC_ALPHA },
	{ VI_BLEND_FACTOR_DST_ALPHA,           VK_BLEND_FACTOR_DST_ALPHA,           GL_DST_ALPHA },
	{ VI_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA },
	{ VI_BLEND_FACTOR_ONE_MINUS_DST_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA, GL_ONE_MINUS_DST_ALPHA },
};

struct VIBlendOpEntry
{
	VIBlendOp vi_blend_op;
	VkBlendOp vk_blend_op;
	GLenum gl_blend_op;
};

static const VIBlendOpEntry vi_blend_op_table[5] = {
	{ VI_BLEND_OP_ADD,              VK_BLEND_OP_ADD,              GL_FUNC_ADD },
	{ VI_BLEND_OP_SUBTRACT,         VK_BLEND_OP_SUBTRACT,         GL_FUNC_SUBTRACT },
	{ VI_BLEND_OP_REVERSE_SUBTRACT, VK_BLEND_OP_REVERSE_SUBTRACT, GL_FUNC_REVERSE_SUBTRACT },
	{ VI_BLEND_OP_MIN,              VK_BLEND_OP_MIN,              GL_MIN },
	{ VI_BLEND_OP_MAX,              VK_BLEND_OP_MAX,              GL_MAX },
};

struct VIStencilOpEntry
{
	VIStencilOp vi_stencil_op;
	VkStencilOp vk_stencil_op;
	GLenum gl_stencil_op;
};

static const VIStencilOpEntry vi_stencil_op_table[] = {
	{ VI_STENCIL_OP_KEEP,     VK_STENCIL_OP_KEEP,    GL_KEEP },
	{ VI_STENCIL_OP_ZERO,     VK_STENCIL_OP_ZERO,    GL_ZERO },
	{ VI_STENCIL_OP_REPLACE,  VK_STENCIL_OP_REPLACE, GL_REPLACE },
};

struct VIPolygonModeEntry
{
	VIPolygonMode vi_polygon_mode;
	VkPolygonMode vk_polygon_mode;
	GLenum gl_polygon_mode;
};

static const VIPolygonModeEntry vi_polygon_mode_table[] = {
	{ VI_POLYGON_MODE_FILL,  VK_POLYGON_MODE_FILL,  GL_FILL },
	{ VI_POLYGON_MODE_LINE,  VK_POLYGON_MODE_LINE,  GL_LINE },
	{ VI_POLYGON_MODE_POINT, VK_POLYGON_MODE_POINT, GL_POINT },
};

struct VICullModeEntry
{
	VICullMode vi_cull_mode;
	VkCullModeFlags vk_cull_mode;
	GLenum gl_cull_mode;
};

static const VICullModeEntry vi_cull_mode_table[] = {
	{ VI_CULL_MODE_NONE,           VK_CULL_MODE_NONE,           (GLenum)0 }, // GL_NONE is not valid for glCullFace, we should disable GL_CULL_FACE
	{ VI_CULL_MODE_BACK,           VK_CULL_MODE_BACK_BIT,       GL_BACK },
	{ VI_CULL_MODE_FRONT,          VK_CULL_MODE_FRONT_BIT,      GL_FRONT },
	{ VI_CULL_MODE_FRONT_AND_BACK, VK_CULL_MODE_FRONT_AND_BACK, GL_FRONT_AND_BACK },
};

struct VIImageTypeEntry
{
	VIImageType vi_type;
	VkImageType vk_type;
	VkImageViewType vk_view_type;
	GLenum gl_type;
};

static const VIImageTypeEntry vi_image_type_table[3] = {
	{ VI_IMAGE_TYPE_2D,        VK_IMAGE_TYPE_2D,  VK_IMAGE_VIEW_TYPE_2D,       GL_TEXTURE_2D },
	{ VI_IMAGE_TYPE_2D_ARRAY,  VK_IMAGE_TYPE_2D,  VK_IMAGE_VIEW_TYPE_2D_ARRAY, GL_TEXTURE_2D_ARRAY },
	{ VI_IMAGE_TYPE_CUBE,      VK_IMAGE_TYPE_2D,  VK_IMAGE_VIEW_TYPE_CUBE,     GL_TEXTURE_CUBE_MAP },
};

enum VIImageAspectFlags : VkImageAspectFlags
{
	VI_IMAGE_ASPECT_COLOR = VK_IMAGE_ASPECT_COLOR_BIT,
	VI_IMAGE_ASPECT_DEPTH = VK_IMAGE_ASPECT_DEPTH_BIT,
	VI_IMAGE_ASPECT_STENCIL = VK_IMAGE_ASPECT_STENCIL_BIT,
	VI_IMAGE_ASPECT_DEPTH_STENCIL = (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT),
};

struct VIFormatEntry
{
	VIFormat vi_format;
	VkImageAspectFlags vk_aspect;
	VkFormat vk_format;
	uint32_t texel_block_size;
	GLenum gl_internal_format;
	GLenum gl_data_format;
	GLenum gl_data_type;
};

static const VIFormatEntry vi_format_table[] = {
	{ VI_FORMAT_UNDEFINED, (VIImageAspectFlags)0,        VK_FORMAT_UNDEFINED,           0,  GL_NONE,               GL_NONE,            GL_NONE },
	{ VI_FORMAT_R8,       VI_IMAGE_ASPECT_COLOR,         VK_FORMAT_R8_UNORM,            1,  GL_R8,                 GL_RED,             GL_UNSIGNED_BYTE },
	{ VI_FORMAT_RG8,      VI_IMAGE_ASPECT_COLOR,         VK_FORMAT_R8G8_UNORM,          2,  GL_RG8,                GL_RG,              GL_UNSIGNED_BYTE },
	{ VI_FORMAT_RGB8,     VI_IMAGE_ASPECT_COLOR,         VK_FORMAT_R8G8B8_UNORM,        3,  GL_RGB8,               GL_RGB,             GL_UNSIGNED_BYTE },
	{ VI_FORMAT_RGBA8,    VI_IMAGE_ASPECT_COLOR,         VK_FORMAT_R8G8B8A8_UNORM,      4,  GL_RGBA8,              GL_RGBA,            GL_UNSIGNED_BYTE },
	{ VI_FORMAT_BGRA8,    VI_IMAGE_ASPECT_COLOR,         VK_FORMAT_B8G8R8A8_UNORM,      4,  GL_RGBA8,              GL_BGRA,            GL_UNSIGNED_BYTE },
	{ VI_FORMAT_R16F,     VI_IMAGE_ASPECT_COLOR,         VK_FORMAT_R16_SFLOAT,          2,  GL_R16F,               GL_RED,             GL_HALF_FLOAT },
	{ VI_FORMAT_RG16F,    VI_IMAGE_ASPECT_COLOR,         VK_FORMAT_R16G16_SFLOAT,       4,  GL_RG16F,              GL_RG,              GL_HALF_FLOAT },
	{ VI_FORMAT_RGB16F,   VI_IMAGE_ASPECT_COLOR,         VK_FORMAT_R16G16B16_SFLOAT,    6,  GL_RGB16F,             GL_RGB,             GL_HALF_FLOAT },
	{ VI_FORMAT_RGBA16F,  VI_IMAGE_ASPECT_COLOR,         VK_FORMAT_R16G16B16A16_SFLOAT, 8,  GL_RGBA16F,            GL_RGBA,            GL_HALF_FLOAT },
	{ VI_FORMAT_RGB32F,   VI_IMAGE_ASPECT_COLOR,         VK_FORMAT_R32G32B32_SFLOAT,    12, GL_RGB32F,             GL_RGB,             GL_FLOAT },
	{ VI_FORMAT_RGBA32F,  VI_IMAGE_ASPECT_COLOR,         VK_FORMAT_R32G32B32A32_SFLOAT, 16, GL_RGBA32F,            GL_RGBA,            GL_FLOAT },
	{ VI_FORMAT_D32F_S8U, VI_IMAGE_ASPECT_DEPTH_STENCIL, VK_FORMAT_D32_SFLOAT_S8_UINT,  5,  GL_DEPTH32F_STENCIL8,  GL_DEPTH_STENCIL,   GL_FLOAT_32_UNSIGNED_INT_24_8_REV },
	{ VI_FORMAT_D24_S8U,  VI_IMAGE_ASPECT_DEPTH_STENCIL, VK_FORMAT_D24_UNORM_S8_UINT,   4,  GL_DEPTH24_STENCIL8,   GL_DEPTH_STENCIL,   GL_UNSIGNED_INT_24_8, },
	{ VI_FORMAT_D32F,     VI_IMAGE_ASPECT_DEPTH,         VK_FORMAT_D32_SFLOAT,          4,  GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT, GL_FLOAT },
};

static inline void swrite32(uint8_t** mem, uint32_t value)
{
	uint8_t* now = *mem;
	*now++ = value & 0xFF;
	*now++ = (value >> 8) & 0xFF;
	*now++ = (value >> 16) & 0xFF;
	*now++ = (value >> 24) & 0xFF;
	*mem = now;
}

static inline void swrite_bytes(uint8_t** mem, size_t byte_size, const void* bytes)
{
	memcpy(*mem, bytes, byte_size);
	*mem += byte_size;
}

static inline void swrite_header(uint8_t** mem, const VIBinaryHeader& header)
{
	swrite32(mem, (uint32_t)header.payload_size);
	swrite32(mem, (uint32_t)header.header_size);
	swrite32(mem, (uint32_t)header.backend_type);
	swrite32(mem, (uint32_t)header.module_type);
	swrite32(mem, (uint32_t)header.glpc_count);
}

static inline void swrite_glpc(uint8_t** mem, const GLPushConstant& pc)
{
	swrite32(mem, pc.size);
	swrite32(mem, pc.offset);
	swrite32(mem, pc.uniform_arr_size);
	swrite32(mem, pc.uniform_glsl_type);
	swrite32(mem, pc.uniform_name.size());
	swrite_bytes(mem, pc.uniform_name.size(), pc.uniform_name.data());
}

static inline uint32_t sread32(uint8_t** mem)
{
	uint8_t* now = *mem;
	uint32_t word = 0;

	word |= static_cast<uint32_t>(now[0]);
	word |= static_cast<uint32_t>(now[1]) << 8;
	word |= static_cast<uint32_t>(now[2]) << 16;
	word |= static_cast<uint32_t>(now[3]) << 24;

	*mem = now + 4;
	return word;
}

static inline void sread_bytes(uint8_t** mem, size_t size, void* dst)
{
	memcpy(dst, *mem, size);
	*mem += size;
}

static void sread_header(uint8_t** mem, VIBinaryHeader* header)
{
	header->payload_size = sread32(mem);
	header->header_size = sread32(mem);
	header->backend_type = sread32(mem);
	header->module_type = sread32(mem);
	header->glpc_count = sread32(mem);
}

static inline void sread_glpc(uint8_t** mem, GLPushConstant* pc)
{
	pc->size = sread32(mem);
	pc->offset = sread32(mem);
	pc->uniform_arr_size = sread32(mem);
	pc->uniform_glsl_type = (VIGLSLType)sread32(mem);
	size_t uniform_name_size = (size_t)sread32(mem);
	pc->uniform_name.resize(uniform_name_size);
	sread_bytes(mem, uniform_name_size, pc->uniform_name.data());
}

void* vi_malloc(size_t size)
{
	HostMalloc* header = (HostMalloc*)malloc(size + sizeof(HostMalloc));
	VI_ASSERT(header != nullptr);

	header->size = size;
	host_malloc_usage += size;

	if (host_malloc_usage > host_malloc_peak)
		host_malloc_peak = host_malloc_usage;

	return ((char*)header) + sizeof(HostMalloc);
}

void vi_free(void* ptr)
{
	VI_ASSERT(ptr != nullptr);

	HostMalloc* header = (HostMalloc*)((((char*)ptr) - sizeof(HostMalloc)));
	host_malloc_usage -= header->size;

	free(header);
}

static void vk_create_instance(VIVulkan* vk, bool enable_validation)
{
	// available layers and extensions
	{
		uint32_t layer_count;
		VK_CHECK(vkEnumerateInstanceLayerProperties(&layer_count, NULL));
		vk->layer_props.resize((size_t)layer_count);
		VK_CHECK(vkEnumerateInstanceLayerProperties(&layer_count, vk->layer_props.data()));

		for (uint32_t i = 0; i < layer_count; i++)
		{
			VkLayerProperties* prop = vk->layer_props.data() + i;
			//printf("layer prop: %s\n", prop->layerName);
		}

		uint32_t ext_count;
		VK_CHECK(vkEnumerateInstanceExtensionProperties(NULL, &ext_count, NULL));
		vk->ext_props.resize(ext_count);
		VK_CHECK(vkEnumerateInstanceExtensionProperties(NULL, &ext_count, vk->ext_props.data()));

		for (uint32_t i = 0; i < ext_count; i++)
		{
			VkExtensionProperties* prop = vk->ext_props.data() + i;
			//printf("layer ext: %s\n", prop->extensionName);
		}
	}

	// desired layers and extensions
	const char* desired_layers[] = {
		"VK_LAYER_KHRONOS_validation",
	};
	
	const char* desired_exts[] = {
#ifdef VK_KHR_surface
		VK_KHR_SURFACE_EXTENSION_NAME,
#endif
#ifdef VK_KHR_win32_surface
		VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#endif
#ifdef VK_EXT_debug_utils
		VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
#endif
	};

	// TODO: check intersection, assert for required layers and exts

	// create instance
	VkApplicationInfo appI{};
	appI.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appI.applicationVersion = VK_MAKE_VERSION(0, 0, 0);
	appI.pApplicationName = "Vise";
	appI.apiVersion = VI_VK_API_VERSION;

	VkInstanceCreateInfo instanceCI{};
	instanceCI.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instanceCI.enabledLayerCount = enable_validation ? VI_ARR_SIZE(desired_layers) : 0;
	instanceCI.ppEnabledLayerNames = desired_layers;
	instanceCI.enabledExtensionCount = VI_ARR_SIZE(desired_exts);
	instanceCI.ppEnabledExtensionNames = desired_exts;
	instanceCI.pApplicationInfo = &appI;

	VK_CHECK(vkCreateInstance(&instanceCI, NULL, &vk->instance));
}

static void vk_destroy_instance(VIVulkan* vk)
{
	vkDestroyInstance(vk->instance, NULL);
	vk->instance = NULL;
}

static void vk_create_surface(VIVulkan* vk)
{
	GLFWwindow* window = glfwGetCurrentContext();

	VkWin32SurfaceCreateInfoKHR surfaceCI{};
	surfaceCI.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
	surfaceCI.hinstance = GetModuleHandle(NULL);
	surfaceCI.hwnd = glfwGetWin32Window(window);

	VK_CHECK(vkCreateWin32SurfaceKHR(vk->instance, &surfaceCI, NULL, &vk->surface));
}

static void vk_destroy_surface(VIVulkan* vk)
{
	vkDestroySurfaceKHR(vk->instance, vk->surface, NULL);
	vk->surface = NULL;
}

static void vk_create_device(VIVulkan* vk, VIDevice device, const VIDeviceInfo* info)
{
	std::vector<VkPhysicalDevice> handles;
	uint32_t pdevice_count;
	VK_CHECK(vkEnumeratePhysicalDevices(vk->instance, &pdevice_count, NULL));
	VI_ASSERT(pdevice_count > 0); // TODO: error handling
	handles.resize(pdevice_count);
	VK_CHECK(vkEnumeratePhysicalDevices(vk->instance, &pdevice_count, handles.data()));

	vk->pdevices.resize(pdevice_count);

	for (uint32_t i = 0; i < pdevice_count; i++)
	{
		VIPhysicalDevice* pdevice = vk->pdevices.data() + i;
		VkPhysicalDeviceProperties* props = &pdevice->device_props;
		vkGetPhysicalDeviceProperties(handles[i], props);

		pdevice->handle = handles[i];
		pdevice->surface = vk->surface;

		// queue families on this physical device
		uint32_t family_count;
		vkGetPhysicalDeviceQueueFamilyProperties(handles[i], &family_count, NULL);
		pdevice->family_props.resize(family_count);
		vkGetPhysicalDeviceQueueFamilyProperties(handles[i], &family_count, pdevice->family_props.data());

		// extensions on this physical device
		uint32_t ext_count;
		VK_CHECK(vkEnumerateDeviceExtensionProperties(handles[i], NULL, &ext_count, NULL));
		pdevice->ext_props.resize(ext_count);
		VK_CHECK(vkEnumerateDeviceExtensionProperties(handles[i], NULL, &ext_count, pdevice->ext_props.data()));

		// features on this physical device
		VkPhysicalDeviceExtendedDynamicStateFeaturesEXT extendedDynamicStateFeatures;
		extendedDynamicStateFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT;
		extendedDynamicStateFeatures.pNext = nullptr;
		extendedDynamicStateFeatures.extendedDynamicState = VK_TRUE;

		pdevice->features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
		pdevice->features.pNext = &extendedDynamicStateFeatures;
		vkGetPhysicalDeviceFeatures2(handles[i], &pdevice->features);

		// compatible surface formats on this physical device
		uint32_t format_count;
		vkGetPhysicalDeviceSurfaceFormatsKHR(pdevice->handle, pdevice->surface, &format_count, NULL);
		if (format_count > 0)
		{
			pdevice->surface_formats.resize(format_count);
			vkGetPhysicalDeviceSurfaceFormatsKHR(pdevice->handle, pdevice->surface, &format_count, pdevice->surface_formats.data());
		}

		// available present modes for the surface on this physical device
		uint32_t mode_count;
		vkGetPhysicalDeviceSurfacePresentModesKHR(pdevice->handle, pdevice->surface, &mode_count, NULL);
		if (mode_count > 0)
		{
			pdevice->present_modes.resize(mode_count);
			vkGetPhysicalDeviceSurfacePresentModesKHR(pdevice->handle, pdevice->surface, &mode_count, pdevice->present_modes.data());
		}

		VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(pdevice->handle, vk->surface, &pdevice->surface_caps));

		VkFormatProperties format_props;
		std::array<VkFormat, 2> depth_stencil_candidates = {
			VK_FORMAT_D32_SFLOAT_S8_UINT,
			VK_FORMAT_D24_UNORM_S8_UINT,
		};
		for (size_t i = 0; i < depth_stencil_candidates.size(); i++)
		{
			vkGetPhysicalDeviceFormatProperties(pdevice->handle, depth_stencil_candidates[i], &format_props);
			if (format_props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
				pdevice->depth_stencil_formats.push_back(depth_stencil_candidates[i]);
		}

		// printf("pdevice: %s, %d families, %d exts\n", props->deviceName, (int)pdevice->family_props.size(), (int)pdevice->ext_props.size());
	}


	// NOTE: we are retrieving a pointer into std::vector,
	//       which is invalidated the moment the vector resizes
	VIPhysicalDevice* chosen = vk->pdevices.data();

	if (info->vulkan.select_physical_device != NULL)
	{
		int idx = info->vulkan.select_physical_device((int)vk->pdevices.size(), vk->pdevices.data());
		chosen = vk->pdevices.data() + idx;
	}

	// create one queue for each family
	uint32_t family_count = chosen->family_props.size();
	uint32_t family_idx_graphics = family_count;
	uint32_t family_idx_transfer = family_count;
	uint32_t family_idx_present = family_count;

	std::vector<VkDeviceQueueCreateInfo> queueCI(family_count);
	float priority = 1.0f;

	for (uint32_t idx = 0; idx < family_count; idx++)
	{
		queueCI[idx].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCI[idx].queueCount = 1;
		queueCI[idx].queueFamilyIndex = idx;
		queueCI[idx].pQueuePriorities = &priority;

		if (family_idx_graphics == family_count && chosen->family_props[idx].queueFlags | VK_QUEUE_GRAPHICS_BIT)
			family_idx_graphics = idx;

		if (family_idx_transfer == family_count && chosen->family_props[idx].queueFlags | VK_QUEUE_TRANSFER_BIT)
			family_idx_transfer = idx;

		VkBool32 is_supported;
		vkGetPhysicalDeviceSurfaceSupportKHR(chosen->handle, idx, vk->surface, &is_supported);
		if (family_idx_present == family_count && is_supported)
			family_idx_present = idx;
	}

	VI_ASSERT(family_idx_graphics != family_count && "graphics queue family not found");
	VI_ASSERT(family_idx_transfer != family_count && "transfer queue family not found");
	VI_ASSERT(family_idx_present != family_count && "present queue family not found");

	// TODO: check if required extensions are present on physical device
	const char* desired_device_exts[] = {
#ifdef VK_KHR_swapchain
		VK_KHR_SWAPCHAIN_EXTENSION_NAME,
#endif
#ifdef VK_EXT_extended_dynamic_state
		VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME, // TODO: included in VK_API_VERSION_1_3, driver bug not detecting VkApplicationInfo::apiVersion?
#endif
	};

	VkDeviceCreateInfo deviceCI{};
	deviceCI.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceCI.pNext = &chosen->features;
	deviceCI.queueCreateInfoCount = queueCI.size();
	deviceCI.pQueueCreateInfos = queueCI.data();
	deviceCI.enabledExtensionCount = VI_ARR_SIZE(desired_device_exts);
	deviceCI.ppEnabledExtensionNames = desired_device_exts;
	deviceCI.pEnabledFeatures = nullptr;
	VK_CHECK(vkCreateDevice(chosen->handle, &deviceCI, NULL, &vk->device));

	vk->pdevice_chosen = chosen;
	vk->pdevice = vk->pdevice_chosen->handle;
	vk->family_idx_graphics = family_idx_graphics;
	vk->family_idx_transfer = family_idx_transfer;
	vk->family_idx_present = family_idx_present;

	vkGetDeviceQueue(vk->device, family_idx_graphics, 0, &device->queue_graphics.vk_handle);
	vkGetDeviceQueue(vk->device, family_idx_transfer, 0, &device->queue_transfer.vk_handle);
	vkGetDeviceQueue(vk->device, family_idx_present, 0, &device->queue_present.vk_handle);
}

static void vk_destroy_device(VIVulkan* vk)
{
	vkDestroyDevice(vk->device, NULL);
	vk->device = NULL;
}

static void vk_create_swapchain(VIVulkan* vk, const VISwapchainInfo* info, uint32_t min_image_count)
{
	VIPhysicalDevice* pdevice = vk->pdevice_chosen;

	VkSwapchainCreateInfoKHR swapchainCI{};
	swapchainCI.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapchainCI.minImageCount = min_image_count;
	swapchainCI.presentMode = info->present_mode;
	swapchainCI.imageExtent = info->image_extent;
	swapchainCI.imageColorSpace = info->image_color_space;
	swapchainCI.imageFormat = info->image_format;
	swapchainCI.imageArrayLayers = 1;
	swapchainCI.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	swapchainCI.preTransform = pdevice->surface_caps.currentTransform; // TODO: parameterize
	swapchainCI.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	swapchainCI.clipped = VK_TRUE;
	swapchainCI.oldSwapchain = VK_NULL_HANDLE;
	swapchainCI.surface = vk->surface;

	uint32_t family_indices[2] = { vk->family_idx_graphics, vk->family_idx_present };

	if (vk->family_idx_graphics == vk->family_idx_present)
	{
		swapchainCI.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		swapchainCI.queueFamilyIndexCount = 0;
	}
	else
	{
		swapchainCI.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		swapchainCI.queueFamilyIndexCount = 2;
		swapchainCI.pQueueFamilyIndices = family_indices;
	}

	VK_CHECK(vkCreateSwapchainKHR(vk->device, &swapchainCI, NULL, &vk->swapchain.handle));

	uint32_t image_count;
	VK_CHECK(vkGetSwapchainImagesKHR(vk->device, vk->swapchain.handle, &image_count, NULL));
	vk->swapchain.image_handles.resize(image_count);
	vk->swapchain.images.resize(image_count);
	VK_CHECK(vkGetSwapchainImagesKHR(vk->device, vk->swapchain.handle, &image_count, vk->swapchain.image_handles.data()));

	vk->swapchain.image_extent = info->image_extent;
	vk->swapchain.image_format = info->image_format;
	vk->swapchain.depth_stencil_format = info->depth_stencil_format;
	if (info->depth_stencil_format != VK_FORMAT_UNDEFINED)
		vk->swapchain.depth_stencils.resize(image_count);

	for (uint32_t i = 0; i < vk->swapchain.image_handles.size(); i++)
	{
		VkImageViewCreateInfo viewCI{};
		viewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewCI.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewCI.subresourceRange.levelCount = 1;
		viewCI.subresourceRange.baseMipLevel = 0;
		viewCI.subresourceRange.layerCount = 1;
		viewCI.subresourceRange.baseArrayLayer = 0;

		// swapchain framebuffer depth stencil attachment
		if (info->depth_stencil_format != VK_FORMAT_UNDEFINED)
		{
			VkImageCreateInfo depthStencilImageCI{};
			depthStencilImageCI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
			depthStencilImageCI.extent.width = info->image_extent.width;
			depthStencilImageCI.extent.height = info->image_extent.height;
			depthStencilImageCI.extent.depth = 1;
			depthStencilImageCI.arrayLayers = 1;
			depthStencilImageCI.mipLevels = 1;
			depthStencilImageCI.imageType = VK_IMAGE_TYPE_2D;
			depthStencilImageCI.format = vk->swapchain.depth_stencil_format;
			depthStencilImageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
			depthStencilImageCI.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT; 
			depthStencilImageCI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			depthStencilImageCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			depthStencilImageCI.samples = VK_SAMPLE_COUNT_1_BIT;
			vk_create_image(vk, vk->swapchain.depth_stencils.data() + i, &depthStencilImageCI, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

			viewCI.image = vk->swapchain.depth_stencils[i].vk.handle;
			viewCI.format = vk->swapchain.depth_stencil_format;
			viewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT; // TODO: only if there are stencil bits
			vk_create_image_view(vk, vk->swapchain.depth_stencils.data() + i, &viewCI);
		}
		
		// swapchain framebuffer color attachment
		viewCI.image = vk->swapchain.image_handles[i];
		viewCI.format = vk->swapchain.image_format;
		viewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		vk_create_image_view(vk, vk->swapchain.images.data() + i, &viewCI);
	}
}

static void vk_destroy_swapchain(VIVulkan* vk)
{
	for (uint32_t i = 0; i < vk->swapchain.images.size(); i++)
		vk_destroy_image_view(vk, vk->swapchain.images.data() + i);

	for (uint32_t i = 0; i < vk->swapchain.depth_stencils.size(); i++)
	{
		vk_destroy_image_view(vk, vk->swapchain.depth_stencils.data() + i);
		vk_destroy_image(vk, vk->swapchain.depth_stencils.data() + i);
	}

	vkDestroySwapchainKHR(vk->device, vk->swapchain.handle, nullptr);
	vk->swapchain.handle = VK_NULL_HANDLE;
}

static void vk_create_image(VIVulkan* vk, VIImage image, const VkImageCreateInfo* info, const VkMemoryPropertyFlags& properties)
{
	image->flags |= VI_IMAGE_FLAG_CREATED_IMAGE_BIT;

	VmaAllocationCreateInfo allocCI = {};
	allocCI.usage = VMA_MEMORY_USAGE_AUTO;
	allocCI.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
	allocCI.priority = 1.0f;
	allocCI.requiredFlags = properties;
	allocCI.preferredFlags = properties;
	VK_CHECK(vmaCreateImage(vk->vma, info, &allocCI, &image->vk.handle, &image->vk.alloc, nullptr));
}

static void vk_destroy_image(VIVulkan* vk, VIImage image)
{
	VI_ASSERT(image->flags & VI_IMAGE_FLAG_CREATED_IMAGE_BIT);

	vmaDestroyImage(vk->vma, image->vk.handle, image->vk.alloc);
	image->vk.handle = VK_NULL_HANDLE;
	image->flags &= ~VI_IMAGE_FLAG_CREATED_IMAGE_BIT;
}

static void vk_create_image_view(VIVulkan* vk, VIImage image, const VkImageViewCreateInfo* info)
{
	image->flags |= VI_IMAGE_FLAG_CREATED_IMAGE_VIEW_BIT;

	VK_CHECK(vkCreateImageView(vk->device, info, NULL, &image->vk.view_handle));
}

static void vk_destroy_image_view(VIVulkan* vk, VIImage image)
{
	VI_ASSERT(image->flags & VI_IMAGE_FLAG_CREATED_IMAGE_VIEW_BIT);

	vkDestroyImageView(vk->device, image->vk.view_handle, NULL);
	image->vk.view_handle = VK_NULL_HANDLE;
	image->flags &= ~VI_IMAGE_FLAG_CREATED_IMAGE_VIEW_BIT;
}

static void vk_create_sampler(VIVulkan* vk, VIImage image, const VkSamplerCreateInfo* info)
{
	image->flags |= VI_IMAGE_FLAG_CREATED_SAMPLER_BIT;

	VK_CHECK(vkCreateSampler(vk->device, info, nullptr, &image->vk.sampler_handle));
}

static void vk_destroy_sampler(VIVulkan* vk, VIImage image)
{
	VI_ASSERT(image->flags & VI_IMAGE_FLAG_CREATED_SAMPLER_BIT);

	vkDestroySampler(vk->device, image->vk.sampler_handle, nullptr);
	image->vk.sampler_handle = VK_NULL_HANDLE;
	image->flags &= ~VI_IMAGE_FLAG_CREATED_SAMPLER_BIT;
}

static void vk_create_framebuffer(VIVulkan* vk, VIFramebuffer fb, VIPass pass, VkExtent2D extent, uint32_t atch_count, VIImage* atchs)
{
	VkImageView attachments[32];
	VI_ASSERT(atch_count <= VI_ARR_SIZE(attachments));
	for (uint32_t i = 0; i < atch_count; i++)
		attachments[i] = atchs[i]->vk.view_handle;

	VkFramebufferCreateInfo fbI{};
	fbI.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	fbI.width = extent.width;
	fbI.height = extent.height;
	fbI.layers = 1;
	fbI.attachmentCount = atch_count;
	fbI.pAttachments = attachments;
	fbI.renderPass = pass->vk.handle;

	VK_CHECK(vkCreateFramebuffer(vk->device, &fbI, NULL, &fb->vk.handle));
}

static void vk_destroy_framebuffer(VIVulkan* vk, VIFramebuffer fb)
{
	vkDestroyFramebuffer(vk->device, fb->vk.handle, NULL);

	fb->vk.handle = VK_NULL_HANDLE;
}

static void vk_alloc_cmd_buffer(VIVulkan* vk, VICommand cmd, VkCommandPool pool, VkCommandBufferLevel level)
{
	VkCommandBufferAllocateInfo bufferAI{};
	bufferAI.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	bufferAI.level = level;
	bufferAI.commandPool = pool;
	bufferAI.commandBufferCount = 1;

	VK_CHECK(vkAllocateCommandBuffers(vk->device, &bufferAI, &cmd->vk.handle));
}

static void vk_free_cmd_buffer(VIVulkan* vk, VICommand cmd)
{
	VICommandPool pool = cmd->pool;

	vkFreeCommandBuffers(vk->device, pool->vk_handle, 1, &cmd->vk.handle);
}

bool vk_has_format_features(VIVulkan* vk, VkFormat format, VkImageTiling tiling, VkFormatFeatureFlags features)
{
	VkFormatProperties props;
	vkGetPhysicalDeviceFormatProperties(vk->pdevice, format, &props);

	if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features)
		return true;

	if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features)
		return true;

	return false;
}

static void vk_default_configure_swapchain(const VIPhysicalDevice* pdevice, void* window, VISwapchainInfo* out_info)
{
	// configure initial swapchain extent
	{
		int width, height;
		glfwGetFramebufferSize((GLFWwindow*)window, &width, &height);
		out_info->image_extent.width = (uint32_t)width;
		out_info->image_extent.height = (uint32_t)height;
		// TODO: clamp
	}

	// configure swapchain framebuffer color format
	{
		out_info->image_format = pdevice->surface_formats[0].format;
		out_info->image_color_space = pdevice->surface_formats[0].colorSpace;
		for (auto& surface_fmt : pdevice->surface_formats)
		{
			if (surface_fmt.format == VK_FORMAT_B8G8R8A8_UNORM && surface_fmt.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
			{
				out_info->image_format = surface_fmt.format;
				out_info->image_color_space = surface_fmt.colorSpace;
				break;
			}
		}
	}

	// configure swapchain framebuffer depth stencil format
	{
		std::array<VkFormat, 2> candidates = {
			VK_FORMAT_D32_SFLOAT_S8_UINT,
			VK_FORMAT_D24_UNORM_S8_UINT,
		};

		VkFormat depth_stencil_format = VK_FORMAT_UNDEFINED;

		for (VkFormat format : pdevice->depth_stencil_formats)
		{
			for (size_t i = 0; i < candidates.size(); i++)
			{
				if (format == candidates[i])
				{
					depth_stencil_format = candidates[i];
					break;
				}
			}

			if (depth_stencil_format != VK_FORMAT_UNDEFINED)
				break;
		}

		VI_ASSERT(depth_stencil_format != VK_FORMAT_UNDEFINED);
		out_info->depth_stencil_format = depth_stencil_format;
	}

	// configure swapchain present mode
	{
		out_info->present_mode = VK_PRESENT_MODE_FIFO_KHR;

		for (auto& mode : pdevice->present_modes)
		{
			if (mode == VK_PRESENT_MODE_MAILBOX_KHR)
			{
				out_info->present_mode = mode;
				break;
			}
		}
	}
}

static void gl_device_present_frame(VIDevice device)
{
	VIOpenGL* gl = &device->gl;
	GLFWwindow* window = glfwGetCurrentContext();

	glfwSwapBuffers(window);
}

// append a submission that will later be executed once all wait semaphores are signaled
static void gl_device_append_submission(VIDevice device, const VISubmitInfo* submit)
{
	// can't cache pointer members in info struct, copy them over
	device->gl.submits.push_back({ submit->cmd_count, submit->wait_count, submit->signal_count });
	GLSubmitInfo& gl_submit = device->gl.submits.back();

	for (uint32_t i = 0; i < submit->cmd_count; i++)
		gl_submit.cmds[i] = submit->cmds[i];

	for (uint32_t i = 0; i < submit->wait_count; i++)
		gl_submit.waits[i] = submit->waits[i];

	for (uint32_t i = 0; i < submit->signal_count; i++)
		gl_submit.signals[i] = submit->signals[i];
}

// returns the number of submissions flushed in queue
static int gl_device_flush_submission(VIDevice device)
{
	int total_flush_count = 0;
	int flush_count;

	// modify semaphore signal state and perform cascading submission
	do {
		flush_count = 0;

		for (GLSubmitInfo& submit : device->gl.submits)
		{
			size_t cmd_count = submit.cmds.size();
			size_t wait_count = submit.waits.size();
			size_t signal_count = submit.signals.size();

			bool is_submit_ready = true;

			for (size_t j = 0; j < wait_count; j++)
			{
				if (!submit.waits[j]->gl_signal)
				{
					is_submit_ready = false;
					break;
				}
			}

			if (!is_submit_ready || submit.cmds.empty())
				continue;

			// execute all command buffers in submission and signal semaphores
			for (size_t j = 0; j < cmd_count; j++)
				gl_cmd_execute(device, submit.cmds[j]);

			submit.cmds.clear();

			for (size_t j = 0; j < signal_count; j++)
				submit.signals[j]->gl_signal = true;

			flush_count++;
		}

		total_flush_count += flush_count;
	} while (flush_count > 0);

	device->gl.submits.erase(std::remove_if(
		device->gl.submits.begin(),
		device->gl.submits.end(),
		[](const GLSubmitInfo& submit) { return submit.cmds.empty(); }
	));

	return total_flush_count;
}

static void gl_create_module(VIDevice device, VIModule module, const VIModuleInfo* info)
{
	VI_ASSERT(info->pipeline_layout);

	EShLanguage stage;
	GLenum glstage;

	cast_module_type_glslang(info->type, &stage);
	cast_module_type_gl(info->type, &glstage);

	VICompileResult result;
	uint32_t remap_count = info->pipeline_layout->gl.remap_count;
	const GLRemap* remaps = info->pipeline_layout->gl.remaps;

	GLint glsl_size = result.gl_patched.size() + 1;
	const char* glsl_data;

	if (info->vise_binary)
	{
		VIBinaryHeader header;
		uint8_t* now = (uint8_t*)info->vise_binary;
		sread_header(&now, &header);
		uint32_t header_size = header.header_size;

		// load GL push constant table, used during gl_cmd_execute_push_constants
		module->gl.push_constant_count = header.glpc_count;
		module->gl.push_constants = (GLPushConstant*)vi_malloc(sizeof(GLPushConstant) * header.glpc_count);
		for (uint32_t i = 0; i < module->gl.push_constant_count; i++)
		{
			new (module->gl.push_constants + i)GLPushConstant();
			sread_glpc(&now, module->gl.push_constants + i);
		}

		// load patched GLSL
		glsl_size = (GLint)header.payload_size;
		glsl_data = ((const char*)info->vise_binary) + header_size;
	}
	else if (info->vise_glsl)
	{
		compile_gl(result, stage, info->vise_glsl, remap_count, remaps);
		VI_ASSERT(result.success && "gl_create_module: compilation failed");
		glsl_size = (GLint)result.gl_patched.size();
		glsl_data = (const char*)result.gl_patched.data();

		// copy GL push constant table to VIModule
		module->gl.push_constant_count = (uint32_t)result.gl_push_constants.size();
		module->gl.push_constants = (GLPushConstant*)vi_malloc(sizeof(GLPushConstant) * module->gl.push_constant_count);
		for (uint32_t i = 0; i < module->gl.push_constant_count; i++)
		{
			new (module->gl.push_constants + i)GLPushConstant();
			module->gl.push_constants[i] = result.gl_push_constants[i];
		}
	}
	else
		VI_UNREACHABLE;

	GLuint shader = module->gl.shader = glCreateShader(glstage);
	glShaderSource(shader, 1, &glsl_data, &glsl_size);
	glCompileShader(shader);

	GLint success;
	std::string infoLog;
	infoLog.resize(512);

	glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		glGetShaderInfoLog(shader, infoLog.size(), NULL, infoLog.data());
		std::cout << "vise glCompileShader failed\n" << infoLog << std::endl;
	}
	VI_ASSERT(success);
}

static void gl_destroy_module(VIDevice device, VIModule module)
{
	if (module->gl.push_constant_count > 0)
	{
		for (uint32_t i = 0; i < module->gl.push_constant_count; i++)
			module->gl.push_constants[i].~GLPushConstant();
		vi_free(module->gl.push_constants);
	}

	glDeleteShader(module->gl.shader);
}

static void gl_create_pipeline_layout(VIDevice device, VIPipelineLayout layout, const VIPipelineLayoutInfo* info)
{
	uint32_t set_count = layout->set_layouts.size();
	std::vector<uint32_t> binding_counts(set_count);
	std::vector<const VIBinding*> set_bindings(set_count);

	for (uint32_t i = 0; i < set_count; i++)
	{
		binding_counts[i] = (uint32_t)layout->set_layouts[i]->bindings.size();
	
		for (uint32_t j = 0; j < binding_counts[i]; j++)
			set_bindings[i] = layout->set_layouts[i]->bindings.data();
	}

	std::vector<GLRemap> remaps;
	gl_remap(remaps, set_count, binding_counts.data(), set_bindings.data());

	uint32_t remap_count = (uint32_t)remaps.size();
	layout->gl.remap_count = remap_count;

	if (remap_count > 0)
	{
		layout->gl.remaps = (GLRemap*)vi_malloc(sizeof(GLRemap) * remap_count);
		for (uint32_t i = 0; i < remap_count; i++)
			layout->gl.remaps[i] = remaps[i];
	}
	else
		layout->gl.remaps = nullptr;
}

static void gl_remap(std::vector<GLRemap>& remaps, uint32_t set_count, uint32_t* binding_counts, const VIBinding** bindings)
{
	remaps.clear();

	uint32_t buffer_remap_count = 0;
	uint32_t image_remap_count = 0;

	for (uint32_t set_idx = 0; set_idx < set_count; set_idx++)
	{
		uint32_t binding_count = binding_counts[set_idx];

		for (uint32_t i = 0; i < binding_count; i++)
		{
			const VIBinding* binding = bindings[set_idx] + i;

			GLRemap remap;
			remap.type = binding->type;
			remap.vk_set_binding = set_idx * 100 + binding->binding_index;

			switch (remap.type)
			{
			case VI_BINDING_TYPE_STORAGE_BUFFER:
			case VI_BINDING_TYPE_UNIFORM_BUFFER:
				remap.gl_binding = buffer_remap_count;
				buffer_remap_count += binding->array_count;
				break;
			case VI_BINDING_TYPE_COMBINED_IMAGE_SAMPLER:
			case VI_BINDING_TYPE_STORAGE_IMAGE:
				// layout (binding = N) uniform samplerX -> sample from texture unit N
				// layout (binding = N) uniform image2D -> sample from image unit N
				remap.gl_binding = image_remap_count;
				image_remap_count += binding->array_count;
				break;
			default:
				VI_UNREACHABLE;
			}

			remaps.push_back(remap);
		}
	}
}

static void gl_destroy_pipeline_layout(VIDevice device, VIPipelineLayout layout)
{
	if (layout->gl.remaps)
		vi_free(layout->gl.remaps);
}

static void gl_create_pipeline(VIDevice device, VIPipeline pipeline, VIModule vm, VIModule fm)
{
	pipeline->gl.program = glCreateProgram();

	glAttachShader(pipeline->gl.program, vm->gl.shader);
	glAttachShader(pipeline->gl.program, fm->gl.shader);
	glLinkProgram(pipeline->gl.program);

	GLint success;
	std::string infoLog;
	infoLog.resize(512);

	glGetProgramiv(pipeline->gl.program, GL_LINK_STATUS, &success);
	if (!success)
	{
		glGetProgramInfoLog(pipeline->gl.program, infoLog.size(), NULL, infoLog.data());
		std::cout << "vise glLinkProgram failed\n" << infoLog << std::endl;
	}
	VI_ASSERT(success);

	// TODO: make vi_cmd_bind_index_buffer, vi_cmd_bind_vertex_buffers, and vi_cmd_bind_pipeline order independent
	glCreateVertexArrays(1, &pipeline->gl.vao);
	glBindVertexArray(pipeline->gl.vao);

	for (uint32_t binding = 0; binding < pipeline->vertex_bindings.size(); binding++)
	{
		GLuint divisor = pipeline->vertex_bindings[binding].rate == VK_VERTEX_INPUT_RATE_INSTANCE ? 1 : 0;

		glVertexBindingDivisor(binding, divisor);
	}

	for (uint32_t location = 0; location < pipeline->vertex_attributes.size(); location++)
	{
		VIVertexAttribute* attr = pipeline->vertex_attributes.data() + location;

		GLint attr_component_count;
		GLenum attr_component_type;
		GLuint attr_offset = (GLuint)attr->offset;
		GLuint attr_binding = (GLuint)attr->binding;
		GLboolean normalized = GL_FALSE; // TODO:
		cast_glsl_type_gl(attr->type, &attr_component_count, &attr_component_type);

		glEnableVertexAttribArray(location);
		glVertexAttribFormat(location, attr_component_count, attr_component_type, normalized, attr_offset);
		glVertexAttribBinding(location, attr_binding);
	}
}

static void gl_destroy_pipeline(VIDevice device, VIPipeline pipeline)
{
	glDeleteVertexArrays(1, &pipeline->gl.vao);
	glDeleteProgram(pipeline->gl.program);
}

static void gl_create_compute_pipeline(VIDevice device, VIComputePipeline pipeline, VIModule compute_module)
{
	pipeline->gl.program = glCreateProgram();

	glAttachShader(pipeline->gl.program, compute_module->gl.shader);
	glLinkProgram(pipeline->gl.program);

	GLint success;
	std::string infoLog;
	infoLog.resize(512);

	glGetProgramiv(pipeline->gl.program, GL_LINK_STATUS, &success);
	if (!success)
	{
		glGetProgramInfoLog(pipeline->gl.program, infoLog.size(), NULL, infoLog.data());
		std::cout << "vise glLinkProgram failed\n" << infoLog << std::endl;
	}
}

static void gl_destroy_compute_pipeline(VIDevice device, VIComputePipeline pipeline)
{
	glDeleteProgram(pipeline->gl.program);
}

static void gl_create_buffer(VIDevice device, VIBuffer buffer, const VIBufferInfo* info)
{
	if (info->type == VI_BUFFER_TYPE_TRANSFER)
	{
		buffer->map = (uint8_t*)vi_malloc(buffer->size);
		buffer->gl.target = GL_NONE;
		return;
	}
	else
		buffer->map = nullptr;

	GLenum gltype;
	cast_buffer_type(info->type, &gltype);

	buffer->gl.target = gltype;

	glCreateBuffers(1, &buffer->gl.handle);
	glBindBuffer(buffer->gl.target, buffer->gl.handle);
	glBufferData(buffer->gl.target, buffer->size, nullptr, GL_STATIC_DRAW);
	GL_CHECK();
}

static void gl_destroy_buffer(VIDevice device, VIBuffer buffer)
{
	if (buffer->map)
		vi_free(buffer->map);

	glDeleteBuffers(1, &buffer->gl.handle);
}

static void gl_create_image(VIOpenGL* gl, VIImage image, const VIImageInfo* info)
{
	GLenum target;
	cast_image_type(image->info.type, &target);
	image->gl.target = target;

	uint32_t texel_size;
	GLenum internal_format, data_format, data_type;
	cast_format_gl(info->format, &internal_format, &data_format, &data_type, &texel_size);
	image->gl.internal_format = internal_format;
	image->gl.data_format = data_format;
	image->gl.data_type = data_type;
	
	GL_CHECK(glCreateTextures(target, 1, &image->gl.handle));
	glBindTexture(target, image->gl.handle);

	if (target == GL_TEXTURE_2D || target == GL_TEXTURE_CUBE_MAP)
		glTexStorage2D(target, info->levels, internal_format, info->width, info->height);
	else if (target == GL_TEXTURE_2D_ARRAY)
		glTexStorage3D(target, info->levels, internal_format, info->width, info->height, info->layers);
	else
		VI_UNREACHABLE;

	GL_CHECK();

	GLenum address_mode;
	cast_sampler_address_mode_gl(image->info.sampler.address_mode, &address_mode);
	GL_CHECK(glTexParameteri(target, GL_TEXTURE_WRAP_S, address_mode));
	GL_CHECK(glTexParameteri(target, GL_TEXTURE_WRAP_T, address_mode));

	if (target == GL_TEXTURE_CUBE_MAP)
		GL_CHECK(glTexParameteri(target, GL_TEXTURE_WRAP_R, address_mode));

	GLenum min_filter, mag_filter;
	cast_filter_gl(image->info.sampler, &min_filter, &mag_filter);
	GL_CHECK(glTexParameteri(target, GL_TEXTURE_MIN_FILTER, min_filter));
	GL_CHECK(glTexParameteri(target, GL_TEXTURE_MAG_FILTER, mag_filter));

	GL_CHECK(glTexParameterf(target, GL_TEXTURE_MIN_LOD, info->sampler.min_lod));
	GL_CHECK(glTexParameterf(target, GL_TEXTURE_MAX_LOD, info->sampler.max_lod));
}

static void gl_destroy_image(VIOpenGL* gl, VIImage image)
{
	GL_CHECK(glDeleteTextures(1, &image->gl.handle));
}

static void gl_create_framebuffer(VIOpenGL* gl, VIFramebuffer fb, const VIFramebufferInfo* info)
{
	GL_CHECK(glCreateFramebuffers(1, &fb->gl.handle));
	glBindFramebuffer(GL_FRAMEBUFFER, fb->gl.handle);

	for (uint32_t i = 0; i < info->color_attachment_count; i++)
	{
		VI_ASSERT(info->color_attachments[i]->info.usage & VI_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, info->color_attachments[i]->gl.handle, 0);
	}

	if (info->depth_stencil_attachment)
	{
		VI_ASSERT(info->depth_stencil_attachment->info.usage & VI_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);

		GLenum attachment_type;
		cast_format_attachment_gl(info->depth_stencil_attachment->info.format, &attachment_type);
		glFramebufferTexture2D(GL_FRAMEBUFFER, attachment_type, GL_TEXTURE_2D, info->depth_stencil_attachment->gl.handle, 0);
	}

	GLenum status;
	if ((status = glCheckFramebufferStatus(GL_FRAMEBUFFER)) != GL_FRAMEBUFFER_COMPLETE)
	{
		printf("glCheckFramebufferStatus(GL_FRAMEBUFFER)) %d\n", status);
		VI_UNREACHABLE;
	}
}

static void gl_destroy_framebuffer(VIOpenGL* gl, VIFramebuffer fb)
{
	GL_CHECK(glDeleteFramebuffers(1, &fb->gl.handle));
}

// OpenGL swapchain-framebuffer is just a wrapper over the default-framebuffer
static void gl_create_swapchain_framebuffer(VIOpenGL* gl, VIFramebuffer fb)
{
	int width, height;
	GLFWwindow* window = glfwGetCurrentContext();
	glfwGetFramebufferSize(window, &width, &height);

	fb->device = gl->vi_device;
	fb->gl.handle = 0;
	fb->extent.width = (uint32_t)width;
	fb->extent.height = (uint32_t)height;
}

static void gl_create_swapchain_pass(VIOpenGL* gl, VIPass pass)
{
	// TODO: single source of truth with vulkan swapchain pass

	VkAttachmentDescription atch[2];
	atch[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	atch[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	atch[0].format = VK_FORMAT_D32_SFLOAT_S8_UINT; // TODO: query default framebuffer
	atch[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	atch[0].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	atch[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	atch[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	atch[0].flags = 0;
	atch[0].samples = VK_SAMPLE_COUNT_1_BIT;

	atch[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	atch[1].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	atch[1].format = VK_FORMAT_B8G8R8A8_UNORM; // TODO: query default framebuffer
	atch[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	atch[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	atch[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	atch[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	atch[1].flags = 0;
	atch[1].samples = VK_SAMPLE_COUNT_1_BIT;
}

static void gl_alloc_cmd_buffer(VIDevice device, VICommand cmd)
{
	cmd->gl.list_capacity = VI_GL_COMMAND_LIST_CAPACITY;
	cmd->gl.list_size = 0;
	cmd->gl.list = (GLCommand*)vi_malloc(sizeof(GLCommand) * VI_GL_COMMAND_LIST_CAPACITY);
}

static void gl_free_command(VIDevice device, VICommand cmd)
{
	gl_reset_command(device, cmd);

	vi_free(cmd->gl.list);
}

static void gl_alloc_set(VIDevice device, VISet set)
{
	size_t binding_count = set->layout->bindings.size();
	VI_ASSERT(binding_count > 0);

	set->gl.binding_sites = (void**)vi_malloc(sizeof(void*) * binding_count);

	for (uint32_t i = 0; i < binding_count; i++)
		set->gl.binding_sites[i] = nullptr;
}

static void gl_free_set(VIDevice device, VISet set)
{
	vi_free(set->gl.binding_sites);
}

static void gl_set_update(VISet set, uint32_t update_count, const VISetUpdateInfo* updates)
{
	for (uint32_t i = 0; i < update_count; i++)
	{
		uint32_t binding = updates[i].binding_index;

		switch (set->layout->bindings[binding].type)
		{
		case VI_BINDING_TYPE_UNIFORM_BUFFER:
		case VI_BINDING_TYPE_STORAGE_BUFFER:
			VI_ASSERT(updates[i].buffer);
			set->gl.binding_sites[binding] = (void*)updates[i].buffer;
			break;
		case VI_BINDING_TYPE_STORAGE_IMAGE:
		case VI_BINDING_TYPE_COMBINED_IMAGE_SAMPLER:
			VI_ASSERT(updates[i].image);
			set->gl.binding_sites[binding] = (void*)updates[i].image;
			break;
		default:
			VI_UNREACHABLE;
		}
	}
}

static void gl_pipeline_layout_get_remapped_binding(VIPipelineLayout layout,
	uint32_t set_index, uint32_t binding_idx, uint32_t* remapped_binding)
{
	for (uint32_t i = 0; i < layout->gl.remap_count; i++)
	{
		GLRemap* remap = layout->gl.remaps + i;

		if (remap->vk_set_binding == set_index * 100 + binding_idx)
		{
			*remapped_binding = remap->gl_binding;
			return;
		}
	}

	// failed to find (set_idx + binding_idx) combination in pipeline layout
	VI_UNREACHABLE;
}

static void gl_copy_buffer(VIBuffer src, VIBuffer dst, uint32_t src_offset, uint32_t dst_offset, uint32_t size)
{
	VI_ASSERT(src && src_offset + size <= src->size);
	VI_ASSERT(dst && dst_offset + size <= dst->size);

	if (src->type == VI_BUFFER_TYPE_TRANSFER && dst->type == VI_BUFFER_TYPE_TRANSFER)
	{
		memcpy(dst->map + dst_offset, src->map + src_offset, size);
		return;
	}

	if (src->type == VI_BUFFER_TYPE_TRANSFER && dst->type != VI_BUFFER_TYPE_TRANSFER)
	{
		glBindBuffer(dst->gl.target, dst->gl.handle);
		glBufferSubData(dst->gl.target, dst_offset, size, src->map + src_offset);
		GL_CHECK();
		return;
	}

	if (src->type != VI_BUFFER_TYPE_TRANSFER && dst->type == VI_BUFFER_TYPE_TRANSFER)
	{
		glBindBuffer(src->gl.target, src->gl.handle);
		glGetBufferSubData(src->gl.target, src_offset, size, dst->map + dst_offset);
		GL_CHECK();
		return;
	}

	// src->type != VI_BUFFER_TYPE_TRANSFER && dst->type != VI_BUFFER_TYPE_TRANSFER
	glCopyNamedBufferSubData(src->gl.handle, dst->gl.handle, src_offset, dst_offset, size);
	GL_CHECK();
}

static void gl_copy_buffer_to_image(VIBuffer buffer, VIImage image, uint32_t buffer_offset, const VkOffset3D& image_offset, const VkExtent3D& image_extent,
	const VkImageSubresourceLayers& image_subresource)
{
	VI_ASSERT(image_offset.z == 0 && image_extent.depth >= 1);

	uint32_t texel_size;
	GLenum internal_format, data_format, data_type;
	cast_format_gl(image->info.format, &internal_format, &data_format, &data_type, &texel_size);
	uint32_t layer_start = image_subresource.baseArrayLayer;
	uint32_t layer_count = image_subresource.layerCount;
	uint32_t layer_size = image_extent.width * image_extent.height * image_extent.depth * texel_size;
	uint32_t access_size = layer_size * layer_count;

	VI_ASSERT(buffer_offset + access_size <= buffer->size);

	if (!buffer->map)
		buffer->map = (uint8_t*)vi_malloc(buffer->size);
	void* data = buffer->map + buffer_offset;

	if (buffer->type != VI_BUFFER_TYPE_TRANSFER)
	{
		glBindBuffer(buffer->gl.target, buffer->gl.handle);
		glGetBufferSubData(buffer->gl.target, buffer_offset, access_size, data);
	}
	
	GL_CHECK();

	uint32_t mip_level = image_subresource.mipLevel;

	if (image->info.type == VI_IMAGE_TYPE_2D)
	{
		glBindTexture(GL_TEXTURE_2D, image->gl.handle);
		glTexSubImage2D(GL_TEXTURE_2D, mip_level, image_offset.x, image_offset.y, image_extent.width, image_extent.height, data_format, data_type, data);
	}
	else if (image->info.type == VI_IMAGE_TYPE_2D_ARRAY)
	{
		glBindTexture(GL_TEXTURE_2D_ARRAY, image->gl.handle);
		glTexSubImage3D(GL_TEXTURE_2D_ARRAY, mip_level, image_offset.x, image_offset.y, layer_start, image_extent.width, image_extent.height, layer_count, data_format, data_type, data);
	}
	else if (image->info.type == VI_IMAGE_TYPE_CUBE)
	{
		glBindTexture(GL_TEXTURE_CUBE_MAP, image->gl.handle);

		for (uint32_t i = layer_start; i < layer_count; i++)
		{
			uint8_t* face_data = (uint8_t*)data + layer_size * i;
			glTexSubImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, mip_level, image_offset.x, image_offset.y, image_extent.width, image_extent.height,data_format, data_type, face_data);
		}
	}
	else
		VI_UNREACHABLE;

	GL_CHECK();
}

static void gl_copy_image(VIImage src, VIImage dst, const VkOffset3D& src_offset, const VkOffset3D& dst_offset, const VkExtent3D& extent,
	const VkImageSubresourceLayers& src_subresource, const VkImageSubresourceLayers& dst_subresource)
{
	VI_ASSERT(src->info.format == dst->info.format && "format aliasing not supported");

	GLint src_mip_level = src_subresource.mipLevel;
	GLint dst_mip_level = dst_subresource.mipLevel;
	GLint src_layer_start = src_subresource.baseArrayLayer;
	GLint src_layer_count = src_subresource.layerCount;
	GLint src_offset_z = src_offset.z;
	GLint src_depth = extent.depth;
	GLint dst_layer_start = dst_subresource.baseArrayLayer;
	GLint dst_layer_count = dst_subresource.layerCount;
	GLint dst_offset_z = dst_offset.z;

	VI_ASSERT(src_layer_count == 1 && dst_layer_count == 1); // TODO:

	uint32_t texel_size;
	GLenum internal_format, data_format, data_type;
	cast_format_gl(src->info.format, &internal_format, &data_format, &data_type, &texel_size);
	uint32_t access_size = extent.width * extent.height * extent.depth * texel_size;

	// reinterpret Z axis as array layers if necessary
	
	if (src->info.type == VI_IMAGE_TYPE_CUBE || src->info.type == VI_IMAGE_TYPE_2D_ARRAY)
	{
		src_offset_z = src_layer_start;
		src_depth = src_layer_count;
	}

	if (dst->info.type == VI_IMAGE_TYPE_CUBE || dst->info.type == VI_IMAGE_TYPE_2D_ARRAY)
		dst_offset_z = dst_layer_start;

	glCopyImageSubData(
		src->gl.handle, src->gl.target, src_mip_level, src_offset.x, src_offset.y, src_offset_z,
		dst->gl.handle, dst->gl.target, dst_mip_level, dst_offset.x, dst_offset.y, dst_offset_z,
		extent.width, extent.height, src_depth);

	GL_CHECK();
}

static void gl_copy_image_to_buffer(VIImage image, VIBuffer buffer, uint32_t buffer_offset, const VkOffset3D& image_offset, const VkExtent3D& image_extent,
	const VkImageSubresourceLayers& image_subresource)
{
	uint32_t texel_size;
	GLenum internal_format, data_format, data_type;
	cast_format_gl(image->info.format, &internal_format, &data_format, &data_type, &texel_size);
	uint32_t layer_start = image_subresource.baseArrayLayer;
	uint32_t layer_count = image_subresource.layerCount;
	uint32_t layer_size = image_extent.width * image_extent.height * image_extent.depth * texel_size;
	uint32_t access_size = layer_size * layer_count;
	
	VI_ASSERT(buffer_offset + access_size <= buffer->size);

	if (!buffer->map)
		buffer->map = (uint8_t*)vi_malloc(buffer->size);
	void* data = buffer->map + buffer_offset;

	uint32_t mip_level = image_subresource.mipLevel;

	if (image->info.type == VI_IMAGE_TYPE_2D)
	{
		glGetTextureSubImage(image->gl.handle, mip_level, image_offset.x, image_offset.y, image_offset.z, image_extent.width, image_extent.height, image_extent.depth, data_format, data_type, access_size, data);
	}
	else if (image->info.type == VI_IMAGE_TYPE_2D_ARRAY || image->info.type == VI_IMAGE_TYPE_CUBE)
	{
		glGetTextureSubImage(image->gl.handle, mip_level, image_offset.x, image_offset.y, layer_start, image_extent.width, image_extent.height, layer_count, data_format, data_type, access_size, data);
	}
	else
		VI_UNREACHABLE;

	GL_CHECK();

	if (buffer->type == VI_BUFFER_TYPE_TRANSFER)
		return;

	glBindBuffer(buffer->gl.target, buffer->gl.handle);
	glBufferSubData(buffer->gl.target, buffer_offset, access_size, data);
	GL_CHECK();
}

static GLCommand* gl_append_command(VICommand cmd, GLCommandType type)
{
	if (cmd->gl.list_size == cmd->gl.list_capacity)
	{
		uint32_t new_capacity = cmd->gl.list_capacity * 2;
		GLCommand* new_list = (GLCommand*)vi_malloc(sizeof(GLCommand) * new_capacity);

		memcpy(new_list, cmd->gl.list, sizeof(GLCommand) * cmd->gl.list_capacity);
		vi_free(cmd->gl.list);

		cmd->gl.list = new_list;
		cmd->gl.list_capacity = new_capacity;
	}

	GLCommand* glcmd = cmd->gl.list + cmd->gl.list_size++;
	glcmd->type = type;

	return glcmd;
}

static void gl_reset_command(VIDevice device, VICommand cmd)
{
	for (uint32_t i = 0; i < cmd->gl.list_size; i++)
	{
		GLCommand* glcmd = cmd->gl.list + i;

		switch (glcmd->type)
		{
		case GL_COMMAND_TYPE_BIND_VERTEX_BUFFERS:
			glcmd->bind_vertex_buffers.~GLCommandBindVertexBuffers();
			break;
		case GL_COMMAND_TYPE_PUSH_CONSTANTS:
			glcmd->push_constants.~GLCommandPushConstants();
			break;
		case GL_COMMAND_TYPE_BEGIN_PASS:
			glcmd->begin_pass.~GLCommandBeginPass();
			break;
		case GL_COMMAND_TYPE_COPY_BUFFER:
			glcmd->copy_buffer.~GLCommandCopyBuffer();
			break;
		case GL_COMMAND_TYPE_COPY_BUFFER_TO_IMAGE:
			glcmd->copy_buffer_to_image.~GLCommandCopyBufferToImage();
			break;
		case GL_COMMAND_TYPE_COPY_IMAGE:
			glcmd->copy_image.~GLCommandCopyImage();
			break;
		case GL_COMMAND_TYPE_COPY_IMAGE_TO_BUFFER:
			glcmd->copy_image_to_buffer.~GLCommandCopyImageToBuffer();
			break;
		default:
			break;
		}
	}

	cmd->gl.list_size = 0;
}

static void gl_cmd_execute(VIDevice device, VICommand cmd)
{
	for (uint32_t i = 0; i < cmd->gl.list_size; i++)
	{
		GLCommand* glcmd = cmd->gl.list + i;
		VI_ASSERT(gl_cmd_execute_table[glcmd->type] != nullptr);

		gl_cmd_execute_table[glcmd->type](device, glcmd);
	}
}

static void gl_cmd_execute_opengl_callback(VIDevice device, GLCommand* glcmd)
{
	VI_ASSERT(glcmd->type == GL_COMMAND_TYPE_OPENGL_CALLBACK);

	void* data = glcmd->opengl_callback.data;

	if (glcmd->opengl_callback.callback)
		glcmd->opengl_callback.callback(data);
}

static void gl_cmd_execute_set_viewport(VIDevice device, GLCommand* glcmd)
{
	VI_ASSERT(glcmd->type == GL_COMMAND_TYPE_SET_VIEWPORT);

	GLint x = (GLint)glcmd->set_viewport.x;
	GLint y = (GLint)glcmd->set_viewport.y;
	GLsizei width = (GLsizei)glcmd->set_viewport.width;
	GLsizei height = (GLsizei)glcmd->set_viewport.height;
	GLsizei fb_height = (GLsizei)device->gl.active_framebuffer->extent.height;

	glViewport(x, y, width, height);
}

static void gl_cmd_execute_set_scissor(VIDevice device, GLCommand* glcmd)
{
	VI_ASSERT(glcmd->type == GL_COMMAND_TYPE_SET_SCISSOR);

	GLint x = (GLint)glcmd->set_scissor.offset.x;
	GLint y = (GLint)glcmd->set_scissor.offset.y;
	GLsizei width = (GLsizei)glcmd->set_scissor.extent.width;
	GLsizei height = (GLsizei)glcmd->set_scissor.extent.height;
	GLsizei fb_height = (GLsizei)device->gl.active_framebuffer->extent.height;

	glEnable(GL_SCISSOR_TEST);
	glScissor(x, y, width, height);
}

static void gl_cmd_execute_draw(VIDevice device, GLCommand* glcmd)
{
	VI_ASSERT(glcmd->type == GL_COMMAND_TYPE_DRAW);

	GLenum mode = GL_TRIANGLES; // TODO:
	GLint first = (GLint)glcmd->draw.vertex_start;
	GLsizei count = (GLsizei)glcmd->draw.vertex_count;
	GLsizei instance_count = (GLsizei)glcmd->draw.instance_count;
	GLuint base_instance = (GLuint)glcmd->draw.instance_start;

	GLint location = glGetUniformLocation(device->gl.active_program, "SPIRV_Cross_BaseInstance");
	if (location >= 0)
		glUniform1i(location, glcmd->draw.instance_start);
	
	glDrawArraysInstancedBaseInstance(mode, first, count, instance_count, base_instance);
}

static void gl_cmd_execute_draw_indexed(VIDevice device, GLCommand* glcmd)
{
	VI_ASSERT(glcmd->type == GL_COMMAND_TYPE_DRAW_INDEXED);

	size_t index_size = device->gl.index_size;
	GLenum index_type = device->gl.index_type;
	GLenum mode = GL_TRIANGLES; // TODO:
	GLsizei index_count = (GLsizei)glcmd->draw_indexed.index_count;
	GLint base_index = (GLint)glcmd->draw_indexed.index_start;
	GLuint base_instance = (GLuint)glcmd->draw_indexed.instance_start;
	GLsizei instance_count = (GLsizei)glcmd->draw_indexed.instance_count;

	GLint location = glGetUniformLocation(device->gl.active_program, "SPIRV_Cross_BaseInstance");
	if (location >= 0)
		glUniform1i(location, glcmd->draw.instance_start);

	glDrawElementsInstancedBaseVertexBaseInstance(mode, index_count, index_type, (const void*)(base_index * index_size), instance_count, 0, base_instance);
}

static void gl_cmd_execute_push_constants(VIDevice device, GLCommand* glcmd)
{
	VI_ASSERT(glcmd->type == GL_COMMAND_TYPE_PUSH_CONSTANTS);

	uint32_t range_size = glcmd->push_constants.size;
	uint32_t range_offset = glcmd->push_constants.offset;
	VIModule module = device->gl.active_module;

	for (uint32_t i = 0; i < module->gl.push_constant_count; i++)
	{
		const GLPushConstant* pc = module->gl.push_constants + i;
		GLint pc_loc = glGetUniformLocation(device->gl.active_program, pc->uniform_name.c_str());

		// only update uniform variable if it is completely in range.
		if (pc_loc >= 0 && (pc->offset >= range_offset) && (pc->offset + pc->size <= range_offset + range_size))
		{
			const uint8_t* value_base = glcmd->push_constants.value + (pc->offset - range_offset);

			// TODO: maybe flatten with table of function pointers?
			//       void (*uniform_fn)(GLint, GLsizei, const void*)
			switch (pc->uniform_glsl_type)
			{
			case VI_GLSL_TYPE_FLOAT:
				glUniform1fv(pc_loc, pc->uniform_arr_size, (const GLfloat*)value_base);
				break;
			case VI_GLSL_TYPE_VEC2:
				glUniform2fv(pc_loc, pc->uniform_arr_size, (const GLfloat*)value_base);
				break;
			case VI_GLSL_TYPE_VEC3:
				glUniform3fv(pc_loc, pc->uniform_arr_size, (const GLfloat*)value_base);
				break;
			case VI_GLSL_TYPE_VEC4:
				glUniform4fv(pc_loc, pc->uniform_arr_size, (const GLfloat*)value_base);
				break;
			case VI_GLSL_TYPE_UINT:
				glUniform1uiv(pc_loc, pc->uniform_arr_size, (const GLuint*)value_base);
				break;
			case VI_GLSL_TYPE_UVEC2:
				glUniform2uiv(pc_loc, pc->uniform_arr_size, (const GLuint*)value_base);
				break;
			case VI_GLSL_TYPE_UVEC3:
				glUniform3uiv(pc_loc, pc->uniform_arr_size, (const GLuint*)value_base);
				break;
			case VI_GLSL_TYPE_UVEC4:
				glUniform4uiv(pc_loc, pc->uniform_arr_size, (const GLuint*)value_base);
				break;
			case VI_GLSL_TYPE_INT:
				glUniform1iv(pc_loc, pc->uniform_arr_size, (const GLint*)value_base);
				break;
			case VI_GLSL_TYPE_IVEC2:
				glUniform2iv(pc_loc, pc->uniform_arr_size, (const GLint*)value_base);
				break;
			case VI_GLSL_TYPE_IVEC3:
				glUniform3iv(pc_loc, pc->uniform_arr_size, (const GLint*)value_base);
				break;
			case VI_GLSL_TYPE_IVEC4:
				glUniform4iv(pc_loc, pc->uniform_arr_size, (const GLint*)value_base);
				break;
			case VI_GLSL_TYPE_MAT4:
				glUniformMatrix4fv(pc_loc, 1, false, (const GLfloat*)value_base);
				break;
			default:
				VI_UNREACHABLE;
			}
		}
	}
}

static void gl_cmd_execute_bind_set(VIDevice device, GLCommand* glcmd)
{
	VI_ASSERT(glcmd->type == GL_COMMAND_TYPE_BIND_SET);

	uint32_t set_idx = glcmd->bind_set.set_index;
	VISet set = glcmd->bind_set.set;
	VIPipelineLayout pipeline_layout = glcmd->bind_set.pipeline_layout;
	VIBuffer buffer;
	VIImage image;
	GLenum internal_format;
	GLenum data_format, data_type;
	uint32_t texel_size;
	uint32_t remapped_binding;
	uint32_t binding_count = (uint32_t)set->layout->bindings.size();

	for (uint32_t binding_idx = 0; binding_idx < binding_count; binding_idx++)
	{
		gl_pipeline_layout_get_remapped_binding(pipeline_layout, set_idx, binding_idx, &remapped_binding);

		switch (set->layout->bindings[binding_idx].type)
		{
		case VI_BINDING_TYPE_UNIFORM_BUFFER:
			buffer = (VIBuffer)set->gl.binding_sites[binding_idx];
			if (buffer)
			{
				glBindBufferBase(GL_UNIFORM_BUFFER, remapped_binding, buffer->gl.handle);
			}
			break;
		case VI_BINDING_TYPE_STORAGE_BUFFER:
			buffer = (VIBuffer)set->gl.binding_sites[binding_idx];
			if (buffer)
			{
				glBindBufferBase(GL_SHADER_STORAGE_BUFFER, remapped_binding, buffer->gl.handle);
			}
			break;
		case VI_BINDING_TYPE_COMBINED_IMAGE_SAMPLER:
			image = (VIImage)set->gl.binding_sites[binding_idx];
			if (image)
			{
				glActiveTexture(GL_TEXTURE0 + remapped_binding);
				glBindTexture(image->gl.target, image->gl.handle);
			}
			break;
		case VI_BINDING_TYPE_STORAGE_IMAGE:
			image = (VIImage)set->gl.binding_sites[binding_idx];
			if (image)
			{
				cast_format_gl(image->info.format, &internal_format, &data_format, &data_type, &texel_size);
				glBindImageTexture(remapped_binding, image->gl.handle, 0, GL_FALSE, 0, GL_READ_ONLY /* TODO: reflect SPIRV */, internal_format);
			}
			break;
		default:
			VI_UNREACHABLE;
		}
	}
}

static void gl_cmd_execute_bind_pipeline(VIDevice device, GLCommand* glcmd)
{
	VI_ASSERT(glcmd->type == GL_COMMAND_TYPE_BIND_PIPELINE);

	VIPipeline pipeline = device->active_pipeline = glcmd->bind_pipeline;

	// for OpenGL push constants, the lookup table is stored in each module,
	// if both VM and FM have a lookup table they should be identical.
	VIModule active_vm = pipeline->vertex_module;
	VIModule active_fm = pipeline->fragment_module;

	device->gl.active_program = pipeline->gl.program;
	device->gl.active_module = active_vm;
	if (active_vm->gl.push_constant_count == 0 && active_fm->gl.push_constant_count > 0)
		device->gl.active_module = active_fm;

	glBindVertexArray(glcmd->bind_pipeline->gl.vao);
	glUseProgram(glcmd->bind_pipeline->gl.program);

	const VIPipelineRasterizationStateInfo* rasterizationI = &pipeline->rasterization_state;
	
	if (rasterizationI->cull_mode != VI_CULL_MODE_NONE)
	{
		glEnable(GL_CULL_FACE);

		GLenum cullMode;
		cast_cull_mode_gl(rasterizationI->cull_mode, &cullMode);
		glCullFace(cullMode);
	}
	else
		glDisable(GL_CULL_FACE);

	GLenum polygonMode;
	cast_polygon_mode_gl(rasterizationI->polygon_mode, &polygonMode);
	glPolygonMode(GL_FRONT_AND_BACK, polygonMode);
	if (polygonMode == GL_LINE)
		glLineWidth(rasterizationI->line_width);

	const VIPipelineDepthStencilStateInfo* dsI = &pipeline->depth_stencil_state;

	// depth stencil states
	if (dsI->depth_test_enabled)
	{
		GLenum depthCompareOp;
		cast_compare_op_gl(dsI->depth_compare_op, &depthCompareOp);

		glEnable(GL_DEPTH_TEST);
		glDepthFunc(depthCompareOp);
		glDepthMask(dsI->depth_write_enabled);
	}
	else
		glDisable(GL_DEPTH_TEST);

	if (dsI->stencil_test_enabled)
	{
		GLenum front_stencil_func, front_sfail, front_dpfail, front_dppass;
		cast_compare_op_gl(dsI->stencil_front.compare_op, &front_stencil_func);
		cast_stencil_op_gl(dsI->stencil_front.fail_op, &front_sfail);
		cast_stencil_op_gl(dsI->stencil_front.depth_fail_op, &front_dpfail);
		cast_stencil_op_gl(dsI->stencil_front.pass_op, &front_dppass);

		GLenum back_stencil_func, back_sfail, back_dpfail, back_dppass;
		cast_compare_op_gl(dsI->stencil_back.compare_op, &back_stencil_func);
		cast_stencil_op_gl(dsI->stencil_back.fail_op, &back_sfail);
		cast_stencil_op_gl(dsI->stencil_back.depth_fail_op, &back_dpfail);
		cast_stencil_op_gl(dsI->stencil_back.pass_op, &back_dppass);

		glEnable(GL_STENCIL_TEST);
		glStencilOpSeparate(GL_FRONT, front_sfail, front_dpfail, front_dppass);
		glStencilFuncSeparate(GL_FRONT, front_stencil_func, dsI->stencil_front.reference, dsI->stencil_front.compare_mask);
		glStencilMaskSeparate(GL_FRONT, dsI->stencil_front.write_mask);
		glStencilOpSeparate(GL_BACK, back_sfail, back_dpfail, back_dppass);
		glStencilFuncSeparate(GL_BACK, back_stencil_func, dsI->stencil_back.reference, dsI->stencil_back.compare_mask);
		glStencilMaskSeparate(GL_BACK, dsI->stencil_back.write_mask);
	}
	else
		glDisable(GL_STENCIL_TEST);

	// blend states
	if (pipeline->blend_state.enabled)
	{
		glEnable(GL_BLEND);

		GLenum srcColorFactor, dstColorFactor, srcAlphaFactor, dstAlphaFactor;
		cast_blend_factor_gl(pipeline->blend_state.src_color_factor, &srcColorFactor);
		cast_blend_factor_gl(pipeline->blend_state.dst_color_factor, &dstColorFactor);
		cast_blend_factor_gl(pipeline->blend_state.src_alpha_factor, &srcAlphaFactor);
		cast_blend_factor_gl(pipeline->blend_state.dst_alpha_factor, &dstAlphaFactor);
		glBlendFuncSeparate(srcColorFactor, dstColorFactor, srcAlphaFactor, dstAlphaFactor);
		
		GLenum colorBlendOp, alphaBlendOp;
		cast_blend_op_gl(pipeline->blend_state.color_blend_op, &colorBlendOp);
		cast_blend_op_gl(pipeline->blend_state.alpha_blend_op, &alphaBlendOp);
		glBlendEquationSeparate(colorBlendOp, alphaBlendOp);
	}
	else
		glDisable(GL_BLEND);
}

void gl_cmd_execute_bind_compute_pipeline(VIDevice device, GLCommand* glcmd)
{
	VI_ASSERT(glcmd->type == GL_COMMAND_TYPE_BIND_COMPUTE_PIPELINE);

	device->gl.active_program = glcmd->bind_compute_pipeline->gl.program;
	device->gl.active_module = glcmd->bind_compute_pipeline->compute_module;

	glUseProgram(glcmd->bind_compute_pipeline->gl.program);
}

static void gl_cmd_execute_bind_vertex_buffers(VIDevice device, GLCommand* glcmd)
{
	VI_ASSERT(glcmd->type == GL_COMMAND_TYPE_BIND_VERTEX_BUFFERS);

	GLuint first_binding = (GLuint)glcmd->bind_vertex_buffers.first_binding;
	GLintptr offset = 0;
	VIPipeline pipeline = glcmd->bind_vertex_buffers.pipeline;

	for (size_t i = 0; i < glcmd->bind_vertex_buffers.buffers.size(); i++)
	{
		VIBuffer vbo = glcmd->bind_vertex_buffers.buffers[i];
		GLsizei stride = (GLsizei)pipeline->vertex_bindings[i].stride;

		glBindVertexBuffer(first_binding + i, vbo->gl.handle, offset, stride);
	}
}

static void gl_cmd_execute_bind_index_buffer(VIDevice device, GLCommand* glcmd)
{
	VI_ASSERT(glcmd->type == GL_COMMAND_TYPE_BIND_INDEX_BUFFER);

	GLuint ibo = glcmd->bind_index_buffer.buffer->gl.handle;
	GLenum index_type;
	size_t index_size;

	cast_index_type(glcmd->bind_index_buffer.index_type, &index_type, &index_size);
	device->gl.index_type = index_type;
	device->gl.index_size = index_size;

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
}

static void gl_cmd_execute_begin_pass(VIDevice device, GLCommand* glcmd)
{
	VI_ASSERT(glcmd->type == GL_COMMAND_TYPE_BEGIN_PASS);
	VI_ASSERT(glcmd->begin_pass.framebuffer && glcmd->begin_pass.pass);

	const std::vector<VkClearValue>& color_clear_values = glcmd->begin_pass.color_clear_values;
	const std::optional<VkClearValue>& depth_stencil_clear_value = glcmd->begin_pass.depth_stencil_clear_value;
	VIFramebuffer framebuffer = glcmd->begin_pass.framebuffer;
	VIPass pass = glcmd->begin_pass.pass;

	device->gl.active_framebuffer = framebuffer;
	glDisable(GL_SCISSOR_TEST); // until gl_cmd_execute_set_scissor

	// flip VIOpenGL clip space Y axis when rendering to offscreen framebuffers
	bool flip_gl_clip_origin = framebuffer != device->swapchain_framebuffers;
	flip_gl_clip_origin = false;
	GLenum clip_origin = flip_gl_clip_origin ? GL_UPPER_LEFT : GL_LOWER_LEFT;
	glClipControl(clip_origin, GL_ZERO_TO_ONE);

	// TODO: swapchain_framebuffer should not be a special case
	if (framebuffer == device->swapchain_framebuffers)
	{
		VI_ASSERT(color_clear_values.size() == 1);
		VI_ASSERT(depth_stencil_clear_value.has_value());

		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		GLfloat depth = (GLfloat)depth_stencil_clear_value.value().depthStencil.depth; // TODO: paraphrase this unholy mess
		glClearDepth(depth);

		GLfloat stencil = (GLfloat)depth_stencil_clear_value.value().depthStencil.stencil; // TODO: only if there are stencil bits
		glClearStencil(stencil);

		GLfloat r = (GLfloat)color_clear_values[0].color.float32[0];
		GLfloat g = (GLfloat)color_clear_values[0].color.float32[1];
		GLfloat b = (GLfloat)color_clear_values[0].color.float32[2];
		GLfloat a = (GLfloat)color_clear_values[0].color.float32[3];
		glClearColor(r, g, b, a);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);// TODO: only if there are stencil bits

		return;
	}

	GLenum clear_bits = 0;
	size_t color_attachment_count = pass->color_attachments.size();
	std::vector<GLenum> draw_buffers(color_attachment_count);

	// TODO: only clear color buffers with VK_ATTACHMENT_LOAD_OP_CLEAR in this pass
	for (size_t i = 0; i < color_attachment_count; i++)
		draw_buffers[i] = GL_COLOR_ATTACHMENT0 + i;

	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer->gl.handle);
	glDrawBuffers(draw_buffers.size(), draw_buffers.data());

	if (!color_clear_values.empty())
	{
		for (size_t i = 0; i < color_clear_values.size(); i++)
			glClearBufferfv(GL_COLOR, i, (const GLfloat*)color_clear_values[i].color.float32);
	}

	if (depth_stencil_clear_value.has_value())
	{
		VkClearValue clear_value = depth_stencil_clear_value.value();
		glClearDepthf(clear_value.depthStencil.depth);
		clear_bits |= GL_DEPTH_BUFFER_BIT;
		clear_bits |= GL_STENCIL_BUFFER_BIT; // TODO: only if there are stencil bits
	}

	glClear(clear_bits);
}

static void gl_cmd_execute_end_pass(VIDevice device, GLCommand* glcmd)
{
	VI_ASSERT(glcmd->type == GL_COMMAND_TYPE_END_PASS);

	device->gl.active_framebuffer = nullptr;
}

static void gl_cmd_execute_copy_buffer(VIDevice device, GLCommand* glcmd)
{
	VI_ASSERT(glcmd->type == GL_COMMAND_TYPE_COPY_BUFFER);

	VIBuffer src = glcmd->copy_buffer.src;
	VIBuffer dst = glcmd->copy_buffer.dst;

	for (const VkBufferCopy& region : glcmd->copy_buffer.regions)
		gl_copy_buffer(src, dst, region.srcOffset, region.dstOffset, region.size);
}

static void gl_cmd_execute_copy_buffer_to_image(VIDevice device, GLCommand* glcmd)
{
	VI_ASSERT(glcmd->type == GL_COMMAND_TYPE_COPY_BUFFER_TO_IMAGE);

	VIBuffer buffer = glcmd->copy_buffer_to_image.buffer;
	VIImage image = glcmd->copy_buffer_to_image.image;

	for (const VkBufferImageCopy& region : glcmd->copy_buffer_to_image.regions)
	{
		gl_copy_buffer_to_image(buffer, image, region.bufferOffset, region.imageOffset, region.imageExtent, region.imageSubresource);
	}
}

static void gl_cmd_execute_copy_image(VIDevice device, GLCommand* glcmd)
{
	VI_ASSERT(glcmd->type == GL_COMMAND_TYPE_COPY_IMAGE);

	VIImage src = glcmd->copy_image.src;
	VIImage dst = glcmd->copy_image.dst;

	for (const VkImageCopy& region : glcmd->copy_image.regions)
	{
		gl_copy_image(src, dst, region.srcOffset, region.dstOffset, region.extent, region.srcSubresource, region.dstSubresource);
	}
}

static void gl_cmd_execute_copy_image_to_buffer(VIDevice device, GLCommand* glcmd)
{
	VI_ASSERT(glcmd->type == GL_COMMAND_TYPE_COPY_IMAGE_TO_BUFFER);

	VIBuffer buffer = glcmd->copy_image_to_buffer.buffer;
	VIImage image = glcmd->copy_image_to_buffer.image;

	for (const VkBufferImageCopy& region : glcmd->copy_image_to_buffer.regions)
	{
		gl_copy_image_to_buffer(image, buffer, region.bufferOffset, region.imageOffset, region.imageExtent, region.imageSubresource);
	}
}

static void gl_cmd_execute_dispatch(VIDevice device, GLCommand* glcmd)
{
	VI_ASSERT(glcmd->type == GL_COMMAND_TYPE_DISPATCH);

	glDispatchCompute(glcmd->dispatch.group_count_x, glcmd->dispatch.group_count_y, glcmd->dispatch.group_count_z);

	glMemoryBarrier(GL_ALL_BARRIER_BITS);
}

static void compile_vk(VICompileResult& result, EShLanguage stage, const char* vise_glsl)
{
	if (!has_glslang_initialized)
	{
		glslang::InitializeProcess();
		has_glslang_initialized = true;
	}

	result = VICompileResult{};

	glslang::TShader shader_tmp(stage);
	shader_tmp.setStrings(&vise_glsl, 1);

	EShMessages messages = EShMsgDefault;
	glslang::EshTargetClientVersion client_version = VI_VK_GLSLANG_VERSION;
	glslang::EShTargetLanguageVersion lang_version = glslang::EShTargetSpv_1_0;
	const TBuiltInResource* resources = ::GetDefaultResources();

	shader_tmp.setEnvInput(glslang::EShSourceGlsl, stage, glslang::EShClientVulkan, VI_SHADER_GLSL_VERSION);
	shader_tmp.setEnvClient(glslang::EShClientVulkan, client_version);
	shader_tmp.setEnvTarget(glslang::EShTargetSpv, lang_version);
	shader_tmp.setEntryPoint(VI_SHADER_ENTRY_POINT);
	shader_tmp.setSourceEntryPoint(VI_SHADER_ENTRY_POINT);

	std::string preprocessed_glsl;
	glslang::TShader::ForbidIncluder includer;

	// TODO: Doing just preprocessing to obtain a correct preprocessed shader string
	// is not an officially supported or fully working path.
	if (!shader_tmp.preprocess(resources, VI_SHADER_GLSL_VERSION, ENoProfile, false, false, messages, &preprocessed_glsl, includer))
	{
		std::cout << "Preprocessing failed for shader: " << std::endl;
		std::cout << vise_glsl << std::endl;
		std::cout << shader_tmp.getInfoLog() << std::endl;
		std::cout << shader_tmp.getInfoDebugLog() << std::endl;
		VI_ASSERT(0 && "preprocessing failed");
		return;
	}

	glslang::TShader shader(stage);
	const char* preprocessed_glsl_cstr = preprocessed_glsl.c_str();
	shader.setStrings(&preprocessed_glsl_cstr, 1);
	shader.setEnvInput(glslang::EShSourceGlsl, stage, glslang::EShClientVulkan, VI_SHADER_GLSL_VERSION);
	shader.setEnvClient(glslang::EShClientVulkan, client_version);
	shader.setEnvTarget(glslang::EShTargetSpv, lang_version);
	shader.setEntryPoint(VI_SHADER_ENTRY_POINT);
	shader.setSourceEntryPoint(VI_SHADER_ENTRY_POINT);

	if (!shader.parse(resources, VI_SHADER_GLSL_VERSION, false, messages, includer))
	{
		std::cout << "Parsing failed for shader: " << std::endl;
		std::cout << shader.getInfoLog() << std::endl;
		std::cout << shader.getInfoDebugLog() << std::endl;
		VI_ASSERT(0 && "parsing failed");
		return;
	}

	glslang::TProgram program;
	program.addShader(&shader);
	if (!program.link(messages))
	{
		std::cout << "Linking failed for shader " << std::endl;
		std::cout << program.getInfoLog() << std::endl;
		std::cout << program.getInfoDebugLog() << std::endl;
		VI_ASSERT(0 && "link failed");
		return;
	}

	std::vector<uint32_t> spirv_data;
	spv::SpvBuildLogger spv_logger;
	glslang::SpvOptions options{};
	shader.setDebugInfo(true);
	options.generateDebugInfo = true;
	options.disableOptimizer = true;
	options.optimizeSize = false;
	options.stripDebugInfo = false;
	glslang::GlslangToSpv(*program.getIntermediate(stage), result.vk_spirv, &spv_logger, &options);

	result.success = true;
}

static void compile_gl(VICompileResult& result, EShLanguage stage, const char* vise_glsl, uint32_t remap_count, const GLRemap* remaps)
{
	result = VICompileResult{};

	VICompileResult reflect_result;
	compile_vk(reflect_result, stage, vise_glsl);
	VI_ASSERT(reflect_result.success && "compile_gl failed: unable to compile spirv");

	try
	{
		spirv_cross::CompilerGLSL compiler(reflect_result.vk_spirv);
		//debug_print_compilation(compiler, stage);

		const spirv_cross::ShaderResources& resources = compiler.get_shader_resources();

		auto perform_remap = [](spirv_cross::ID id, spirv_cross::CompilerGLSL& compiler, uint32_t remap_count, const GLRemap* remaps) -> bool {
			bool success = false;
			uint32_t set_idx = compiler.get_decoration(id, spv::DecorationDescriptorSet);
			uint32_t binding_idx = compiler.get_decoration(id, spv::DecorationBinding);
			uint32_t vk_set_binding = set_idx * 100 + binding_idx;

			for (uint32_t j = 0; j < remap_count; j++)
			{
				const GLRemap* remap = remaps + j;

				if (vk_set_binding == remap->vk_set_binding)
				{
					success = true;
					compiler.unset_decoration(id, spv::DecorationDescriptorSet);
					compiler.set_decoration(id, spv::DecorationBinding, remap->gl_binding);
					break;
				}
			}

			return success;
		};

		// build push constant lookup table for OpenGL, lookup table is stored in VIModule.
		if (!resources.push_constant_buffers.empty())
		{
			spirv_cross::ID id = resources.push_constant_buffers[0].id;
			spirv_cross::TypeID base_type_id = resources.push_constant_buffers[0].base_type_id;
			spirv_cross::SPIRType block_type = compiler.get_type(base_type_id);
			const std::string& instance_name = resources.push_constant_buffers[0].name;

			// push_constant block name reflection not supported: https://github.com/KhronosGroup/SPIRV-Cross/issues/518
			VI_ASSERT(!instance_name.empty() && "push_constant block must define an instance name");

			// each member in push_constant block is an OpenGL uniform
			size_t push_constant_count = block_type.member_types.size();

			result.gl_push_constants.resize(push_constant_count);

			for (size_t i = 0; i < push_constant_count; i++)
			{
				const spirv_cross::SPIRType& member_type = compiler.get_type(block_type.member_types[i]);
				const std::string& member_name = compiler.get_member_name(base_type_id, i);
				uint32_t member_offset = (uint32_t)compiler.type_struct_member_offset(block_type, i);
				uint32_t member_size = (uint32_t)compiler.get_declared_struct_member_size(block_type, i);

				VIGLSLType vi_glsl_type;
				cast_glsl_type_spirv(member_type, &vi_glsl_type);

				result.gl_push_constants[i].offset = member_offset;
				result.gl_push_constants[i].size = member_size;
				result.gl_push_constants[i].uniform_glsl_type = vi_glsl_type;
				result.gl_push_constants[i].uniform_name = instance_name;
				result.gl_push_constants[i].uniform_name.push_back('.');
				result.gl_push_constants[i].uniform_name += member_name;
				result.gl_push_constants[i].uniform_arr_size = 1;
				
				if (!member_type.array.empty())
				{
					VI_ASSERT(member_type.array.size() == 1 && "does not support array of arrays");
					result.gl_push_constants[i].uniform_arr_size = member_type.array[0];
				}
			}
		}

		for (size_t i = 0; i < resources.uniform_buffers.size(); i++)
		{
			spirv_cross::ID id = resources.uniform_buffers[i].id;
			bool found_remap = perform_remap(id, compiler, remap_count, remaps);
			VI_ASSERT(found_remap && "failed to remap OpenGL uniform buffer binding");
		}

		for (size_t i = 0; i < resources.storage_buffers.size(); i++)
		{
			spirv_cross::ID id = resources.storage_buffers[i].id;
			bool found_remap = perform_remap(id, compiler, remap_count, remaps);
			VI_ASSERT(found_remap && "failed to remap OpenGL shader storage buffer binding");
		}

		for (size_t i = 0; i < resources.sampled_images.size(); i++)
		{
			spirv_cross::ID id = resources.sampled_images[i].id;
			bool found_remap = perform_remap(id, compiler, remap_count, remaps);
			VI_ASSERT(found_remap && "failed to remap OpenGL sampler binding");
		}

		for (size_t i = 0; i < resources.storage_images.size(); i++)
		{
			spirv_cross::ID id = resources.storage_images[i].id;
			bool found_remap = perform_remap(id, compiler, remap_count, remaps);
			VI_ASSERT(found_remap && "failed to remap OpenGL storage image binding");
		}
		
		// TODO: confirm that with subpassLoad and subpassInput, this is still the desired behaviour
		if (stage == EShLangFragment)
		{
			//compiler.add_header_line("layout(origin_upper_left) in vec4 gl_FragCoord;");
		}

		result.gl_patched = compiler.compile();
	}
	catch (spirv_cross::CompilerError error)
	{
		std::cout << "spirv_cross::CompilerError " << error.what() << std::endl;
		return;
	};

	//std::cout << "= BEGIN PATCHED GLSL" << std::endl << patched << "= END PATCHED GLSL" << std::endl;
	
	result.success = true;
}

static void flip_image_data(uint8_t* data, uint32_t image_width, uint32_t image_height, uint32_t texel_size)
{
	uint32_t bytes_per_row = texel_size * image_width;
	uint8_t temp[2048];

	for (uint32_t row = 0; row < image_height / 2; row++)
	{
		uint8_t* row0 = data + row * bytes_per_row;
		uint8_t* row1 = data + (image_height - row - 1) * bytes_per_row;
		uint32_t bytes_left = bytes_per_row;

		while (bytes_left)
		{
			size_t bytes_copy = (bytes_left < sizeof(temp)) ? bytes_left : sizeof(temp);
			memcpy(temp, row0, bytes_copy);
			memcpy(row0, row1, bytes_copy);
			memcpy(row1, temp, bytes_copy);
			row0 += bytes_copy;
			row1 += bytes_copy;
			bytes_left -= bytes_copy;
		}
	}
}

static void debug_print_compilation(const spirv_cross::CompilerGLSL& compiler, EShLanguage stage)
{
	if (stage == EShLangCompute)
	{
		uint32_t local_size_x = compiler.get_execution_mode_argument(spv::ExecutionModeLocalSize, 0);
		uint32_t local_size_y = compiler.get_execution_mode_argument(spv::ExecutionModeLocalSize, 1);
		uint32_t local_size_z = compiler.get_execution_mode_argument(spv::ExecutionModeLocalSize, 2);
		printf("LocalSize vec3(%d, %d, %d)\n", local_size_x, local_size_y, local_size_z);
	}

	const spirv_cross::ShaderResources& resources = compiler.get_shader_resources();
	std::cout << "Stage Inputs: " << resources.stage_inputs.size() << std::endl;
	std::cout << "Stage Output: " << resources.stage_outputs.size() << std::endl;
	std::cout << "UBO  count: " << resources.uniform_buffers.size() << std::endl;
	std::cout << "SSBO count: " << resources.storage_buffers.size() << std::endl;
	std::cout << "Sampled Image count: " << resources.sampled_images.size() << std::endl;
	std::cout << "Storage Image count: " << resources.storage_images.size() << std::endl;
	std::cout << "Builtin Inputs count: " << resources.builtin_inputs.size() << std::endl;
	std::cout << "Builtin Output count: " << resources.builtin_outputs.size() << std::endl;
}

static void cast_compare_op_vk(VICompareOp in_op, VkCompareOp* out_op)
{
	*out_op = vi_compare_op_table[(int)in_op].vk_compare_op;
}

static void cast_compare_op_gl(VICompareOp in_op, GLenum* out_op)
{
	*out_op = vi_compare_op_table[(int)in_op].gl_compare_op;
}

static void cast_module_type_vk(VIModuleType in_type, VkShaderStageFlagBits* out_bit)
{
	*out_bit = vi_module_type_table[(int)in_type].vk_type;
}

static void cast_module_type_glslang(VIModuleType in_type, EShLanguage* out_type)
{
	*out_type = vi_module_type_table[(int)in_type].glslang_type;
}

static void cast_module_type_gl(VIModuleType in_type, GLenum* out_type)
{
	*out_type = vi_module_type_table[(int)in_type].gl_type;
}

static void cast_index_type(VkIndexType in_type, GLenum* out_type, size_t* out_size)
{
	switch (in_type)
	{
	case VK_INDEX_TYPE_UINT16:
		*out_type = GL_UNSIGNED_SHORT;
		*out_size = 2;
		break;
	case VK_INDEX_TYPE_UINT32:
		*out_type = GL_UNSIGNED_INT;
		*out_size = 4;
		break;
	default:
		VI_UNREACHABLE;
	}
}

static void cast_buffer_usages(VIBufferType in_type, VIBufferUsageFlags in_usages, VkBufferUsageFlags* out_usages)
{
	VkBufferUsageFlags usages = 0;

	switch (in_type)
	{
	case VI_BUFFER_TYPE_TRANSFER:
		break;
	case VI_BUFFER_TYPE_VERTEX:
		usages |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
		break;
	case VI_BUFFER_TYPE_INDEX:
		usages |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
		break;
	case VI_BUFFER_TYPE_UNIFORM:
		usages |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
		break;
	case VI_BUFFER_TYPE_STORAGE:
		usages |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
		break;
	default:
		VI_UNREACHABLE;
	}

	if (in_usages & VI_BUFFER_USAGE_TRANSFER_SRC_BIT)
		usages |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

	if (in_usages & VI_BUFFER_USAGE_TRANSFER_DST_BIT)
		usages |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;

	*out_usages = usages;
}

static void cast_buffer_type(VIBufferType in_type, GLenum* out_type)
{
	switch (in_type)
	{
	case VI_BUFFER_TYPE_VERTEX:
		*out_type = GL_ARRAY_BUFFER;
		break;
	case VI_BUFFER_TYPE_INDEX:
		*out_type = GL_ELEMENT_ARRAY_BUFFER;
		break;
	case VI_BUFFER_TYPE_UNIFORM:
		*out_type = GL_UNIFORM_BUFFER;
		break;
	case VI_BUFFER_TYPE_STORAGE:
		*out_type = GL_SHADER_STORAGE_BUFFER;
		break;
	default:
		VI_UNREACHABLE;
	}
}

static void cast_image_usages(VIImageUsageFlags in_usages, VkImageUsageFlags* out_usages)
{
	VkImageUsageFlags usages = 0;

	if (in_usages & VI_IMAGE_USAGE_SAMPLED_BIT)
		usages |= VK_IMAGE_USAGE_SAMPLED_BIT;

	if (in_usages & VI_IMAGE_USAGE_TRANSFER_SRC_BIT)
		usages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

	if (in_usages & VI_IMAGE_USAGE_TRANSFER_DST_BIT)
		usages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

	if (in_usages & VI_IMAGE_USAGE_STORAGE_BIT)
		usages |= VK_IMAGE_USAGE_STORAGE_BIT;

	if (in_usages & VI_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
		usages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	if (in_usages & VI_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
		usages |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

	*out_usages = usages;
}

static void cast_image_type(VIImageType in_type, VkImageType* out_type, VkImageViewType* out_view_type)
{
	const VIImageTypeEntry* entry = vi_image_type_table + (int)in_type;
	*out_type = entry->vk_type;
	*out_view_type = entry->vk_view_type;
}

static void cast_image_type(VIImageType in_type, GLenum* out_type)
{
	*out_type = vi_image_type_table[(int)in_type].gl_type;
}

static void cast_filter_vk(const VISamplerInfo& in_sampler, VkFilter* out_filter, VkSamplerMipmapMode* out_mipmap_mode)
{
	*out_filter = vi_filter_table[(int)in_sampler.filter].vk_filter;
	*out_mipmap_mode = vi_filter_table[(int)in_sampler.mipmap_filter].vk_mipmap_mode;
}

static void cast_filter_gl(const VISamplerInfo& in_sampler, GLenum* out_min_filter, GLenum* out_mag_filter)
{
	*out_mag_filter = vi_filter_table[(int)in_sampler.filter].gl_filter;

	if (in_sampler.filter == VI_FILTER_LINEAR && in_sampler.mipmap_filter == VI_FILTER_LINEAR)
		*out_min_filter = GL_LINEAR_MIPMAP_LINEAR;
	else if (in_sampler.filter == VI_FILTER_LINEAR && in_sampler.mipmap_filter == VI_FILTER_NEAREST)
		*out_min_filter = GL_LINEAR_MIPMAP_NEAREST;
	else if (in_sampler.filter == VI_FILTER_NEAREST && in_sampler.mipmap_filter == VI_FILTER_LINEAR)
		*out_min_filter = GL_NEAREST_MIPMAP_LINEAR;
	else if (in_sampler.filter == VI_FILTER_NEAREST && in_sampler.mipmap_filter == VI_FILTER_NEAREST)
		*out_min_filter = GL_NEAREST_MIPMAP_NEAREST;
	else
		VI_UNREACHABLE;
}

static void cast_sampler_address_mode_vk(VISamplerAddressMode in_address_mode, VkSamplerAddressMode* out_address_mode)
{
	*out_address_mode = vi_sampler_address_mode_table[(int)in_address_mode].vk_address_mode;
}

static void cast_sampler_address_mode_gl(VISamplerAddressMode in_address_mode, GLenum* out_address_mode)
{
	*out_address_mode = vi_sampler_address_mode_table[(int)in_address_mode].gl_address_mode;
}

static void cast_blend_factor_vk(VIBlendFactor in_factor, VkBlendFactor* out_factor)
{
	*out_factor = vi_blend_factor_table[(int)in_factor].vk_blend_factor;
}

static void cast_blend_factor_gl(VIBlendFactor in_factor, GLenum* out_factor)
{
	*out_factor = vi_blend_factor_table[(int)in_factor].gl_blend_factor;
}

static void cast_blend_op_vk(VIBlendOp in_op, VkBlendOp* out_op)
{
	*out_op = vi_blend_op_table[(int)in_op].vk_blend_op;
}

static void cast_blend_op_gl(VIBlendOp in_op, GLenum* out_op)
{
	*out_op = vi_blend_op_table[(int)in_op].gl_blend_op;
}

static void cast_stencil_op_vk(VIStencilOp in_op, VkStencilOp* out_op)
{
	*out_op = vi_stencil_op_table[(int)in_op].vk_stencil_op;
}

static void cast_stencil_op_gl(VIStencilOp in_op, GLenum* out_op)
{
	*out_op = vi_stencil_op_table[(int)in_op].gl_stencil_op;
}

static void cast_polygon_mode_vk(VIPolygonMode in_mode, VkPolygonMode* out_mode)
{
	*out_mode = vi_polygon_mode_table[(int)in_mode].vk_polygon_mode;
}

static void cast_polygon_mode_gl(VIPolygonMode in_mode, GLenum* out_mode)
{
	*out_mode = vi_polygon_mode_table[(int)in_mode].gl_polygon_mode;
}

static void cast_cull_mode_vk(VICullMode in_mode, VkCullModeFlags* out_mode)
{
	*out_mode = vi_cull_mode_table[(int)in_mode].vk_cull_mode;
}

static void cast_cull_mode_gl(VICullMode in_mode, GLenum* out_mode)
{
	*out_mode = vi_cull_mode_table[(int)in_mode].gl_cull_mode;
}

static void cast_stencil_op_state_vk(const VIStencilOpStateInfo& in_state, VkStencilOpState* out_state)
{
	cast_stencil_op_vk(in_state.pass_op, &out_state->passOp);
	cast_stencil_op_vk(in_state.fail_op, &out_state->failOp);
	cast_stencil_op_vk(in_state.depth_fail_op, &out_state->depthFailOp);
	cast_compare_op_vk(in_state.compare_op, &out_state->compareOp);
	out_state->compareMask = in_state.compare_mask;
	out_state->writeMask = in_state.write_mask;
	out_state->reference = in_state.reference;
}

static void cast_format_vk(VIFormat in_format, VkFormat* out_format, VkImageAspectFlags* out_aspects)
{
	const VIFormatEntry* entry = vi_format_table + (int)in_format;

	*out_format = entry->vk_format;
	*out_aspects = entry->vk_aspect;
}

static void cast_format_vk(VkFormat in_format, VIFormat* out_format)
{
	for (uint32_t i = 0; i < VI_ARR_SIZE(vi_format_table); i++)
	{
		if (vi_format_table[i].vk_format == in_format)
		{
			*out_format = vi_format_table[i].vi_format;
			return;
		}
	}

	VI_UNREACHABLE;
}

static void cast_format_gl(VIFormat in_format, GLenum* out_internal_format, GLenum* out_data_format, GLenum* out_data_type, uint32_t* out_texel_size)
{
	const VIFormatEntry* entry = vi_format_table + (int)in_format;
	*out_internal_format = entry->gl_internal_format;
	*out_data_format = entry->gl_data_format;
	*out_data_type = entry->gl_data_type;
	*out_texel_size = entry->texel_block_size;
}

static void cast_format_attachment_gl(VIFormat in_format, GLenum* out_attachment)
{
	VIImageAspectFlags aspect = (VIImageAspectFlags)vi_format_table[(int)in_format].vk_aspect;

	if (aspect == VI_IMAGE_ASPECT_DEPTH_STENCIL)
		*out_attachment = GL_DEPTH_STENCIL_ATTACHMENT;
	else if (aspect == VI_IMAGE_ASPECT_DEPTH)
		*out_attachment = GL_DEPTH_ATTACHMENT;
	else if (aspect == VI_IMAGE_ASPECT_STENCIL)
		*out_attachment = GL_STENCIL_ATTACHMENT;
	else
		VI_UNREACHABLE; // color attachment
}

static void cast_set_pool_resources(uint32_t in_res_count, const VISetPoolResource* in_res, std::vector<VkDescriptorPoolSize>& out_sizes)
{
	out_sizes.resize(in_res_count);

	for (uint32_t i = 0; i < in_res_count; i++)
	{
		VkDescriptorType vktype;
		cast_binding_type(in_res[i].type, &vktype);

		out_sizes[i].descriptorCount = in_res[i].count;
		out_sizes[i].type = vktype;
	}
}

static void cast_binding(const VIBinding* in_binding, VkDescriptorSetLayoutBinding* out_binding)
{
	VkDescriptorType descriptor_type;
	cast_binding_type(in_binding->type, &descriptor_type);

	out_binding->binding = in_binding->binding_index;
	out_binding->descriptorCount = in_binding->array_count;
	out_binding->descriptorType = descriptor_type;
	out_binding->stageFlags = VK_SHADER_STAGE_ALL; // TODO:
	out_binding->pImmutableSamplers = nullptr;
}

static void cast_binding_type(VIBindingType in_type, VkDescriptorType* out_type)
{
	switch (in_type)
	{
	case VI_BINDING_TYPE_UNIFORM_BUFFER:
		*out_type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		break;
	case VI_BINDING_TYPE_COMBINED_IMAGE_SAMPLER:
		*out_type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		break;
	case VI_BINDING_TYPE_STORAGE_BUFFER:
		*out_type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		break;
	case VI_BINDING_TYPE_STORAGE_IMAGE:
		*out_type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		break;
	default:
		VI_UNREACHABLE;
	}
}

static void cast_glsl_type_vk(VIGLSLType in_type, VkFormat* out_format)
{
	*out_format = vi_glsl_type_table[(int)in_type].vk_vertex_format;
}

static void cast_glsl_type_gl(VIGLSLType in_type, GLint* out_component_count, GLenum* out_component_type)
{
	const VIGLSLTypeEntry* entry = vi_glsl_type_table + (int)in_type;
	*out_component_count = entry->gl_component_count;
	*out_component_type = entry->gl_component_type;
}

static void cast_glsl_type_spirv(const spirv_cross::SPIRType& in_type, VIGLSLType* out_type)
{
	if (in_type.basetype == spirv_cross::SPIRType::Float)
	{
		switch (in_type.vecsize)
		{
		case 1:
			*out_type = VI_GLSL_TYPE_FLOAT;
			return;
		case 2:
			*out_type = VI_GLSL_TYPE_VEC2;
			return;
		case 3:
			*out_type = VI_GLSL_TYPE_VEC3;
			return;
		case 4:
			if (in_type.columns == 4)
			{
				*out_type = VI_GLSL_TYPE_MAT4;
				return;
			}
			*out_type = VI_GLSL_TYPE_VEC4;
			return;
		}
	}
	else if (in_type.basetype == spirv_cross::SPIRType::Double)
	{
		switch (in_type.vecsize)
		{
		case 1:
			*out_type = VI_GLSL_TYPE_DOUBLE;
			return;
		case 2:
			*out_type = VI_GLSL_TYPE_DVEC2;
			return;
		case 3:
			*out_type = VI_GLSL_TYPE_DVEC3;
			return;
		case 4:
			*out_type = VI_GLSL_TYPE_DVEC4;
			return;
		}
	}
	else if (in_type.basetype == spirv_cross::SPIRType::UInt)
	{
		switch (in_type.vecsize)
		{
		case 1:
			*out_type = VI_GLSL_TYPE_UINT;
			return;
		case 2:
			*out_type = VI_GLSL_TYPE_UVEC2;
			return;
		case 3:
			*out_type = VI_GLSL_TYPE_UVEC3;
			return;
		case 4:
			*out_type = VI_GLSL_TYPE_UVEC4;
			return;
		}
	}

	VI_UNREACHABLE;
}

static void cast_pipeline_vertex_input(uint32_t attr_count, VIVertexAttribute* attrs,
	uint32_t binding_count, VIVertexBinding* bindings,
	std::vector<VkVertexInputAttributeDescription>& out_attrs,
	std::vector<VkVertexInputBindingDescription>& out_bindings)
{
	out_attrs.resize(attr_count);
	out_bindings.resize(binding_count);

	for (uint32_t i = 0; i < attr_count; i++)
	{
		// TODO: check if the vertex_format has feature VK_FORMAT_FEATURE_VERTEX_BUFFER_BIT,
		//       do the query here, or maybe during vulkan device creation?

		VkFormat vertex_format;
		cast_glsl_type_vk(attrs[i].type, &vertex_format);
		VI_ASSERT(vertex_format != VK_FORMAT_UNDEFINED && "not supported");

		out_attrs[i].binding = attrs[i].binding;
		out_attrs[i].location = i;
		out_attrs[i].format = vertex_format;
		out_attrs[i].offset = attrs[i].offset;
	}

	for (uint32_t i = 0; i < binding_count; i++)
	{
		out_bindings[i].inputRate = bindings[i].rate;
		out_bindings[i].binding = i;
		out_bindings[i].stride = bindings[i].stride;
	}
}

static void cast_memory_barrier(const VIMemoryBarrier& in_barrier, VkMemoryBarrier* out_barrier)
{
	out_barrier->sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
	out_barrier->pNext = nullptr;
	out_barrier->srcAccessMask = in_barrier.src_access;
	out_barrier->dstAccessMask = in_barrier.dst_access;
}

static void cast_image_memory_barrier(const VIImageMemoryBarrier& in_barrier, VkImageMemoryBarrier* out_barrier)
{
	out_barrier->sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	out_barrier->pNext = nullptr;
	out_barrier->image = in_barrier.image->vk.handle;
	out_barrier->newLayout = in_barrier.new_layout;
	out_barrier->oldLayout = in_barrier.old_layout;
	out_barrier->srcAccessMask = in_barrier.src_access;
	out_barrier->dstAccessMask = in_barrier.dst_access;
	out_barrier->srcQueueFamilyIndex = in_barrier.src_family_index;
	out_barrier->dstQueueFamilyIndex = in_barrier.dst_family_index;
	out_barrier->subresourceRange = in_barrier.subresource_range;
}

static void cast_buffer_memory_barrier(const VIBufferMemoryBarrier& in_barrier, VkBufferMemoryBarrier* out_barrier)
{
	out_barrier->sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	out_barrier->pNext = nullptr;
	out_barrier->buffer = in_barrier.buffer->vk.handle;
	out_barrier->srcAccessMask = in_barrier.src_access;
	out_barrier->dstAccessMask = in_barrier.dst_access;
	out_barrier->srcQueueFamilyIndex = in_barrier.src_family_index;
	out_barrier->dstQueueFamilyIndex = in_barrier.dst_family_index;
	out_barrier->offset = (VkDeviceSize)in_barrier.offset;
	out_barrier->size = (VkDeviceSize)in_barrier.size;
}

static void cast_subpass_info(const VIPassInfo& in_pass_info, const VISubpassInfo& in_subpass_info,
	std::vector<VkAttachmentReference>* out_color_refs,
	std::optional<VkAttachmentReference>* out_depth_stencil_ref)
{
	out_depth_stencil_ref->reset();
	if (in_subpass_info.depth_stencil_attachment_ref)
	{
		VkAttachmentReference vk_depth_stencil_ref;
		vk_depth_stencil_ref.attachment = in_pass_info.color_attachment_count;
		vk_depth_stencil_ref.layout = in_subpass_info.depth_stencil_attachment_ref->layout;
		*out_depth_stencil_ref = vk_depth_stencil_ref;
	}

	out_color_refs->resize(in_subpass_info.color_attachment_ref_count);
	for (uint32_t i = 0; i < in_subpass_info.color_attachment_ref_count; i++)
	{
		(*out_color_refs)[i].attachment = in_subpass_info.color_attachment_refs[i].index;
		(*out_color_refs)[i].layout = in_subpass_info.color_attachment_refs[i].layout;
	}
}

static void cast_pass_color_attachment(const VIPassColorAttachment& in_atch, VkAttachmentDescription* out_atch)
{
	VkFormat vk_color_format;
	VkImageAspectFlags vk_aspect;
	cast_format_vk(in_atch.color_format, &vk_color_format, &vk_aspect);

	out_atch->flags = 0;
	out_atch->format = vk_color_format;
	out_atch->samples = VK_SAMPLE_COUNT_1_BIT;
	out_atch->initialLayout = in_atch.initial_layout;
	out_atch->finalLayout = in_atch.final_layout;
	out_atch->loadOp = in_atch.color_load_op;
	out_atch->storeOp = in_atch.color_store_op;
	out_atch->stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	out_atch->stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
}

static void cast_pass_depth_stencil_attachment(const VIPassDepthStencilAttachment& in_atch, VkAttachmentDescription* out_atch)
{
	VkFormat vk_depth_stencil_format;
	VkImageAspectFlags vk_aspect;
	cast_format_vk(in_atch.depth_stencil_format, &vk_depth_stencil_format, &vk_aspect);

	out_atch->flags = 0;
	out_atch->format = vk_depth_stencil_format;
	out_atch->samples = VK_SAMPLE_COUNT_1_BIT;
	out_atch->initialLayout = in_atch.initial_layout;
	out_atch->finalLayout = in_atch.final_layout;
	out_atch->loadOp = in_atch.depth_load_op;
	out_atch->storeOp = in_atch.depth_store_op;
	out_atch->stencilLoadOp = in_atch.stencil_load_op;
	out_atch->stencilStoreOp = in_atch.stencil_store_op;
}

VIDevice vi_create_device_vk(const VIDeviceInfo* info, VIDeviceLimits* limits)
{
	VI_ASSERT(info->desired_swapchain_framebuffer_count > 0);

	VK_CHECK(volkInitialize());

	uint32_t loader_version = volkGetInstanceVersion();
	int major = VK_VERSION_MAJOR(loader_version);
	int minor = VK_VERSION_MINOR(loader_version);
	int patch = VK_VERSION_PATCH(loader_version);
	int required_major = VK_VERSION_MAJOR(VI_VK_API_VERSION);
	int required_minor = VK_VERSION_MINOR(VI_VK_API_VERSION);

	if ((major < required_major) || (major == required_major && minor < required_minor))
	{
		printf("VISE: vulkan loader version unsupported: %d.%d.%d\n", major, minor, patch);
		return VI_NULL;
	}

	VIDevice device = (VIDevice)vi_malloc(sizeof(VIDeviceObj));
	new (device)VIDeviceObj();
	device->backend = VI_BACKEND_VULKAN;
	device->queue_graphics.device = device;
	device->queue_transfer.device = device;
	device->queue_present.device = device;

	VIVulkan* vk = &device->vk;
	new (vk)VIVulkan();
	vk->vi_device = device;

	// create Instance, Surface, and a Device
	{
		vk_create_instance(vk, info->vulkan.enable_validation_layers);
		volkLoadInstance(vk->instance);

		vk_create_surface(vk);
		vk_create_device(vk, device, info);
		volkLoadDevice(vk->device);
	}

	// VMA configuration
	{
		VmaVulkanFunctions vma_callbacks{};
		vma_callbacks.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
		vma_callbacks.vkGetDeviceProcAddr = vkGetDeviceProcAddr;
#if VMA_VULKAN_VERSION >= 1003000
		vma_callbacks.vkGetDeviceBufferMemoryRequirements = vkGetDeviceBufferMemoryRequirements;
		vma_callbacks.vkGetDeviceImageMemoryRequirements = vkGetDeviceImageMemoryRequirements;
#endif

		VmaAllocator allocator = nullptr;
		VmaAllocatorCreateInfo allocator_ci{};
		allocator_ci.physicalDevice = vk->pdevice_chosen->handle;
		allocator_ci.device = vk->device;
		allocator_ci.instance = vk->instance;
		allocator_ci.pVulkanFunctions = &vma_callbacks;
		allocator_ci.vulkanApiVersion = VI_VK_API_VERSION;
		VK_CHECK(vmaCreateAllocator(&allocator_ci, &vk->vma));
	}

	// create Swapchain, Swapchain-Pass and Swapchain-Framebuffer
	{
		void (*configure_swapchain)(const VIPhysicalDevice* pdevice, void* window, VISwapchainInfo* out_info) = info->vulkan.configure_swapchain;
		if (configure_swapchain == nullptr)
			configure_swapchain = &vk_default_configure_swapchain;

		VISwapchainInfo swapchainI;
		configure_swapchain(vk->pdevice_chosen, info->window, &swapchainI);

		uint32_t surface_min_image_count = vk->pdevice_chosen->surface_caps.minImageCount;
		uint32_t surface_max_image_count = vk->pdevice_chosen->surface_caps.maxImageCount; // zero if there is no actual limit
		uint32_t min_image_count = std::max<uint32_t>(info->desired_swapchain_framebuffer_count, surface_min_image_count);
		if (min_image_count > surface_max_image_count)
			min_image_count = surface_max_image_count;

		vk_create_swapchain(vk, &swapchainI, min_image_count);

		uint32_t swapchain_image_count = vk->swapchain.images.size();
		limits->swapchain_framebuffer_count = swapchain_image_count;

		vk->frames_in_flight = swapchain_image_count;
		vk->frames = (VIFrame*)vi_malloc(sizeof(VIFrame) * swapchain_image_count);
		vk->frame_idx = 0;

		VIFormat vi_color_format;
		cast_format_vk(vk->swapchain.image_format, &vi_color_format);

		VIPassColorAttachment color_atch;
		color_atch.color_format = vi_color_format;
		color_atch.color_load_op = VK_ATTACHMENT_LOAD_OP_CLEAR;
		color_atch.color_store_op = VK_ATTACHMENT_STORE_OP_STORE;
		color_atch.initial_layout = VK_IMAGE_LAYOUT_UNDEFINED;
		color_atch.final_layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		VIPassDepthStencilAttachment depth_stencil_atch;
		bool has_depth_stencil_atch = vk->swapchain.depth_stencil_format != VK_FORMAT_UNDEFINED;
		if (has_depth_stencil_atch)
		{
			VIFormat vi_depth_stencil_format;
			cast_format_vk(vk->swapchain.depth_stencil_format, &vi_depth_stencil_format);

			depth_stencil_atch.depth_stencil_format = vi_depth_stencil_format;
			depth_stencil_atch.depth_load_op = VK_ATTACHMENT_LOAD_OP_CLEAR;
			depth_stencil_atch.depth_store_op = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			depth_stencil_atch.stencil_load_op = VK_ATTACHMENT_LOAD_OP_CLEAR;
			depth_stencil_atch.stencil_store_op = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			depth_stencil_atch.initial_layout = VK_IMAGE_LAYOUT_UNDEFINED;
			depth_stencil_atch.final_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL; // TODO: DEPTH_ATTACHMENT_OPTIMAL
		}

		std::array<VkSubpassDependency, 2> dependencies;
		dependencies[0].dependencyFlags = 0;
		dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[0].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[0].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dependencies[0].dstSubpass = 0;
		dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

		dependencies[1].dependencyFlags = 0;
		dependencies[1].srcSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[1].srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		dependencies[1].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		dependencies[1].dstSubpass = 0;
		dependencies[1].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		dependencies[1].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

		VISubpassColorAttachment color_atch_ref;
		color_atch_ref.index = 0;
		color_atch_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VISubpassDepthStencilAttachment depth_stencil_atch_ref;
		depth_stencil_atch_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VISubpassInfo subpassI;
		subpassI.color_attachment_ref_count = 1;
		subpassI.color_attachment_refs = &color_atch_ref;
		subpassI.depth_stencil_attachment_ref = has_depth_stencil_atch ? &depth_stencil_atch_ref : nullptr;

		VIPassInfo passI;
		passI.color_attachment_count = 1;
		passI.color_attachments = &color_atch;
		passI.depenency_count = dependencies.size();
		passI.dependencies = dependencies.data();
		passI.depth_stencil_attachment = has_depth_stencil_atch ? &depth_stencil_atch : nullptr;
		passI.subpass_count = 1;
		passI.subpasses = &subpassI;

		device->swapchain_pass = vi_create_pass(device, &passI);
		
		uint32_t image_count = device->vk.swapchain.images.size();
		device->swapchain_framebuffers = (VIFramebuffer)vi_malloc(sizeof(VIFramebufferObj) * image_count);

		for (uint32_t i = 0; i < image_count; i++)
		{
			device->swapchain_framebuffers[i].extent = device->vk.swapchain.image_extent;

			std::vector<VIImage> atchs;

			atchs.push_back(vk->swapchain.images.data() + i);
			if (has_depth_stencil_atch)
				atchs.push_back(vk->swapchain.depth_stencils.data() + i);

			vk_create_framebuffer(vk,
				device->swapchain_framebuffers + i,
				device->swapchain_pass,
				vk->swapchain.image_extent,
				atchs.size(),
				atchs.data()
			);
		}
	}

	// per-frame resources
	{
		VkCommandPoolCreateInfo cmdPoolCI{};
		cmdPoolCI.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		cmdPoolCI.queueFamilyIndex = vk->family_idx_graphics;
		cmdPoolCI.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		VK_CHECK(vkCreateCommandPool(device->vk.device, &cmdPoolCI, nullptr, &vk->cmd_pool_graphics.vk_handle));

		for (uint32_t i = 0; i < vk->frames_in_flight; i++)
		{
			VIFrame* frame = vk->frames + i;

			VkFenceCreateInfo fenceCI;
			fenceCI.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
			fenceCI.pNext = NULL;
			fenceCI.flags = VK_FENCE_CREATE_SIGNALED_BIT;
			VK_CHECK(vkCreateFence(vk->device, &fenceCI, NULL, &frame->fence.frame_complete.vk_handle));

			VkSemaphoreCreateInfo semCI;
			semCI.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
			semCI.pNext = NULL;
			semCI.flags = 0;
			VK_CHECK(vkCreateSemaphore(vk->device, &semCI, NULL, &frame->semaphore.image_acquired.vk_handle));
			VK_CHECK(vkCreateSemaphore(vk->device, &semCI, NULL, &frame->semaphore.present_ready.vk_handle));
		}
	}

	const VkPhysicalDeviceLimits* vk_limits = &vk->pdevice_chosen->device_props.limits;

	limits->max_push_constant_size = vk_limits->maxPushConstantsSize;
	limits->max_compute_workgroup_size[0] = vk_limits->maxComputeWorkGroupSize[0];
	limits->max_compute_workgroup_size[1] = vk_limits->maxComputeWorkGroupSize[1];
	limits->max_compute_workgroup_size[2] = vk_limits->maxComputeWorkGroupSize[2];
	limits->max_compute_workgroup_count[0] = vk_limits->maxComputeWorkGroupCount[0];
	limits->max_compute_workgroup_count[1] = vk_limits->maxComputeWorkGroupCount[1];
	limits->max_compute_workgroup_count[2] = vk_limits->maxComputeWorkGroupCount[2];
	limits->max_compute_workgroup_invocations = vk_limits->maxComputeWorkGroupInvocations;

	device->limits = *limits;
	return device;
}

VIDevice vi_create_device_gl(const VIDeviceInfo* info, VIDeviceLimits* limits)
{
	VIDevice device = (VIDevice)vi_malloc(sizeof(VIDeviceObj));
	device->backend = VI_BACKEND_OPENGL;
	new (device)VIDeviceObj();
	device->queue_graphics.device = device;
	device->queue_transfer.device = device;
	device->queue_present.device = device;

	VIOpenGL* gl = &device->gl;
	new (gl)VIOpenGL();
	gl->vi_device = device;
	gl->frame.fence.frame_complete.device = device;
	gl->frame.semaphore.image_acquired.device = device;
	gl->frame.semaphore.present_ready.device = device;

	int success = gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
	VI_ASSERT(success);

	// Swapchain-Pass and Swapchain-Framebuffer
	device->swapchain_framebuffers = (VIFramebuffer)vi_malloc(sizeof(VIFramebufferObj));

	// TODO: gl_create_swapchain_pass(gl, device->swapchain_pass);
	gl_create_swapchain_framebuffer(gl, device->swapchain_framebuffers);

	glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
	glFrontFace(GL_CCW);

	GLint gl_max_compute_workgroup_invocations;
	GLint gl_max_compute_workgroup_count_x, gl_max_compute_workgroup_size_x;
	GLint gl_max_compute_workgroup_count_y, gl_max_compute_workgroup_size_y;
	GLint gl_max_compute_workgroup_count_z, gl_max_compute_workgroup_size_z;
	glGetIntegerv(GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS, &gl_max_compute_workgroup_invocations);
	glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 0, &gl_max_compute_workgroup_count_x);
	glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 1, &gl_max_compute_workgroup_count_y);
	glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 2, &gl_max_compute_workgroup_count_z);
	glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 0, &gl_max_compute_workgroup_size_x);
	glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 1, &gl_max_compute_workgroup_size_y);
	glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 2, &gl_max_compute_workgroup_size_z);

	limits->swapchain_framebuffer_count = 1;
	limits->max_push_constant_size = 128;
	limits->max_compute_workgroup_count[0] = gl_max_compute_workgroup_count_x;
	limits->max_compute_workgroup_count[1] = gl_max_compute_workgroup_count_y;
	limits->max_compute_workgroup_count[2] = gl_max_compute_workgroup_count_z;
	limits->max_compute_workgroup_size[0] = gl_max_compute_workgroup_size_x;
	limits->max_compute_workgroup_size[1] = gl_max_compute_workgroup_size_y;
	limits->max_compute_workgroup_size[2] = gl_max_compute_workgroup_size_z;
	limits->max_compute_workgroup_invocations = gl_max_compute_workgroup_invocations;
	
	device->limits = *limits;
	return device;
}


void vi_destroy_device(VIDevice device)
{
	if (device->backend == VI_BACKEND_VULKAN)
	{
		VIVulkan* vk = &device->vk;

		for (uint32_t i = 0; i < vk->frames_in_flight; i++)
		{
			VIFrame* frame = vk->frames + i;
			vkDestroySemaphore(vk->device, frame->semaphore.present_ready.vk_handle, nullptr);
			vkDestroySemaphore(vk->device, frame->semaphore.image_acquired.vk_handle, nullptr);
			vkDestroyFence(vk->device, frame->fence.frame_complete.vk_handle, nullptr);
		}
		vkDestroyCommandPool(vk->device, vk->cmd_pool_graphics.vk_handle, nullptr);

		for (uint32_t i = 0; i < vk->swapchain.images.size(); i++)
			vk_destroy_framebuffer(vk, device->swapchain_framebuffers + i);
		
		vi_destroy_pass(device, device->swapchain_pass);
		vk_destroy_swapchain(vk);

		vmaDestroyAllocator(vk->vma);

		vk_destroy_device(vk);
		vk_destroy_surface(vk);
		vk_destroy_instance(vk);

		vi_free(vk->frames);

		vk->~VIVulkan();
	}
	else
	{
		VIOpenGL* gl = &device->gl;

		gl->~VIOpenGL();
	}

	vi_free(device->swapchain_framebuffers);

	device->~VIDeviceObj();
	vi_free(device);

	// TODO: send notification via user debug callback
	VI_ASSERT(host_malloc_usage == 0);
}

VIFence vi_create_fence(VIDevice device, VkFenceCreateFlags flags)
{
	VIFence fence = (VIFence)vi_malloc(sizeof(VIFenceObj));
	fence->device = device;
	
	if (device->backend == VI_BACKEND_OPENGL)
	{
		fence->gl_signal = false;
		return fence;
	}

	VkFenceCreateInfo fenceCI;
	fenceCI.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceCI.pNext = nullptr;
	fenceCI.flags = flags;
	VK_CHECK(vkCreateFence(device->vk.device, &fenceCI, nullptr, &fence->vk_handle));

	return fence;
}

void vi_destroy_fence(VIDevice device, VIFence fence)
{
	if (device->backend == VI_BACKEND_VULKAN)
		vkDestroyFence(device->vk.device, fence->vk_handle, nullptr);

	vi_free(fence);
}

void vi_wait_for_fences(VIDevice device, uint32_t fence_count, VIFence* fences, bool wait_all, uint64_t timeout)
{
	if (device->backend == VI_BACKEND_OPENGL)
	{
		for (uint32_t i = 0; i < fence_count; i++)
			fences[i]->gl_signal = true;
		return;
	}

	std::vector<VkFence> vk_fences(fence_count);
	for (uint32_t i = 0; i < fence_count; i++)
		vk_fences[i] = fences[i]->vk_handle;

	VK_CHECK(vkWaitForFences(device->vk.device, fence_count, vk_fences.data(), wait_all, timeout));
}

void vi_queue_wait_idle(VIQueue queue)
{
	if (queue->device->backend == VI_BACKEND_OPENGL)
		return;

	VK_CHECK(vkQueueWaitIdle(queue->vk_handle));
}

void vi_queue_submit(VIQueue queue, uint32_t submit_count, VISubmitInfo* submits, VIFence fence)
{
	VIDevice device = queue->device;

	if (device->backend == VI_BACKEND_OPENGL)
	{
		for (uint32_t i = 0; i < submit_count; i++)
			gl_device_append_submission(device, submits + i);

		gl_device_flush_submission(device);
		return;
	}

	std::vector<VkSubmitInfo> infos(submit_count);
	std::vector<VkCommandBuffer> vk_cmds;
	std::vector<VkSemaphore> vk_waits;
	std::vector<VkSemaphore> vk_signals;

	size_t reserve_guess = 4;
	vk_cmds.reserve(reserve_guess);
	vk_waits.reserve(reserve_guess);
	vk_signals.reserve(reserve_guess);

	uint32_t vk_cmds_base = 0;
	uint32_t vk_waits_base = 0;
	uint32_t vk_signals_base = 0;

	for (uint32_t i = 0; i < submit_count; i++)
	{
		const VISubmitInfo& submit = submits[i];

		for (uint32_t j = 0; j < submit.cmd_count; j++)
			vk_cmds.push_back(submit.cmds[j]->vk.handle);

		for (uint32_t j = 0; j < submit.wait_count; j++)
			vk_waits.push_back(submit.waits[j]->vk_handle);

		for (uint32_t j = 0; j < submit.signal_count; j++)
			vk_signals.push_back(submit.signals[j]->vk_handle);

		infos[i].sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		infos[i].pNext = nullptr;
		infos[i].pWaitDstStageMask = submit.wait_stages;
		infos[i].commandBufferCount = submit.cmd_count;
		infos[i].pCommandBuffers = vk_cmds.data() + vk_cmds_base;
		infos[i].waitSemaphoreCount = submit.wait_count;
		infos[i].pWaitSemaphores = vk_waits.data() + vk_waits_base;
		infos[i].signalSemaphoreCount = submit.signal_count;
		infos[i].pSignalSemaphores = vk_signals.data() + vk_signals_base;

		vk_cmds_base += submit.cmd_count;
		vk_waits_base += submit.wait_count;
		vk_signals_base += submit.signal_count;
	}

	VkFence vk_fence = (fence == VI_NULL) ? VK_NULL_HANDLE : fence->vk_handle;
	VK_CHECK(vkQueueSubmit(queue->vk_handle, submit_count, infos.data(), vk_fence));
}

void vi_set_update(VISet set, uint32_t update_count, const VISetUpdateInfo* updates)
{
	if (set->device->backend == VI_BACKEND_OPENGL)
	{
		gl_set_update(set, update_count, updates);
		return;
	}

	VIVulkan* vk = &set->device->vk;

	std::vector<VkDescriptorImageInfo> write_images;
	std::vector<VkDescriptorBufferInfo> write_buffers;
	std::vector<VkWriteDescriptorSet> writes;

	for (uint32_t i = 0; i < update_count; i++)
	{
		uint32_t binding_idx = updates[i].binding_index;
		uint32_t descriptor_count = 1;
		VIBindingType binding_type = set->layout->bindings[binding_idx].type;
		VkDescriptorType descriptor_type;
		cast_binding_type(binding_type, &descriptor_type);

		VkWriteDescriptorSet write;
		write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write.pNext = nullptr;
		write.dstSet = set->vk.handle;
		write.dstBinding = binding_idx;
		write.dstArrayElement = 0;
		write.descriptorType = descriptor_type;
		write.descriptorCount = 1;
		write.pImageInfo = nullptr;
		write.pBufferInfo = nullptr;
		write.pTexelBufferView = nullptr;

		if (descriptor_type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ||
			descriptor_type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
		{
			VI_ASSERT(updates[i].buffer != VI_NULL);

			VkDescriptorBufferInfo bufferI;
			bufferI.buffer = updates[i].buffer->vk.handle;
			bufferI.range = updates[i].buffer->size;
			bufferI.offset = 0;
			write_buffers.push_back(bufferI);
		}
		else if (descriptor_type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER ||
			descriptor_type == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
		{
			VI_ASSERT(updates[i].image != VI_NULL);

			VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			if (updates[i].image->info.usage & VI_IMAGE_USAGE_STORAGE_BIT)
				layout = VK_IMAGE_LAYOUT_GENERAL;

			VkDescriptorImageInfo imageI;
			imageI.imageLayout = layout; // TODO: deprecate updates[i].image->vk.image_layout;
			imageI.imageView = updates[i].image->vk.view_handle;
			imageI.sampler = updates[i].image->vk.sampler_handle;
			write_images.push_back(imageI);
		}

		writes.push_back(write);
	}

	uint32_t write_buffer_idx = 0;
	uint32_t write_image_idx = 0;

	// only store pointers after vector sizes are determined
	for (uint32_t i = 0; i < update_count; i++)
	{
		uint32_t binding_idx = updates[i].binding_index;
		VIBindingType binding_type = set->layout->bindings[binding_idx].type;

		if (binding_type == VI_BINDING_TYPE_UNIFORM_BUFFER ||
			binding_type == VI_BINDING_TYPE_STORAGE_BUFFER)
		{
			writes[i].pBufferInfo = write_buffers.data() + write_buffer_idx++;
		}
		else if (binding_type == VI_BINDING_TYPE_COMBINED_IMAGE_SAMPLER ||
			binding_type == VI_BINDING_TYPE_STORAGE_IMAGE)
		{
			writes[i].pImageInfo = write_images.data() + write_image_idx++;
		}
	}

	vkUpdateDescriptorSets(vk->device, writes.size(), writes.data(), 0, nullptr);
}

VIPass vi_create_pass(VIDevice device, const VIPassInfo* info)
{
	VIPass pass = (VIPass)vi_malloc(sizeof(VIPassObj));
	new (pass)VIPassObj();
	pass->device = device;

	pass->color_attachments.resize(info->color_attachment_count);
	for (uint32_t i = 0; i < info->color_attachment_count; i++)
		pass->color_attachments[i] = info->color_attachments[i];

	if (info->depth_stencil_attachment)
		pass->depth_stencil_attachment = *info->depth_stencil_attachment;
	else
		pass->depth_stencil_attachment.reset();

	if (device->backend == VI_BACKEND_OPENGL)
		return pass;

	VIVulkan* vk = &device->vk;

	std::vector<VkAttachmentDescription> vk_attachments(info->color_attachment_count);
	for (uint32_t i = 0; i < info->color_attachment_count; i++)
		cast_pass_color_attachment(info->color_attachments[i], vk_attachments.data() + i);

	if (info->depth_stencil_attachment)
	{
		VkAttachmentDescription vk_depth_stencil_description;
		cast_pass_depth_stencil_attachment(*info->depth_stencil_attachment, &vk_depth_stencil_description);
		vk_attachments.push_back(vk_depth_stencil_description);
	}

	std::vector<VkSubpassDescription> vk_subpasses(info->subpass_count);
	std::vector<std::vector<VkAttachmentReference>> vk_subpass_color_attachments(info->subpass_count);
	std::vector<std::optional<VkAttachmentReference>> vk_subpass_depth_stencil_attachments(info->subpass_count);

	for (uint32_t i = 0; i < info->subpass_count; i++)
	{
		cast_subpass_info(*info, info->subpasses[i],
			vk_subpass_color_attachments.data() + i,
			vk_subpass_depth_stencil_attachments.data() + i
		);

		VkAttachmentReference* depth_stencil_attachment = vk_subpass_depth_stencil_attachments[i].has_value() ?
			std::addressof(vk_subpass_depth_stencil_attachments[i].value()) : nullptr;

		vk_subpasses[i].flags = 0;
		vk_subpasses[i].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		vk_subpasses[i].inputAttachmentCount = 0;
		vk_subpasses[i].pInputAttachments = nullptr;
		vk_subpasses[i].preserveAttachmentCount = 0;
		vk_subpasses[i].pPreserveAttachments = nullptr;
		vk_subpasses[i].pResolveAttachments = nullptr;
		vk_subpasses[i].colorAttachmentCount = vk_subpass_color_attachments[i].size();
		vk_subpasses[i].pColorAttachments = vk_subpass_color_attachments[i].data();
		vk_subpasses[i].pDepthStencilAttachment = depth_stencil_attachment;
	}

	VkRenderPassCreateInfo passCI{};
	passCI.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	passCI.subpassCount = vk_subpasses.size();
	passCI.pSubpasses = vk_subpasses.data();
	passCI.attachmentCount = vk_attachments.size();
	passCI.pAttachments = vk_attachments.data();
	passCI.dependencyCount = info->depenency_count;
	passCI.pDependencies = info->dependencies;
	VK_CHECK(vkCreateRenderPass(vk->device, &passCI, nullptr, &pass->vk.handle));

	return pass;
}

void vi_destroy_pass(VIDevice device, VIPass pass)
{
	if (device->backend == VI_BACKEND_VULKAN)
	{
		VIVulkan* vk = &pass->device->vk;
		vkDestroyRenderPass(vk->device, pass->vk.handle, nullptr);
	}

	pass->~VIPassObj();
	vi_free(pass);
}

VIModule vi_create_module(VIDevice device, const VIModuleInfo* info)
{
	VIModule module = (VIModule)vi_malloc(sizeof(VIModuleObj));
	module->device = device;
	module->type = info->type;

	if (device->backend == VI_BACKEND_OPENGL)
	{
		gl_create_module(device, module, info);
		return module;
	}

	std::vector<char> byte_code;
	EShLanguage stage;
	cast_module_type_glslang(info->type, &stage);

	const uint32_t* code;
	size_t code_size;
	VICompileResult result;
	std::vector<uint32_t> spirv_words;

	if (info->vise_binary)
	{
		VIBinaryHeader* header = (VIBinaryHeader*)info->vise_binary;
		uint32_t header_size = header->header_size;
		uint32_t payload_size = header->payload_size;
		uint8_t* now = ((uint8_t*)header) + header_size;

		spirv_words.resize(payload_size / 4);
		for (size_t i = 0; i < spirv_words.size(); i++)
			spirv_words[i] = sread32(&now);

		code_size = (size_t)payload_size;
		code = spirv_words.data();
	}
	else if (info->vise_glsl)
	{
		compile_vk(result, stage, info->vise_glsl);
		code_size = result.vk_spirv.size() * 4;
		code = result.vk_spirv.data();
	}
	else
		VI_UNREACHABLE;
	
	VkShaderModuleCreateInfo moduleCI{};
	moduleCI.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	moduleCI.pCode = code;
	moduleCI.codeSize = code_size;

	VIVulkan* vk = &device->vk;
	VK_CHECK(vkCreateShaderModule(vk->device, &moduleCI, NULL, &module->vk.handle));

	return module;
}

void vi_destroy_module(VIDevice device, VIModule module)
{
	if (device->backend == VI_BACKEND_OPENGL)
		gl_destroy_module(device, module);
	else
	{
		VIVulkan* vk = &module->device->vk;
		vkDestroyShaderModule(vk->device, module->vk.handle, NULL);
	}

	vi_free(module);
}

VIBuffer vi_create_buffer(VIDevice device, const VIBufferInfo* info)
{
	VI_ASSERT(info->properties != 0);

	VIBuffer buffer = (VIBuffer)vi_malloc(sizeof(VIBufferObj));
	new (buffer) VIBufferObj();
	buffer->device = device;
	buffer->type = info->type;
	buffer->size = info->size;
	buffer->usage = info->usage;
	buffer->properties = info->properties;
	buffer->is_mapped = false;

	if (device->backend == VI_BACKEND_OPENGL)
	{
		gl_create_buffer(device, buffer, info);
		return buffer;
	}

	VIVulkan* vk = &device->vk;

	VmaAllocationCreateInfo allocCI{};
	allocCI.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
	allocCI.usage = VMA_MEMORY_USAGE_UNKNOWN;
	allocCI.requiredFlags = info->properties;
	allocCI.preferredFlags = info->properties;

	VkBufferUsageFlags usage;
	cast_buffer_usages(info->type, info->usage, &usage);

	VkBufferCreateInfo bufferCI{};
	bufferCI.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	bufferCI.size = (VkDeviceSize)info->size;
	bufferCI.usage = usage;

	VK_CHECK(vmaCreateBuffer(vk->vma, &bufferCI, &allocCI, &buffer->vk.handle, &buffer->vk.alloc, nullptr));

	return buffer;
}

void vi_destroy_buffer(VIDevice device, VIBuffer buffer)
{
	if (device->backend == VI_BACKEND_OPENGL)
		gl_destroy_buffer(device, buffer);
	else
	{
		VIVulkan* vk = &device->vk;
		vmaDestroyBuffer(vk->vma, buffer->vk.handle, buffer->vk.alloc);
	}

	buffer->~VIBufferObj();
	vi_free(buffer);
}

VIImage vi_create_image(VIDevice device, const VIImageInfo* info)
{
	VI_ASSERT(!(info->type == VI_IMAGE_TYPE_2D && info->layers != 1));
	VI_ASSERT(!(info->type == VI_IMAGE_TYPE_2D_ARRAY && info->layers <= 1));
	VI_ASSERT(!(info->type == VI_IMAGE_TYPE_CUBE && info->layers != 6));

	VIImage image = (VIImage)vi_malloc(sizeof(VIImageObj));
	new (image) VIImageObj();
	image->device = device;
	image->info = *info;
	image->flags = 0;

	if (device->backend == VI_BACKEND_OPENGL)
	{
		gl_create_image(&device->gl, image, info);
		return image;
	}

	VIVulkan* vk = &device->vk;

	VkFormat format;
	VkImageAspectFlags aspect;
	VkImageUsageFlags usage;
	VkImageType type;
	VkImageViewType view_type;
	cast_format_vk(image->info.format, &format, &aspect);
	cast_image_usages(image->info.usage, &usage);
	cast_image_type(image->info.type, &type, &view_type);

	VkImageCreateFlags flags = 0;
	if (info->type == VI_IMAGE_TYPE_CUBE)
		flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

	VkImageCreateInfo imageCI{};
	imageCI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageCI.flags = flags;
	imageCI.extent.width = info->width;
	imageCI.extent.height = info->height;
	imageCI.extent.depth = 1;
	imageCI.mipLevels = info->levels;
	imageCI.arrayLayers = info->layers;
	imageCI.imageType = type;
	imageCI.format = format;
	imageCI.usage = usage;
	imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageCI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
	vk_create_image(vk, image, &imageCI, info->properties);

	VkImageViewCreateInfo viewCI{};
	viewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewCI.image = image->vk.handle;
	viewCI.viewType = view_type;
	viewCI.format = format;
	viewCI.subresourceRange.aspectMask = aspect;
	viewCI.subresourceRange.baseArrayLayer = 0;
	viewCI.subresourceRange.baseMipLevel = 0;
	viewCI.subresourceRange.layerCount = info->layers;
	viewCI.subresourceRange.levelCount = info->levels;
	vk_create_image_view(vk, image, &viewCI);
	
	VkSamplerAddressMode address_mode;
	cast_sampler_address_mode_vk(image->info.sampler.address_mode, &address_mode);

	VkFilter filter;
	VkSamplerMipmapMode mipmap_mode;
	cast_filter_vk(image->info.sampler, &filter, &mipmap_mode);

	VkSamplerCreateInfo samplerCI{};
	samplerCI.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerCI.addressModeU = address_mode;
	samplerCI.addressModeV = address_mode;
	samplerCI.addressModeW = address_mode;
	samplerCI.minFilter = filter;
	samplerCI.magFilter = filter;
	samplerCI.anisotropyEnable = VK_FALSE; // TODO:
	samplerCI.maxAnisotropy = vk->pdevice_chosen->device_props.limits.maxSamplerAnisotropy;
	samplerCI.borderColor;// TODO:
	samplerCI.unnormalizedCoordinates = VK_FALSE;
	samplerCI.compareEnable = VK_FALSE;
	samplerCI.mipmapMode = mipmap_mode;
	samplerCI.mipLodBias = 0.0f;
	samplerCI.minLod = info->sampler.min_lod;
	samplerCI.maxLod = info->sampler.max_lod;
	vk_create_sampler(vk, image, &samplerCI);

	return image;
}

void vi_destroy_image(VIDevice device, VIImage image)
{
	if (device->backend == VI_BACKEND_OPENGL)
		gl_destroy_image(&device->gl, image);
	else
	{
		if (image->flags & VI_IMAGE_FLAG_CREATED_SAMPLER_BIT)
			vk_destroy_sampler(&device->vk, image);

		if (image->flags & VI_IMAGE_FLAG_CREATED_IMAGE_VIEW_BIT)
			vk_destroy_image_view(&device->vk, image);

		if (image->flags & VI_IMAGE_FLAG_CREATED_IMAGE_BIT)
			vk_destroy_image(&device->vk, image);
	}

	image->~VIImageObj();
	vi_free(image);
}

VISetLayout vi_create_set_layout(VIDevice device, const VISetLayoutInfo* info)
{
	VISetLayout layout = (VISetLayout)vi_malloc(sizeof(VISetLayoutObj));
	new (layout) VISetLayoutObj();

	layout->device = device;
	layout->bindings.resize(info->binding_count);

	for (size_t i = 0; i < info->binding_count; i++)
		layout->bindings[i] = info->bindings[i];

	if (device->backend == VI_BACKEND_OPENGL)
	{
		return layout;
	}

	VIVulkan* vk = &device->vk;

	std::vector<VkDescriptorSetLayoutBinding> bindings;
	bindings.resize(info->binding_count);

	for (uint32_t i = 0; i < info->binding_count; i++)
	{
		cast_binding(info->bindings + i, bindings.data() + i);
	}

	VkDescriptorSetLayoutCreateInfo layoutCI;
	layoutCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutCI.pNext = nullptr;
	layoutCI.flags = 0;
	layoutCI.bindingCount = bindings.size();
	layoutCI.pBindings = bindings.data();
	VK_CHECK(vkCreateDescriptorSetLayout(vk->device, &layoutCI, nullptr, &layout->vk.handle));

	return layout;
}

void vi_destroy_set_layout(VIDevice device, VISetLayout layout)
{
	if (device->backend == VI_BACKEND_VULKAN)
	{
		VIVulkan* vk = &device->vk;
		vkDestroyDescriptorSetLayout(vk->device, layout->vk.handle, nullptr);
	}

	layout->~VISetLayoutObj();
	vi_free(layout);
}

VISetPool vi_create_set_pool(VIDevice device, const VISetPoolInfo* info)
{
	VISetPool pool = (VISetPool)vi_malloc(sizeof(VISetPoolObj));
	new (pool) VISetPoolObj();
	pool->device = device;

	if (device->backend == VI_BACKEND_OPENGL)
		return pool;

	VIVulkan* vk = &device->vk;

	std::vector<VkDescriptorPoolSize> poolSizes;
	cast_set_pool_resources(info->resource_count, info->resources, poolSizes);

	VkDescriptorPoolCreateInfo poolCI{};
	poolCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolCI.pNext = nullptr;
	poolCI.flags = 0;
	poolCI.maxSets = info->max_set_count;
	poolCI.poolSizeCount = poolSizes.size();
	poolCI.pPoolSizes = poolSizes.data();

	pool->vk.flags = poolCI.flags;

	VK_CHECK(vkCreateDescriptorPool(vk->device, &poolCI, nullptr, &pool->vk.handle));

	return pool;
}

void vi_destroy_set_pool(VIDevice device, VISetPool pool)
{
	if (device->backend == VI_BACKEND_VULKAN)
		vkDestroyDescriptorPool(device->vk.device, pool->vk.handle, nullptr);

	pool->~VISetPoolObj();
	vi_free(pool);
}

VISet vi_allocate_set(VIDevice device, VISetPool pool, VISetLayout layout)
{
	VISet set = (VISet)vi_malloc(sizeof(VISetObj));
	new (set) VISetObj();

	set->device = device;
	set->pool = pool;
	set->layout = layout;

	if (device->backend == VI_BACKEND_OPENGL)
	{
		gl_alloc_set(device, set);
		return set;
	}

	VkDescriptorSetAllocateInfo allocI;
	allocI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocI.pNext = nullptr;
	allocI.descriptorPool = pool->vk.handle;
	allocI.descriptorSetCount = 1;
	allocI.pSetLayouts = &layout->vk.handle;

	VK_CHECK(vkAllocateDescriptorSets(device->vk.device, &allocI, &set->vk.handle));

	return set;
}

void vi_free_set(VIDevice device, VISet set)
{
	if (device->backend == VI_BACKEND_OPENGL)
		gl_free_set(device, set);
	else
	{
		// NOTE: different from Vulkan usage, vi_free_set must be called to prevent leaks,
		//       vi_alloc_set can be called at most VISetPoolInfo::max_set_count times
		if (set->pool->vk.flags & VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT)
		{
			VK_CHECK(vkFreeDescriptorSets(device->vk.device, set->pool->vk.handle, 1, &set->vk.handle));
		}
	}

	set->~VISetObj();
	vi_free(set);
}

VIPipelineLayout vi_create_pipeline_layout(VIDevice device, const VIPipelineLayoutInfo* info)
{
	VI_ASSERT(info->push_constant_size <= device->limits.max_push_constant_size);

	VIPipelineLayout layout = (VIPipelineLayout)vi_malloc(sizeof(VIPipelineLayoutObj));
	new (layout) VIPipelineLayoutObj();
	layout->push_constant_size = info->push_constant_size;
	layout->set_layouts.resize(info->set_layout_count);
	for (uint32_t i = 0; i < info->set_layout_count; i++)
		layout->set_layouts[i] = info->set_layouts[i];

	if (device->backend == VI_BACKEND_OPENGL)
	{
		gl_create_pipeline_layout(device, layout, info);
		return layout;
	}

	VIVulkan* vk = &device->vk;

	std::vector<VkDescriptorSetLayout> vklayouts(info->set_layout_count);
	for (uint32_t i = 0; i < info->set_layout_count; i++)
		vklayouts[i] = info->set_layouts[i]->vk.handle;

	VkPushConstantRange range;
	range.offset = 0;
	range.size = info->push_constant_size;
	range.stageFlags = VK_SHADER_STAGE_ALL;

	VkPipelineLayoutCreateInfo layoutCI{};
	layoutCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	layoutCI.setLayoutCount = vklayouts.size();
	layoutCI.pSetLayouts = vklayouts.data();
	layoutCI.pushConstantRangeCount = info->push_constant_size > 0 ? 1 : 0;
	layoutCI.pPushConstantRanges = &range;
	VK_CHECK(vkCreatePipelineLayout(vk->device, &layoutCI, nullptr, &layout->vk.handle));

	return layout;
}

void vi_destroy_pipeline_layout(VIDevice device, VIPipelineLayout layout)
{
	if (device->backend == VI_BACKEND_OPENGL)
		gl_destroy_pipeline_layout(device, layout);
	else
	{
		VIVulkan* vk = &device->vk;
		vkDestroyPipelineLayout(vk->device, layout->vk.handle, nullptr);
	}

	layout->~VIPipelineLayoutObj();
	vi_free(layout);
}

VIPipeline vi_create_pipeline(VIDevice device, const VIPipelineInfo* info)
{
	VIPipeline pipeline = (VIPipeline)vi_malloc(sizeof(VIPipelineObj));
	new (pipeline) VIPipelineObj();
	pipeline->device = device;
	pipeline->blend_state = info->blend_state;
	pipeline->depth_stencil_state = info->depth_stencil_state;
	pipeline->rasterization_state = info->rasterization_state;
	pipeline->layout = info->layout;
	pipeline->vertex_bindings.resize(info->vertex_binding_count);
	pipeline->vertex_attributes.resize(info->vertex_attribute_count);
	pipeline->vertex_module = info->vertex_module;
	pipeline->fragment_module = info->fragment_module;

	for (uint32_t i = 0; i < info->vertex_binding_count; i++)
		pipeline->vertex_bindings[i] = info->vertex_bindings[i];

	for (uint32_t i = 0; i < info->vertex_attribute_count; i++)
		pipeline->vertex_attributes[i] = info->vertex_attributes[i];

	if (device->backend == VI_BACKEND_OPENGL)
	{
		gl_create_pipeline(device, pipeline, info->vertex_module, info->fragment_module);
		return pipeline;
	}

	VIVulkan* vk = &device->vk;

	VkPipelineColorBlendAttachmentState blendState{};
	blendState.blendEnable = info->blend_state.enabled ? VK_TRUE : VK_FALSE;
	blendState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	if (blendState.blendEnable)
	{
		cast_blend_factor_vk(info->blend_state.src_color_factor, &blendState.srcColorBlendFactor);
		cast_blend_factor_vk(info->blend_state.dst_color_factor, &blendState.dstColorBlendFactor);
		cast_blend_factor_vk(info->blend_state.src_alpha_factor, &blendState.srcAlphaBlendFactor);
		cast_blend_factor_vk(info->blend_state.dst_alpha_factor, &blendState.dstAlphaBlendFactor);
		cast_blend_op_vk(info->blend_state.color_blend_op, &blendState.colorBlendOp);
		cast_blend_op_vk(info->blend_state.alpha_blend_op, &blendState.alphaBlendOp);
	}

	// NOTE: OpenGL does not allow individual blend states for each color attachment,
	//       here we are using the same blend state for each color attachment in Vulkan.
	std::vector<VkPipelineColorBlendAttachmentState> blendAttachments(info->pass->color_attachments.size());
	std::fill(blendAttachments.begin(), blendAttachments.end(), blendState);

	VkPipelineColorBlendStateCreateInfo blendStateCI{};
	blendStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	blendStateCI.logicOpEnable = VK_FALSE;
	blendStateCI.logicOp = VK_LOGIC_OP_COPY;
	blendStateCI.attachmentCount = blendAttachments.size();
	blendStateCI.pAttachments = blendAttachments.data();
	blendStateCI.blendConstants;  // Optional

	// vertex and fragment shader stage
	VkPipelineShaderStageCreateInfo shaderStageCI[2]{};
	shaderStageCI[0].module = info->vertex_module->vk.handle;
	shaderStageCI[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderStageCI[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	shaderStageCI[0].pName = VI_SHADER_ENTRY_POINT;
	shaderStageCI[1].module = info->fragment_module->vk.handle;
	shaderStageCI[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderStageCI[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	shaderStageCI[1].pName = VI_SHADER_ENTRY_POINT;

	std::array<VkDynamicState, 3> dynamicStates = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR,
		VK_DYNAMIC_STATE_FRONT_FACE,
	};

	VkPipelineDynamicStateCreateInfo dynamicStateCI{};
	dynamicStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicStateCI.dynamicStateCount = dynamicStates.size();
	dynamicStateCI.pDynamicStates = dynamicStates.data();

	std::vector<VkVertexInputAttributeDescription> vertexAttrs;
	std::vector<VkVertexInputBindingDescription> vertexBindings;
	cast_pipeline_vertex_input(
		info->vertex_attribute_count,
		info->vertex_attributes,
		info->vertex_binding_count,
		info->vertex_bindings,
		vertexAttrs, vertexBindings);

	VkPipelineVertexInputStateCreateInfo vertexInputCI{};
	vertexInputCI.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputCI.pNext = nullptr;
	vertexInputCI.flags = 0;
	vertexInputCI.vertexAttributeDescriptionCount = vertexAttrs.size();
	vertexInputCI.pVertexAttributeDescriptions = vertexAttrs.data();
	vertexInputCI.vertexBindingDescriptionCount = vertexBindings.size();
	vertexInputCI.pVertexBindingDescriptions = vertexBindings.data();

	VkPipelineInputAssemblyStateCreateInfo assemblyCI{};
	assemblyCI.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	assemblyCI.primitiveRestartEnable = VK_FALSE;
	assemblyCI.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

	VkPipelineMultisampleStateCreateInfo multisampleStateCI{};
	multisampleStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampleStateCI.sampleShadingEnable = VK_FALSE;
	multisampleStateCI.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisampleStateCI.minSampleShading = 1.0f;
	multisampleStateCI.pSampleMask = nullptr;
	multisampleStateCI.alphaToCoverageEnable = VK_FALSE;
	multisampleStateCI.alphaToOneEnable = VK_FALSE;

	VkPolygonMode polygonMode;
	VkCullModeFlags cullMode;
	cast_polygon_mode_vk(info->rasterization_state.polygon_mode, &polygonMode);
	cast_cull_mode_vk(info->rasterization_state.cull_mode, &cullMode);

	VkPipelineRasterizationStateCreateInfo rasterizationCI{};
	rasterizationCI.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizationCI.depthClampEnable = VK_FALSE;
	rasterizationCI.rasterizerDiscardEnable = VK_FALSE;
	rasterizationCI.polygonMode = polygonMode;
	rasterizationCI.depthBiasEnable = VK_FALSE;
	rasterizationCI.depthBiasConstantFactor = 0.0f;
	rasterizationCI.depthBiasClamp = 0.0f;
	rasterizationCI.depthBiasSlopeFactor = 0.0f;
	rasterizationCI.lineWidth = info->rasterization_state.line_width;
	rasterizationCI.cullMode = cullMode;
	pipeline->vk.front_face = VK_FRONT_FACE_COUNTER_CLOCKWISE;

	// TODO: this doesnt really matter for dynamic viewport states
	// TODO: flip initial viewport?
	VkViewport viewport;
	viewport.width = 1600;
	viewport.height = 900;
	viewport.x = 0;
	viewport.y = 0;
	VkRect2D scissor;
	scissor.offset.x = 0;
	scissor.offset.y = 0;
	scissor.extent.width = 1600;
	scissor.extent.height = 900;

	VkPipelineViewportStateCreateInfo viewportStateCI{};
	viewportStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportStateCI.scissorCount = 1;
	viewportStateCI.pScissors = &scissor;
	viewportStateCI.viewportCount = 1;
	viewportStateCI.pViewports = &viewport;

	VkCompareOp depth_compare_op;
	cast_compare_op_vk(info->depth_stencil_state.depth_compare_op, &depth_compare_op);
	VkPipelineDepthStencilStateCreateInfo depthStencilStateCI{};
	depthStencilStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencilStateCI.depthTestEnable = info->depth_stencil_state.depth_test_enabled;
	depthStencilStateCI.depthWriteEnable = info->depth_stencil_state.depth_write_enabled;
	depthStencilStateCI.depthCompareOp = depth_compare_op;
	depthStencilStateCI.depthBoundsTestEnable = VK_FALSE;
	depthStencilStateCI.minDepthBounds = 0.0f;
	depthStencilStateCI.maxDepthBounds = 1.0f;
	depthStencilStateCI.stencilTestEnable = info->depth_stencil_state.stencil_test_enabled;
	if (depthStencilStateCI.stencilTestEnable)
	{
		cast_stencil_op_state_vk(info->depth_stencil_state.stencil_front, &depthStencilStateCI.front);
		cast_stencil_op_state_vk(info->depth_stencil_state.stencil_back, &depthStencilStateCI.back);
	}

	VkGraphicsPipelineCreateInfo pipelineCI{};
	pipelineCI.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineCI.stageCount = 2;
	pipelineCI.pStages = shaderStageCI;
	pipelineCI.pVertexInputState = &vertexInputCI;
	pipelineCI.pInputAssemblyState = &assemblyCI;
	pipelineCI.pViewportState = &viewportStateCI;
	pipelineCI.pRasterizationState = &rasterizationCI;
	pipelineCI.pMultisampleState = &multisampleStateCI;
	pipelineCI.pDepthStencilState = &depthStencilStateCI;
	pipelineCI.pColorBlendState = &blendStateCI;
	pipelineCI.pDynamicState = &dynamicStateCI;
	pipelineCI.renderPass = info->pass->vk.handle;
	pipelineCI.layout = pipeline->layout->vk.handle;
	pipelineCI.basePipelineHandle = VK_NULL_HANDLE;  // Optional
	pipelineCI.basePipelineIndex = -1;               // Optional

	VK_CHECK(vkCreateGraphicsPipelines(vk->device, VK_NULL_HANDLE, 1, &pipelineCI, NULL, &pipeline->vk.handle));

	return pipeline;
}

void vi_destroy_pipeline(VIDevice device, VIPipeline pipeline)
{
	if (device->backend == VI_BACKEND_OPENGL)
		gl_destroy_pipeline(device, pipeline);
	else
	{
		VIVulkan* vk = &device->vk;
		vkDestroyPipeline(vk->device, pipeline->vk.handle, nullptr);
	}

	pipeline->~VIPipelineObj();
	vi_free(pipeline);
}

VIComputePipeline vi_create_compute_pipeline(VIDevice device, const VIComputePipelineInfo* info)
{
	VIComputePipeline pipeline = (VIComputePipeline)vi_malloc(sizeof(VIComputePipelineObj));
	new (pipeline) VIComputePipelineObj();
	pipeline->device = device;
	pipeline->layout = info->layout;
	pipeline->compute_module = info->compute_module;

	if (device->backend == VI_BACKEND_OPENGL)
	{
		gl_create_compute_pipeline(device, pipeline, info->compute_module);
		return pipeline;
	}

	VIVulkan* vk = &device->vk;

	VkPipelineShaderStageCreateInfo stageCI{};
	stageCI.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stageCI.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	stageCI.pName = VI_SHADER_ENTRY_POINT;
	stageCI.module = info->compute_module->vk.handle;

	VkComputePipelineCreateInfo pipelineCI{};
	pipelineCI.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	pipelineCI.basePipelineHandle = VK_NULL_HANDLE;
	pipelineCI.layout = info->layout->vk.handle;
	pipelineCI.stage = stageCI;

	VK_CHECK(vkCreateComputePipelines(vk->device, VK_NULL_HANDLE, 1, &pipelineCI, nullptr, &pipeline->vk.handle));

	return pipeline;
}

void vi_destroy_compute_pipeline(VIDevice device, VIComputePipeline pipeline)
{
	if (device->backend == VI_BACKEND_OPENGL)
		gl_destroy_compute_pipeline(device, pipeline);
	else
	{
		VIVulkan* vk = &device->vk;
		vkDestroyPipeline(vk->device, pipeline->vk.handle, nullptr);
	}

	pipeline->~VIComputePipelineObj();
	vi_free(pipeline);
}


VIFramebuffer vi_create_framebuffer(VIDevice device, const VIFramebufferInfo* info)
{
	VIFramebuffer framebuffer = (VIFramebuffer)vi_malloc(sizeof(VIFramebufferObj));
	new (framebuffer) VIFramebufferObj();
	framebuffer->device = device;
	framebuffer->extent.width = info->width;
	framebuffer->extent.height = info->height;
	framebuffer->depth_stencil_attachment = info->depth_stencil_attachment;
	framebuffer->color_attachments.resize(info->color_attachment_count);
	for (uint32_t i = 0; i < info->color_attachment_count; i++)
		framebuffer->color_attachments[i] = info->color_attachments[i];

	if (device->backend == VI_BACKEND_OPENGL)
	{
		VIOpenGL* gl = &device->gl;
		gl_create_framebuffer(gl, framebuffer, info);
		return framebuffer;
	}

	std::vector<VkImageView> vk_views(info->color_attachment_count);
	for (uint32_t i = 0; i < info->color_attachment_count; i++)
	{
		VI_ASSERT(info->color_attachments[i]->info.usage & VI_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
		vk_views[i] = info->color_attachments[i]->vk.view_handle;
	}

	if (info->depth_stencil_attachment)
		vk_views.push_back(info->depth_stencil_attachment->vk.view_handle);

	VkFramebufferCreateInfo framebufferCI{};
	framebufferCI.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	framebufferCI.width = info->width;
	framebufferCI.height = info->height;
	framebufferCI.layers = 1;
	framebufferCI.attachmentCount = vk_views.size();
	framebufferCI.pAttachments = vk_views.data();
	framebufferCI.renderPass = info->pass->vk.handle;
	VK_CHECK(vkCreateFramebuffer(device->vk.device, &framebufferCI, nullptr, &framebuffer->vk.handle));

	return framebuffer;
}

void vi_destroy_framebuffer(VIDevice device, VIFramebuffer framebuffer)
{
	if (device->backend == VI_BACKEND_OPENGL)
	{
		VIOpenGL* gl = &device->gl;
		gl_destroy_framebuffer(gl, framebuffer);
	}
	else
	{
		VIVulkan* vk = &device->vk;
		vkDestroyFramebuffer(vk->device, framebuffer->vk.handle, nullptr);
	}

	framebuffer->~VIFramebufferObj();
	vi_free(framebuffer);
}

VICommandPool vi_create_command_pool(VIDevice device, uint32_t family_idx, VkCommandPoolCreateFlags flags)
{
	VICommandPool pool = (VICommandPool)vi_malloc(sizeof(VICommandPoolObj));
	new (pool)VICommandPoolObj();
	pool->device = device;

	if (device->backend == VI_BACKEND_OPENGL)
		return pool;

	VkCommandPoolCreateInfo poolCI;
	poolCI.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolCI.pNext = nullptr;
	poolCI.flags = flags;
	poolCI.queueFamilyIndex = family_idx;
	VK_CHECK(vkCreateCommandPool(device->vk.device, &poolCI, nullptr, &pool->vk_handle));

	return pool;
}

void vi_destroy_command_pool(VIDevice device, VICommandPool pool)
{
	if (device->backend == VI_BACKEND_VULKAN)
		vkDestroyCommandPool(device->vk.device, pool->vk_handle, nullptr);

	pool->~VICommandPoolObj();
	vi_free(pool);
}

VICommand vi_allocate_primary_command(VIDevice device, VICommandPool pool)
{
	VIVulkan* vk = &device->vk;
	VICommand cmd = (VICommand)vi_malloc(sizeof(VICommandObj));
	new (cmd)VICommandObj();
	cmd->device = device;

	if (device->backend == VI_BACKEND_OPENGL)
	{
		gl_alloc_cmd_buffer(device, cmd);
		return cmd;
	}

	cmd->pool = pool;
	vk_alloc_cmd_buffer(vk, cmd, pool->vk_handle, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	return cmd;
}

void vi_free_command(VIDevice device, VICommand cmd)
{
	if (device->backend == VI_BACKEND_OPENGL)
		gl_free_command(device, cmd);
	else
	{
		VIVulkan* vk = &device->vk;
		vkFreeCommandBuffers(vk->device, cmd->pool->vk_handle, 1, &cmd->vk.handle);
	}

	cmd->~VICommandObj();
	vi_free(cmd);
}

void vi_device_wait_idle(VIDevice device)
{
	if (device->backend == VI_BACKEND_OPENGL)
		return;

	VK_CHECK(vkDeviceWaitIdle(device->vk.device));
}

const VIPhysicalDevice* vi_device_get_physical_device(VIDevice device)
{
	VI_ASSERT(device->backend == VI_BACKEND_VULKAN);

	return device->vk.pdevice_chosen;
}

uint32_t vi_device_get_graphics_family_index(VIDevice device)
{
	return device->vk.family_idx_graphics;
}

VIQueue vi_device_get_graphics_queue(VIDevice device)
{
	return &device->queue_graphics;
}

bool vi_device_has_depth_stencil_format(VIDevice device, VIFormat format, VkImageTiling tiling)
{
	if (device->backend == VI_BACKEND_OPENGL)
	{
		// TODO:
		VI_UNREACHABLE;
	}
	
	VkFormat vk_format;
	VkImageAspectFlags vk_aspect;
	cast_format_vk(format, &vk_format, &vk_aspect);

	return vk_has_format_features(&device->vk, vk_format, tiling, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}

VIPass vi_device_get_swapchain_pass(VIDevice device)
{
	if (device->backend == VI_BACKEND_OPENGL)
		return device->swapchain_pass;

	VI_ASSERT(device->swapchain_pass->vk.handle != VK_NULL_HANDLE);
	return device->swapchain_pass;
}

VIFramebuffer vi_device_get_swapchain_framebuffer(VIDevice device, uint32_t index)
{
	return device->swapchain_framebuffers + index;
}

uint32_t vi_device_next_frame(VIDevice device, VISemaphore* image_acquired, VISemaphore* present_ready, VIFence* frame_complete)
{
	VI_ASSERT(image_acquired && present_ready);

	if (device->backend == VI_BACKEND_OPENGL)
	{
		VIOpenGL* gl = &device->gl;
		gl->frame.semaphore.image_acquired.gl_signal = true;
		gl->frame.semaphore.present_ready.gl_signal = false;
		gl->frame.fence.frame_complete.gl_signal = false;

		*image_acquired = &gl->frame.semaphore.image_acquired;
		*present_ready = &gl->frame.semaphore.present_ready;
		*frame_complete = &gl->frame.fence.frame_complete;
		return 0;
	}

	VIVulkan* vk = &device->vk;

	vk->frame_idx = (vk->frame_idx + 1) % vk->frames_in_flight;

	VIFrame* frame = vk->frames + vk->frame_idx;
	VK_CHECK(vkWaitForFences(vk->device, 1, &frame->fence.frame_complete.vk_handle, VK_TRUE, UINT64_MAX));

	VkResult result = vkAcquireNextImageKHR(
		vk->device,
		vk->swapchain.handle,
		UINT64_MAX,
		frame->semaphore.image_acquired.vk_handle,
		VK_NULL_HANDLE,
		&vk->swapchain.image_idx
	);
	VK_CHECK(result);

	// assumes successful acquiring
	VK_CHECK(vkResetFences(vk->device, 1, &frame->fence.frame_complete.vk_handle));

	*image_acquired = &frame->semaphore.image_acquired;
	*present_ready = &frame->semaphore.present_ready;
	*frame_complete = &frame->fence.frame_complete;

	return device->vk.swapchain.image_idx;
}

void vi_device_present_frame(VIDevice device)
{
	if (device->backend == VI_BACKEND_OPENGL)
	{
		VI_ASSERT(device->gl.frame.semaphore.present_ready.gl_signal);
		gl_device_present_frame(device);
		return;
	}

	VIVulkan* vk = &device->vk;
	VIFrame* frame = vk->frames + vk->frame_idx;

	VkPresentInfoKHR presentI;
	presentI.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentI.pNext = NULL;
	presentI.pResults = NULL;
	presentI.waitSemaphoreCount = 1;
	presentI.pWaitSemaphores = &frame->semaphore.present_ready.vk_handle;
	presentI.swapchainCount = 1;
	presentI.pSwapchains = &vk->swapchain.handle;
	presentI.pImageIndices = &vk->swapchain.image_idx;

	VK_CHECK(vkQueuePresentKHR(device->queue_present.vk_handle, &presentI));
}

void vi_buffer_map(VIBuffer buffer)
{
	VI_ASSERT(!buffer->is_mapped);

	buffer->is_mapped = true;
	VIDevice device = buffer->device;

	if (device->backend == VI_BACKEND_OPENGL)
	{
		// OpenGL persistent mapping is not currently supported as it requires additional
		// memory barriers and fence syncs. We use glBufferSubData and glGetBufferSubData
		// to emulate persistant mapping with memory coherency.
		if (!buffer->map)
			buffer->map = (uint8_t*)vi_malloc((size_t)buffer->size);
		return;
	}

	VK_CHECK(vmaMapMemory(device->vk.vma, buffer->vk.alloc, (void**) &buffer->map));
}

void* vi_buffer_map_read(VIBuffer buffer, uint32_t offset, uint32_t size)
{
	VI_ASSERT(buffer->is_mapped);
	VI_ASSERT(offset + size <= buffer->size);
	
	VIDevice device = buffer->device;

	if (device->backend == VI_BACKEND_OPENGL)
	{
		if (buffer->type != VI_BUFFER_TYPE_TRANSFER)
		{
			glBindBuffer(buffer->gl.target, buffer->gl.handle);
			glGetBufferSubData(buffer->gl.target, offset, size, buffer->map + offset);
			GL_CHECK();
		}

		return buffer->map + offset;
	}

	return buffer->map + offset;
}

void vi_buffer_map_write(VIBuffer buffer, uint32_t offset, uint32_t size, const void* write)
{
	VI_ASSERT(buffer->is_mapped);
	VI_ASSERT(offset + size <= buffer->size);

	VIDevice device = buffer->device;

	if (device->backend == VI_BACKEND_OPENGL)
	{
		if (buffer->type != VI_BUFFER_TYPE_TRANSFER)
		{
			glBindBuffer(buffer->gl.target, buffer->gl.handle);
			glBufferSubData(buffer->gl.target, offset, size, write);
			GL_CHECK();
		}
		else
			memcpy(buffer->map + offset, write, size);

		return;
	}

	memcpy(buffer->map + offset, write, size);
}

void vi_buffer_map_flush(VIBuffer buffer, uint32_t offset, uint32_t size)
{
	VI_ASSERT(buffer->is_mapped);

	VIDevice device = buffer->device;

	if (device->backend == VI_BACKEND_OPENGL || (buffer->properties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
		return;

	VK_CHECK(vmaFlushAllocation(device->vk.vma, buffer->vk.alloc, offset, size));
}

void vi_buffer_map_invalidate(VIBuffer buffer, uint32_t offset, uint32_t size)
{
	VI_ASSERT(buffer->is_mapped);

	VIDevice device = buffer->device;

	if (device->backend == VI_BACKEND_OPENGL || (buffer->properties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
		return;

	VK_CHECK(vmaInvalidateAllocation(device->vk.vma, buffer->vk.alloc, offset, size));
}

void vi_buffer_unmap(VIBuffer buffer)
{
	VI_ASSERT(buffer->is_mapped);

	buffer->is_mapped = false;
	VIDevice device = buffer->device;

	if (device->backend == VI_BACKEND_OPENGL)
		return;

	vmaUnmapMemory(device->vk.vma, buffer->vk.alloc);
}

void vi_reset_command(VICommand cmd)
{
	VIDevice device = cmd->device;

	if (device->backend == VI_BACKEND_OPENGL)
	{
		gl_reset_command(device, cmd);
		return;
	}

	// TODO: assert if cmd is allocated from a pool with VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT

	VK_CHECK(vkResetCommandBuffer(cmd->vk.handle, 0));
}

void vi_begin_command(VICommand cmd, VkCommandBufferUsageFlags flags)
{
	if (cmd->device->backend == VI_BACKEND_OPENGL)
	{
		gl_reset_command(cmd->device, cmd);
		return;
	}

	VkCommandBufferBeginInfo bufferBI;
	bufferBI.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	bufferBI.pNext = NULL;
	bufferBI.pInheritanceInfo = NULL;
	bufferBI.flags = flags;

	VK_CHECK(vkBeginCommandBuffer(cmd->vk.handle, &bufferBI));

	// TODO: put in thread safe memory, use VICommandPool for each CPU thread?
	cmd->device->active_pipeline = VI_NULL;
}

void vi_end_command(VICommand cmd)
{
	// TODO: put in thread safe memory, use VICommandPool for each CPU thread?
	cmd->device->active_pipeline = VI_NULL;

	if (cmd->device->backend == VI_BACKEND_OPENGL)
		return;

	VK_CHECK(vkEndCommandBuffer(cmd->vk.handle));
}

void vi_cmd_opengl_callback(VICommand cmd, void (*callback)(void* data), void* data)
{
	if (cmd->device->backend == VI_BACKEND_VULKAN)
		return;

	GLCommand* glcmd = gl_append_command(cmd, GL_COMMAND_TYPE_OPENGL_CALLBACK);
	glcmd->opengl_callback.callback = callback;
	glcmd->opengl_callback.data = data;
}

void vi_cmd_copy_buffer(VICommand cmd, VIBuffer src, VIBuffer dst, uint32_t region_count, const VkBufferCopy* regions)
{
	if (cmd->device->backend == VI_BACKEND_OPENGL)
	{
		GLCommand* glcmd = gl_append_command(cmd, GL_COMMAND_TYPE_COPY_BUFFER);
		new (&glcmd->copy_buffer) GLCommandCopyBuffer();
		glcmd->copy_buffer.src = src;
		glcmd->copy_buffer.dst = dst;
		glcmd->copy_buffer.regions.resize(region_count);
		std::copy(regions, regions + region_count, glcmd->copy_buffer.regions.begin());
		return;
	}

	vkCmdCopyBuffer(cmd->vk.handle, src->vk.handle, dst->vk.handle, region_count, regions);
}

void vi_cmd_copy_buffer_to_image(VICommand cmd, VIBuffer buffer, VIImage image, VkImageLayout layout, uint32_t region_count, const VkBufferImageCopy* regions)
{
	if (cmd->device->backend == VI_BACKEND_OPENGL)
	{
		GLCommand* glcmd = gl_append_command(cmd, GL_COMMAND_TYPE_COPY_BUFFER_TO_IMAGE);
		new (&glcmd->copy_buffer_to_image) GLCommandCopyBufferToImage();
		glcmd->copy_buffer_to_image.buffer = buffer;
		glcmd->copy_buffer_to_image.image = image;
		glcmd->copy_buffer_to_image.regions.resize(region_count);
		std::copy(regions, regions + region_count, glcmd->copy_buffer_to_image.regions.begin());
		return;
	}

	vkCmdCopyBufferToImage(cmd->vk.handle, buffer->vk.handle, image->vk.handle, layout, region_count, regions);
}

void vi_cmd_copy_image(VICommand cmd, VIImage src, VkImageLayout src_layout, VIImage dst, VkImageLayout dst_layout, uint32_t region_count, const VkImageCopy* regions)
{
	VI_ASSERT(src->info.usage & VI_IMAGE_USAGE_TRANSFER_SRC_BIT);
	VI_ASSERT(dst->info.usage & VI_IMAGE_USAGE_TRANSFER_DST_BIT);

	if (cmd->device->backend == VI_BACKEND_OPENGL)
	{
		GLCommand* glcmd = gl_append_command(cmd, GL_COMMAND_TYPE_COPY_IMAGE);
		new (&glcmd->copy_image) GLCommandCopyImage();
		glcmd->copy_image.src = src;
		glcmd->copy_image.dst = dst;
		glcmd->copy_image.regions.resize(region_count);
		std::copy(regions, regions + region_count, glcmd->copy_image.regions.begin());
		return;
	}

	vkCmdCopyImage(cmd->vk.handle, src->vk.handle, src_layout, dst->vk.handle, dst_layout, region_count, regions);
}

void vi_cmd_copy_image_to_buffer(VICommand cmd, VIImage image, VkImageLayout layout, VIBuffer buffer, uint32_t region_count, const VkBufferImageCopy* regions)
{
	VI_ASSERT(image->info.usage & VI_IMAGE_USAGE_TRANSFER_SRC_BIT);

	if (cmd->device->backend == VI_BACKEND_OPENGL)
	{
		GLCommand* glcmd = gl_append_command(cmd, GL_COMMAND_TYPE_COPY_IMAGE_TO_BUFFER);
		new (&glcmd->copy_image_to_buffer) GLCommandCopyImageToBuffer();
		glcmd->copy_image_to_buffer.image = image;
		glcmd->copy_image_to_buffer.buffer = buffer;
		glcmd->copy_image_to_buffer.regions.resize(region_count);
		std::copy(regions, regions + region_count, glcmd->copy_image_to_buffer.regions.begin());
		return;
	}

	vkCmdCopyImageToBuffer(cmd->vk.handle, image->vk.handle, layout, buffer->vk.handle, region_count, regions);
}

void vi_cmd_copy_color_attachment_to_buffer(VICommand cmd, VIFramebuffer framebuffer, uint32_t index, VkImageLayout layout, VIBuffer buffer)
{
	VI_ASSERT(index < framebuffer->color_attachments.size());
	VI_ASSERT(framebuffer->color_attachments[index]->info.usage & VI_IMAGE_USAGE_TRANSFER_SRC_BIT);

	if (cmd->device->backend == VI_BACKEND_OPENGL)
	{
		VI_UNREACHABLE;
	}

	VkBufferImageCopy region;
	region.bufferImageHeight = 0;
	region.bufferOffset = 0;
	region.bufferRowLength = 0;
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel = 0;
	region.imageSubresource.baseArrayLayer = 0;
	region.imageSubresource.layerCount = 1;
	region.imageExtent = { framebuffer->extent.width, framebuffer->extent.height, 1 };
	region.imageOffset = { 0, 0, 0 };
	vi_cmd_copy_image_to_buffer(cmd, framebuffer->color_attachments[index], layout, buffer, 1, &region);
}

void vi_cmd_copy_depth_stencil_attachment_to_buffer(VICommand cmd, VIFramebuffer framebuffer, VIBuffer buffer)
{
	VI_UNREACHABLE;

	// TODO: does this even make sense
}

void vi_cmd_begin_pass(VICommand cmd, const VIPassBeginInfo* info)
{
	if (cmd->device->backend == VI_BACKEND_OPENGL)
	{
		GLCommand* glcmd = gl_append_command(cmd, GL_COMMAND_TYPE_BEGIN_PASS);
		new (&glcmd->begin_pass) GLCommandBeginPass();
		glcmd->begin_pass.pass = info->pass;
		glcmd->begin_pass.framebuffer = info->framebuffer;
		glcmd->begin_pass.color_clear_values.resize(info->color_clear_value_count);
		for (uint32_t i = 0; i < info->color_clear_value_count; i++)
			glcmd->begin_pass.color_clear_values[i] = info->color_clear_values[i];
		if (info->depth_stencil_clear_value)
			glcmd->begin_pass.depth_stencil_clear_value = *info->depth_stencil_clear_value;
		else
			glcmd->begin_pass.depth_stencil_clear_value.reset();
		return;
	}

	VIDevice device = cmd->device;
	VIVulkan* vk = &device->vk;

	size_t swapchain_framebuffer_count = device->vk.swapchain.images.size();
	device->vk.pass_uses_swapchain_framebuffer = false;
	
	for (size_t i = 0; i < swapchain_framebuffer_count; i++)
	{
		if (info->framebuffer == device->swapchain_framebuffers + i)
		{
			device->vk.pass_uses_swapchain_framebuffer = true;
			break;
		}
	}

	VkRect2D render_area;
	render_area.extent = info->framebuffer->extent;
	render_area.offset.x = 0;
	render_area.offset.y = 0;

	// merge color clear values and depth stencil clear value
	std::vector<VkClearValue> vk_clear_values(info->pass->color_attachments.size());

	for (uint32_t i = 0; i < info->color_clear_value_count; i++)
	{
		if (info->pass->color_attachments[i].color_load_op == VK_ATTACHMENT_LOAD_OP_CLEAR)
			vk_clear_values[i] = info->color_clear_values[i];
	}

	if (info->pass->depth_stencil_attachment.has_value() &&
		(info->pass->depth_stencil_attachment->stencil_load_op == VK_ATTACHMENT_LOAD_OP_CLEAR
		|| info->pass->depth_stencil_attachment->depth_load_op == VK_ATTACHMENT_LOAD_OP_CLEAR))
	{
		VI_ASSERT(info->depth_stencil_clear_value);
		vk_clear_values.push_back(*info->depth_stencil_clear_value);
	}

	VkRenderPassBeginInfo passBI;
	passBI.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	passBI.pNext = nullptr;
	passBI.clearValueCount = vk_clear_values.size();
	passBI.pClearValues = vk_clear_values.data();
	passBI.renderArea = render_area;
	passBI.renderPass = info->pass->vk.handle;
	passBI.framebuffer = info->framebuffer->vk.handle;

	vkCmdBeginRenderPass(cmd->vk.handle, &passBI, VK_SUBPASS_CONTENTS_INLINE);
}

void vi_cmd_end_pass(VICommand cmd)
{
	if (cmd->device->backend == VI_BACKEND_OPENGL)
	{
		gl_append_command(cmd, GL_COMMAND_TYPE_END_PASS);
		return;
	}

	vkCmdEndRenderPass(cmd->vk.handle);
}

void vi_cmd_bind_graphics_pipeline(VICommand cmd, VIPipeline pipeline)
{
	cmd->device->active_pipeline = pipeline;

	if (cmd->device->backend == VI_BACKEND_OPENGL)
	{
		GLCommand* glcmd = gl_append_command(cmd, GL_COMMAND_TYPE_BIND_PIPELINE);
		glcmd->bind_pipeline = pipeline;
		return;
	}

	vkCmdBindPipeline(cmd->vk.handle, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->vk.handle);

	// when rendering to offscreen framebuffers, render the contents flipped
	bool flip_vk_front_face = !cmd->device->vk.pass_uses_swapchain_framebuffer;
	//flip_vk_front_face = false;
	VkFrontFace front_face = pipeline->vk.front_face;

	if (flip_vk_front_face)
		front_face = (front_face == VK_FRONT_FACE_CLOCKWISE) ? VK_FRONT_FACE_COUNTER_CLOCKWISE : VK_FRONT_FACE_CLOCKWISE;

	vkCmdSetFrontFaceEXT(cmd->vk.handle, front_face);
}

void vi_cmd_bind_compute_pipeline(VICommand cmd, VIComputePipeline pipeline)
{
	if (cmd->device->backend == VI_BACKEND_OPENGL)
	{
		GLCommand* glcmd = gl_append_command(cmd, GL_COMMAND_TYPE_BIND_COMPUTE_PIPELINE);
		glcmd->bind_compute_pipeline = pipeline;
		return;
	}

	vkCmdBindPipeline(cmd->vk.handle, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline->vk.handle);
}

void vi_cmd_dispatch(VICommand cmd, uint32_t group_count_x, uint32_t group_count_y, uint32_t group_count_z)
{
	VI_ASSERT(group_count_x <= cmd->device->limits.max_compute_workgroup_count[0]);
	VI_ASSERT(group_count_y <= cmd->device->limits.max_compute_workgroup_count[1]);
	VI_ASSERT(group_count_z <= cmd->device->limits.max_compute_workgroup_count[2]);

	if (cmd->device->backend == VI_BACKEND_OPENGL)
	{
		GLCommand* glcmd = gl_append_command(cmd, GL_COMMAND_TYPE_DISPATCH);
		glcmd->dispatch.group_count_x = (GLuint)group_count_x;
		glcmd->dispatch.group_count_y = (GLuint)group_count_y;
		glcmd->dispatch.group_count_z = (GLuint)group_count_z;
		return;
	}

	vkCmdDispatch(cmd->vk.handle, group_count_x, group_count_y, group_count_z);
}

void vi_cmd_bind_vertex_buffers(VICommand cmd, uint32_t first_binding, uint32_t binding_count, VIBuffer* buffers)
{
	if (cmd->device->backend == VI_BACKEND_OPENGL)
	{
		VI_ASSERT(cmd->device->active_pipeline != VI_NULL);

		GLCommand* glcmd = gl_append_command(cmd, GL_COMMAND_TYPE_BIND_VERTEX_BUFFERS);
		new (&glcmd->bind_vertex_buffers) GLCommandBindVertexBuffers();
		glcmd->bind_vertex_buffers.first_binding = first_binding;
		glcmd->bind_vertex_buffers.pipeline = cmd->device->active_pipeline;
		glcmd->bind_vertex_buffers.buffers.resize(binding_count);
		for (uint32_t i = 0; i < binding_count; i++)
			glcmd->bind_vertex_buffers.buffers[i] = buffers[i];
		return;
	}

	std::vector<VkBuffer> vkbuffers(binding_count);
	std::vector<VkDeviceSize> offsets(binding_count);

	for (uint32_t i = 0; i < binding_count; i++)
	{
		vkbuffers[i] = buffers[i]->vk.handle;
		offsets[i] = 0;
	}

	vkCmdBindVertexBuffers(cmd->vk.handle, first_binding, binding_count, vkbuffers.data(), offsets.data());
}

void vi_cmd_bind_index_buffer(VICommand cmd, VIBuffer buffer, VkIndexType index_type)
{
	VI_ASSERT(buffer->type == VI_BUFFER_TYPE_INDEX);

	if (cmd->device->backend == VI_BACKEND_OPENGL)
	{
		GLCommand* glcmd = gl_append_command(cmd, GL_COMMAND_TYPE_BIND_INDEX_BUFFER);
		glcmd->bind_index_buffer.buffer = buffer;
		glcmd->bind_index_buffer.index_type = index_type;
		return;
	}

	vkCmdBindIndexBuffer(cmd->vk.handle, buffer->vk.handle, 0, index_type);
}

void vi_cmd_bind_graphics_set(VICommand cmd, VIPipelineLayout layout, uint32_t set_idx, VISet set)
{
	if (cmd->device->backend == VI_BACKEND_OPENGL)
	{
		GLCommand* glcmd = gl_append_command(cmd, GL_COMMAND_TYPE_BIND_SET);
		glcmd->bind_set.set = set;
		glcmd->bind_set.set_index = set_idx;
		glcmd->bind_set.pipeline_layout = layout;
		return;
	}

	vkCmdBindDescriptorSets(cmd->vk.handle, VK_PIPELINE_BIND_POINT_GRAPHICS, layout->vk.handle, set_idx, 1, &set->vk.handle, 0, nullptr);
}

void vi_cmd_bind_compute_set(VICommand cmd, VIPipelineLayout layout, uint32_t set_idx, VISet set)
{
	if (cmd->device->backend == VI_BACKEND_OPENGL)
	{
		GLCommand* glcmd = gl_append_command(cmd, GL_COMMAND_TYPE_BIND_SET);
		glcmd->bind_set.set = set;
		glcmd->bind_set.set_index = set_idx;
		glcmd->bind_set.pipeline_layout = layout;
		return;
	}

	vkCmdBindDescriptorSets(cmd->vk.handle, VK_PIPELINE_BIND_POINT_COMPUTE, layout->vk.handle, set_idx, 1, &set->vk.handle, 0, nullptr);
}

void vi_cmd_push_constants(VICommand cmd, VIPipelineLayout layout, uint32_t offset, uint32_t size, const void* value)
{
	if (cmd->device->backend == VI_BACKEND_OPENGL)
	{
		GLCommand* glcmd = gl_append_command(cmd, GL_COMMAND_TYPE_PUSH_CONSTANTS);
		new (&glcmd->push_constants)GLCommandPushConstants(offset, size);
		memcpy(glcmd->push_constants.value, value, size);
		return;
	}

	vkCmdPushConstants(cmd->vk.handle, layout->vk.handle, VK_SHADER_STAGE_ALL, offset, size, value);
}

void vi_cmd_set_viewport(VICommand cmd, VkViewport viewport)
{
	if (cmd->device->backend == VI_BACKEND_OPENGL)
	{
		GLCommand* glcmd = gl_append_command(cmd, GL_COMMAND_TYPE_SET_VIEWPORT);
		glcmd->set_viewport = viewport;
		return;
	}

	bool flip_vk_viewport = cmd->device->vk.pass_uses_swapchain_framebuffer;
	//flip_vk_viewport = true;

	if (flip_vk_viewport)
	{
		VkViewport flipped;
		flipped.maxDepth = viewport.maxDepth;
		flipped.minDepth = viewport.minDepth;
		flipped.x = viewport.x;
		flipped.y = viewport.y + viewport.height;
		flipped.width = viewport.width;
		flipped.height = -viewport.height;
		viewport = flipped;
	}

	vkCmdSetViewport(cmd->vk.handle, 0, 1, &viewport);
}

void vi_cmd_set_scissor(VICommand cmd, VkRect2D scissor)
{
	if (cmd->device->backend == VI_BACKEND_OPENGL)
	{
		GLCommand* glcmd = gl_append_command(cmd, GL_COMMAND_TYPE_SET_SCISSOR);
		glcmd->set_scissor = scissor;
		return;
	}

	vkCmdSetScissor(cmd->vk.handle, 0, 1, &scissor);
}

void vi_cmd_draw(VICommand cmd, const VIDrawInfo* info)
{
	if (cmd->device->backend == VI_BACKEND_OPENGL)
	{
		GLCommand* glcmd = gl_append_command(cmd, GL_COMMAND_TYPE_DRAW);
		glcmd->draw = *info;
		return;
	}

	vkCmdDraw(cmd->vk.handle, info->vertex_count, info->instance_count, info->vertex_start, info->instance_start);
}

void vi_cmd_draw_indexed(VICommand cmd, const VIDrawIndexedInfo* info)
{
	if (cmd->device->backend == VI_BACKEND_OPENGL)
	{
		GLCommand* glcmd = gl_append_command(cmd, GL_COMMAND_TYPE_DRAW_INDEXED);
		glcmd->draw_indexed = *info;
		return;
	}

	vkCmdDrawIndexed(cmd->vk.handle, info->index_count, info->instance_count, info->index_start, 0, info->instance_start);
}

void vi_cmd_pipeline_barrier_memory(VICommand cmd, VkPipelineStageFlags src_stages, VkPipelineStageFlags dst_stages,
	VkDependencyFlags deps, uint32_t barrier_count, const VIMemoryBarrier* barriers)
{
	if (cmd->device->backend == VI_BACKEND_OPENGL)
		return;

	std::vector<VkMemoryBarrier> vk_barriers(barrier_count);

	for (uint32_t i = 0; i < barrier_count; i++)
		cast_memory_barrier(barriers[i], vk_barriers.data() + i);

	vkCmdPipelineBarrier(cmd->vk.handle, src_stages, dst_stages, deps, vk_barriers.size(), vk_barriers.data(), 0, nullptr, 0, nullptr);
}

void vi_cmd_pipeline_barrier_image_memory(VICommand cmd, VkPipelineStageFlags src_stages, VkPipelineStageFlags dst_stages,
	VkDependencyFlags deps, uint32_t barrier_count, const VIImageMemoryBarrier* barriers)
{
	if (cmd->device->backend == VI_BACKEND_OPENGL)
		return;

	std::vector<VkImageMemoryBarrier> vk_barriers(barrier_count);

	for (uint32_t i = 0; i < barrier_count; i++)
		cast_image_memory_barrier(barriers[i], vk_barriers.data() + i);

	vkCmdPipelineBarrier(cmd->vk.handle, src_stages, dst_stages, deps, 0, nullptr, 0, nullptr, vk_barriers.size(), vk_barriers.data());
}

void vi_cmd_pipeline_barrier_buffer_memory(VICommand cmd, VkPipelineStageFlags src_stages, VkPipelineStageFlags dst_stages,
	VkDependencyFlags deps, uint32_t barrier_count, const VIBufferMemoryBarrier* barriers)
{
	if (cmd->device->backend == VI_BACKEND_OPENGL)
		return;

	std::vector<VkBufferMemoryBarrier> vk_barriers(barrier_count);

	for (uint32_t i = 0; i < barrier_count; i++)
		cast_buffer_memory_barrier(barriers[i], vk_barriers.data() + i);

	vkCmdPipelineBarrier(cmd->vk.handle, src_stages, dst_stages, deps, 0, nullptr, vk_barriers.size(), vk_barriers.data(), 0, nullptr);
}

char* vi_compile_binary(VIDevice device, VIModuleType type, VIPipelineLayout layout, const char* vise_glsl, uint32_t* binary_size)
{
	uint32_t set_layout_count = (uint32_t)layout->set_layouts.size();
	std::vector<VISetLayoutInfo> set_layouts(set_layout_count);

	uint32_t binding_ctr = 0;
	for (size_t i = 0; i < set_layout_count; i++)
		binding_ctr += (uint32_t)layout->set_layouts[i]->bindings.size();

	std::vector<VIBinding> set_bindings(binding_ctr);
	binding_ctr = 0;

	for (uint32_t i = 0; i < set_layout_count; i++)
	{
		const std::vector<VIBinding>& bindings = layout->set_layouts[i]->bindings;
		uint32_t binding_count = (uint32_t)bindings.size();

		set_layouts[i].binding_count = binding_count;
		set_layouts[i].bindings = set_bindings.data() + binding_ctr;

		for (uint32_t j = 0; j < binding_count; j++)
			set_bindings[binding_ctr++] = bindings[j];
	}

	VIPipelineLayoutData layout_data;
	layout_data.push_constant_size = layout->push_constant_size;
	layout_data.set_layout_count = set_layout_count;
	layout_data.set_layouts = set_layouts.data();
	return vi_compile_binary_offline(device->backend, type, &layout_data, vise_glsl, binary_size);
}

char* vi_compile_binary_offline(VIBackend backend, VIModuleType type, const VIPipelineLayoutData* layout_data, const char* vise_glsl, uint32_t* out_binary_size)
{
	char* payload_data;
	size_t payload_size;
	VICompileResult result;
	EShLanguage stage;
	cast_module_type_glslang(type, &stage);

	std::vector<char> spirv_bytes;
	uint32_t glpc_count = 0;
	uint32_t header_size = sizeof(VIBinaryHeader);

	if (backend == VI_BACKEND_OPENGL)
	{
		uint32_t set_count = layout_data->set_layout_count;
		std::vector<uint32_t> binding_counts(set_count);
		std::vector<const VIBinding*> set_bindings(set_count);

		for (uint32_t i = 0; i < set_count; i++)
		{
			const VISetLayoutInfo& set_layout = layout_data->set_layouts[i];
			binding_counts[i] = set_layout.binding_count;

			for (uint32_t j = 0; j < binding_counts[i]; j++)
				set_bindings[i] = set_layout.bindings;
		}

		std::vector<GLRemap> remaps;
		gl_remap(remaps, set_count, binding_counts.data(), set_bindings.data());
		compile_gl(result, stage, vise_glsl, (uint32_t)remaps.size(), remaps.data());

		glpc_count = (uint32_t)result.gl_push_constants.size();
		for (uint32_t i = 0; i < glpc_count; i++)
			header_size += (uint32_t)result.gl_push_constants[i].GetSerialSize();

		payload_size = result.gl_patched.size();
		payload_data = (char*)result.gl_patched.data();
	}
	else
	{
		compile_vk(result, stage, vise_glsl);
		spirv_bytes.resize(result.vk_spirv.size() * 4);
		uint8_t* now = (uint8_t*)spirv_bytes.data();
		for (const uint32_t& word : result.vk_spirv)
			swrite32(&now, word);

		payload_size = spirv_bytes.size();
		payload_data = spirv_bytes.data();
	}

	// serializtaion
	// - header consists of VIBinaryHeader fields and GLPushConstant entries
	// - binary payload is SPIRV for Vulkan or patched GLSL for OpenGL

	VIBinaryHeader header;
	header.backend_type = backend;
	header.module_type = type;
	header.glpc_count = glpc_count;
	header.header_size = header_size;
	header.payload_size = payload_size;

	uint32_t binary_size = header_size + payload_size;
	uint8_t* binary = (uint8_t*)vi_malloc(binary_size);
	uint8_t* now = binary;
	swrite_header(&now, header);

	for (const GLPushConstant& pc : result.gl_push_constants)
		swrite_glpc(&now, pc);

	swrite_bytes(&now, payload_size, payload_data);

	VI_ASSERT(now - binary == binary_size);

	if (out_binary_size)
		*out_binary_size = binary_size;

	return (char*)binary;
}

void vi_offline_free(void* data)
{
	vi_free(data);
}

VkInstance vi_device_unwrap_instance(VIDevice device)
{
	VI_ASSERT(device && device->backend == VI_BACKEND_VULKAN);

	return device->vk.instance;
}

VkDevice vi_device_unwrap(VIDevice device)
{
	VI_ASSERT(device && device->backend == VI_BACKEND_VULKAN);

	return device->vk.device;
}

VkPhysicalDevice vi_device_unwrap_physical(VIDevice device)
{
	VI_ASSERT(device && device->backend == VI_BACKEND_VULKAN);

	return device->vk.pdevice;
}

VkRenderPass vi_pass_unwrap(VIPass pass)
{
	VI_ASSERT(pass && pass->device->backend == VI_BACKEND_VULKAN);

	return pass->vk.handle;
}

VkSemaphore vi_semaphore_unwrap(VISemaphore semaphore)
{
	VI_ASSERT(semaphore && semaphore->device->backend == VI_BACKEND_VULKAN);

	return semaphore->vk_handle;
}

VkQueue vi_queue_unwrap(VIQueue queue)
{
	VI_ASSERT(queue && queue->device->backend == VI_BACKEND_VULKAN);

	return queue->vk_handle;
}

VkCommandBuffer vi_command_unwrap(VICommand command)
{
	VI_ASSERT(command && command->device->backend == VI_BACKEND_VULKAN);

	return command->vk.handle;
}

VkBuffer vi_buffer_unwrap(VIBuffer buffer)
{
	VI_ASSERT(buffer && buffer->device->backend == VI_BACKEND_VULKAN);

	return buffer->vk.handle;
}

VkImage vi_image_unwrap(VIImage image)
{
	VI_ASSERT(image && image->device->backend == VI_BACKEND_VULKAN);

	return image->vk.handle;
}

VkImageView vi_image_unwrap_view(VIImage image)
{
	VI_ASSERT(image && image->device->backend == VI_BACKEND_VULKAN);

	return image->vk.view_handle;
}

VkSampler vi_image_unwrap_sampler(VIImage image)
{
	VI_ASSERT(image && image->device->backend == VI_BACKEND_VULKAN);

	return image->vk.sampler_handle;
}

uint32_t vi_image_unwrap_gl(VIImage image)
{
	VI_ASSERT(image && image->device->backend == VI_BACKEND_OPENGL);

	return (uint32_t)image->gl.handle;
}