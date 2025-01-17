#include <array>
#include <random>
#include <cstring>
#include <cstdlib>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <vise.h>
#include "ExampleSSAO.h"
#include "../Application/Common.h"
#include "../Application/Model.h"

#define SSAO_SAMPLE_COUNT  64

#define SHOW_RESULT_COMPOSITION  0
#define SHOW_RESULT_POSITION     1
#define SHOW_RESULT_NORMALS      2
#define SHOW_RESULT_SSAO         3

static const char geometry_vm_glsl[] = R"(
#version 460

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aUV;

layout (location = 0) out vec3 vPos;
layout (location = 1) out vec3 vNormal;
layout (location = 2) out vec2 vUV;

layout (set = 0, binding = 0) uniform Scene
{
	mat4 view;
	mat4 proj;
} uScene;

layout (push_constant) uniform PC
{
	mat4 node_transform;
	uint normal_mapping;
} uPC;

void main()
{
	vec4 modelPos = uPC.node_transform * vec4(aPos, 1.0);
	mat4 normalMat = transpose(inverse(uScene.view * uPC.node_transform));
	vPos = vec3(uScene.view * modelPos); // store view space positions
	vNormal = vec3(normalMat * vec4(aNormal, 1.0)); // store view space normals
	vUV = aUV;

	gl_Position = uScene.proj * vec4(vPos, 1.0);
}
)";

static const char geometry_fm_glsl[] = R"(
#version 460

layout (location = 0) in vec3 vPos;
layout (location = 1) in vec3 vNormal;
layout (location = 2) in vec2 vUV;

layout (location = 0) out vec4 fPos;
layout (location = 1) out vec4 fNormal;
layout (location = 2) out vec4 fDiffuse;

// MATERIAL SET

layout (set = 1, binding = 0) uniform Mat
{
	uint HasColorMap;
	uint HasNormalMap;
	uint HasMetallicRoughnessMap;
	uint HasOcclusionMap;
	float MetallicFactor;
	float RoughnessFactor;
} uMat;

layout (set = 1, binding = 1) uniform sampler2D uMatColor;
layout (set = 1, binding = 2) uniform sampler2D uMatNormal;
layout (set = 1, binding = 3) uniform sampler2D uMatMR;

layout (push_constant) uniform PC
{
	mat4 node_transform;
	uint normal_mapping;
} uPC;

// http://www.thetenthplanet.de/archives/1180
vec3 getNormal()
{
	if (uPC.normal_mapping == 0 || uMat.HasNormalMap == 0)
		return normalize(vNormal);

	vec3 tangentNormal = texture(uMatNormal, vUV).xyz * 2.0 - 1.0;

	// normal mapping to view space
	vec3 q1 = dFdx(vPos);
	vec3 q2 = dFdy(vPos);
	vec2 st1 = dFdx(vUV);
	vec2 st2 = dFdy(vUV);

	vec3 N = normalize(vNormal);
	vec3 T = normalize(q1 * st2.t - q2 * st1.t);
	vec3 B = -normalize(cross(N, T));
	mat3 TBN = mat3(T, B, N);

	return normalize(TBN * tangentNormal);
}

void main()
{
	fPos = vec4(vPos, 1.0);
	fNormal = vec4(getNormal(), 1.0);
	fDiffuse = texture(uMatColor, vUV);
}
)";

static const char quad_vm_glsl[] = R"(
#version 460

layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexUV;
layout (location = 0) out vec2 vTexUV;

void main()
{
	gl_Position = vec4(aPos, 0.0, 1.0);
	vTexUV = aTexUV;
}
)";

static const char ssao_fm_glsl[] = R"(
#version 460

layout (location = 0) in vec2 vTexUV;
layout (location = 0) out float fOcclusion;

// SSAO SET

layout (set = 0, binding = 0) uniform Kernel
{
	vec4 samples[)" STR(SSAO_SAMPLE_COUNT) R"(];
} uKernel;

layout (set = 0, binding = 1) uniform sampler2D uPos;
layout (set = 0, binding = 2) uniform sampler2D uNormal;
layout (set = 0, binding = 3) uniform sampler2D uNoise;

layout (push_constant) uniform PC
{
	mat4 proj;
	uint sample_count; // less or equal to SSAO_SAMPLE_COUNT
	uint use_range_check;
	float depth_bias;
	float kernel_radius;
} uPC;

const vec2 noiseScale = vec2(
)" STR(APP_WINDOW_WIDTH / 4.0) R"(,
)" STR(APP_WINDOW_HEIGHT / 4.0) R"();

