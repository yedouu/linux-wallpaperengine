#pragma once

#include <X11/Xlib.h>
#include <vector>

#include "Output.h"

namespace WallpaperEngine::Render::Drivers::Output {

/**
 * GNOME X11 desktop window output.
 *
 * Creates a managed X11 window (via GLFW) and configures it as a desktop
 * background layer through EWMH properties:
 *   - _NET_WM_STATE_BELOW      (window sits below normal apps)
 *   - _NET_WM_STATE_STICKY      (visible on all workspaces)
 *   - _NET_WM_STATE_SKIP_TASKBAR
 *   - _NET_WM_STATE_SKIP_PAGER
 *   - _NET_WM_DESKTOP = 0xFFFFFFFF (all desktops)
 *   - WM_HINTS.input = False       (do not grab keyboard focus)
 *
 * The window geometry covers the bounding box of all configured XRandR
 * outputs so that each monitor gets a dedicated viewport for its wallpaper.
 */
class GNOMEX11WindowOutput final : public Output {
public:
	explicit GNOMEX11WindowOutput (ApplicationContext& context, VideoDriver& driver);
	~GNOMEX11WindowOutput () override;

	void reset () override;
	bool renderVFlip () const override;
	bool renderMultiple () const override;
	bool haveImageBuffer () const override;
	void* getImageBuffer () const override;
	uint32_t getImageBufferSize () const override;
	void updateRender () const override;

	/**
	 * Apply EWMH / X11 window properties after the GLFW window has been
	 * created and the native X11 handle is available.  Must be called
	 * exactly once, from the GLFW driver, after both the window and this
	 * output object are constructed.
	 */
	void configureDesktopWindow ();

	/**
	 * Re-map the desktop window if GNOME's "Show Desktop" (Win+D) unmapped
	 * it.  Independent of framebuffer size so it also runs while the window
	 * is hidden/iconified (framebuffer is 0 in that state).
	 */
	void ensureVisible () const;

private:
	/** Open X11 display and query XRandR outputs. */
	void discoverOutputs ();

	/** Set _NET_WM_STATE, _NET_WM_DESKTOP, WM_HINTS, etc. */
	void setupEWMHProperties ();

	/** Release X11 resources. */
	void freeX11Resources ();

	Display* m_display = nullptr;
	Window m_x11Window = None;
	bool m_desktopConfigured = false;
	/** Owning storage for viewport objects (m_viewports holds non-owning
	 *  pointers into this vector). */
	std::vector<OutputViewport*> m_screens = {};
};

} // namespace WallpaperEngine::Render::Drivers::Output
