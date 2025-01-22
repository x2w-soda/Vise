#pragma once

#include <chrono>
#include <cmath>
#include <cstdint>
#include <vector>
#include <vise.h>
#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define STR(A) STR_(A)
#define STR_(A) #A

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

class Camera
{
public:

	glm::vec3 GetPosition()
	{
		return mPosition;
	}

	void SetPosition(const glm::vec3 pos)
	{
		mPosition = pos;
	}

	void MoveLocalForward(float forward)
	{
		mPosition += mDirection * forward;
	}

	void MoveLocalUp(float up)
	{
		mPosition += mLocalUp * up;
	}

	void MoveLocalRight(float right)
	{
		mPosition += mLocalRight * right;
	}

	void RotateLocal(float pitch, float yaw)
	{
		mPitch = std::clamp(mPitch + pitch, -89.0f, 89.0f);
		mYaw += yaw;

		if (mYaw > 360.0f)
			mYaw -= 360.0f;
		if (mYaw < 0.0f)
			mYaw += 360.0f;
	}

	void Update()
	{
		glm::vec3 direction;
		direction.x = std::cos(glm::radians(mYaw)) * std::cos(glm::radians(mPitch));
		direction.y = std::sin(glm::radians(mPitch));
		direction.z = std::sin(glm::radians(mYaw)) * std::cos(glm::radians(mPitch));

		mDirection = glm::normalize(direction);
		mLocalRight = glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), mDirection);
		mLocalUp = glm::cross(mDirection, mLocalRight);

		glm::vec3 target = mPosition + mDirection;

		mView = glm::lookAt(mPosition, target, glm::vec3(0.0f, 1.0f, 0.0f));
		mProj = glm::perspective(glm::radians(fov), aspect, 0.1f, 100.0f);
	}

	glm::vec3 GetDirection()
	{
		return mDirection;
	}

	glm::mat4 GetViewMat()
	{
		return mView;
	}

	glm::mat4 GetProjMat()
	{
		return mProj;
	}

	float fov = 30.0f;
	float aspect = APP_WINDOW_ASPECT_RATIO;

private:
	glm::vec3 mDirection;
	glm::vec3 mLocalRight;
	glm::vec3 mLocalUp;
	glm::vec3 mPosition = { 0.0f, 0.0f, 0.0f };
	glm::mat4 mView;
	glm::mat4 mProj;
	float mPitch = 0.0f;
	float mYaw = 0.0f;
};