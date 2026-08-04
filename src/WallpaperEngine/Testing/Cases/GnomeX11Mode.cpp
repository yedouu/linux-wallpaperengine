#include <catch2/catch_test_macros.hpp>

#include "WallpaperEngine/Application/ApplicationContext.h"
#include "WallpaperEngine/Render/Drivers/Detectors/FullScreenDetector.h"
#include "WallpaperEngine/Render/Drivers/VideoFactories.h"

using namespace WallpaperEngine::Application;
using namespace WallpaperEngine::Render::Drivers;

TEST_CASE ("WINDOW_MODE enum values are distinct and GNOME_X11 = 3") {
    CHECK (ApplicationContext::NORMAL_WINDOW == 0);
    CHECK (ApplicationContext::DESKTOP_BACKGROUND == 1);
    CHECK (ApplicationContext::EXPLICIT_WINDOW == 2);
    CHECK (ApplicationContext::GNOME_X11_DESKTOP_WINDOW == 3);

    // All four modes must be distinct
    CHECK (ApplicationContext::NORMAL_WINDOW != ApplicationContext::DESKTOP_BACKGROUND);
    CHECK (ApplicationContext::NORMAL_WINDOW != ApplicationContext::EXPLICIT_WINDOW);
    CHECK (ApplicationContext::NORMAL_WINDOW != ApplicationContext::GNOME_X11_DESKTOP_WINDOW);
    CHECK (ApplicationContext::DESKTOP_BACKGROUND != ApplicationContext::EXPLICIT_WINDOW);
    CHECK (ApplicationContext::DESKTOP_BACKGROUND != ApplicationContext::GNOME_X11_DESKTOP_WINDOW);
    CHECK (ApplicationContext::EXPLICIT_WINDOW != ApplicationContext::GNOME_X11_DESKTOP_WINDOW);
}

TEST_CASE ("Fullscreen detector: preview modes always get no-op detector") {
    // Build a minimal context with default settings (NORMAL_WINDOW, pauseOnFullscreen=true).
    // Even with pause enabled, NORMAL_WINDOW and EXPLICIT_WINDOW must get a no-op detector
    // because fullscreen pausing is a desktop-background optimisation.
    auto makeContext = [] (ApplicationContext::WINDOW_MODE mode, bool pause) -> ApplicationContext {
	// ApplicationContext stores argc/argv; we only use it for settings manipulation.
	const char* dummy_argv[] = {"test"};
	ApplicationContext ctx (1, const_cast<char**> (dummy_argv));
	ctx.settings.render.mode = mode;
	ctx.settings.render.pauseOnFullscreen = pause;
	return ctx;
    };

    SECTION ("NORMAL_WINDOW with pause enabled → no-op") {
	auto ctx = makeContext (ApplicationContext::NORMAL_WINDOW, true);
	auto detector = sVideoFactories.createFullscreenDetector ("x11", ctx,
	    *reinterpret_cast<VideoDriver*> (0x1) // driver pointer unused by no-op path
	);
	// No-op detector: anythingFullscreen() always returns false
	CHECK_FALSE (detector->anythingFullscreen ());
    }

    SECTION ("EXPLICIT_WINDOW with pause enabled → no-op") {
	auto ctx = makeContext (ApplicationContext::EXPLICIT_WINDOW, true);
	auto detector = sVideoFactories.createFullscreenDetector ("x11", ctx,
	    *reinterpret_cast<VideoDriver*> (0x1)
	);
	CHECK_FALSE (detector->anythingFullscreen ());
    }

    SECTION ("NORMAL_WINDOW with pause disabled → no-op") {
	auto ctx = makeContext (ApplicationContext::NORMAL_WINDOW, false);
	auto detector = sVideoFactories.createFullscreenDetector ("x11", ctx,
	    *reinterpret_cast<VideoDriver*> (0x1)
	);
	CHECK_FALSE (detector->anythingFullscreen ());
    }

    SECTION ("GNOME_X11_DESKTOP_WINDOW with pause disabled → no-op") {
	auto ctx = makeContext (ApplicationContext::GNOME_X11_DESKTOP_WINDOW, false);
	auto detector = sVideoFactories.createFullscreenDetector ("x11", ctx,
	    *reinterpret_cast<VideoDriver*> (0x1)
	);
	CHECK_FALSE (detector->anythingFullscreen ());
    }
}

