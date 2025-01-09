#include <array>
#include <vector>
#include <imgui.h>
#include <glm/mat4x4.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vise.h>
#include "ExamplePBR.h"
#include "../Application/Common.h"
#include "../../Extern/stb/stb_image.h"

#define CUBEMAP_SIZE            1024
#define BRDFLUT_SIZE            512
#define BRDFLUT_SAMPLE_COUNT    1024
#define IRRADIANCE_SIZE         64
#define PREFILTER_BASE_SIZE     128
#define PREFILTER_MIP_LEVELS    6
#define PREFILTER_SAMPLE_COUNT  4096

#define SHOW_FINAL_RESULT       0
#define SHOW_CHANNEL_ALBEDO     1
#define SHOW_CHANNEL_METALLIC   2
#define SHOW_CHANNEL_ROUGHNESS  3

// push constant declaration should remain the same
// throughout different stages within the same pipeline
#define GLSL_CUBEMAP_PUSH_CONSTANT R"(
layout (push_constant) uniform uPC
{
	mat4 mvp;
	float delta_phi;
	float delta_theta;
	float roughness;
	uint sample_count;
} PC;)"


#define GLSL_HAMMERSLEY R"(
vec2 hammersley(uint i, uint N)
{
	// radical_inverse_vdc
    uint bits = (i << 16u) | (i >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
	float rdi = float(bits) * 2.3283064365386963e-10;
	return vec2(float(i) / float(N), rdi);
}
)"

#define GLSL_IMPORTANCE_SAMPLE_GGX R"(
vec3 importance_sample_GGX(vec2 Xi, vec3 N, float roughness)
{
	float a = roughness * roughness;
	
	float phi = 2.0 * PI * Xi.x;
	float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a*a - 1.0) * Xi.y));
	float sinTheta = sqrt(1.0 - cosTheta*cosTheta);
	
	// from spherical coordinates to cartesian coordinates
	vec3 H;
	H.x = cos(phi) * sinTheta;
	H.y = sin(phi) * sinTheta;
	H.z = cosTheta;
	
	// from tangent-space vector to world-space sample vector
	vec3 up        = abs(N.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
	vec3 tangent   = normalize(cross(up, N));
	vec3 bitangent = cross(N, tangent);
	
	vec3 sampleVec = tangent * H.x + bitangent * H.y + N * H.z;
	return normalize(sampleVec);
}
)"

static char skybox_vertex_glsl[] = R"(
#version 460
layout (location = 0) in vec3 aPos;
layout (location = 0) out vec3 vPos;

layout (push_constant) uniform uPC
{
	mat4 mvp;
	float prefilter_roughness; // unused
} PC;

void main()
{
	vPos = aPos;
	gl_Position = PC.mvp * vec4(aPos, 1.0f);
}
)";

static char skybox_fragment_glsl[] = R"(
#version 460
layout (location = 0) in vec3 vPos;
layout (location = 0) out vec4 fColor;

layout (push_constant) uniform uPC
{
	mat4 mvp;
	float prefilter_roughness;
} PC;

layout (set = 0, binding = 0) uniform samplerCube uCubemap;

void main()
{
	vec3 hdrColor;

	if (PC.prefilter_roughness > 0.0f)
		hdrColor = textureLod(uCubemap, vPos, PC.prefilter_roughness).rgb;
	else
		hdrColor = texture(uCubemap, vPos).rgb;

	vec3 ldrColor = hdrColor / (hdrColor + vec3(1.0));
	fColor = vec4(ldrColor, 1.0);
}
)";

static const char cubemap_face_vertex_glsl[] = R"(
#version 460

layout (location = 0) in vec3 aPos;
layout (location = 0) out vec3 vPos;
)"
GLSL_CUBEMAP_PUSH_CONSTANT
R"(
void main()
{
	gl_Position = PC.mvp * vec4(aPos, 1.0);
	vPos = aPos;
}
)";

static const char hdri_to_cube_fragment_glsl[] = R"(
#version 460

layout (location = 0) in vec3 vPos;
layout (location = 0) out vec4 fColor;

layout (set = 0, binding = 0) uniform sampler2D uHDRI;

const vec2 invAtan = vec2(0.1591, 0.3183);

vec2 SampleSphericalMap(vec3 v)
{
    vec2 uv = vec2(atan(v.z, v.x), asin(v.y));
    uv *= invAtan;
    uv += 0.5;
	uv.y = 1.0 - uv.y;
    return uv;
}

void main()
{		
    vec2 uv = SampleSphericalMap(normalize(vPos));
	fColor = vec4(texture(uHDRI, uv).rgb, 1.0);
}
)";

static const char irradiance_fragment_glsl[] = R"(
#version 460
#define PI 3.14159265359

layout (location = 0) in vec3 vPos;
layout (location = 0) out vec4 fColor;

layout (set = 0, binding = 0) uniform samplerCube uCubemap;
)"
GLSL_CUBEMAP_PUSH_CONSTANT
R"(

void main()
{
    vec3 N = normalize(vPos);
	vec3 up = vec3(0.0, 1.0, 0.0);
	vec3 right = normalize(cross(up, N));
	up = cross(N, right);

    vec3 irradiance = vec3(0.0);
	uint sampleCount = 0;
	
	for (float phi = 0.0; phi < 2 * PI; phi += PC.delta_phi)
	{
		for (float theta = 0.0; theta < 0.5 * PI; theta += PC.delta_theta)
		{
			vec3 tangentVector = vec3(sin(theta) * cos(phi),  sin(theta) * sin(phi), cos(theta));
			vec3 sampleVector = tangentVector.x * right + tangentVector.y * up + tangentVector.z * N;
			irradiance += texture(uCubemap, sampleVector).rgb * cos(theta) * sin(theta);
			sampleCount++;
		}
	}

	fColor = vec4(PI * irradiance / float(sampleCount), 1.0);
}
)";

static const char prefilter_fragment_glsl[] = R"(
#version 460
#define PI 3.14159265359

layout (location = 0) in vec3 vPos;
layout (location = 0) out vec4 fColor;

layout (set = 0, binding = 0) uniform samplerCube uCubemap;
)"
GLSL_CUBEMAP_PUSH_CONSTANT
GLSL_HAMMERSLEY
GLSL_IMPORTANCE_SAMPLE_GGX
R"(

