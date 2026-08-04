#include <X11/Xlib.h>
#include "GLFWMouseInput.h"
#include <glm/common.hpp>

#include "WallpaperEngine/Render/Drivers/GLFWOpenGLDriver.h"
#define GLFW_EXPOSE_NATIVE_X11
#include <GLFW/glfw3native.h>

using namespace WallpaperEngine::Input::Drivers;

GLFWMouseInput::GLFWMouseInput (const Render::Drivers::GLFWOpenGLDriver& driver) : m_driver (driver) { }

void GLFWMouseInput::update () {
    if (!this->m_driver.getApp ().getContext ().settings.mouse.enabled) {
	this->m_reportedPosition = { 0, 0 };
	return;
    }

    const int leftClickState = glfwGetMouseButton (this->m_driver.getWindow (), GLFW_MOUSE_BUTTON_LEFT);
    const int rightClickState = glfwGetMouseButton (this->m_driver.getWindow (), GLFW_MOUSE_BUTTON_RIGHT);

    this->m_leftClick = leftClickState == GLFW_RELEASE ? MouseClickStatus::Released : MouseClickStatus::Clicked;
    this->m_rightClick = rightClickState == GLFW_RELEASE ? MouseClickStatus::Released : MouseClickStatus::Clicked;

    // update current mouse position
	// GNOME_X11 desktop: empty input shape → GLFW cursor stale.
	// Use XQueryPointer to get global cursor position instead.
	if (this->m_driver.getApp ().getContext ().settings.render.mode
		== WallpaperEngine::Application::ApplicationContext::GNOME_X11_DESKTOP_WINDOW) {
		GLFWwindow* glfwWin = this->m_driver.getWindow ();
		Window x11Win = glfwGetX11Window (glfwWin);
		if (x11Win != None) {
			Display* dpy = glfwGetX11Display ();
			Window root, child;
			int rootX, rootY, winX, winY;
			unsigned int mask;
			if (XQueryPointer (dpy, x11Win, &root, &child,
						&rootX, &rootY, &winX, &winY, &mask)) {
				this->m_mousePosition.x = winX;
				this->m_mousePosition.y = winY;
			}
		}
	} else {
		glfwGetCursorPos (this->m_driver.getWindow (), &this->m_mousePosition.x, &this->m_mousePosition.y);
	}

    // Convert from GLFW coordinate system (Y=0 at top) to OpenGL coordinate system (Y=0 at bottom)
    const glm::ivec2 framebufferSize = this->m_driver.getFramebufferSize ();
    this->m_mousePosition.y = static_cast<double> (framebufferSize.y) - this->m_mousePosition.y;

    // interpolate to the new position
    this->m_reportedPosition = glm::mix (this->m_reportedPosition, this->m_mousePosition, 1.0);
}

glm::dvec2 GLFWMouseInput::position () const { return this->m_reportedPosition; }

WallpaperEngine::Input::MouseClickStatus GLFWMouseInput::leftClick () const { return m_leftClick; }

WallpaperEngine::Input::MouseClickStatus GLFWMouseInput::rightClick () const { return m_rightClick; }