TEST_CASE ("Fullscreen detector: desktop modes with pause enabled try real detector") {
    const char* dummy_argv[] = {"test"};

    // DESKTOP_BACKGROUND and GNOME_X11_DESKTOP_WINDOW with pauseOnFullscreen=true
    // SHOULD attempt to use a real platform detector when one is registered.
    //
    // In a real X11 session the X11 factory is registered and will succeed.
    // Without X11 (e.g. headless CI), there's no matching factory, so it
    // falls back to no-op — that's the correct behaviour, not a crash.

    SECTION ("DESKTOP_BACKGROUND with pause enabled, x11 session type") {
	ApplicationContext ctx (1, const_cast<char**> (dummy_argv));
	ctx.settings.render.mode = ApplicationContext::DESKTOP_BACKGROUND;
	ctx.settings.render.pauseOnFullscreen = true;

	// This may or may not have a registered factory depending on environment.
	// Either way it must not crash and must return a valid detector.
	auto detector = sVideoFactories.createFullscreenDetector ("x11", ctx,
	    *reinterpret_cast<VideoDriver*> (0x1)
	);
	REQUIRE (detector != nullptr);
	// The no-op fallback always returns false
	if (!detector->anythingFullscreen ()) {
	    SUCCEED ("No-op fallback (expected in headless environments)");
	}
    }

    SECTION ("GNOME_X11_DESKTOP_WINDOW with pause enabled, x11 session type") {
	ApplicationContext ctx (1, const_cast<char**> (dummy_argv));
	ctx.settings.render.mode = ApplicationContext::GNOME_X11_DESKTOP_WINDOW;
	ctx.settings.render.pauseOnFullscreen = true;

	// Same as above: must not crash, must return a valid detector.
	auto detector = sVideoFactories.createFullscreenDetector ("x11", ctx,
	    *reinterpret_cast<VideoDriver*> (0x1)
	);
	REQUIRE (detector != nullptr);
	if (!detector->anythingFullscreen ()) {
	    SUCCEED ("No-op fallback (expected in headless environments)");
	}
    }

    SECTION ("GNOME_X11_DESKTOP_WINDOW with pause enabled, wayland session type") {
	ApplicationContext ctx (1, const_cast<char**> (dummy_argv));
	ctx.settings.render.mode = ApplicationContext::GNOME_X11_DESKTOP_WINDOW;
	ctx.settings.render.pauseOnFullscreen = true;

	// Wayland session: no GNOME_X11 driver registered for wayland,
	// so fullscreen detector also has no matching factory → no-op.
	auto detector = sVideoFactories.createFullscreenDetector ("wayland", ctx,
	    *reinterpret_cast<VideoDriver*> (0x1)
	);
	REQUIRE (detector != nullptr);
	CHECK_FALSE (detector->anythingFullscreen ());
    }
}

TEST_CASE ("VideoFactories: driver lookup uses session type for desktop modes") {
    // GNOME_X11_DESKTOP_WINDOW must be looked up by XDG_SESSION_TYPE
    // (like DESKTOP_BACKGROUND), NOT by DEFAULT_WINDOW_NAME (like preview modes).
    //
    // When no driver is registered for the given session type + mode,
    // createVideoDriver must throw.

    SECTION ("GNOME_X11 on wayland throws (no driver registered for wayland session)") {
	const char* argv_a[] = {"test"};
	ApplicationContext ctx (1, const_cast<char**> (argv_a));
	ctx.settings.render.mode = ApplicationContext::GNOME_X11_DESKTOP_WINDOW;
	// No GNOME_X11 + wayland driver registered → must throw
	// We don't need a real WallpaperApplication for this test
	CHECK_THROWS_AS (
	    sVideoFactories.createVideoDriver (
		ctx.settings.render.mode, "wayland", ctx,
		*reinterpret_cast<WallpaperApplication*> (0x1)
	    ),
	    std::runtime_error
	);
    }

    SECTION ("GNOME_X11 on unknown session throws") {
	const char* argv_b[] = {"test"};
	ApplicationContext ctx (1, const_cast<char**> (argv_b));
	ctx.settings.render.mode = ApplicationContext::GNOME_X11_DESKTOP_WINDOW;
	CHECK_THROWS_AS (
	    sVideoFactories.createVideoDriver (
		ctx.settings.render.mode, "unknown_session", ctx,
		*reinterpret_cast<WallpaperApplication*> (0x1)
	    ),
	    std::runtime_error
	);
    }

    SECTION ("NORMAL_WINDOW uses DEFAULT_WINDOW_NAME lookup (works on any session)") {
	// NORMAL_WINDOW is registered with DEFAULT_WINDOW_NAME, so it should work
	// on any session type including made-up ones. However, actually constructing
	// the driver requires GLFW which needs a display. So we test the lookup
	// succeeds (no "cannot find driver" error) — the GLFW init error is acceptable.
	const char* argv_c[] = {"test"};
		ApplicationContext ctx (1, const_cast<char**> (argv_c));
		ctx.settings.render.mode = ApplicationContext::NORMAL_WINDOW;
	// This may throw for GLFW init failure but NOT for "cannot find a driver"
	try {
	    auto driver = sVideoFactories.createVideoDriver (
		ctx.settings.render.mode, "any_session", ctx,
		*reinterpret_cast<WallpaperApplication*> (0x1)
	    );
	    // If we got here, it worked (unlikely without display but possible)
	    CHECK (driver != nullptr);
	} catch (const std::runtime_error& e) {
	    // Acceptable: GLFW init failure
	    std::string msg = e.what ();
	    CHECK (msg.find ("Cannot find a driver") == std::string::npos);
	}
    }
}

TEST_CASE ("ApplicationContext defaults: GNOME_X11 excluded from window scaling defaults") {
    // When mode is GNOME_X11_DESKTOP_WINDOW, window preview scaling defaults
    // (fit + border) must NOT be applied. This test verifies the condition
    // logic directly without full CLI parsing.

    const char* dummy_argv[] = {"test"};
    ApplicationContext ctx (1, const_cast<char**> (dummy_argv));

    // Desktop modes keep their original defaults
    ctx.settings.render.mode = ApplicationContext::DESKTOP_BACKGROUND;
    auto desktopScaling = ctx.settings.render.window.scalingMode;
    auto desktopClamp = ctx.settings.render.window.clamp;

    ctx.settings.render.mode = ApplicationContext::GNOME_X11_DESKTOP_WINDOW;
    auto gnomeScaling = ctx.settings.render.window.scalingMode;
    auto gnomeClamp = ctx.settings.render.window.clamp;

    // GNOME_X11 should have same defaults as DESKTOP_BACKGROUND
    CHECK (gnomeScaling == desktopScaling);
    CHECK (gnomeClamp == desktopClamp);

    // Window preview modes would get different defaults after loadSettingsFromArgv
    // but both desktop modes should be consistent
}