void main()
{
	vec3 pos = texture(uPos, vTexUV).xyz;
	vec3 normal = texture(uNormal, vTexUV).rgb;
	vec3 randomVec = texture(uNoise, vTexUV * noiseScale).xyz;

	vec3 tangent   = normalize(randomVec - normal * dot(randomVec, normal));
	vec3 bitangent = cross(normal, tangent);
	mat3 TBN       = mat3(tangent, bitangent, normal);

	float occlusion = 0.0;

	for (uint i = 0; i < uPC.sample_count; i++)
	{
		vec3 samplePos = TBN * uKernel.samples[i].xyz;
		samplePos = pos + samplePos * uPC.kernel_radius;

		vec4 sampleUV = uPC.proj * vec4(samplePos, 1.0);
		sampleUV.xyz /= sampleUV.w;
		sampleUV.xyz = sampleUV.xyz * 0.5 + 0.5;
		float sampleDepth = texture(uPos, sampleUV.xy).z;

		float test = sampleDepth >= samplePos.z + uPC.depth_bias ? 1.0 : 0.0;

		if (bool(uPC.use_range_check))
		{
			float rangeCheck = smoothstep(0.0, 1.0, uPC.kernel_radius / abs(pos.z - sampleDepth));
			test *= rangeCheck;
		}

		occlusion += test;
	}

	fOcclusion = 1.0 - occlusion / float(uPC.sample_count);
}
)";

static const char ssao_blur_fm_glsl[] = R"(
#version 460

layout (location = 0) in vec2 vTexUV;
layout (location = 0) out float fBlur;

layout (set = 0, binding = 0) uniform sampler2D uSSAO;

layout (push_constant) uniform PC
{
	uint blur_ssao;
} uPC;

void main()
{
	if (bool(uPC.blur_ssao))
	{
		vec2 texelSize = 1.0 / vec2(textureSize(uSSAO, 0));
		float result = 0.0;
		for (int x = -2; x < 2; ++x) 
		{
			for (int y = -2; y < 2; ++y) 
			{
				vec2 offset = vec2(float(x), float(y)) * texelSize;
				result += texture(uSSAO, vTexUV + offset).r;
			}
		}
		fBlur = result / (4.0 * 4.0);
	}
	else
	{
		fBlur = texture(uSSAO, vTexUV).r;
	}
}
)";

static const char composition_fm_glsl[] = R"(
#version 460

layout (location = 0) in vec2 vTexUV;
layout (location = 0) out vec4 fColor;

// GBUFFER SET

layout (set = 0, binding = 0) uniform sampler2D uPos;
layout (set = 0, binding = 1) uniform sampler2D uNormal;
layout (set = 0, binding = 2) uniform sampler2D uDiffuse;
layout (set = 0, binding = 3) uniform sampler2D uSSAO;

layout (push_constant) uniform PC
{
	uint show_result;
	uint use_ssao;
} uPC;

void main()
{
	vec3 pos = texture(uPos, vTexUV).rgb;
	vec3 normal = normalize(texture(uNormal, vTexUV).rgb);
	vec4 diffuse = texture(uDiffuse, vTexUV);
	float occlusion = texture(uSSAO, vTexUV).r;

	if (uPC.use_ssao == 0)
		occlusion = 1.0;
	
	if (uPC.show_result == )" STR(SHOW_RESULT_POSITION) R"()
		fColor = vec4(pos, 1.0);
	else if (uPC.show_result == )" STR(SHOW_RESULT_NORMALS) R"()
		fColor = vec4(normal, 1.0);
	else if (uPC.show_result == )" STR(SHOW_RESULT_SSAO) R"()
		fColor = vec4(vec3(occlusion), 1.0);
	else
	{
		vec3 light_dir = normalize(vec3(0.2, 1.0, 0.2));
		float diffuse_factor = max(dot(light_dir, normal), 0.3);
		diffuse_factor *= occlusion;
		fColor = vec4(diffuse_factor * diffuse.rgb, diffuse.a);
	}
}
)";

struct SceneUBO
{
	glm::mat4 ViewMat;
	glm::mat4 ProjMat;
};

