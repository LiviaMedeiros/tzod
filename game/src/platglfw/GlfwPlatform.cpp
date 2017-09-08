#include "inc/platglfw/GlfwPlatform.h"
#include "inc/platglfw/GlfwKeys.h"
#include <GLFW/glfw3.h>

vec2d GetCursorPosInPixels(GLFWwindow *window, double dipX, double dipY)
{
	int pxWidth;
	int pxHeight;
	glfwGetFramebufferSize(window, &pxWidth, &pxHeight);

	int dipWidth;
	int dipHeight;
	glfwGetWindowSize(window, &dipWidth, &dipHeight);

	return{ float(dipX * pxWidth / dipWidth), float(dipY * pxHeight / dipHeight) };
}

vec2d GetCursorPosInPixels(GLFWwindow *window)
{
	double dipX = 0;
	double dipY = 0;
	glfwGetCursorPos(window, &dipX, &dipY);
	return GetCursorPosInPixels(window, dipX, dipY);
}


GlfwInput::GlfwInput(GLFWwindow &window)
	: _window(window)
{}

bool GlfwInput::IsKeyPressed(UI::Key key) const
{
	int platformKey = UnmapGlfwKeyCode(key);
	return GLFW_PRESS == glfwGetKey(&_window, platformKey);
}

bool GlfwInput::IsMousePressed(int button) const
{
	return GLFW_PRESS == glfwGetMouseButton(&_window, button-1);
}

vec2d GlfwInput::GetMousePos() const
{
	return GetCursorPosInPixels(&_window);
}

UI::GamepadState GlfwInput::GetGamepadState() const
{
	UI::GamepadState gamepadState = {};
	if (glfwJoystickPresent(GLFW_JOYSTICK_1))
	{
		// XBox One/360 controller mapping

		int axesCount = 0;
		const float *axes = glfwGetJoystickAxes(GLFW_JOYSTICK_1, &axesCount);
		if (axesCount >= 6)
		{
			gamepadState.leftThumbstickPos.x = axes[0];
			gamepadState.leftThumbstickPos.y = -axes[1];
			gamepadState.rightThumbstickPos.x = axes[2];
			gamepadState.rightThumbstickPos.y = -axes[3];
			gamepadState.leftTrigger = axes[4];
			gamepadState.rightTrigger = axes[5];
		}

		int buttonsCount = 0;
		const unsigned char *buttons = glfwGetJoystickButtons(GLFW_JOYSTICK_1, &buttonsCount);
		if (buttonsCount >= 14)
		{
			gamepadState.A = !!buttons[0];
			gamepadState.B = !!buttons[1];
			gamepadState.X = !!buttons[2];
			gamepadState.Y = !!buttons[3];
			gamepadState.leftShoulder = !!buttons[4];
			gamepadState.rightShoulder = !!buttons[5];
			gamepadState.view = !!buttons[6];
			gamepadState.menu = !!buttons[7];
			gamepadState.leftThumbstickPressed = !!buttons[8];
			gamepadState.rightThumbstickPressed = !!buttons[9];
			gamepadState.DPadUp = !!buttons[10];
			gamepadState.DPadRight = !!buttons[11];
			gamepadState.DPadDown = !!buttons[12];
			gamepadState.DPadLeft = !!buttons[13];
		}
	}
	return gamepadState;
}


GlfwClipboard::GlfwClipboard(GLFWwindow &window)
	: _window(window)
{}

const char* GlfwClipboard::GetClipboardText() const
{
	return glfwGetClipboardString(&_window);
}

void GlfwClipboard::SetClipboardText(std::string text)
{
	glfwSetClipboardString(&_window, text.c_str());
}
