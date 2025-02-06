#include <cstring>
#include "Application.h"
#include "Common.h"
#include "../stb/stb_image.h"

static float vertices[] = {
		-1.0f,  1.0f, -1.0f,
		-1.0f, -1.0f, -1.0f,
		 1.0f, -1.0f, -1.0f,
		 1.0f, -1.0f, -1.0f,
		 1.0f,  1.0f, -1.0f,
		-1.0f,  1.0f, -1.0f,

		-1.0f, -1.0f,  1.0f,
		-1.0f, -1.0f, -1.0f,
		-1.0f,  1.0f, -1.0f,
		-1.0f,  1.0f, -1.0f,
		-1.0f,  1.0f,  1.0f,
		-1.0f, -1.0f,  1.0f,

		 1.0f, -1.0f, -1.0f,
		 1.0f, -1.0f,  1.0f,
		 1.0f,  1.0f,  1.0f,
		 1.0f,  1.0f,  1.0f,
		 1.0f,  1.0f, -1.0f,
		 1.0f, -1.0f, -1.0f,

		-1.0f, -1.0f,  1.0f,
		-1.0f,  1.0f,  1.0f,
		 1.0f,  1.0f,  1.0f,
		 1.0f,  1.0f,  1.0f,
		 1.0f, -1.0f,  1.0f,
		-1.0f, -1.0f,  1.0f,

		-1.0f,  1.0f, -1.0f,
		 1.0f,  1.0f, -1.0f,
		 1.0f,  1.0f,  1.0f,
		 1.0f,  1.0f,  1.0f,
		-1.0f,  1.0f,  1.0f,
		-1.0f,  1.0f, -1.0f,

		-1.0f, -1.0f, -1.0f,
		-1.0f, -1.0f,  1.0f,
		 1.0f, -1.0f, -1.0f,
		 1.0f, -1.0f, -1.0f,
		-1.0f, -1.0f,  1.0f,
		 1.0f, -1.0f,  1.0f,
};

const float* GetSkyboxVertices(uint32_t* vertexCount, uint32_t* byteSize, std::vector<VIVertexAttribute>* attrs, std::vector<VIVertexBinding>* bindings)
{
	if (vertexCount)
		*vertexCount = 36;

	if (byteSize)
		*byteSize = sizeof(vertices);

	if (attrs)
	{
		attrs->resize(1);
		attrs[0][0].binding = 0;
		attrs[0][0].offset = 0;
		attrs[0][0].type = VI_GLSL_TYPE_VEC3;
	}

	if (bindings)
	{
		bindings->resize(1);
		bindings[0][0].rate = VK_VERTEX_INPUT_RATE_VERTEX;
		bindings[0][0].stride = sizeof(float) * 3;
	}

	return vertices;
}

unsigned char* LoadCubemap(const std::string& path, int * dim)
{
	int width, height, ch;
	stbi_uc* face_pixels[6];

	const char* face_ext[]{
		"/px.jpg",
		"/nx.jpg",
		"/py.jpg",
		"/ny.jpg",
		"/pz.jpg",
		"/nz.jpg",
	};

	for (int i = 0; i < 6; i++)
	{
		std::string face = path;
		face += face_ext[i];
		face_pixels[i] = stbi_load(face.c_str(), &width, &height, &ch, STBI_rgb_alpha);
		assert(width == height);
	}

	size_t face_size = width * height * 4;
	unsigned char* pixels = new unsigned char[face_size * 6];

	for (int i = 0; i < 6; i++)
	{
		uint32_t offset = i * face_size;
		memcpy(pixels + offset, face_pixels[i], face_size);
		stbi_image_free(face_pixels[i]);
	}

	*dim = width;
	return pixels;
}

void FreeCubemap(unsigned char* pixels)
{
	delete[] pixels;
}

void Timer::Start()
{
	mStartTime = std::chrono::high_resolution_clock::now();
	mIsRunning = true;
}

void Timer::Stop()
{
	mEndTime = std::chrono::high_resolution_clock::now();
	mIsRunning = false;
}

double Timer::GetMilliSeconds()
{
	std::chrono::time_point<std::chrono::high_resolution_clock> endTime;

	if (mIsRunning)
		endTime = std::chrono::high_resolution_clock::now();
	else
		endTime = mEndTime;

	return std::chrono::duration_cast<std::chrono::microseconds>(endTime - mStartTime).count() / 1000.0;
}

double Timer::GetSeconds()
{
	return GetMilliSeconds() / 1000.0;
}