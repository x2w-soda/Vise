#pragma once

#include <memory>
#include <string>
#include <vector>
#include <array>
#include <glm/gtc/quaternion.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <vise.h>
#include "Common.h"

#define GLSL_MATERIAL_SET(INDEX)\
"layout (set = " STR(INDEX) ", binding = 0) uniform Mat\n"\
"{\n"\
"    uint hasColorMap;\n"\
"    uint hasNormalMap;\n"\
"    uint hasMetallicRoughness_map;\n"\
"    uint hasOcclusionMap;\n"\
"    vec4 colorFactor;\n"\
"    float metallicFactor;\n"\
"    float roughnessFactor;\n"\
"} uMat;\n"\
"\n"\
"layout(set = " STR(INDEX) ", binding = 1) uniform sampler2D uMatColor;\n"\
"layout(set = " STR(INDEX) ", binding = 2) uniform sampler2D uMatNormal;\n"\
"layout(set = " STR(INDEX) ", binding = 3) uniform sampler2D uMatMR;\n"

struct MeshVertex;
struct MeshData;
struct ModelData;

struct MeshVertex
{
	glm::vec3 Position;
	glm::vec3 Normal;
	glm::vec2 TextureUV;

	static void GetBindingAndAttributes(VIVertexBinding& binding, std::vector<VIVertexAttribute>& attributes)
	{
		binding.rate = VK_VERTEX_INPUT_RATE_VERTEX;
		binding.stride = sizeof(float) * 8;

		attributes.resize(3);
		attributes[0].type = VI_GLSL_TYPE_VEC3; // positions
		attributes[0].binding = 0;
		attributes[0].offset = 0;
		attributes[1].type = VI_GLSL_TYPE_VEC3; // normals
		attributes[1].binding = 0;
		attributes[1].offset = sizeof(float) * 3;
		attributes[2].type = VI_GLSL_TYPE_VEC2; // texture UVs
		attributes[2].binding = 0;
		attributes[2].offset = sizeof(float) * 6;
	}
};

struct MeshData
{
	MeshData();
	MeshData(const MeshData&) = delete;
	~MeshData();

	MeshData& operator=(const MeshData&) = delete;

	static std::shared_ptr<MeshData> GenerateBox(VIDevice device, const glm::vec3& halfExtent, const glm::mat4& transform = glm::mat4(1.0f));
	static std::shared_ptr<MeshData> GenerateSphereMesh(VIDevice device, float radius, int stackCount, int sectorCount, const glm::vec3& position = glm::vec3(0.0f));

	VIDevice Device;
	VIBuffer VBO;
	VIBuffer IBO;
	uint32_t IndexCount;
};

struct ModelData
{
	ModelData();
	ModelData(const ModelData&) = delete;
	~ModelData();

	ModelData& operator=(const ModelData&) = delete;

	std::vector<MeshData> Meshes;
};

namespace tinygltf
{
	class Node;
	class Mesh;
	class Model;
	class Material;
	class Image;
	class Texture;
}

struct GLTFMaterial;
struct GLTFTexture;
struct GLTFMesh;
struct GLTFNode;
struct GLTFPrimitive;
class GLTFModel;

// GLTF may also include data for animations and skinning,
// currently we only load the GLTF models as static meshes.
using GLTFVertex = MeshVertex;

struct GLTFTexture
{
	GLTFTexture();
	GLTFTexture(const GLTFTexture&) = delete;
	GLTFTexture(GLTFTexture&&) = default;
	~GLTFTexture();

	GLTFTexture& operator=(const GLTFTexture&) = delete;
	GLTFTexture& operator=(GLTFTexture&&) = default;

	void LoadFromImage(tinygltf::Image& gltf, VIDevice device);

	uint32_t Index;
	VIDevice Device;
	VIImage Image;
};

enum GLTFAlphaMode
{
	GLTF_ALPHA_MODE_OPAQUE,
	GLTF_ALPHA_MODE_BLEND,
	GLTF_ALPHA_MODE_MASK,
};

struct GLTFMaterialUBO
{
	uint32_t HasColorMap;
	uint32_t HasNormalMap;
	uint32_t HasMetallicRoughnessMap;
	uint32_t HasOcclusionMap;
	glm::vec4 ColorFactor;
	float MetallicFactor;
	float RoughnessFactor;
};

struct GLTFMaterial
{
	GLTFMaterial();
	GLTFMaterial(const GLTFMaterial&) = delete;
	GLTFMaterial(GLTFMaterial&&) = default;
	~GLTFMaterial();

	GLTFMaterial& operator=(const GLTFMaterial&) = delete;
	GLTFMaterial& operator=(GLTFMaterial&&) = default;

