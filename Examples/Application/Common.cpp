#include <cstring>
#include "Common.h"

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