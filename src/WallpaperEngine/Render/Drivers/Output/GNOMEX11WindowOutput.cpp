#include "GNOMEX11WindowOutput.h"
#include "GLFWOutputViewport.h"
#include "WallpaperEngine/Logging/Log.h"

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>
#include <X11/Xutil.h>

#define GLFW_EXPOSE_NATIVE_X11
#include <GLFW/glfw3native.h>

#include "WallpaperEngine/Render/Drivers/GLFWOpenGLDriver.h"

using namespace WallpaperEngine::Render::Drivers::Output;

/* -------------------------------------------------------------------------- */
/*  Constructor / Destructor                                                  */
/* -------------------------------------------------------------------------- */

GNOMEX11WindowOutput::GNOMEX11WindowOutput (ApplicationContext& context, VideoDriver& driver) :
	Output (context, driver) {
	// Open X11 display so we can inspect the screen layout.
	this->m_display = XOpenDisplay (nullptr);

	if (this->m_display == nullptr) {
		sLog.exception ("GNOME X11 desktop mode requires a running X server");
	}

	this->discoverOutputs ();
}

GNOMEX11WindowOutput::~GNOMEX11WindowOutput () { this->freeX11Resources (); }

/* -------------------------------------------------------------------------- */
/*  Output interface                                                          */
/* -------------------------------------------------------------------------- */

void GNOMEX11WindowOutput::reset () {
	// Re-discover outputs (handles monitor layout changes).
	// We don't recreate the X display on reset to keep it lightweight.
	for (const auto& screen : this->m_screens) {
		delete screen;
	}
	this->m_screens.clear ();
	this->m_viewports.clear ();
	this->discoverOutputs ();
}

bool GNOMEX11WindowOutput::renderVFlip () const {
	// Window framebuffer: OpenGL renders upside-down relative to the
	// window surface; tell the render pipeline to flip vertically.
	return true;
}

bool GNOMEX11WindowOutput::renderMultiple () const {
	return this->m_viewports.size () > 1;
}

bool GNOMEX11WindowOutput::haveImageBuffer () const { return false; }

void* GNOMEX11WindowOutput::getImageBuffer () const { return nullptr; }

uint32_t GNOMEX11WindowOutput::getImageBufferSize () const { return 0; }

void GNOMEX11WindowOutput::updateRender () const {
	// Track framebuffer size for viewport calculations.
	auto& glfwDriver = dynamic_cast<GLFWOpenGLDriver&> (this->m_driver);
	this->m_fullWidth  = glfwDriver.getFramebufferSize ().x;
	this->m_fullHeight = glfwDriver.getFramebufferSize ().y;


		// Recover from Show Desktop (Win+D): Mutter minimizes NORMAL-type
		// windows, so re-map ours if it disappeared.
		if (this->m_x11Window != None) {
			XWindowAttributes attrs;
			if (XGetWindowAttributes (this->m_display, this->m_x11Window, &attrs)
				&& attrs.map_state == IsUnmapped) {
				XMapWindow (this->m_display, this->m_x11Window);
				XLowerWindow (this->m_display, this->m_x11Window);
			}
		}
	// Re-map the default viewport to cover the current framebuffer.
	auto vpIt = this->m_viewports.find ("default");
	if (vpIt != this->m_viewports.end ()) {
		vpIt->second->viewport    = {0, 0, this->m_fullWidth, this->m_fullHeight};
		vpIt->second->logicalSize = {this->m_fullWidth, this->m_fullHeight};
	}
}

/* -------------------------------------------------------------------------- */
/*  EWMH desktop configuration                                                */
/* -------------------------------------------------------------------------- */

