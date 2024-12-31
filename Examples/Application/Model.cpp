#include <string>
#include <cassert>
#include <iostream>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <tiny_gltf.h>
#include "Model.h"
#include "Application.h"

MeshData::MeshData()
	: VBO(VI_NULL)
	, IBO(VI_NULL)
{
}

MeshData::~MeshData()
{
	if (VBO)
		vi_destroy_buffer(Device, VBO);

	if (IBO)
		vi_destroy_buffer(Device, IBO);
}

std::shared_ptr<MeshData> MeshData::GenerateBox(VIDevice device, const glm::vec3& halfExtent, const glm::mat4& transform)
{
	auto mesh = std::make_shared<MeshData>();
	mesh->Device = device;

	int idxBase = 0;
	MeshVertex p0, p1, p2, p3;
	p0.TextureUV = { 0.0f, 0.0f };
	p1.TextureUV = { 0.0f, 0.0f };
	p2.TextureUV = { 0.0f, 0.0f };
	p3.TextureUV = { 0.0f, 0.0f };

	std::vector<MeshVertex> vertices;
	std::vector<uint32_t> indices;

	for (int sign = -1; sign <= 1; sign += 2)
	{
		p0.Position = glm::vec3(transform * glm::vec4(sign * -halfExtent.x, sign * halfExtent.y, -halfExtent.z, 1.0f));
		p1.Position = glm::vec3(transform * glm::vec4(sign * -halfExtent.x, sign * halfExtent.y, halfExtent.z, 1.0f));
		p2.Position = glm::vec3(transform * glm::vec4(sign * halfExtent.x, sign * halfExtent.y, halfExtent.z, 1.0f));
		p3.Position = glm::vec3(transform * glm::vec4(sign * halfExtent.x, sign * halfExtent.y, -halfExtent.z, 1.0f));
		p0.Normal = { 0.0f, (float)sign, 0.0f };
		p1.Normal = { 0.0f, (float)sign, 0.0f };
		p2.Normal = { 0.0f, (float)sign, 0.0f };
		p3.Normal = { 0.0f, (float)sign, 0.0f };

		vertices.push_back(p0);
		vertices.push_back(p1);
		vertices.push_back(p2);
		vertices.push_back(p3);

		indices.push_back(idxBase + 0);
		indices.push_back(idxBase + 1);
		indices.push_back(idxBase + 2);
		indices.push_back(idxBase + 2);
		indices.push_back(idxBase + 3);
		indices.push_back(idxBase + 0);
		idxBase += 4;

		p0.Position = glm::vec3(transform * glm::vec4(sign * halfExtent.x, sign * halfExtent.y, halfExtent.z, 1.0f));
		p1.Position = glm::vec3(transform * glm::vec4(sign * halfExtent.x, sign * -halfExtent.y, halfExtent.z, 1.0f));
		p2.Position = glm::vec3(transform * glm::vec4(sign * halfExtent.x, sign * -halfExtent.y, -halfExtent.z, 1.0f));
		p3.Position = glm::vec3(transform * glm::vec4(sign * halfExtent.x, sign * halfExtent.y, -halfExtent.z, 1.0f));
		p0.Normal = { (float)sign, 0.0f, 0.0f };
		p1.Normal = { (float)sign, 0.0f, 0.0f };
		p2.Normal = { (float)sign, 0.0f, 0.0f };
		p3.Normal = { (float)sign, 0.0f, 0.0f };

		vertices.push_back(p0);
		vertices.push_back(p1);
		vertices.push_back(p2);
		vertices.push_back(p3);

		indices.push_back(idxBase + 0);
		indices.push_back(idxBase + 1);
		indices.push_back(idxBase + 2);
		indices.push_back(idxBase + 2);
		indices.push_back(idxBase + 3);
		indices.push_back(idxBase + 0);
		idxBase += 4;

		p0.Position = glm::vec3(transform * glm::vec4(-halfExtent.x, sign * halfExtent.y, sign * halfExtent.z, 1.0f));
		p1.Position = glm::vec3(transform * glm::vec4(-halfExtent.x, sign * -halfExtent.y, sign * halfExtent.z, 1.0f));
		p2.Position = glm::vec3(transform * glm::vec4(halfExtent.x, sign * -halfExtent.y, sign * halfExtent.z, 1.0f));
		p3.Position = glm::vec3(transform * glm::vec4(halfExtent.x, sign * halfExtent.y, sign * halfExtent.z, 1.0f));
		p0.Normal = { 0.0f, 0.0f, (float)sign };
		p1.Normal = { 0.0f, 0.0f, (float)sign };
		p2.Normal = { 0.0f, 0.0f, (float)sign };
		p3.Normal = { 0.0f, 0.0f, (float)sign };

		vertices.push_back(p0);
		vertices.push_back(p1);
		vertices.push_back(p2);
		vertices.push_back(p3);

		indices.push_back(idxBase + 0);
		indices.push_back(idxBase + 1);
		indices.push_back(idxBase + 2);
		indices.push_back(idxBase + 2);
		indices.push_back(idxBase + 3);
		indices.push_back(idxBase + 0);
		idxBase += 4;
	}
	VIBufferInfo bufferI;
	bufferI.type = VI_BUFFER_TYPE_VERTEX;
	bufferI.usage = VI_BUFFER_USAGE_TRANSFER_DST_BIT;
	bufferI.size = sizeof(MeshVertex) * vertices.size();
	bufferI.properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
	mesh->VBO = CreateBufferStaged(device, &bufferI, vertices.data());

	bufferI.type = VI_BUFFER_TYPE_INDEX;
	bufferI.size = sizeof(uint32_t) * indices.size();
	mesh->IBO = CreateBufferStaged(device, &bufferI, indices.data());

	mesh->IndexCount = indices.size(); // 36

	return mesh;
}