void main()
{
	vec3 N = normalize(vPos);    
	vec3 R = N;
	vec3 V = R;
	float roughness = PC.roughness;
	float totalWeight = 0.0;
	vec3 prefilteredColor = vec3(0.0);

	for (uint i = 0; i < PC.sample_count; i++)
	{
		vec2 Xi = hammersley(i, PC.sample_count);
		vec3 H = importance_sample_GGX(Xi, N, roughness);
		vec3 L  = normalize(2.0 * dot(V, H) * H - V);

		float NdotL = max(dot(N, L), 0.0);
        if (NdotL > 0.0)
        {
            prefilteredColor += texture(uCubemap, L).rgb * NdotL;
            totalWeight += NdotL;
        }
	}

	fColor = vec4(prefilteredColor / totalWeight, 1.0);
}
)";

static const char brdflut_vertex_glsl[] = R"(
#version 460

layout (location = 0) out vec2 vUV;

void main()
{
	vUV = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
	gl_Position = vec4(vUV * 2.0f - 1.0f, 0.0f, 1.0f);
}
)";

static const char brdflut_fragment_glsl[] = R"(
#version 460
#define PI 3.14159265359

layout (location = 0) in vec2 vUV;
layout (location = 0) out vec2 fColor;

layout (push_constant) uniform uPC
{
	uint sample_count;
} PC;

)"
GLSL_HAMMERSLEY
GLSL_IMPORTANCE_SAMPLE_GGX
R"(

float G_SchlicksmithGGX(float NdotL, float NdotV, float roughness)
{
	float k = (roughness * roughness) / 2.0;
	float GL = NdotL / (NdotL * (1.0 - k) + k);
	float GV = NdotV / (NdotV * (1.0 - k) + k);
	return GL * GV;
}

vec2 BRDF(float NdotV, float roughness)
{
	vec3 V;
    V.x = sqrt(1.0 - NdotV * NdotV);
    V.y = 0.0;
    V.z = NdotV;

    float scale = 0.0;
    float bias = 0.0;
    vec3 N = vec3(0.0, 0.0, 1.0);

    for(uint i = 0u; i < PC.sample_count; ++i)
    {
        vec2 Xi = hammersley(i, PC.sample_count);
        vec3 H  = importance_sample_GGX(Xi, N, roughness);
        vec3 L  = normalize(2.0 * dot(V, H) * H - V);

        float NdotL = max(L.z, 0.0);
		float NdotV = max(dot(N, V), 0.0);
        float NdotH = max(H.z, 0.0);
        float VdotH = max(dot(V, H), 0.0);

        if (NdotL > 0.0)
        {
            float G = G_SchlicksmithGGX(NdotL, NdotV, roughness);
            float G_Vis = (G * VdotH) / (NdotH * NdotV);
            float Fc = pow(1.0 - VdotH, 5.0);

            scale += (1.0 - Fc) * G_Vis;
            bias += Fc * G_Vis;
        }
    }
    scale /= float(PC.sample_count);
    bias /= float(PC.sample_count);
    return vec2(scale, bias);
}

void main()
{
	fColor.rg = BRDF(vUV.x, 1.0 - vUV.y);
}
)";

static const char pbr_vertex_glsl[] = R"(
#version 460

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aUV;

layout (location = 0) out vec3 vPos;
layout (location = 1) out vec3 vNormal;
layout (location = 2) out vec2 vUV;

layout (set = 0, binding = 0) uniform uScene
{
	mat4 view;
	mat4 proj;
	vec4 camera_pos;
	uint show_channel;
	uint metallic_state;
	float clamp_max_roughness;
} Scene;

layout (push_constant) uniform uPC
{
	mat4 node_transform;
} PC;

void main()
{
	vec4 modelPos = PC.node_transform * vec4(aPos, 1.0);
	vPos = modelPos.xyz / modelPos.w;
	vNormal = aNormal;
	vUV = aUV;

	gl_Position = Scene.proj * Scene.view * vec4(vPos, 1.0);
}
)";

// PBR fragment shader with Image Based Lighting
// - Irradiance Cubemap stores diffuse lighting information
// - Prefiltered Cubemap stores specular lighting, taking roughness into account
// - BRDF lookup table stores specular scale and bias
static const char pbr_fragment_glsl[] = R"(
#version 460
#define MIN_ROUGHNESS 0.04

layout (location = 0) in vec3 vPos;
layout (location = 1) in vec3 vNormal;
layout (location = 2) in vec2 vUV;

layout (location = 0) out vec4 fColor;

// SCENE SET

layout (set = 0, binding = 0) uniform uScene
{
	mat4 view;
	mat4 proj;
	vec4 camera_pos;
	uint show_channel;
	uint metallic_state;
	float clamp_max_roughness;
} Scene;

layout (set = 0, binding = 1) uniform sampler2D uBRDFLUT;
layout (set = 0, binding = 2) uniform samplerCube uIrradiance;
layout (set = 0, binding = 3) uniform samplerCube uPrefilter;

// MATERIAL SET

layout (set = 1, binding = 0) uniform uMat
{
	uint has_color_map;
	uint has_normal_map;
	uint has_metallic_roughness_map;
	uint has_occlusion_map;
	float metallic_factor;
	float roughness_factor;
} Mat;

layout (set = 1, binding = 1) uniform sampler2D uMatColor;
layout (set = 1, binding = 2) uniform sampler2D uMatNormal;
layout (set = 1, binding = 3) uniform sampler2D uMatMR;

