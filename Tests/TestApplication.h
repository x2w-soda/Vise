#pragma once

#include "../Examples/Application/Application.h"

#define TEST_WINDOW_WIDTH   512
#define TEST_WINDOW_HEIGHT  512

// regression testing is done by rendering a scene with both backends
// and comparing the difference between the two screenshots
class TestApplication : public Application
{
public:
	TestApplication(const char* name, VIBackend backend);
	TestApplication(const TestApplication&) = delete;
	virtual ~TestApplication();

	TestApplication& operator=(const Application&) = delete;

protected:
	VIPass mScreenshotPass;
	VIFramebuffer mScreenshotFBO;
	VIBuffer mScreenshotBuffer;
	VIImage mScreenshotImage;
};