void GNOMEX11WindowOutput::configureDesktopWindow () {
	if (this->m_desktopConfigured) {
		return;
	}
	this->m_desktopConfigured = true;

	auto& glfwDriver = dynamic_cast<GLFWOpenGLDriver&> (this->m_driver);
	GLFWwindow* glfwWindow = glfwDriver.getWindow ();
	if (glfwWindow == nullptr) {
		sLog.exception ("Cannot configure GNOME desktop window: no GLFW window");
	}

	this->m_x11Window = glfwGetX11Window (glfwWindow);
	if (this->m_x11Window == None) {
		sLog.exception ("Cannot configure GNOME desktop window: no X11 handle");
	}

	// ---- EWMH atoms ---------------------------------------------------
	this->setupEWMHProperties ();

	// ---- WM_HINTS: do not take keyboard focus --------------------------
	XWMHints wmHints;
	wmHints.flags = InputHint;
	wmHints.input = False;
	XSetWMHints (this->m_display, this->m_x11Window, &wmHints);

	// ---- Position and size: cover the bounding box of all viewports ----
	int minX = 0, minY = 0, maxX = 0, maxY = 0;
	bool first = true;

	for (const auto& [name, vp] : this->m_viewports) {
		const int x = vp->globalPosition.x;
		const int y = vp->globalPosition.y;
		const int w = vp->logicalSize.x;
		const int h = vp->logicalSize.y;

		if (first) {
			minX = x; minY = y; maxX = x + w; maxY = y + h;
			first = false;
		} else {
			if (x < minX) minX = x;
			if (y < minY) minY = y;
			if (x + w > maxX) maxX = x + w;
			if (y + h > maxY) maxY = y + h;
		}
	}

	const int winW = maxX - minX;
	const int winH = maxY - minY;

	sLog.out ("GNOME X11 desktop window: ", minX, "x", minY, " ", winW, "x", winH);

		// ---- Show the window first (GLFW maps with 640x480) ------------
		glfwDriver.showWindow ();

		// ---- Resize to cover the full desktop after GLFW has mapped it -
		XMoveResizeWindow (this->m_display, this->m_x11Window, minX, minY, winW, winH);

		XSizeHints sizeHints;
		sizeHints.flags      = PPosition | PSize | PMinSize | PMaxSize;
		sizeHints.x          = minX;
		sizeHints.y          = minY;
		sizeHints.width      = winW;
		sizeHints.height     = winH;
		sizeHints.min_width  = winW;
		sizeHints.max_width  = winW;
		sizeHints.min_height = winH;
		sizeHints.max_height = winH;
		XSetWMNormalHints (this->m_display, this->m_x11Window, &sizeHints);

		// ---- Push window below normal windows ------------------------------
		XLowerWindow (this->m_display, this->m_x11Window);
		XFlush (this->m_display);

	sLog.out ("GNOME X11 desktop window configured successfully");
}

/* -------------------------------------------------------------------------- */
/*  Private helpers                                                           */
/* -------------------------------------------------------------------------- */

void GNOMEX11WindowOutput::setupEWMHProperties () {
	// Build the _NET_WM_STATE atom list.
	Atom net_wm_state         = XInternAtom (this->m_display, "_NET_WM_STATE",         False);
	Atom net_wm_state_below   = XInternAtom (this->m_display, "_NET_WM_STATE_BELOW",   False);
	Atom net_wm_state_sticky  = XInternAtom (this->m_display, "_NET_WM_STATE_STICKY",  False);
	Atom net_wm_skip_taskbar  = XInternAtom (this->m_display, "_NET_WM_STATE_SKIP_TASKBAR", False);
	Atom net_wm_skip_pager    = XInternAtom (this->m_display, "_NET_WM_STATE_SKIP_PAGER",   False);

	Atom states[] = {
		net_wm_state_below,
		net_wm_state_sticky,
		net_wm_skip_taskbar,
		net_wm_skip_pager,
	};

	XChangeProperty (
		this->m_display, this->m_x11Window,
		net_wm_state, XA_ATOM, 32,
		PropModeReplace,
		reinterpret_cast<unsigned char*> (states),
		sizeof (states) / sizeof (states[0])
	);

	// _NET_WM_DESKTOP = 0xFFFFFFFF → visible on all desktops / workspaces.
	Atom net_wm_desktop = XInternAtom (this->m_display, "_NET_WM_DESKTOP", False);
	long desktopAll = 0xFFFFFFFF;
	XChangeProperty (
		this->m_display, this->m_x11Window,
		net_wm_desktop, XA_CARDINAL, 32,
		PropModeReplace,
		reinterpret_cast<unsigned char*> (&desktopAll), 1
	);


	sLog.out ("EWMH desktop properties applied to window 0x", std::hex, this->m_x11Window);
}