std::shared_ptr<MeshData> MeshData::GenerateSphereMesh(VIDevice device, float radius, int stackCount, int sectorCount, const glm::vec3& position)
{
	auto mesh = std::make_shared<MeshData>();
	mesh->Device = device;

	std::vector<MeshVertex> vertices;
	std::vector<uint32_t> indices;

	// implementation from https://www.songho.ca/opengl/gl_sphere.html
	float x, y, z, xz;
	float radiusInv = 1.0f / radius;
	float pi = glm::pi<float>();

	// in radians
	float sectorStep = 2 * pi / sectorCount;
	float stackStep = pi / stackCount;
	float sectorAngle, stackAngle;

	// generate (sectorCount + 1) vertices for each stack
	for (int i = 0; i <= stackCount; i++)
	{
		// stack angle from PI/2 to -PI/2
		stackAngle = pi / 2 - i * stackStep;
		xz = radius * std::cos(stackAngle);
		y = radius * std::sin(stackAngle);

		for (int j = 0; j <= sectorCount; j++)
		{
			// sector angle from 0 to 2 PI
			sectorAngle = j * sectorStep;

			z = xz * std::cos(sectorAngle);
			x = xz * std::sin(sectorAngle);

			MeshVertex vertex;
			vertex.Position = glm::vec3(x, y, z) + position;
			vertex.Normal = { x * radiusInv, y * radiusInv, z * radiusInv };
			vertex.TextureUV = { j / (float)sectorCount, i / (float)stackCount };

			vertices.push_back(vertex);
		}
	}

	// generate indices
	int k1, k2;

	for (int i = 0; i < stackCount; ++i)
	{
		k1 = i * (sectorCount + 1); // beginning of current stack
		k2 = k1 + sectorCount + 1;  // beginning of next stack

		// the sectors in first and last stack only has 1 triangle
		for (int j = 0; j < sectorCount; ++j, ++k1, ++k2)
		{
			// k1 => k2 => k1+1
			if (i != 0)
			{
				indices.push_back(k1);
				indices.push_back(k2);
				indices.push_back(k1 + 1);
			}

			// k1+1 => k2 => k2+1
			if (i != (stackCount - 1))
			{
				indices.push_back(k1 + 1);
				indices.push_back(k2);
				indices.push_back(k2 + 1);
			}
		}
	}

	VIBufferInfo bufferI;
	bufferI.type = VI_BUFFER_TYPE_VERTEX;
	bufferI.usage = VI_BUFFER_USAGE_TRANSFER_DST_BIT;
	bufferI.size = sizeof(MeshVertex) * vertices.size();
	bufferI.properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
	mesh->VBO = CreateBufferStaged(device, &bufferI, vertices.data());

	bufferI.type = VI_BUFFER_TYPE_INDEX;
	bufferI.size = sizeof(uint32_t) * indices.size();
	mesh->IBO = CreateBufferStaged(device, &bufferI, indices.data());

	mesh->IndexCount = indices.size();

	return mesh;
}