vec3 fresnel_schlick_IBL(float cosTheta, vec3 F0, float roughness)
{
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

void main()
{
	// gather parameters
	vec3 camPos = Scene.camera_pos.xyz / Scene.camera_pos.w;
	vec3 albedo = texture(uMatColor, vUV).rgb;
	vec3 N = normalize(vNormal); // TODO: normal mapping
	vec3 V = normalize(camPos - vPos);
    vec3 R = reflect(-V, N);   
	vec4 MR = texture(uMatMR, vUV);
	float NdotV = max(dot(N, V), 0.0);
	float roughness = clamp(MR.g * Mat.roughness_factor, 0.0, 1.0);
	float metallic = clamp(MR.b * Mat.metallic_factor, 0.0, 1.0);
	
	// overrides
	roughness = clamp(roughness, 0.0, Scene.clamp_max_roughness);
	if (Scene.metallic_state == 0)
		metallic = 0.0;
	else if (Scene.metallic_state == 1)
		metallic = 1.0;

	vec3 F0 = mix(vec3(0.04), albedo, metallic);
	vec3 kS = fresnel_schlick_IBL(NdotV, F0, roughness); 
	vec3 kD = 1.0 - kS;

	// indirect diffuse image lighting
	kD *= 1.0 - metallic;
	vec3 irradiance = texture(uIrradiance, N).rgb;
	vec3 diffuse = kD * irradiance * albedo;

	// indirect specular image lighting
	float lod = roughness * )" STR(PREFILTER_MIP_LEVELS) R"(;
    vec3 prefilter = textureLod(uPrefilter, R, lod).rgb;
	vec2 lut = texture(uBRDFLUT, vec2(NdotV, roughness)).rg;
	vec3 specular = prefilter * (kS * lut.r + lut.g);

	vec3 ambient = diffuse + specular;
	vec3 ldrColor = ambient / (ambient + vec3(1.0));
	fColor = vec4(ldrColor, 1.0);

	// debug and visualization
	if (Scene.show_channel == )" STR(SHOW_CHANNEL_ALBEDO) R"()
		fColor = vec4(albedo, 1.0);
	else if (Scene.show_channel == )" STR(SHOW_CHANNEL_METALLIC) R"()
		fColor = vec4(vec3(metallic), 1.0);
	else if (Scene.show_channel == )" STR(SHOW_CHANNEL_ROUGHNESS) R"()
		fColor = vec4(vec3(roughness), 1.0);
}
)";