	VIDevice Device;
	VISet Set = VI_NULL;
	VIBuffer UBO = VI_NULL;
	glm::vec4 BaseColorFactor = glm::vec4(1.0f);
	float MetallicFactor = 1.0f;
	float RoughnessFactor = 1.0f;
	float AlphaCutoff = 1.0f;
	GLTFAlphaMode AlphaMode = GLTF_ALPHA_MODE_OPAQUE;
	GLTFTexture* BaseColorTexture = nullptr;
	GLTFTexture* NormalTexture = nullptr;
	GLTFTexture* EmissiveTexture = nullptr;
	GLTFTexture* OcclusionTexture = nullptr;
	GLTFTexture* MetallicRoughnessTexture = nullptr;
	struct
	{
		int Color;
		int Normal;
		int Emissive;
		int Occlusion;
		int MetallicRoughness;
	} TexCoordSet;

	static VISetLayout CreateSetLayout(VIDevice device)
	{
		std::array<VIBinding, 4> bindings;
		bindings[0] = { VI_BINDING_TYPE_UNIFORM_BUFFER, 0, 1 };
		bindings[1] = { VI_BINDING_TYPE_COMBINED_IMAGE_SAMPLER, 1, 1 };
		bindings[2] = { VI_BINDING_TYPE_COMBINED_IMAGE_SAMPLER, 2, 1 };
		bindings[3] = { VI_BINDING_TYPE_COMBINED_IMAGE_SAMPLER, 3, 1 };

		VISetLayoutInfo setLI;
		setLI.binding_count = bindings.size();
		setLI.bindings = bindings.data();
		return vi_create_set_layout(device, &setLI);
	}
};

struct GLTFNode
{
	GLTFNode* Parent;
	GLTFMesh* Mesh;
	uint32_t Index;
	std::string Name;
	std::vector<GLTFNode*> Children;
	glm::vec3 Translation{};
	glm::vec3 Scale{ 1.0f };
	glm::quat Rotation{};
	glm::mat4 Transform;
};

struct GLTFMesh
{
	std::vector<GLTFPrimitive> Primitives;
};

struct GLTFPrimitive
{
	uint32_t IndexStart;
	uint32_t IndexCount;
	uint32_t VertexCount;
	GLTFMaterial* Material;
};

// NOTE: currently only loads GLTF models as static meshes.
// - per-node transform is uploaded as mat4 push constant during Draw(), make sure the PipelineLayout is compatible
// - GLTF Alpha Mode not implemented yet
class GLTFModel
{
public:
	GLTFModel() = delete;
	GLTFModel(VIDevice device);
	GLTFModel(const GLTFModel&) = delete;
	~GLTFModel();

	GLTFModel& operator=(const GLTFModel&) = delete;

	void Draw(VICommand cmd, VIPipelineLayout layout, uint32_t materialSetIndex, const glm::mat4& transform = glm::mat4(1.0f));

	void GetBoundingBox(glm::vec3& minPos, glm::vec3& maxPos);
	void GetBoundingSphere(glm::vec3& pos, float& radius);

	enum LoadFlag
	{
		LOAD_FLAG_APPLY_NODE_TRANSFORM_BIT = 1,   // apply node transform to each vertex
		LOAD_FLAG_CALCULATE_BOUNDING_BOX_BIT = 2, // calculate AABB containing all primitives
	};

	static std::shared_ptr<GLTFModel> LoadFromFile(const char* path, VIDevice device, VISetLayout materialSL, int loadFlags = 0);

private:
	void DrawNode(VICommand cmd, GLTFNode* node);
	void ScanNodePrimitives(tinygltf::Model& model, tinygltf::Node& node, uint32_t& vertexCount, uint32_t& indexCount);
	void Load(tinygltf::Model& model);
	void LoadImages(tinygltf::Model& model);
	void LoadMaterials(tinygltf::Model& model);
	void LoadNode(tinygltf::Model& tinyModel, tinygltf::Node& tinyNode, uint32_t nodeIndex, GLTFNode* parent);
	GLTFMesh* LoadMesh(tinygltf::Model& model, tinygltf::Mesh& mesh, GLTFNode* node);
	void AllocateSets();
	void FreeSets();

	int mLoadFlags;
	glm::vec3 mMaxPos;
	glm::vec3 mMinPos;
	uint32_t mVertexCount;
	uint32_t mVertexBase;
	uint32_t mIndexCount;
	uint32_t mIndexBase;
	uint32_t mMaterialSetIndex;
	VIDevice mDevice;
	VISetLayout mMaterialSetLayout;
	VIBuffer mVBO = VI_NULL;
	VIBuffer mIBO = VI_NULL;
	VISetPool mSetPool = VI_NULL;
	VIPipelineLayout mDrawPipelineLayout = VI_NULL;
	glm::mat4 mDrawTransform;
	GLTFMaterial* mDrawMaterial;
	GLTFVertex* mVertices;
	GLTFTexture mEmptyTexture;
	uint32_t* mIndices;
	std::vector<GLTFNode*> mNodes;
	std::vector<GLTFNode*> mRootNodes;
	std::vector<GLTFTexture> mTextures;
	std::vector<GLTFMaterial> mMaterials;
};

std::vector<std::shared_ptr<MeshData>> GenerateMeshSceneV1(VIDevice device);