void GNOMEX11WindowOutput::discoverOutputs () {
	int xrandrEvent, xrandrError;
	if (!XRRQueryExtension (this->m_display, &xrandrEvent, &xrandrError)) {
		sLog.error ("XRandr not available, GNOME X11 desktop will use default geometry");
		this->m_fullWidth  = 1920;
		this->m_fullHeight = 1080;
		auto* vp = new GLFWOutputViewport {{0, 0, 1920, 1080}, "default"};
		vp->globalPosition = {0, 0};
		vp->logicalSize    = {1920, 1080};
		this->m_screens.push_back (vp);
		this->m_viewports["default"] = vp;
		return;
	}

	Window root = DefaultRootWindow (this->m_display);
	XRRScreenResources* screenRes = XRRGetScreenResources (this->m_display, root);
	if (screenRes == nullptr) {
		sLog.error ("Cannot read XRandR screen resources, using fallback geometry");
		this->m_fullWidth  = 1920;
		this->m_fullHeight = 1080;
		auto* vp = new GLFWOutputViewport {{0, 0, 1920, 1080}, "default"};
		vp->globalPosition = {0, 0};
		vp->logicalSize    = {1920, 1080};
		this->m_screens.push_back (vp);
		this->m_viewports["default"] = vp;
		return;
	}

		// Compute bounding box of all active XRandR outputs and create
		// a single "default" viewport covering the entire desktop.
		int bbMinX = 0, bbMinY = 0, bbMaxX = 0, bbMaxY = 0;
		bool anyOutput = false;

		for (int i = 0; i < screenRes->noutput; i++) {
			XRROutputInfo* info = XRRGetOutputInfo (this->m_display, screenRes, screenRes->outputs[i]);
			if (info == nullptr || info->connection != RR_Connected) {
				if (info) XRRFreeOutputInfo (info);
				continue;
			}

			XRRCrtcInfo* crtc = XRRGetCrtcInfo (this->m_display, screenRes, info->crtc);
			if (crtc == nullptr) {
				XRRFreeOutputInfo (info);
				continue;
			}

			sLog.out ("GNOME X11: output ", info->name, " at ", crtc->x, "x", crtc->y,
			          " ", crtc->width, "x", crtc->height);

			if (!anyOutput) {
				bbMinX = crtc->x; bbMinY = crtc->y;
				bbMaxX = crtc->x + crtc->width;
				bbMaxY = crtc->y + crtc->height;
				anyOutput = true;
			} else {
				if (crtc->x < bbMinX) bbMinX = crtc->x;
				if (crtc->y < bbMinY) bbMinY = crtc->y;
				if ((int) (crtc->x + crtc->width)  > bbMaxX) bbMaxX = (int) (crtc->x + crtc->width);
				if ((int) (crtc->y + crtc->height) > bbMaxY) bbMaxY = (int) (crtc->y + crtc->height);
			}

			XRRFreeCrtcInfo (crtc);
			XRRFreeOutputInfo (info);
		}

		XRRFreeScreenResources (screenRes);

		if (!anyOutput) {
			sLog.error ("No active XRandR outputs, falling back to 1920x1080");
			bbMinX = 0; bbMinY = 0; bbMaxX = 1920; bbMaxY = 1080;
		}

		int winW = bbMaxX - bbMinX;
		int winH = bbMaxY - bbMinY;

		auto* vp = new GLFWOutputViewport {{bbMinX, bbMinY, winW, winH}, "default"};
		vp->globalPosition = {bbMinX, bbMinY};
		vp->logicalSize    = {winW, winH};
		this->m_screens.push_back (vp);
		this->m_viewports["default"] = vp;

		sLog.out ("GNOME X11 desktop bounding box: ", bbMinX, "x", bbMinY,
		          " ", winW, "x", winH);
}

void GNOMEX11WindowOutput::freeX11Resources () {
	for (const auto& screen : this->m_screens) {
		delete screen;
	}
	this->m_screens.clear ();
	this->m_viewports.clear ();

	if (this->m_display != nullptr) {
		XCloseDisplay (this->m_display);
		this->m_display = nullptr;
	}
}
