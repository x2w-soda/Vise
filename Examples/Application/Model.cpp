#include "Model.h"
#include "Application.h"
#include <glm/gtc/matrix_transform.hpp>

Mesh::Mesh()
    : VBO(VI_NULL)
    , IBO(VI_NULL)
{
}

Mesh::~Mesh()
{
	if (VBO)
		vi_destroy_buffer(Device, VBO);

	if (IBO)
		vi_destroy_buffer(Device, IBO);
}

std::shared_ptr<Mesh> Mesh::GenerateBox(VIDevice device, const glm::vec3& halfExtent, const glm::vec3& color, const glm::mat4& transform)
{
	auto mesh = std::make_shared<Mesh>();
	mesh->Device = device;

    int idxBase = 0;
    MeshVertex p0, p1, p2, p3;
    p0.TextureUV = { 0.0f, 0.0f };
    p1.TextureUV = { 0.0f, 0.0f };
    p2.TextureUV = { 0.0f, 0.0f };
    p3.TextureUV = { 0.0f, 0.0f };
    p0.Albedo = color;
    p1.Albedo = color;
    p2.Albedo = color;
    p3.Albedo = color;

    std::vector<MeshVertex> vertices;
    std::vector<uint32_t> indices;

    for (int sign = -1; sign <= 1; sign += 2)
    {
        p0.Position = glm::vec3(transform * glm::vec4(sign * -halfExtent.x, sign * halfExtent.y, -halfExtent.z, 1.0f));
        p1.Position = glm::vec3(transform * glm::vec4(sign * -halfExtent.x, sign * halfExtent.y, halfExtent.z, 1.0f));
        p2.Position = glm::vec3(transform * glm::vec4(sign * halfExtent.x, sign * halfExtent.y,  halfExtent.z, 1.0f));
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
    Application* app = Application::Get();
	VIBufferInfo bufferI;
	bufferI.type = VI_BUFFER_TYPE_VERTEX;
	bufferI.usage = VI_BUFFER_USAGE_TRANSFER_DST_BIT;
	bufferI.size = sizeof(MeshVertex) * vertices.size();
	bufferI.properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
	mesh->VBO = app->CreateBufferStaged(device, &bufferI, vertices.data());

	bufferI.type = VI_BUFFER_TYPE_INDEX;
	bufferI.size = sizeof(uint32_t) * indices.size();
	mesh->IBO = app->CreateBufferStaged(device, &bufferI, indices.data());

    mesh->IndexCount = indices.size(); // 36

	return mesh;
}

std::shared_ptr<Mesh> Mesh::GenerateSphereMesh(VIDevice device, float radius, const glm::vec3& color, int stackCount, int sectorCount, const glm::vec3& position)
{
    auto mesh = std::make_shared<Mesh>();
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
            vertex.Albedo = color;
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

    Application* app = Application::Get();

    VIBufferInfo bufferI;
    bufferI.type = VI_BUFFER_TYPE_VERTEX;
    bufferI.usage = VI_BUFFER_USAGE_TRANSFER_DST_BIT;
    bufferI.size = sizeof(MeshVertex) * vertices.size();
    bufferI.properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    mesh->VBO = app->CreateBufferStaged(device, &bufferI, vertices.data());

    bufferI.type = VI_BUFFER_TYPE_INDEX;
    bufferI.size = sizeof(uint32_t) * indices.size();
    mesh->IBO = app->CreateBufferStaged(device, &bufferI, indices.data());

    mesh->IndexCount = indices.size();

    return mesh;
}

std::vector<std::shared_ptr<Mesh>> GenerateMeshSceneV1(VIDevice device)
{
    std::vector<std::shared_ptr<Mesh>> meshes;

    // floor box mesh
    glm::mat4 transform = glm::mat4(1.0f);
    meshes.push_back(Mesh::GenerateBox(device, { 1.0f, 0.2f, 1.0f }, { 0.9f, 0.9f, 0.9f }, transform));

    // red box mesh
    transform = glm::translate(glm::rotate(glm::mat4(1.0f), glm::radians(30.0f), { 0.0f, 1.0f, 0.0f}), {1.0f, 1.0f, 0.0f});
    meshes.push_back(Mesh::GenerateBox(device, { 0.2f, 0.2f, 0.2f }, { 0.8f, 0.1f, 0.1f }, transform));

    // green ball mesh
    meshes.push_back(Mesh::GenerateSphereMesh(device, 0.3f, { 0.1f, 0.8f, 0.1f }, 30, 30, { 0.0f, 0.5f, 0.0f }));

    return meshes;
}