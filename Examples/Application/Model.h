#pragma once

#include <memory>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <vise.h>

struct MeshVertex;
struct Mesh;
struct Model;

struct MeshVertex
{
	glm::vec3 Position;
	glm::vec3 Normal;
	glm::vec3 Albedo;
	glm::vec2 TextureUV;

	static void GetBindingAndAttributes(VIVertexBinding& binding, std::vector<VIVertexAttribute>& attributes)
	{
		binding.rate = VK_VERTEX_INPUT_RATE_VERTEX;
		binding.stride = sizeof(float) * 11;

		attributes.resize(4);
		attributes[0].type = VI_GLSL_TYPE_VEC3; // positions
		attributes[0].binding = 0;
		attributes[0].offset = 0;
		attributes[1].type = VI_GLSL_TYPE_VEC3; // normals
		attributes[1].binding = 0;
		attributes[1].offset = sizeof(float) * 3;
		attributes[2].type = VI_GLSL_TYPE_VEC3; // albedo
		attributes[2].binding = 0;
		attributes[2].offset = sizeof(float) * 6;
		attributes[3].type = VI_GLSL_TYPE_VEC2; // texture UVs
		attributes[3].binding = 0;
		attributes[3].offset = sizeof(float) * 9;
	}
};

struct Mesh
{
	Mesh();
	Mesh(const Mesh&) = delete;
	~Mesh();

	Mesh& operator=(const Mesh&) = delete;

	static std::shared_ptr<Mesh> GenerateBox(VIDevice device, const glm::vec3& halfExtent, const glm::vec3& color, const glm::mat4& transform = glm::mat4(1.0f));
	static std::shared_ptr<Mesh> GenerateSphereMesh(VIDevice device, float radius, const glm::vec3& color, int stackCount, int sectorCount, const glm::vec3& position = glm::vec3(0.0f));

	VIDevice Device;
	VIBuffer VBO;
	VIBuffer IBO;
	uint32_t IndexCount;
};

struct Model
{
	Model();
	Model(const Model&) = delete;
	~Model();

	Model& operator=(const Model&) = delete;

	std::vector<Mesh> Meshes;
};

std::vector<std::shared_ptr<Mesh>> GenerateMeshSceneV1(VIDevice device);