std::vector<std::shared_ptr<MeshData>> GenerateMeshSceneV1(VIDevice device)
{
	std::vector<std::shared_ptr<MeshData>> meshes;

	// floor box mesh
	glm::mat4 transform = glm::mat4(1.0f);
	meshes.push_back(MeshData::GenerateBox(device, { 1.0f, 0.2f, 1.0f }, transform));

	// red box mesh
	transform = glm::translate(glm::rotate(glm::mat4(1.0f), glm::radians(30.0f), { 0.0f, 1.0f, 0.0f }), { 1.0f, 1.0f, 0.0f });
	meshes.push_back(MeshData::GenerateBox(device, { 0.2f, 0.2f, 0.2f }, transform));

	// green ball mesh
	meshes.push_back(MeshData::GenerateSphereMesh(device, 0.3f, 30, 30, { 0.0f, 0.5f, 0.0f }));

	return meshes;
}

// GLTF MODEL LOADING
// - tinygltf

GLTFTexture::GLTFTexture()
	: Device(VI_NULL), Image(VI_NULL)
{
}

GLTFTexture::~GLTFTexture()
{
	if (Image != VI_NULL)
		vi_destroy_image(Device, Image);
}

void GLTFTexture::LoadFromImage(tinygltf::Image& gltf, VIDevice device)
{
	Device = device;

	assert(gltf.component == 4);

	uint32_t imageSize = gltf.width * gltf.height * 4;

	VIImageInfo imageI = MakeImageInfo2D(VI_FORMAT_RGBA8, gltf.width, gltf.height, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	imageI.usage = VI_IMAGE_USAGE_TRANSFER_DST_BIT | VI_IMAGE_USAGE_SAMPLED_BIT;
	Image = CreateImageStaged(device, &imageI, gltf.image.data(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

GLTFMaterial::GLTFMaterial()
{
}

GLTFMaterial::~GLTFMaterial()
{
	if (UBO)
		vi_destroy_buffer(Device, UBO);
}

GLTFModel::GLTFModel(VIDevice device)
	: mDevice(device)
{
	uint32_t pixel = 0xFFFFFFFF;
	VIImageInfo imageI = MakeImageInfo2D(VI_FORMAT_RGBA8, 1, 1, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	imageI.usage = VI_IMAGE_USAGE_TRANSFER_DST_BIT | VI_IMAGE_USAGE_SAMPLED_BIT;
	mEmptyTexture.Device = mDevice;
	mEmptyTexture.Image = CreateImageStaged(mDevice, &imageI, &pixel, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

GLTFModel::~GLTFModel()
{
	if (mSetPool)
		FreeSets();

	if (mVBO)
		vi_destroy_buffer(mDevice, mVBO);

	if (mIBO)
		vi_destroy_buffer(mDevice, mIBO);

	vi_destroy_image(mDevice, mEmptyTexture.Image);
	mEmptyTexture.Image = VI_NULL;
}

void GLTFModel::Draw(VICommand cmd, VIPipelineLayout layout)
{
	vi_cmd_bind_vertex_buffers(cmd, 0, 1, &mVBO);
	vi_cmd_bind_index_buffer(cmd, mIBO, VK_INDEX_TYPE_UINT32);

	mDrawMaterial = nullptr;
	mDrawPipelineLayout = layout;

	for (GLTFNode* node : mRootNodes)
		DrawNode(cmd, node);
}

void GLTFModel::DrawNode(VICommand cmd, GLTFNode* node)
{
	if (node->Mesh)
	{
		for (const GLTFPrimitive& prim : node->Mesh->Primitives)
		{
			assert(prim.Material);

			if (mDrawMaterial != prim.Material)
			{
				mDrawMaterial = prim.Material;
				vi_cmd_bind_graphics_set(cmd, mDrawPipelineLayout, 1, prim.Material->Set);
			}

			glm::mat4 transform = node->Transform;
			vi_cmd_push_constants(cmd, mDrawPipelineLayout, 0, sizeof(transform), &transform);

			VIDrawIndexedInfo drawI;
			drawI.index_count = prim.IndexCount;
			drawI.index_start = prim.IndexStart;
			drawI.instance_count = 1;
			drawI.instance_start = 0;
			vi_cmd_draw_indexed(cmd, &drawI);
		}
	}

	for (GLTFNode* child : node->Children)
		DrawNode(cmd, child);
}

std::shared_ptr<GLTFModel> GLTFModel::LoadFromFile(const char* path, VIDevice device, VISetLayout materialSL)
{
	std::shared_ptr<GLTFModel> model = std::make_shared<GLTFModel>(device);
	model->mMaterialSetLayout = materialSL;

	tinygltf::TinyGLTF loader;
	tinygltf::Model tinyModel;
	std::string err, warn;
	bool result = loader.LoadASCIIFromFile(&tinyModel, &err, &warn, path);

	if (!warn.empty())
		std::cout << "GLTFModel: " << warn << std::endl;

	if (!err.empty())
		std::cout << "GLTFModel: " << err << std::endl;

	if (!result)
		return nullptr;

	model->Load(tinyModel);
	return model;
}

void GLTFModel::Load(tinygltf::Model& tinyModel)
{
	LoadImages(tinyModel);
	LoadMaterials(tinyModel);

	const tinygltf::Scene& tinyScene = tinyModel.scenes[tinyModel.defaultScene > -1 ? tinyModel.defaultScene : 0];

	mVertexCount = 0;
	mIndexCount = 0;

	for (size_t i = 0; i < tinyScene.nodes.size(); i++)
		ScanNodePrimitives(tinyModel, tinyModel.nodes[tinyScene.nodes[i]], mVertexCount, mIndexCount);

	mVertices = new GLTFVertex[mVertexCount];
	mIndices = new uint32_t[mIndexCount];
	mVertexBase = 0;
	mIndexBase = 0;

	for (size_t i = 0; i < tinyScene.nodes.size(); i++)
	{
		uint32_t nodeIndex = tinyScene.nodes[i];
		LoadNode(tinyModel, tinyModel.nodes[nodeIndex], nodeIndex, nullptr);
	}

	VIBufferInfo bufferI;
	bufferI.properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
	bufferI.usage = VI_BUFFER_USAGE_TRANSFER_DST_BIT;
	bufferI.type = VI_BUFFER_TYPE_VERTEX;
	bufferI.size = sizeof(GLTFVertex) * mVertexCount;
	mVBO = CreateBufferStaged(mDevice, &bufferI, mVertices);

	bufferI.type = VI_BUFFER_TYPE_INDEX;
	bufferI.size = sizeof(uint32_t) * mIndexCount;
	mIBO = CreateBufferStaged(mDevice, &bufferI, mIndices);

	delete[]mVertices;
	delete[]mIndices;
	mVertices = nullptr;
	mIndices = nullptr;

	AllocateSets();
}

void GLTFModel::LoadImages(tinygltf::Model& tinyModel)
{
	Application* app = Application::Get();

	mTextures.resize(tinyModel.images.size());

	for (size_t i = 0; i < mTextures.size(); i++)
	{
		mTextures[i].LoadFromImage(tinyModel.images[i], mDevice);
		mTextures[i].Index = i;
	}
}

void GLTFModel::LoadMaterials(tinygltf::Model& tinyModel)
{
	Application* app = Application::Get();

	mMaterials.resize(tinyModel.materials.size());

	for (size_t i = 0; i < mMaterials.size(); i++)
	{
		tinygltf::Material& tinyMat = tinyModel.materials[i];
		GLTFMaterialUBO ubo;
		GLTFMaterial& mat = mMaterials[i];
		mat.Device = mDevice;

		if (tinyMat.values.find("baseColorFactor") != tinyMat.values.end())
		{
			mat.BaseColorFactor = glm::make_vec4(tinyMat.values["baseColorFactor"].ColorFactor().data());
		}

		if (tinyMat.values.find("roughnessFactor") != tinyMat.values.end())
		{
			float value = static_cast<float>(tinyMat.values["roughnessFactor"].number_value);
			mat.RoughnessFactor = value;
			// TODO: ubo
		}

		if (tinyMat.values.find("metallicFactor") != tinyMat.values.end())
		{
			float value = static_cast<float>(tinyMat.values["metallicFactor"].number_value);
			mat.MetallicFactor = value;
			// TODO: ubo
		}

		if (tinyMat.values.find("baseColorTexture") != tinyMat.values.end())
		{
			uint32_t source = tinyModel.textures[tinyMat.values["baseColorTexture"].TextureIndex()].source;
			mat.BaseColorTexture = mTextures.data() + source;
			ubo.HasColorMap = true;
		}
		else
		{
			mat.BaseColorTexture = &mEmptyTexture;
			ubo.HasColorMap = false;
		}

		if (tinyMat.additionalValues.find("normalTexture") != tinyMat.additionalValues.end())
		{
			uint32_t source = tinyModel.textures[tinyMat.additionalValues["normalTexture"].TextureIndex()].source;
			mat.NormalTexture = mTextures.data() + source;
			ubo.HasNormalMap = true;
		}
		else
		{
			mat.NormalTexture = &mEmptyTexture;
			ubo.HasNormalMap = false;
		}

		if (tinyMat.additionalValues.find("emissiveTexture") != tinyMat.additionalValues.end())
		{
			uint32_t source = tinyModel.textures[tinyMat.additionalValues["emissiveTexture"].TextureIndex()].source;
			mat.EmissiveTexture = mTextures.data() + source;
		}

		if (tinyMat.additionalValues.find("occlusionTexture") != tinyMat.additionalValues.end())
		{
			uint32_t source = tinyModel.textures[tinyMat.additionalValues["occlusionTexture"].TextureIndex()].source;
			mat.OcclusionTexture = mTextures.data() + source;
		}

		if (tinyMat.values.find("metallicRoughnessTexture") != tinyMat.values.end())
		{
			uint32_t source = tinyModel.textures[tinyMat.values["metallicRoughnessTexture"].TextureIndex()].source;
			mat.MetallicRoughnessTexture = mTextures.data() + source;
			ubo.HasMetallicRoughnessMap = true;
		}
		else
		{
			mat.MetallicRoughnessTexture = &mEmptyTexture;
			ubo.HasMetallicRoughnessMap = false;
		}

		if (tinyMat.additionalValues.find("alphaMode") != tinyMat.additionalValues.end())
		{
			std::string& value = tinyMat.additionalValues["alphaMode"].string_value;
			if (value == "BLEND")
				mat.AlphaMode = GLTF_ALPHA_MODE_BLEND;
			else if (value == "MASK")
				mat.AlphaMode = GLTF_ALPHA_MODE_MASK;
		}

		if (tinyMat.additionalValues.find("alphaCutoff") != tinyMat.additionalValues.end())
		{
			float value = static_cast<float>(tinyMat.additionalValues["alphaCutoff"].number_value);
			mat.AlphaCutoff = value;
		}

		VIBufferInfo bufferI;
		bufferI.properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		bufferI.type = VI_BUFFER_TYPE_UNIFORM;
		bufferI.size = sizeof(ubo);
		bufferI.usage = VI_BUFFER_USAGE_TRANSFER_DST_BIT;
		mat.UBO = CreateBufferStaged(mat.Device, &bufferI, &ubo);
	}
}

void GLTFModel::LoadNode(tinygltf::Model& tinyModel, tinygltf::Node& tinyNode, uint32_t nodeIndex, GLTFNode* parent)
{
	GLTFNode* node = new GLTFNode();
	node->Index = nodeIndex;
	node->Parent = parent;
	node->Name = tinyNode.name;
	node->Transform = glm::mat4(1.0f);
	node->Mesh = nullptr;

	mNodes.push_back(node);

	if (tinyNode.translation.size() == 3)
		node->Translation = glm::make_vec3(tinyNode.translation.data());

	if (tinyNode.rotation.size() == 4)
	{
		glm::quat q = glm::make_quat(tinyNode.rotation.data());
		node->Rotation = glm::mat4(q); // ????
	}

	if (tinyNode.scale.size() == 3)
		node->Scale = glm::make_vec3(tinyNode.scale.data());

	if (tinyNode.matrix.size() == 16)
		node->Transform = glm::make_mat4x4(tinyNode.matrix.data());

	for (size_t i = 0; i < tinyNode.children.size(); i++)
		LoadNode(tinyModel, tinyModel.nodes[tinyNode.children[i]], tinyNode.children[i], node);

	if (tinyNode.mesh >= 0)
		node->Mesh = LoadMesh(tinyModel, tinyModel.meshes[tinyNode.mesh]);

	if (parent)
		parent->Children.push_back(node);
	else
		mRootNodes.push_back(node);
}

GLTFMesh* GLTFModel::LoadMesh(tinygltf::Model& tinyModel, tinygltf::Mesh& tinyMesh)
{
	GLTFMesh* mesh = new GLTFMesh();

	mesh->Primitives.resize(tinyMesh.primitives.size());

	for (size_t i = 0; i < tinyMesh.primitives.size(); i++)
	{
		tinygltf::Primitive &tinyPrim = tinyMesh.primitives[i];
		uint32_t vertexBase = mVertexBase;
		uint32_t indexBase = mIndexBase;
		uint32_t indexCount = 0;
		uint32_t vertexCount = 0;

		// load vertices
		{
			const float *bufferPos = nullptr;
			const float *bufferNormals = nullptr;
			const float *bufferTexCoordSet0 = nullptr;
			int posByteStride;
			int normByteStride;
			int uv0ByteStride;

			assert(tinyPrim.attributes.find("POSITION") != tinyPrim.attributes.end());
			{
				tinygltf::Accessor& posAccessor = tinyModel.accessors[tinyPrim.attributes.find("POSITION")->second];
				tinygltf::BufferView& posView = tinyModel.bufferViews[posAccessor.bufferView];
				bufferPos = reinterpret_cast<const float *>(&(tinyModel.buffers[posView.buffer].data[posAccessor.byteOffset + posView.byteOffset]));
				vertexCount = (uint32_t)posAccessor.count;
				posByteStride = posAccessor.ByteStride(posView) ? (posAccessor.ByteStride(posView) / sizeof(float)) : tinygltf::GetNumComponentsInType(TINYGLTF_TYPE_VEC3);
			}

			if (tinyPrim.attributes.find("NORMAL") != tinyPrim.attributes.end())
			{
				tinygltf::Accessor& normAccessor = tinyModel.accessors[tinyPrim.attributes.find("NORMAL")->second];
				tinygltf::BufferView& normView = tinyModel.bufferViews[normAccessor.bufferView];
				bufferNormals = reinterpret_cast<const float *>(&(tinyModel.buffers[normView.buffer].data[normAccessor.byteOffset + normView.byteOffset]));
				normByteStride = normAccessor.ByteStride(normView) ? (normAccessor.ByteStride(normView) / sizeof(float)) : tinygltf::GetNumComponentsInType(TINYGLTF_TYPE_VEC3);
			}

			if (tinyPrim.attributes.find("TEXCOORD_0") != tinyPrim.attributes.end())
			{
				tinygltf::Accessor& uvAccessor = tinyModel.accessors[tinyPrim.attributes.find("TEXCOORD_0")->second];
				tinygltf::BufferView& uvView = tinyModel.bufferViews[uvAccessor.bufferView];
				bufferTexCoordSet0 = reinterpret_cast<const float *>(&(tinyModel.buffers[uvView.buffer].data[uvAccessor.byteOffset + uvView.byteOffset]));
				uv0ByteStride = uvAccessor.ByteStride(uvView) ? (uvAccessor.ByteStride(uvView) / sizeof(float)) : tinygltf::GetNumComponentsInType(TINYGLTF_TYPE_VEC2);
			}

			//assert(tinyPrim.attributes.find("TEXCOORD_1") == tinyPrim.attributes.end());
			assert(tinyPrim.attributes.find("COLOR_0") == tinyPrim.attributes.end());

			for (uint32_t v = 0; v < vertexCount; v++)
			{
				GLTFVertex& vert = mVertices[mVertexBase++];
				vert.Position = glm::vec4(glm::make_vec3(&bufferPos[v * posByteStride]), 1.0f);
				vert.Normal = glm::normalize(glm::vec3(bufferNormals ? glm::make_vec3(&bufferNormals[v * normByteStride]) : glm::vec3(0.0f)));
				vert.TextureUV = bufferTexCoordSet0 ? glm::make_vec2(&bufferTexCoordSet0[v * uv0ByteStride]) : glm::vec3(0.0f);
			}
		}

		// load indices
		if (tinyPrim.indices >= 0)
		{
			tinygltf::Accessor& accessor = tinyModel.accessors[tinyPrim.indices];
			tinygltf::BufferView& bufferView = tinyModel.bufferViews[accessor.bufferView];
			tinygltf::Buffer& buffer = tinyModel.buffers[bufferView.buffer];

			indexCount = (uint32_t)accessor.count;
			const void* dataPtr = &(buffer.data[accessor.byteOffset + bufferView.byteOffset]);

			switch (accessor.componentType)
			{
			case TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT: {
				const uint32_t* buf = (const uint32_t*)dataPtr;
				for (size_t index = 0; index < indexCount; index++)
					mIndices[mIndexBase++] = buf[index] + vertexBase;
				break;
			}
			case TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT: {
				const uint16_t* buf = (const uint16_t*)dataPtr;
				for (size_t index = 0; index < indexCount; index++)
					mIndices[mIndexBase++] = buf[index] + vertexBase;
				break;
			}
			case TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE: {
				const uint8_t* buf = (const uint8_t*)dataPtr;
				for (size_t index = 0; index < indexCount; index++)
					mIndices[mIndexBase++] = buf[index] + vertexBase;
				break;
			}
			default:
				assert(0 && "unsupported component type");
				return nullptr;
			}
		}

		mesh->Primitives[i].IndexStart = indexBase;
		mesh->Primitives[i].IndexCount = indexCount;
		mesh->Primitives[i].VertexCount = vertexCount;
		mesh->Primitives[i].Material = tinyPrim.material >= 0 ? (mMaterials.data() + tinyPrim.material) : nullptr;
	}

	return mesh;
}

void GLTFModel::ScanNodePrimitives(tinygltf::Model& tinyModel, tinygltf::Node& tinyNode, uint32_t& vertexCount, uint32_t& indexCount)
{
	if (tinyNode.children.size() > 0)
	{
		for (size_t i = 0; i < tinyNode.children.size(); i++)
			ScanNodePrimitives(tinyModel, tinyModel.nodes[tinyNode.children[i]], vertexCount, indexCount);
	}

	if (tinyNode.mesh >= 0)
	{
		tinygltf::Mesh& tinyMesh = tinyModel.meshes[tinyNode.mesh];

		for (size_t i = 0; i < tinyMesh.primitives.size(); i++)
		{
			tinygltf::Primitive& tinyPrim = tinyMesh.primitives[i];
			vertexCount += tinyModel.accessors[tinyPrim.attributes.find("POSITION")->second].count;

			if (tinyPrim.indices >= 0)
				indexCount += tinyModel.accessors[tinyPrim.indices].count;
		}
	}
}

void GLTFModel::AllocateSets()
{
	// Allocate Material Sets
	std::array<VISetPoolResource, 2> resources{};
	resources[0].type = VI_SET_BINDING_TYPE_COMBINED_IMAGE_SAMPLER;
	resources[0].count = 3 * mMaterials.size();
	resources[1].type = VI_SET_BINDING_TYPE_UNIFORM_BUFFER;
	resources[1].count = mMaterials.size();

	VISetPoolInfo poolI;
	poolI.max_set_count = mMaterials.size();
	poolI.resource_count = resources.size();
	poolI.resources = resources.data();
	mSetPool = vi_create_set_pool(mDevice, &poolI);

	for (GLTFMaterial& mat : mMaterials)
	{
		mat.Set = vi_alloc_set(mDevice, mSetPool, mMaterialSetLayout);
		std::array<VISetUpdateInfo, 4> updates{};
		updates[0] = { 0, mat.UBO, VI_NULL };
		updates[1] = { 1, VI_NULL, mat.BaseColorTexture->Image };
		updates[2] = { 2, VI_NULL, mat.NormalTexture->Image };
		updates[3] = { 3, VI_NULL, mat.MetallicRoughnessTexture->Image };
		vi_set_update(mat.Set, updates.size(), updates.data());
	}
}

void GLTFModel::FreeSets()
{
	for (GLTFMaterial& mat : mMaterials)
		vi_free_set(mDevice, mat.Set);

	vi_destroy_set_pool(mDevice, mSetPool);
}