ExamplePBR::ExamplePBR(VIBackend backend)
	: Application("Example PBR", backend)
{
	glfwSetKeyCallback(mWindow, &ExamplePBR::KeyCallback);

	uint32_t family = vi_device_get_graphics_family_index(mDevice);
	mCmdPool = vi_create_command_pool(mDevice, family, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

	uint32_t singleImageSetCount = 5;
	uint32_t sceneSetUBOCount = 1;
	uint32_t sceneSetImageCount = 3;
	uint32_t sceneSetCount = mFramesInFlight;

	std::array<VISetPoolResource, 2> resources{};
	resources[0].type = VI_SET_BINDING_TYPE_COMBINED_IMAGE_SAMPLER;
	resources[0].count = singleImageSetCount + sceneSetImageCount * mFramesInFlight;
	resources[1].type = VI_SET_BINDING_TYPE_UNIFORM_BUFFER;
	resources[1].count = sceneSetUBOCount * mFramesInFlight;

	VISetPoolInfo setPoolI;
	setPoolI.max_set_count = singleImageSetCount + sceneSetCount;
	setPoolI.resources = resources.data();
	setPoolI.resource_count = resources.size();
	mSetPool = vi_create_set_pool(mDevice, &setPoolI);

	mSetLayoutSingleImage = CreateSetLayout(mDevice, {
		{ VI_SET_BINDING_TYPE_COMBINED_IMAGE_SAMPLER, 0, 1 },
	});

	mSetLayoutScene = CreateSetLayout(mDevice, {
		{ VI_SET_BINDING_TYPE_UNIFORM_BUFFER,         0, 1 },
		{ VI_SET_BINDING_TYPE_COMBINED_IMAGE_SAMPLER, 1, 1 },
		{ VI_SET_BINDING_TYPE_COMBINED_IMAGE_SAMPLER, 2, 1 },
		{ VI_SET_BINDING_TYPE_COMBINED_IMAGE_SAMPLER, 3, 1 }
	});

	mSetLayoutMaterial = CreateSetLayout(mDevice, {
		{ VI_SET_BINDING_TYPE_UNIFORM_BUFFER,         0, 1 },
		{ VI_SET_BINDING_TYPE_COMBINED_IMAGE_SAMPLER, 1, 1 },
		{ VI_SET_BINDING_TYPE_COMBINED_IMAGE_SAMPLER, 2, 1 },
		{ VI_SET_BINDING_TYPE_COMBINED_IMAGE_SAMPLER, 3, 1 }
	});

	VIPipelineLayoutInfo pipelineLayoutI;
	pipelineLayoutI.push_constant_size = 128;
	pipelineLayoutI.set_layout_count = 1;
	pipelineLayoutI.set_layouts = &mSetLayoutSingleImage;
	mPipelineLayoutSingleImage = vi_create_pipeline_layout(mDevice, &pipelineLayoutI);

	std::array<VISetLayout, 2> pbrSetLayouts = {
		mSetLayoutScene,
		mSetLayoutMaterial,
	};
	pipelineLayoutI.set_layout_count = pbrSetLayouts.size();
	pipelineLayoutI.set_layouts = pbrSetLayouts.data();
	mPipelineLayoutPBR = vi_create_pipeline_layout(mDevice, &pipelineLayoutI);

	uint32_t size;
	std::vector<VIVertexAttribute> skyboxVertexAttrs;
	std::vector<VIVertexBinding> skyboxVertexBindings;
	const float* vertices = GetSkyboxVertices(nullptr, &size, &skyboxVertexAttrs, &skyboxVertexBindings);

	VIBufferInfo bufferI;
	bufferI.type = VI_BUFFER_TYPE_VERTEX;
	bufferI.usage = VI_BUFFER_USAGE_TRANSFER_DST_BIT;
	bufferI.size = size;
	bufferI.properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
	mSkyboxVBO = CreateBufferStaged(mDevice, &bufferI, vertices);

	mSkyboxVM = CreateOrLoadModule(mDevice, mBackend, mPipelineLayoutSingleImage, VI_MODULE_TYPE_VERTEX, skybox_vertex_glsl, "skybox_vm");
	mSkyboxFM = CreateOrLoadModule(mDevice, mBackend, mPipelineLayoutSingleImage, VI_MODULE_TYPE_FRAGMENT, skybox_fragment_glsl, "skybox_fm");

	VIPipelineInfo pipelineI;
	pipelineI.layout = mPipelineLayoutSingleImage;
	pipelineI.pass = vi_device_get_swapchain_pass(mDevice);
	pipelineI.vertex_module = mSkyboxVM;
	pipelineI.fragment_module = mSkyboxFM;
	pipelineI.vertex_attribute_count = skyboxVertexAttrs.size();
	pipelineI.vertex_attributes = skyboxVertexAttrs.data();
	pipelineI.vertex_binding_count = skyboxVertexBindings.size();
	pipelineI.vertex_bindings = skyboxVertexBindings.data();
	pipelineI.depth_stencil_state.depth_test_enabled = false; // render to full viewport
	pipelineI.depth_stencil_state.depth_write_enabled = false;
	mSkyboxPipeline = vi_create_pipeline(mDevice, &pipelineI);

	mPBRVM = CreateOrLoadModule(mDevice, mBackend, mPipelineLayoutPBR, VI_MODULE_TYPE_VERTEX, pbr_vertex_glsl, "pbr_vm");
	mPBRFM = CreateOrLoadModule(mDevice, mBackend, mPipelineLayoutPBR, VI_MODULE_TYPE_FRAGMENT, pbr_fragment_glsl, "pbr_fm");

	VIVertexBinding pbrVertBinding;
	std::vector<VIVertexAttribute> pbrVertAttributes;
	GLTFVertex::GetBindingAndAttributes(pbrVertBinding, pbrVertAttributes);

	pipelineI.layout = mPipelineLayoutPBR;
	pipelineI.pass = vi_device_get_swapchain_pass(mDevice);
	pipelineI.vertex_binding_count = 1;
	pipelineI.vertex_bindings = &pbrVertBinding;
	pipelineI.vertex_attribute_count = pbrVertAttributes.size();
	pipelineI.vertex_attributes = pbrVertAttributes.data();
	pipelineI.vertex_module = mPBRVM;
	pipelineI.fragment_module = mPBRFM;
	pipelineI.depth_stencil_state.depth_test_enabled = true;
	pipelineI.depth_stencil_state.depth_write_enabled = true;
	pipelineI.depth_stencil_state.depth_compare_op = VI_COMPARE_OP_LESS;
	mPBRPipeline = vi_create_pipeline(mDevice, &pipelineI);

	// create baking resources
	{
		VIPassColorAttachment colorAtch;
		colorAtch.color_format = VI_FORMAT_RGBA16F;
		colorAtch.color_load_op = VK_ATTACHMENT_LOAD_OP_CLEAR;
		colorAtch.color_store_op = VK_ATTACHMENT_STORE_OP_STORE;
		colorAtch.initial_layout = VK_IMAGE_LAYOUT_UNDEFINED;
		colorAtch.final_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		VISubpassColorAttachment colorRef;
		colorRef.index = 0;
		colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		VISubpassInfo subpassI;
		subpassI.color_attachment_ref_count = 1;
		subpassI.color_attachment_refs = &colorRef;
		subpassI.depth_stencil_attachment_ref = nullptr;
		VIPassInfo passI;
		passI.depenency_count = 0;
		passI.color_attachment_count = 1;
		passI.color_attachments = &colorAtch;
		passI.depth_stencil_attachment = nullptr;
		passI.subpass_count = 1;
		passI.subpasses = &subpassI;
		mCubemapPass = vi_create_pass(mDevice, &passI);

		colorAtch.color_format = VI_FORMAT_RG16F;
		colorAtch.initial_layout = VK_IMAGE_LAYOUT_UNDEFINED;
		colorAtch.final_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		mBRDFLUTPass = vi_create_pass(mDevice, &passI);

		VIImageInfo imageI = MakeImageInfoCube(VI_FORMAT_RGBA16F, CUBEMAP_SIZE, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		imageI.usage = VI_IMAGE_USAGE_SAMPLED_BIT | VI_IMAGE_USAGE_TRANSFER_DST_BIT;
		mCubemap = vi_create_image(mDevice, &imageI);

		imageI = MakeImageInfoCube(VI_FORMAT_RGBA16F, IRRADIANCE_SIZE, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		imageI.usage = VI_IMAGE_USAGE_SAMPLED_BIT | VI_IMAGE_USAGE_TRANSFER_DST_BIT;
		mIrradiance = vi_create_image(mDevice, &imageI);

		imageI = MakeImageInfoCube(VI_FORMAT_RGBA16F, PREFILTER_BASE_SIZE, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		imageI.usage = VI_IMAGE_USAGE_SAMPLED_BIT | VI_IMAGE_USAGE_TRANSFER_DST_BIT;
		imageI.levels = PREFILTER_MIP_LEVELS;
		imageI.sampler.max_lod = (float)PREFILTER_MIP_LEVELS;
		mPrefilter = vi_create_image(mDevice, &imageI);

		imageI = MakeImageInfo2D(VI_FORMAT_RG16F, BRDFLUT_SIZE, BRDFLUT_SIZE, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		imageI.usage = VI_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VI_IMAGE_USAGE_SAMPLED_BIT;
		mBRDFLUT = vi_create_image(mDevice, &imageI);

		// maximum cubemap face size
		uint32_t offscreenImageSize = std::max(IRRADIANCE_SIZE, std::max(CUBEMAP_SIZE, PREFILTER_BASE_SIZE));

		imageI = MakeImageInfo2D(VI_FORMAT_RGBA16F, offscreenImageSize, offscreenImageSize, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		imageI.usage = VI_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VI_IMAGE_USAGE_TRANSFER_SRC_BIT;
		mOffscreenImage = vi_create_image(mDevice, &imageI);

		// NOTE: stbi_loadf loads each color channel as 32-bit floats, since RGB32F is not widely supported,
		//       we try to load the image as RGBA32F. A more serious application would probably use
		//       a libray such as KTX to store images in a more compressed and GPU friendly format.
		int width, height, nrComponents;
		float *data = stbi_loadf(APP_PATH "../../Assets/hdri/blue_photo_studio_4k.hdr", &width, &height, &nrComponents, 4);
		imageI = MakeImageInfo2D(VI_FORMAT_RGBA32F, width, height, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		imageI.usage = VI_IMAGE_USAGE_SAMPLED_BIT | VI_IMAGE_USAGE_TRANSFER_DST_BIT;
		mHDRI = CreateImageStaged(mDevice, &imageI, data, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		stbi_image_free(data);

		VIFramebufferInfo framebufferI;
		framebufferI.width = offscreenImageSize;
		framebufferI.height = offscreenImageSize;
		framebufferI.pass = mCubemapPass;
		framebufferI.color_attachments = &mOffscreenImage;
		framebufferI.color_attachment_count = 1;
		framebufferI.depth_stencil_attachment = nullptr;
		mOffscreenFBO = vi_create_framebuffer(mDevice, &framebufferI);

		framebufferI.pass = mBRDFLUTPass;
		framebufferI.width = BRDFLUT_SIZE;
		framebufferI.height = BRDFLUT_SIZE;
		framebufferI.color_attachments = &mBRDFLUT;
		mBRDFLUTFBO = vi_create_framebuffer(mDevice, &framebufferI);

		mCubemapFaceVM = CreateOrLoadModule(mDevice, mBackend, mPipelineLayoutSingleImage, VI_MODULE_TYPE_VERTEX, cubemap_face_vertex_glsl, "cubemap_face_vm");
		mHDRI2CubeFM = CreateOrLoadModule(mDevice, mBackend, mPipelineLayoutSingleImage, VI_MODULE_TYPE_FRAGMENT, hdri_to_cube_fragment_glsl, "hdri_to_cube_fm");
		mIrradianceFM = CreateOrLoadModule(mDevice, mBackend, mPipelineLayoutSingleImage, VI_MODULE_TYPE_FRAGMENT, irradiance_fragment_glsl, "irradiance_fm");
		mPrefilterFM = CreateOrLoadModule(mDevice, mBackend, mPipelineLayoutSingleImage, VI_MODULE_TYPE_FRAGMENT, prefilter_fragment_glsl, "prefilter_fm");

		mBRDFLUTVM = CreateOrLoadModule(mDevice, mBackend, mPipelineLayoutSingleImage, VI_MODULE_TYPE_VERTEX, brdflut_vertex_glsl, "brdflut_vm");
		mBRDFLUTFM = CreateOrLoadModule(mDevice, mBackend, mPipelineLayoutSingleImage, VI_MODULE_TYPE_FRAGMENT, brdflut_fragment_glsl, "brdflut_fm");

		VIPipelineInfo pipelineI;
		pipelineI.pass = mCubemapPass;
		pipelineI.layout = mPipelineLayoutSingleImage;
		pipelineI.vertex_attribute_count = skyboxVertexAttrs.size();
		pipelineI.vertex_attributes = skyboxVertexAttrs.data();
		pipelineI.vertex_binding_count = skyboxVertexBindings.size();
		pipelineI.vertex_bindings = skyboxVertexBindings.data();
		pipelineI.vertex_module = mCubemapFaceVM;
		pipelineI.fragment_module = mHDRI2CubeFM;
		mHDRI2CubePipeline = vi_create_pipeline(mDevice, &pipelineI);

		pipelineI.fragment_module = mIrradianceFM;
		mIrradiancePipeline = vi_create_pipeline(mDevice, &pipelineI);

		pipelineI.fragment_module = mPrefilterFM;
		mPrefilterPipeline = vi_create_pipeline(mDevice, &pipelineI);

		pipelineI.pass = mBRDFLUTPass;
		pipelineI.vertex_attribute_count = 0;
		pipelineI.vertex_binding_count = 0;
		pipelineI.vertex_module = mBRDFLUTVM;
		pipelineI.fragment_module = mBRDFLUTFM;
		mBRDFLUTPipeline = vi_create_pipeline(mDevice, &pipelineI);

		mHDRISet = AllocAndUpdateSet(mDevice, mSetPool, mSetLayoutSingleImage, { { 0, VI_NULL, mHDRI } });
		mBRDFLUTSet = AllocAndUpdateSet(mDevice, mSetPool, mSetLayoutSingleImage, { { 0, VI_NULL, mBRDFLUT } });
		mCubemapSet = AllocAndUpdateSet(mDevice, mSetPool, mSetLayoutSingleImage, { { 0, VI_NULL, mCubemap } });
		mPrefilterSet = AllocAndUpdateSet(mDevice, mSetPool, mSetLayoutSingleImage, { { 0, VI_NULL, mPrefilter } });
		mIrradianceSet = AllocAndUpdateSet(mDevice, mSetPool, mSetLayoutSingleImage, { { 0, VI_NULL, mIrradiance } });
	}

	mFrames.resize(mFramesInFlight);
	for (size_t i = 0; i < mFrames.size(); i++)
	{
		mFrames[i].cmd = vi_alloc_command(mDevice, mCmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

		VIBufferInfo bufferI;
		bufferI.type = VI_BUFFER_TYPE_UNIFORM;
		bufferI.properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
		bufferI.usage = 0;
		bufferI.size = sizeof(SceneUBO);
		mFrames[i].scene_ubo = vi_create_buffer(mDevice, &bufferI);
		vi_buffer_map(mFrames[i].scene_ubo);

		mFrames[i].scene_set = vi_alloc_set(mDevice, mSetPool, mSetLayoutScene);
		std::array<VISetUpdateInfo, 4> updates{};
		updates[0].binding = 0;
		updates[0].buffer = mFrames[i].scene_ubo;
		updates[1].binding = 1;
		updates[1].image = mBRDFLUT;
		updates[2].binding = 2;
		updates[2].image = mIrradiance;
		updates[3].binding = 3;
		updates[3].image = mPrefilter;
		vi_set_update(mFrames[i].scene_set, updates.size(), updates.data());
	}

	mImGuiHDRI = ImGuiAddImage(mHDRI, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	mImGuiCubemap = ImGuiAddImage(mCubemap, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	mImGuiBRDFLUT = ImGuiAddImage(mBRDFLUT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

ExamplePBR::~ExamplePBR()
{
	vi_device_wait_idle(mDevice);

	ImGuiRemoveImage(mImGuiBRDFLUT);
	ImGuiRemoveImage(mImGuiCubemap);
	ImGuiRemoveImage(mImGuiHDRI);

	for (size_t i = 0; i < mFrames.size(); i++)
	{
		vi_free_set(mDevice, mFrames[i].scene_set);
		vi_buffer_unmap(mFrames[i].scene_ubo);
		vi_destroy_buffer(mDevice, mFrames[i].scene_ubo);
		vi_free_command(mDevice, mFrames[i].cmd);
	}

	// destroy baking resources
	{
		vi_free_set(mDevice, mIrradianceSet);
		vi_free_set(mDevice, mPrefilterSet);
		vi_free_set(mDevice, mCubemapSet);
		vi_free_set(mDevice, mBRDFLUTSet);
		vi_free_set(mDevice, mHDRISet);
		vi_destroy_pipeline(mDevice, mBRDFLUTPipeline);
		vi_destroy_pipeline(mDevice, mIrradiancePipeline);
		vi_destroy_pipeline(mDevice, mHDRI2CubePipeline);
		vi_destroy_pipeline(mDevice, mPrefilterPipeline);
		vi_destroy_module(mDevice, mBRDFLUTFM);
		vi_destroy_module(mDevice, mBRDFLUTVM);
		vi_destroy_module(mDevice, mPrefilterFM);
		vi_destroy_module(mDevice, mIrradianceFM);
		vi_destroy_module(mDevice, mHDRI2CubeFM);
		vi_destroy_module(mDevice, mCubemapFaceVM);
		vi_destroy_framebuffer(mDevice, mBRDFLUTFBO);
		vi_destroy_framebuffer(mDevice, mOffscreenFBO);
		vi_destroy_image(mDevice, mPrefilter);
		vi_destroy_image(mDevice, mIrradiance);
		vi_destroy_image(mDevice, mHDRI);
		vi_destroy_image(mDevice, mOffscreenImage);
		vi_destroy_image(mDevice, mCubemap);
		vi_destroy_image(mDevice, mBRDFLUT);
		vi_destroy_pass(mDevice, mBRDFLUTPass);
		vi_destroy_pass(mDevice, mCubemapPass);
	}

	vi_destroy_pipeline(mDevice, mPBRPipeline);
	vi_destroy_pipeline(mDevice, mSkyboxPipeline);
	vi_destroy_module(mDevice, mPBRVM);
	vi_destroy_module(mDevice, mPBRFM);
	vi_destroy_module(mDevice, mSkyboxFM);
	vi_destroy_module(mDevice, mSkyboxVM);
	vi_destroy_buffer(mDevice, mSkyboxVBO);
	vi_destroy_pipeline_layout(mDevice, mPipelineLayoutSingleImage);
	vi_destroy_pipeline_layout(mDevice, mPipelineLayoutPBR);
	vi_destroy_set_layout(mDevice, mSetLayoutSingleImage);
	vi_destroy_set_layout(mDevice, mSetLayoutMaterial);
	vi_destroy_set_layout(mDevice, mSetLayoutScene);
	vi_destroy_set_pool(mDevice, mSetPool);
	vi_destroy_command_pool(mDevice, mCmdPool);
}

void ExamplePBR::Run()
{
	mCamera.SetPosition({ -5.0f, 1.0f, 0.0f });
	mConfig.show_background_skybox = mCubemapSet;
	mConfig.show_prefilter_roughness = -1.0f;
	mSceneUBO.show_channel = 0;
	mSceneUBO.metallic_state = 2;
	mSceneUBO.clamp_max_roughness = 1.0f;

	const char* logoModelPath = mBackend == VI_BACKEND_OPENGL ?
		APP_PATH "../../Assets/gltf/opengl_logo/scene.gltf" :
		APP_PATH "../../Assets/gltf/vulkan_logo/scene.gltf";

	mModel = GLTFModel::LoadFromFile(APP_PATH "../../Assets/gltf/hard_surface_crate/scene.gltf", mDevice, mSetLayoutMaterial);
	mLogoModel = GLTFModel::LoadFromFile(logoModelPath, mDevice, mSetLayoutMaterial);
	
	// These may and should be done offline instead of during application startup
	// 1. convert HDRI (RGB32F) to regular cubemap (RGBA16F per face)
	// 2. bake irradiance environment map
	// 3. bake prefilter environment map
	// 4. bake BRDF lookup table
	Bake();

	while (!glfwWindowShouldClose(mWindow))
	{
		Application::NewFrame();
		Application::CameraUpdate();
		Application::ImGuiNewFrame();

		ImGuiUpdate();

		VISemaphore image_acquired;
		VISemaphore present_ready;
		VIFence frame_complete;
		uint32_t frame_idx = vi_device_next_frame(mDevice, &image_acquired, &present_ready, &frame_complete);
		VIFramebuffer fb = vi_device_get_swapchain_framebuffer(mDevice, frame_idx);
		FrameData* frame = mFrames.data() + frame_idx;

		vi_begin_command(frame->cmd, 0);

		VkClearValue clear[2];
		clear[0] = MakeClearDepthStencil(1.0f, 0.0f);
		clear[1] = MakeClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		VIPassBeginInfo beginI;
		beginI.pass = vi_device_get_swapchain_pass(mDevice);
		beginI.framebuffer = fb;
		beginI.color_clear_values = clear + 1;
		beginI.color_clear_value_count = 1;
		beginI.depth_stencil_clear_value = clear;

		vi_cmd_begin_pass(frame->cmd, &beginI);
		{
			vi_cmd_bind_graphics_pipeline(frame->cmd, mSkyboxPipeline);
			vi_cmd_set_viewport(frame->cmd, MakeViewport(APP_WINDOW_WIDTH, APP_WINDOW_HEIGHT));
			vi_cmd_set_scissor(frame->cmd, MakeScissor(APP_WINDOW_WIDTH, APP_WINDOW_HEIGHT));

			// draw skybox
			{
				vi_cmd_bind_graphics_set(frame->cmd, mPipelineLayoutSingleImage, 0, mConfig.show_background_skybox);
				vi_cmd_bind_vertex_buffers(frame->cmd, 0, 1, &mSkyboxVBO);

				// shave off the camera translation at the last column in view matrix
				glm::mat4 view = glm::mat4(glm::mat3(mCamera.GetViewMat()));
				glm::mat4 proj = mCamera.GetProjMat();

				SkyboxPushConstant skyboxPC;
				skyboxPC.mvp = proj * view;
				skyboxPC.prefilter_roughness = mConfig.show_prefilter_roughness;
				vi_cmd_push_constants(frame->cmd, mPipelineLayoutSingleImage, 0, sizeof(skyboxPC), &skyboxPC);

				VIDrawInfo info;
				info.vertex_count = 36;
				info.vertex_start = 0;
				info.instance_count = 1;
				info.instance_start = 0;
				vi_cmd_draw(frame->cmd, &info);
			}

			vi_cmd_bind_graphics_pipeline(frame->cmd, mPBRPipeline);
			vi_cmd_set_viewport(frame->cmd, MakeViewport(APP_WINDOW_WIDTH, APP_WINDOW_HEIGHT));
			vi_cmd_set_scissor(frame->cmd, MakeScissor(APP_WINDOW_WIDTH, APP_WINDOW_HEIGHT));

			// draw model
			{
				vi_cmd_bind_graphics_set(frame->cmd, mPipelineLayoutPBR, 0, frame->scene_set);

				mModel->Draw(frame->cmd, mPipelineLayoutPBR);
				mLogoModel->Draw(frame->cmd, mPipelineLayoutPBR);
			}

			Application::ImGuiRender(frame->cmd);
		}
		vi_cmd_end_pass(frame->cmd);
		vi_end_command(frame->cmd);

		UpdateUBO();
		vi_buffer_map_write(frame->scene_ubo, 0, sizeof(mSceneUBO), &mSceneUBO);

		VkPipelineStageFlags stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		VISubmitInfo submitI;
		submitI.wait_count = 1;
		submitI.wait_stages = &stage;
		submitI.waits = &image_acquired;
		submitI.signal_count = 1;
		submitI.signals = &present_ready;
		submitI.cmd_count = 1;
		submitI.cmds = &frame->cmd;

		VIQueue graphics_queue = vi_device_get_graphics_queue(mDevice);
		vi_queue_submit(graphics_queue, 1, &submitI, frame_complete);

		vi_device_present_frame(mDevice);
	}

	vi_device_wait_idle(mDevice);
	mModel = nullptr;
}

void ExamplePBR::KeyCallback(GLFWwindow * window, int key, int scancode, int action, int mods)
{
	ExamplePBR* example = (ExamplePBR*)Application::Get();

	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
	{
		example->CameraToggleCapture();
	}
}

void ExamplePBR::ImGuiUpdate()
{
	if (CameraIsCaptured())
		return;
	
	ImGui::Begin(mName);

	if (ImGui::CollapsingHeader("Intermediate Results"))
	{
		ImGui::Text("HDRI");
		ImGui::Image(mImGuiHDRI, { 512, 256 });
		ImGui::Text("BRDF LUT");
		ImGui::Image(mImGuiBRDFLUT, { 256, 256 });
	}

	if (ImGui::CollapsingHeader("Background Skybox"))
	{
		if (ImGui::Button("Default Cubemap"))
		{
			mConfig.show_background_skybox = mCubemapSet;
			mConfig.show_prefilter_roughness = -1.0f;
		}
		if (ImGui::Button("Irradiance Cubemap"))
		{
			mConfig.show_background_skybox = mIrradianceSet;
			mConfig.show_prefilter_roughness = -1.0f;
		}
		if (ImGui::Button("Prefilter Cubemap"))
		{
			mConfig.show_background_skybox = mPrefilterSet;
			mConfig.show_prefilter_roughness = 1.2f;
		}

		ImGui::SliderFloat("Prefilter Roughness", &mConfig.show_prefilter_roughness, 0.0f, (float)PREFILTER_MIP_LEVELS);
	}

	if (ImGui::CollapsingHeader("Metallic Overrides", ImGuiTreeNodeFlags_DefaultOpen))
	{
		if (ImGui::Selectable("Load Metallic from Material", mSceneUBO.metallic_state == 2))
			mSceneUBO.metallic_state = 2;
		if (ImGui::Selectable("Force Metallic == 0", mSceneUBO.metallic_state == 0))
			mSceneUBO.metallic_state = 0;
		if (ImGui::Selectable("Force Metallic == 1", mSceneUBO.metallic_state == 1))
			mSceneUBO.metallic_state = 1;
	}

	if (ImGui::CollapsingHeader("Roughness Overrides", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::SliderFloat("Limit Max Roughness", &mSceneUBO.clamp_max_roughness, 0.0f, 1.0f);
	}

	if (ImGui::CollapsingHeader("Render Result", ImGuiTreeNodeFlags_DefaultOpen))
	{
		if (ImGui::Button("Show Final Render"))
			mSceneUBO.show_channel = SHOW_FINAL_RESULT;
		if (ImGui::Button("Show Albedo"))
			mSceneUBO.show_channel = SHOW_CHANNEL_ALBEDO;
		if (ImGui::Button("Show Metallic"))
			mSceneUBO.show_channel = SHOW_CHANNEL_METALLIC;
		if (ImGui::Button("Show Roughness"))
			mSceneUBO.show_channel = SHOW_CHANNEL_ROUGHNESS;
	}

	ImGui::End();
}

void ExamplePBR::Bake()
{
	constexpr int max_mip_levels = PREFILTER_MIP_LEVELS;
	std::array<CubemapPushConstant, max_mip_levels> constants;
	
	// render HDRI image into Cubemap faces
	BakeCubemap(mCubemap, CUBEMAP_SIZE, mHDRI2CubePipeline, mPipelineLayoutSingleImage, mHDRISet, 1, constants.data());

	// render diffuse irradiance cubemap
	constants[0].delta_phi = (2.0f * float(M_PI)) / 180.0f;
	constants[0].delta_theta = (0.5f * float(M_PI)) / 64.0f;
	BakeCubemap(mIrradiance, IRRADIANCE_SIZE, mIrradiancePipeline, mPipelineLayoutSingleImage, mCubemapSet, 1, constants.data());

	// render prefilter cubemap for different roughness
	for (uint32_t i = 0; i < PREFILTER_MIP_LEVELS; i++)
	{
		constants[i].roughness = (float)i / (float)PREFILTER_MIP_LEVELS;
		constants[i].sample_count = PREFILTER_SAMPLE_COUNT;
	}
	BakeCubemap(mPrefilter, PREFILTER_BASE_SIZE, mPrefilterPipeline, mPipelineLayoutSingleImage, mCubemapSet, PREFILTER_MIP_LEVELS, constants.data());

	// render BRDF response into lookup table
	BakeBRDFLUT();
}

void ExamplePBR::BakeCubemap(VIImage targetCubemap, uint32_t cubemapDim, VIPipeline pipeline, VIPipelineLayout layout,
	VISet imageSet, uint32_t mipCount, CubemapPushConstant* constants)
{
	std::array<glm::mat4, 6> viewMats = {
		glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
		glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(-1.0f, 0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
		glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  1.0f,  0.0f), glm::vec3(0.0f,  0.0f,  1.0f)),
		glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f,  0.0f), glm::vec3(0.0f,  0.0f, -1.0f)),
		glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  0.0f,  1.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
		glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  0.0f, -1.0f), glm::vec3(0.0f, -1.0f,  0.0f))
	};

	VICommand cmd = vi_alloc_command(mDevice, mCmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	vi_begin_command(cmd, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

	CmdImageLayoutTransition(cmd, targetCubemap, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 6, mipCount);

	for (uint32_t mip = 0; mip < mipCount; mip++)
	{
		CubemapPushConstant* constant = constants + mip;

		for (uint32_t face = 0; face < 6; face++)
		{
			VkClearValue clearColor = MakeClearColor(0.0f, 0.0f, 0.2f, 1.0f);
			VIPassBeginInfo passBI;
			passBI.pass = mCubemapPass;
			passBI.framebuffer = mOffscreenFBO;
			passBI.depth_stencil_clear_value = nullptr;
			passBI.color_clear_value_count = 1;
			passBI.color_clear_values = &clearColor;
			vi_cmd_begin_pass(cmd, &passBI);
			vi_cmd_bind_graphics_pipeline(cmd, pipeline);
			vi_cmd_set_viewport(cmd, MakeViewport(cubemapDim, cubemapDim));
			vi_cmd_set_scissor(cmd, MakeScissor(cubemapDim, cubemapDim));
			vi_cmd_bind_graphics_set(cmd, layout, 0, imageSet);
			vi_cmd_bind_vertex_buffers(cmd, 0, 1, &mSkyboxVBO);

			constant->mvp = glm::perspective((float)(M_PI / 2.0), 1.0f, 0.1f, 10.0f) * viewMats[face];
			vi_cmd_push_constants(cmd, mPipelineLayoutSingleImage, 0, sizeof(CubemapPushConstant), constant);

			VIDrawInfo drawI;
			drawI.vertex_count = 36;
			drawI.vertex_start = 0;
			drawI.instance_count = 1;
			drawI.instance_start = 0;
			vi_cmd_draw(cmd, &drawI);
			vi_cmd_end_pass(cmd);

			VIImageMemoryBarrier barrier{};
			barrier.image = mOffscreenImage;
			barrier.subresource_range = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
			barrier.old_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			barrier.new_layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			barrier.src_family_index = VK_QUEUE_FAMILY_IGNORED;
			barrier.dst_family_index = VK_QUEUE_FAMILY_IGNORED;
			barrier.src_access = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			barrier.dst_access = VK_ACCESS_TRANSFER_READ_BIT;
			vi_cmd_pipeline_barrier_image_memory(cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 1, &barrier);

			VkImageCopy region{};
			region.extent.width = cubemapDim;
			region.extent.height = cubemapDim;
			region.extent.depth = 1;
			region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			region.srcSubresource.layerCount = 1;
			region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			region.dstSubresource.layerCount = 1;
			region.dstSubresource.baseArrayLayer = face;
			region.dstSubresource.mipLevel = mip;
			vi_cmd_copy_image(cmd, mOffscreenImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, targetCubemap, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

			barrier.old_layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			barrier.new_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			barrier.src_access = VK_ACCESS_TRANSFER_READ_BIT;
			barrier.dst_access = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			vi_cmd_pipeline_barrier_image_memory(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 1, &barrier);
		}

		assert(cubemapDim >= 2);
		cubemapDim /= 2;
	}
	CmdImageLayoutTransition(cmd, targetCubemap, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 6, mipCount);

	vi_end_command(cmd);

	VIFence fence = vi_create_fence(mDevice, 0);
	VIQueue queue = vi_device_get_graphics_queue(mDevice);
	VISubmitInfo submitI{};
	submitI.cmd_count = 1;
	submitI.cmds = &cmd;
	vi_queue_submit(queue, 1, &submitI, fence);
	vi_wait_for_fences(mDevice, 1, &fence, true, UINT64_MAX);

	vi_destroy_fence(mDevice, fence);
	vi_free_command(mDevice, cmd);
}

void ExamplePBR::BakeBRDFLUT()
{
	VICommand cmd = vi_alloc_command(mDevice, mCmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	vi_begin_command(cmd, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

	VkClearValue clearColor = MakeClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	VIPassBeginInfo passBI;
	passBI.pass = mBRDFLUTPass;
	passBI.framebuffer = mBRDFLUTFBO;
	passBI.depth_stencil_clear_value = nullptr;
	passBI.color_clear_value_count = 1;
	passBI.color_clear_values = &clearColor;
	vi_cmd_begin_pass(cmd, &passBI);

	vi_cmd_bind_graphics_pipeline(cmd, mBRDFLUTPipeline);
	vi_cmd_set_viewport(cmd, MakeViewport(BRDFLUT_SIZE, BRDFLUT_SIZE));
	vi_cmd_set_scissor(cmd, MakeScissor(BRDFLUT_SIZE, BRDFLUT_SIZE));

	uint32_t sample_count = BRDFLUT_SAMPLE_COUNT;
	vi_cmd_push_constants(cmd, mPipelineLayoutSingleImage, 0, sizeof(sample_count), &sample_count);
	vi_cmd_bind_graphics_set(cmd, mPipelineLayoutSingleImage, 0, mBRDFLUTSet);

	VIDrawInfo drawI;
	drawI.vertex_count = 3;
	drawI.vertex_start = 0;
	drawI.instance_count = 1;
	drawI.instance_start = 0;
	vi_cmd_draw(cmd, &drawI);

	vi_cmd_end_pass(cmd);
	vi_end_command(cmd);

	VIFence fence = vi_create_fence(mDevice, 0);
	VIQueue queue = vi_device_get_graphics_queue(mDevice);
	VISubmitInfo submitI{};
	submitI.cmd_count = 1;
	submitI.cmds = &cmd;
	vi_queue_submit(queue, 1, &submitI, fence);
	vi_wait_for_fences(mDevice, 1, &fence, true, UINT64_MAX);

	vi_destroy_fence(mDevice, fence);
	vi_free_command(mDevice, cmd);
}

void ExamplePBR::UpdateUBO()
{
	mSceneUBO.view = mCamera.GetViewMat();
	mSceneUBO.proj = mCamera.GetProjMat();
	mSceneUBO.camera_pos = glm::vec4(mCamera.GetPosition(), 1.0f);
}