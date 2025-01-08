#pragma once

#include <chrono>
#include <cmath>
#include <cstdint>
#include <vector>
#include <vise.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

const float* GetSkyboxVertices(uint32_t* vertexCount, uint32_t* byteSize,
	std::vector<VIVertexAttribute>* attrs = nullptr, std::vector<VIVertexBinding>* bindings = nullptr);

class Timer
{
public:

    void Start();
    void Stop();

    double GetMilliSeconds();
    double GetSeconds();

private:
    std::chrono::time_point<std::chrono::high_resolution_clock> mStartTime;
    std::chrono::time_point<std::chrono::high_resolution_clock> mEndTime;
    bool mIsRunning = false;
};