ExampleSSAO::ExampleSSAO(VIBackend backend)
	: Application("Screen Space Ambient Occlusion", backend)
{
	glfwSetKeyCallback(mWindow, &ExampleSSAO::KeyCallback);

	// geometry pass, generates a gbuffer
	{
		std::array<VIPassColorAttachment, 3> gbufferColorAttachments;
		gbufferColorAttachments[0] = MakePassColorAttachment(VI_FORMAT_RGBA16F, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		gbufferColorAttachments[1] = MakePassColorAttachment(VI_FORMAT_RGBA16F, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		gbufferColorAttachments[2] = MakePassColorAttachment(VI_FORMAT_RGBA8, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

		VIPassDepthStencilAttachment gbufferDepthAttachment = MakePassDepthAttachment(VI_FORMAT_D32F, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

		VkSubpassDependency subpassD = MakeSubpassDependency(0, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			VK_SUBPASS_EXTERNAL, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT);

		std::array<VISubpassColorAttachment, 3> subpass_color_refs;
		subpass_color_refs[0].index = 0;
		subpass_color_refs[0].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		subpass_color_refs[1].index = 1;
		subpass_color_refs[1].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		subpass_color_refs[2].index = 2;
		subpass_color_refs[2].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VISubpassDepthStencilAttachment subpass_depth_ref;
		subpass_depth_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VISubpassInfo subpassI;
		subpassI.color_attachment_ref_count = subpass_color_refs.size();
		subpassI.color_attachment_refs = subpass_color_refs.data();
		subpassI.depth_stencil_attachment_ref = &subpass_depth_ref;

		subpassD = MakeSubpassDependency(0, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			VK_SUBPASS_EXTERNAL, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT);

		VIPassInfo passI;
		passI.subpass_count = 1;
		passI.subpasses = &subpassI;
		passI.color_attachment_count = gbufferColorAttachments.size();
		passI.color_attachments = gbufferColorAttachments.data();
		passI.depth_stencil_attachment = &gbufferDepthAttachment;
		passI.depenency_count = 1;
		passI.dependencies = &subpassD;

		mGeometryPass = vi_create_pass(mDevice, &passI);
	}

	// both SSAO and Blur pass fall into this category
	{
		VIPassColorAttachment colorR8 = MakePassColorAttachment(VI_FORMAT_R8, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

		VISubpassColorAttachment colorRef;
		colorRef.index = 0;
		colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VISubpassInfo subpassI;
		subpassI.color_attachment_ref_count = 1;
		subpassI.color_attachment_refs = &colorRef;
		subpassI.depth_stencil_attachment_ref = nullptr;

		VkSubpassDependency subpassD = MakeSubpassDependency(0, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			VK_SUBPASS_EXTERNAL, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT);

		VIPassInfo passI;
		passI.subpass_count = 1;
		passI.subpasses = &subpassI;
		passI.color_attachment_count = 1;
		passI.color_attachments = &colorR8;
		passI.depth_stencil_attachment = nullptr;
		passI.depenency_count = 1;
		passI.dependencies = &subpassD;
		mColorR8Pass = vi_create_pass(mDevice, &passI);
	}

	mSetLayoutUCCC = CreateSetLayout(mDevice, {
		{ VI_BINDING_TYPE_UNIFORM_BUFFER,         0, 1 },
		{ VI_BINDING_TYPE_COMBINED_IMAGE_SAMPLER, 1, 1 },
		{ VI_BINDING_TYPE_COMBINED_IMAGE_SAMPLER, 2, 1 },
		{ VI_BINDING_TYPE_COMBINED_IMAGE_SAMPLER, 3, 1 },
	});

	mSetLayoutCCCC = CreateSetLayout(mDevice, {
		{ VI_BINDING_TYPE_COMBINED_IMAGE_SAMPLER, 0, 1 },
		{ VI_BINDING_TYPE_COMBINED_IMAGE_SAMPLER, 1, 1 },
		{ VI_BINDING_TYPE_COMBINED_IMAGE_SAMPLER, 2, 1 },
		{ VI_BINDING_TYPE_COMBINED_IMAGE_SAMPLER, 3, 1 },
	});

	uint32_t set_count = mFramesInFlight * 4;
	mSetPool = CreateSetPool(mDevice, set_count, {
		{ VI_BINDING_TYPE_UNIFORM_BUFFER, set_count },
		{ VI_BINDING_TYPE_COMBINED_IMAGE_SAMPLER, 4 * set_count },
	});

	std::array<VISetLayout, 2> setLayouts = { mSetLayoutUCCC, mSetLayoutUCCC };

	VIPipelineLayoutInfo pipelineLI;
	pipelineLI.push_constant_size = 128;
	pipelineLI.set_layout_count = setLayouts.size();
	pipelineLI.set_layouts = setLayouts.data();
	mPipelineLayoutUCCC2 = vi_create_pipeline_layout(mDevice, &pipelineLI);

	pipelineLI.set_layout_count = 1;
	pipelineLI.set_layouts = &mSetLayoutCCCC;
	mPipelineLayoutCCCC = vi_create_pipeline_layout(mDevice, &pipelineLI);

	mGeometryVM = CreateOrLoadModule(mDevice, mBackend, mPipelineLayoutUCCC2, VI_MODULE_TYPE_VERTEX, geometry_vm_glsl, "geometry_vm");
	mGeometryFM = CreateOrLoadModule(mDevice, mBackend, mPipelineLayoutUCCC2, VI_MODULE_TYPE_FRAGMENT, geometry_fm_glsl, "geometry_fm");
	mQuadVM = CreateOrLoadModule(mDevice, mBackend, mPipelineLayoutUCCC2, VI_MODULE_TYPE_VERTEX, quad_vm_glsl, "quad_vm");
	mSSAOFM = CreateOrLoadModule(mDevice, mBackend, mPipelineLayoutUCCC2, VI_MODULE_TYPE_FRAGMENT, ssao_fm_glsl, "ssao_fm");
	mSSAOBlurFM = CreateOrLoadModule(mDevice, mBackend, mPipelineLayoutCCCC, VI_MODULE_TYPE_FRAGMENT, ssao_blur_fm_glsl, "ssao_blur_fm");
	mCompositionFM = CreateOrLoadModule(mDevice, mBackend, mPipelineLayoutCCCC, VI_MODULE_TYPE_FRAGMENT, composition_fm_glsl, "composition_fm");

	VIVertexBinding meshVertexBinding;
	std::vector<VIVertexAttribute> meshVertexAttrs;
	MeshVertex::GetBindingAndAttributes(meshVertexBinding, meshVertexAttrs);

	std::array<VIModule, 2> modules;
	modules[0] = mGeometryVM;
	modules[1] = mGeometryFM;

	VIPipelineInfo pipelineI;
	pipelineI.layout = mPipelineLayoutUCCC2;
	pipelineI.pass = mGeometryPass;
	pipelineI.vertex_attribute_count = meshVertexAttrs.size();
	pipelineI.vertex_attributes = meshVertexAttrs.data();
	pipelineI.vertex_binding_count = 1;
	pipelineI.vertex_bindings = &meshVertexBinding;
	pipelineI.module_count = modules.size();
	pipelineI.modules = modules.data();
	mGeometryPipeline = vi_create_pipeline(mDevice, &pipelineI);

	VIVertexBinding quadVertexBinding;
	quadVertexBinding.rate = VK_VERTEX_INPUT_RATE_VERTEX;
	quadVertexBinding.stride = sizeof(float) * 4;

	std::array<VIVertexAttribute, 2> quadVertexAttrs;
	quadVertexAttrs[0].type = VI_GLSL_TYPE_VEC2; // NDC position
	quadVertexAttrs[0].binding = 0;
	quadVertexAttrs[0].offset = 0;
	quadVertexAttrs[1].type = VI_GLSL_TYPE_VEC2; // Texture UV
	quadVertexAttrs[1].binding = 0;
	quadVertexAttrs[1].offset = sizeof(float) * 2;


	pipelineI.vertex_attribute_count = quadVertexAttrs.size();
	pipelineI.vertex_attributes = quadVertexAttrs.data();
	pipelineI.vertex_binding_count = 1;
	pipelineI.vertex_bindings = &quadVertexBinding;
	modules[0] = mQuadVM;

	pipelineI.pass = mColorR8Pass;
	pipelineI.layout = mPipelineLayoutUCCC2;
	modules[1] = mSSAOFM;
	mSSAOPipeline = vi_create_pipeline(mDevice, &pipelineI);

	pipelineI.layout = mPipelineLayoutCCCC;
	modules[1] = mSSAOBlurFM;
	mSSAOBlurPipeline = vi_create_pipeline(mDevice, &pipelineI);

	pipelineI.pass = vi_device_get_swapchain_pass(mDevice);
	pipelineI.layout = mPipelineLayoutCCCC;
	modules[1] = mCompositionFM;
	mCompositionPipeline = vi_create_pipeline(mDevice, &pipelineI);

	uint32_t graphics_family = vi_device_get_graphics_family_index(mDevice);
	mCmdPool = vi_create_command_pool(mDevice, graphics_family, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

	float quadVertices[] = {
		 -1.0f,  1.0f, 0.0f, 1.0f, // top left
		 -1.0f, -1.0f, 0.0f, 0.0f, // bottom left
		  1.0f, -1.0f, 1.0f, 0.0f, // bottom right
		  1.0f, -1.0f, 1.0f, 0.0f, // bottom right
		  1.0f,  1.0f, 1.0f, 1.0f, // top right
		 -1.0f,  1.0f, 0.0f, 1.0f, // top left
	};
	VIBufferInfo vboI;
	vboI.type = VI_BUFFER_TYPE_VERTEX;
	vboI.usage = VI_BUFFER_USAGE_TRANSFER_DST_BIT;
	vboI.properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
	vboI.size = sizeof(quadVertices);
	mQuadVBO = CreateBufferStaged(mDevice, &vboI, quadVertices);

	std::uniform_real_distribution<float> randomFloats(0.0, 1.0);
	std::default_random_engine generator;

	// Generate SSAO Noise Texture
	// used to rotate kernel samples

	const uint32_t noise_dim = 4;
	VIImageInfo imageI = MakeImageInfo2D(VI_FORMAT_RGBA32F, noise_dim, noise_dim, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	imageI.usage = VI_IMAGE_USAGE_SAMPLED_BIT | VI_IMAGE_USAGE_TRANSFER_DST_BIT;
	std::vector<glm::vec2> noise(noise_dim * noise_dim);
	for (size_t i = 0; i < noise.size(); i++)
	{
		noise[i] = glm::vec2(randomFloats(generator) * 2.0 - 1.0, randomFloats(generator) * 2.0 - 1.0);
	}
	mNoise = CreateImageStaged(mDevice, &imageI, noise.data(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	// Generate SSAO Kernel Samples

	std::vector<glm::vec4> samples(SSAO_SAMPLE_COUNT);
	for (size_t i = 0; i < samples.size(); i++)
	{
		glm::vec3 sample(randomFloats(generator) * 2.0 - 1.0, randomFloats(generator) * 2.0 - 1.0, randomFloats(generator));
		sample = glm::normalize(sample);
		sample *= randomFloats(generator);

		float scale = (float)i / SSAO_SAMPLE_COUNT;
		scale = glm::mix(0.1f, 1.0f, scale * scale);
		samples[i] = glm::vec4(sample * scale, 0.0f);
	}

	VIBufferInfo uboI;
	uboI.type = VI_BUFFER_TYPE_UNIFORM;
	uboI.usage = VI_BUFFER_USAGE_TRANSFER_DST_BIT;
	uboI.properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
	uboI.size = samples.size() * sizeof(glm::vec4);
	mKernelUBO = CreateBufferStaged(mDevice, &uboI, samples.data());

	mFrames.resize(mFramesInFlight);
	for (size_t i = 0; i < mFramesInFlight; i++)
	{
		mFrames[i].cmd = vi_allocate_primary_command(mDevice, mCmdPool);

		imageI = MakeImageInfo2D(VI_FORMAT_R8, APP_WINDOW_WIDTH, APP_WINDOW_HEIGHT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		imageI.usage = VI_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VI_IMAGE_USAGE_SAMPLED_BIT;
		mFrames[i].ssao = vi_create_image(mDevice, &imageI);
		mFrames[i].ssao_blur = vi_create_image(mDevice, &imageI);
		imageI.format = VI_FORMAT_RGBA16F;
		mFrames[i].gbuffer_positions = vi_create_image(mDevice, &imageI);
		mFrames[i].gbuffer_normals = vi_create_image(mDevice, &imageI);
		imageI.format = VI_FORMAT_RGBA8;
		mFrames[i].gbuffer_diffuse = vi_create_image(mDevice, &imageI);
		imageI.usage = VI_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		imageI.format = VI_FORMAT_D32F;
		mFrames[i].gbuffer_depth = vi_create_image(mDevice, &imageI);

		std::array<VIImage, 3> color_attachments;
		color_attachments[0] = mFrames[i].gbuffer_positions;
		color_attachments[1] = mFrames[i].gbuffer_normals;
		color_attachments[2] = mFrames[i].gbuffer_diffuse;

		VIBufferInfo bufferI;
		bufferI.type = VI_BUFFER_TYPE_UNIFORM;
		bufferI.size = sizeof(SceneUBO);
		bufferI.properties = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
		mFrames[i].ubo = vi_create_buffer(mDevice, &bufferI);
		vi_buffer_map(mFrames[i].ubo);

		VIFramebufferInfo fbI;
		fbI.pass = mGeometryPass;
		fbI.width = APP_WINDOW_WIDTH;
		fbI.height = APP_WINDOW_HEIGHT;
		fbI.color_attachment_count = color_attachments.size();
		fbI.color_attachments = color_attachments.data();
		fbI.depth_stencil_attachment = mFrames[i].gbuffer_depth;
		mFrames[i].gbuffer = vi_create_framebuffer(mDevice, &fbI);

		fbI.pass = mColorR8Pass;
		fbI.color_attachment_count = 1;
		fbI.color_attachments = &mFrames[i].ssao;
		fbI.depth_stencil_attachment = nullptr;
		mFrames[i].ssao_fbo = vi_create_framebuffer(mDevice, &fbI);

		fbI.color_attachments = &mFrames[i].ssao_blur;
		mFrames[i].ssao_blur_fbo = vi_create_framebuffer(mDevice, &fbI);

		mFrames[i].gbuffer_set = AllocAndUpdateSet(mDevice, mSetPool, mSetLayoutUCCC, {
			{ 0, mFrames[i].ubo, VI_NULL },
			{ 1, VI_NULL, mFrames[i].gbuffer_positions },
			{ 2, VI_NULL, mFrames[i].gbuffer_normals },
			{ 3, VI_NULL, mFrames[i].gbuffer_diffuse },
			});
		mFrames[i].ssao_set = AllocAndUpdateSet(mDevice, mSetPool, mSetLayoutUCCC, {
			{ 0, mKernelUBO, VI_NULL },
			{ 1, VI_NULL, mFrames[i].gbuffer_positions },
			{ 2, VI_NULL, mFrames[i].gbuffer_normals },
			{ 3, VI_NULL, mNoise },
			});
		mFrames[i].ssao_blur_set = AllocAndUpdateSet(mDevice, mSetPool, mSetLayoutCCCC, {
			{ 0, VI_NULL, mFrames[i].ssao },
			});
		mFrames[i].composition_set = AllocAndUpdateSet(mDevice, mSetPool, mSetLayoutCCCC, {
			{ 0, VI_NULL, mFrames[i].gbuffer_positions },
			{ 1, VI_NULL, mFrames[i].gbuffer_normals },
			{ 2, VI_NULL, mFrames[i].gbuffer_diffuse },
			{ 3, VI_NULL, mFrames[i].ssao_blur },
			});
	}
}

ExampleSSAO::~ExampleSSAO()
{
	vi_device_wait_idle(mDevice);

	for (size_t i = 0; i < mFrames.size(); i++)
	{
		vi_free_command(mDevice, mFrames[i].cmd);
		vi_buffer_unmap(mFrames[i].ubo);
		vi_destroy_buffer(mDevice, mFrames[i].ubo);
		vi_free_set(mDevice, mFrames[i].ssao_set);
		vi_free_set(mDevice, mFrames[i].ssao_blur_set);
		vi_free_set(mDevice, mFrames[i].gbuffer_set);
		vi_free_set(mDevice, mFrames[i].composition_set);
		vi_destroy_framebuffer(mDevice, mFrames[i].ssao_fbo);
		vi_destroy_framebuffer(mDevice, mFrames[i].ssao_blur_fbo);
		vi_destroy_framebuffer(mDevice, mFrames[i].gbuffer);
		vi_destroy_image(mDevice, mFrames[i].ssao);
		vi_destroy_image(mDevice, mFrames[i].ssao_blur);
		vi_destroy_image(mDevice, mFrames[i].gbuffer_depth);
		vi_destroy_image(mDevice, mFrames[i].gbuffer_diffuse);
		vi_destroy_image(mDevice, mFrames[i].gbuffer_normals);
		vi_destroy_image(mDevice, mFrames[i].gbuffer_positions);
	}

	vi_destroy_command_pool(mDevice, mCmdPool);
	vi_destroy_set_pool(mDevice, mSetPool);
	vi_destroy_set_layout(mDevice, mSetLayoutUCCC);
	vi_destroy_set_layout(mDevice, mSetLayoutCCCC);
	vi_destroy_pipeline(mDevice, mSSAOBlurPipeline);
	vi_destroy_pipeline(mDevice, mSSAOPipeline);
	vi_destroy_pipeline(mDevice, mGeometryPipeline);
	vi_destroy_pipeline(mDevice, mCompositionPipeline);
	vi_destroy_module(mDevice, mGeometryFM);
	vi_destroy_module(mDevice, mGeometryVM);
	vi_destroy_module(mDevice, mCompositionFM);
	vi_destroy_module(mDevice, mSSAOBlurFM);
	vi_destroy_module(mDevice, mSSAOFM);
	vi_destroy_module(mDevice, mQuadVM);
	vi_destroy_pass(mDevice, mColorR8Pass);
	vi_destroy_pass(mDevice, mGeometryPass);
	vi_destroy_image(mDevice, mNoise);
	vi_destroy_buffer(mDevice, mKernelUBO);
	vi_destroy_buffer(mDevice, mQuadVBO);
	vi_destroy_pipeline_layout(mDevice, mPipelineLayoutUCCC2);
	vi_destroy_pipeline_layout(mDevice, mPipelineLayoutCCCC);
}

void ExampleSSAO::Run()
{
	mSceneModel = GLTFModel::LoadFromFile(APP_PATH "../../Assets/gltf/Sponza/glTF/Sponza.gltf", mDevice, mSetLayoutUCCC);
	mCamera.SetPosition({ 0.0f, 1.0f, 0.0f });
	mConfig.show_result = 0;
	mConfig.ssao_sample_count = SSAO_SAMPLE_COUNT / 2;
	mConfig.ssao_use_range_check = true;
	mConfig.ssao_depth_bias = 0.025f;
	mConfig.ssao_kernel_radius = 0.1f;
	mConfig.blur_ssao = true;
	mConfig.use_ssao = true;
	mConfig.use_normal_map = true;

	while (!glfwWindowShouldClose(mWindow))
	{
		Application::NewFrame();
		Application::ImGuiNewFrame();
		Application::CameraUpdate();

		VISemaphore image_acquired;
		VISemaphore present_ready;
		VIFence frame_complete;

		uint32_t index = vi_device_next_frame(mDevice, &image_acquired, &present_ready, &frame_complete);
		FrameData* frame = mFrames.data() + index;
		VICommand cmd = frame->cmd;

		if (!CameraIsCaptured())
		{
			ImGui::Begin(mName);
			ImGui::Text("Delta Time %.4f (%d FPS)", mFrameTimeDelta, static_cast<int>(1.0f / mFrameTimeDelta));
			if (ImGui::Button("Show Final Composition"))
				mConfig.show_result = SHOW_RESULT_COMPOSITION;
			if (ImGui::Button("Show GBuffer View Space Positions"))
				mConfig.show_result = SHOW_RESULT_POSITION;
			if (ImGui::Button("Show GBuffer View Space Normals"))
				mConfig.show_result = SHOW_RESULT_NORMALS;
			if (ImGui::Button("Show SSAO"))
				mConfig.show_result = SHOW_RESULT_SSAO;
			ImGui::Checkbox("Blur SSAO Result", &mConfig.blur_ssao);
			ImGui::Checkbox("Use SSAO Result in Composition", &mConfig.use_ssao);
			ImGui::Checkbox("Use Normal Map", &mConfig.use_normal_map);
			ImGui::Checkbox("SSAO Use Range Check", &mConfig.ssao_use_range_check);
			ImGui::SliderInt("SSAO Sample Count", &mConfig.ssao_sample_count, 1, SSAO_SAMPLE_COUNT);
			ImGui::SliderFloat("SSAO Kernel Radius", &mConfig.ssao_kernel_radius, 0.1f, 1.0f);
			ImGui::SliderFloat("SSAO Depth Bias", &mConfig.ssao_depth_bias, 0.01f, 0.05f);
			ImGui::End();
		}

		SceneUBO ubo;
		ubo.ViewMat = mCamera.GetViewMat();
		ubo.ProjMat = mCamera.GetProjMat();
		vi_buffer_map_write(frame->ubo, 0, sizeof(ubo), &ubo);

		vi_begin_command(cmd, 0);

		std::array<VkClearValue, 3> colorClearValues;
		colorClearValues[0] = MakeClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		colorClearValues[1] = MakeClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		colorClearValues[2] = MakeClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		VkClearValue dpethClearValue = MakeClearDepthStencil(1.0f, 0);

		// Geometry Pass

		VIPassBeginInfo passBI;
		passBI.pass = mGeometryPass;
		passBI.framebuffer = frame->gbuffer;
		passBI.color_clear_value_count = colorClearValues.size();
		passBI.color_clear_values = colorClearValues.data();
		passBI.depth_stencil_clear_value = &dpethClearValue;
		vi_cmd_begin_pass(cmd, &passBI);
		{
			vi_cmd_bind_graphics_pipeline(cmd, mGeometryPipeline);
			vi_cmd_set_viewport(cmd, MakeViewport(APP_WINDOW_WIDTH, APP_WINDOW_HEIGHT));
			vi_cmd_set_scissor(cmd, MakeScissor(APP_WINDOW_WIDTH, APP_WINDOW_HEIGHT));

			vi_cmd_bind_graphics_set(cmd, mPipelineLayoutUCCC2, 0, frame->gbuffer_set);

			uint32_t use_normal_map = (uint32_t)mConfig.use_normal_map;
			vi_cmd_push_constants(cmd, mPipelineLayoutUCCC2, sizeof(glm::mat4), sizeof(use_normal_map), &use_normal_map);

			uint32_t materialSetIndex = 1;
			mSceneModel->Draw(cmd, mPipelineLayoutUCCC2, materialSetIndex);
		}
		vi_cmd_end_pass(cmd);

		// SSAO Pass

		VkClearValue color_clear = MakeClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		passBI.pass = mColorR8Pass;
		passBI.framebuffer = frame->ssao_fbo;
		passBI.color_clear_value_count = 1;
		passBI.color_clear_values = &color_clear;
		passBI.depth_stencil_clear_value = nullptr;
		vi_cmd_begin_pass(cmd, &passBI);
		{
			vi_cmd_bind_graphics_pipeline(cmd, mSSAOPipeline);
			vi_cmd_set_viewport(cmd, MakeViewport(APP_WINDOW_WIDTH, APP_WINDOW_HEIGHT));
			vi_cmd_set_scissor(cmd, MakeScissor(APP_WINDOW_WIDTH, APP_WINDOW_HEIGHT));

			vi_cmd_bind_graphics_set(cmd, mPipelineLayoutUCCC2, 0, frame->ssao_set);

			vi_cmd_bind_vertex_buffers(cmd, 0, 1, &mQuadVBO);

			struct SSAOPushConstant
			{
				glm::mat4 proj;
				uint32_t sample_count;
				uint32_t use_range_check;
				float depth_bias;
				float kernel_radius;
			} pc;
			pc.proj = mCamera.GetProjMat();
			pc.sample_count = (uint32_t)mConfig.ssao_sample_count;;
			pc.use_range_check = (uint32_t)mConfig.ssao_use_range_check;
			pc.depth_bias = mConfig.ssao_depth_bias;
			pc.kernel_radius = mConfig.ssao_kernel_radius;
			vi_cmd_push_constants(cmd, mPipelineLayoutUCCC2, 0, sizeof(pc), &pc);

			VIDrawInfo drawI;
			drawI.vertex_start = 0;
			drawI.vertex_count = 6;
			drawI.instance_start = 0;
			drawI.instance_count = 1;
			vi_cmd_draw(cmd, &drawI);
		}
		vi_cmd_end_pass(cmd);

		// SSAO Blur Pass

		passBI.framebuffer = frame->ssao_blur_fbo;
		vi_cmd_begin_pass(cmd, &passBI);
		{
			vi_cmd_bind_graphics_pipeline(cmd, mSSAOBlurPipeline);
			vi_cmd_set_viewport(cmd, MakeViewport(APP_WINDOW_WIDTH, APP_WINDOW_HEIGHT));
			vi_cmd_set_scissor(cmd, MakeScissor(APP_WINDOW_WIDTH, APP_WINDOW_HEIGHT));

			vi_cmd_bind_graphics_set(cmd, mPipelineLayoutCCCC, 0, frame->ssao_blur_set);

			vi_cmd_bind_vertex_buffers(cmd, 0, 1, &mQuadVBO);

			uint32_t pc = (uint32_t)mConfig.blur_ssao;
			vi_cmd_push_constants(cmd, mPipelineLayoutCCCC, 0, sizeof(pc), &pc);

			VIDrawInfo drawI;
			drawI.vertex_start = 0;
			drawI.vertex_count = 6;
			drawI.instance_start = 0;
			drawI.instance_count = 1;
			vi_cmd_draw(cmd, &drawI);
		}
		vi_cmd_end_pass(cmd);

		// Composition Pass

		VkClearValue swapchain_clear_color = MakeClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		VkClearValue swapchain_clear_depth = MakeClearDepthStencil(1.0f, 0);
		passBI.pass = vi_device_get_swapchain_pass(mDevice);
		passBI.framebuffer = vi_device_get_swapchain_framebuffer(mDevice, index);
		passBI.color_clear_value_count = 1;
		passBI.color_clear_values = &swapchain_clear_color;
		passBI.depth_stencil_clear_value = &swapchain_clear_depth;
		vi_cmd_begin_pass(cmd, &passBI);
		{
			// use the swapchain pass as composition pass
			vi_cmd_bind_graphics_pipeline(cmd, mCompositionPipeline);
			vi_cmd_set_viewport(cmd, MakeViewport(APP_WINDOW_WIDTH, APP_WINDOW_HEIGHT));
			vi_cmd_set_scissor(cmd, MakeScissor(APP_WINDOW_WIDTH, APP_WINDOW_HEIGHT));

			vi_cmd_bind_graphics_set(cmd, mPipelineLayoutCCCC, 0, frame->composition_set);
			vi_cmd_bind_vertex_buffers(cmd, 0, 1, &mQuadVBO);
			
			struct CompositionPushConstant
			{
				uint32_t show_result;
				uint32_t use_ssao;
			} pc;
			pc.show_result = mConfig.show_result;
			pc.use_ssao = mConfig.use_ssao;
			vi_cmd_push_constants(cmd, mPipelineLayoutCCCC, 0, sizeof(pc), &pc);

			VIDrawInfo drawI;
			drawI.vertex_start = 0;
			drawI.vertex_count = 6;
			drawI.instance_start = 0;
			drawI.instance_count = 1;
			vi_cmd_draw(cmd, &drawI);

			Application::ImGuiRender(cmd);
		}
		vi_cmd_end_pass(cmd);
		vi_end_command(cmd);

		VkPipelineStageFlags stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		VISubmitInfo submitI;
		submitI.wait_count = 1;
		submitI.wait_stages = &stage;
		submitI.waits = &image_acquired;
		submitI.signal_count = 1;
		submitI.signals = &present_ready;
		submitI.cmd_count = 1;
		submitI.cmds = &cmd;

		VIQueue graphics_queue = vi_device_get_graphics_queue(mDevice);
		vi_queue_submit(graphics_queue, 1, &submitI, frame_complete);

		vi_device_present_frame(mDevice);
	}

	vi_device_wait_idle(mDevice);
	mSceneModel = nullptr;
}

void ExampleSSAO::KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	ExampleSSAO* example = (ExampleSSAO*)Application::Get();

	if (action != GLFW_PRESS)
		return;

	switch (key)
	{
	case GLFW_KEY_ESCAPE:
		example->CameraToggleCapture();
		break;
	}
}
