#include <catch2/catch_test_macros.hpp>

#include <cstdio>
#include <fstream>

#include "WallpaperEngine/Application/ApplicationContext.h"

using WallpaperEngine::Application::ApplicationContext;

namespace {
constexpr const char* kAssetsDir = "/tmp";

struct Argv {
	const char* program;
	std::vector<std::string> args;
};

// Builds a real argv vector (argc/argv) from the given tokens.
class ArgvBuilder {
public:
	ArgvBuilder (const std::vector<std::string>& tokens) {
		for (const auto& token : tokens) m_storage.emplace_back (token);
		m_ptrs.reserve (m_storage.size ());
		for (auto& token : m_storage) m_ptrs.push_back (token.data ());
	}

	int argc () const { return static_cast<int> (m_ptrs.size ()); }
	char** argv () { return m_ptrs.data (); }

private:
	std::vector<std::string> m_storage;
	std::vector<char*> m_ptrs;
};
} // namespace

TEST_CASE ("Default mode is NORMAL_WINDOW") {
	ArgvBuilder argv ({ "lwe", "--assets-dir", kAssetsDir, "2955458015" });
	ApplicationContext context (argv.argc (), argv.argv ());
	context.loadSettingsFromArgv ();
	CHECK (context.settings.render.mode == ApplicationContext::NORMAL_WINDOW);
	CHECK (context.settings.general.defaultBackground.string ().find ("2955458015") != std::string::npos);
}

TEST_CASE ("GNOME_X11 flag selects desktop window mode") {
	ArgvBuilder argv ({ "lwe", "--gnome-x11", "--assets-dir", kAssetsDir, "2955458015" });
	ApplicationContext context (argv.argc (), argv.argv ());
	context.loadSettingsFromArgv ();
	CHECK (context.settings.render.mode == ApplicationContext::GNOME_X11_DESKTOP_WINDOW);
}

TEST_CASE ("window flag sets EXPLICIT_WINDOW and geometry") {
	ArgvBuilder argv ({ "lwe", "--window", "0x0x1280x720", "--assets-dir", kAssetsDir, "2955458015" });
	ApplicationContext context (argv.argc (), argv.argv ());
	context.loadSettingsFromArgv ();
	CHECK (context.settings.render.mode == ApplicationContext::EXPLICIT_WINDOW);
	CHECK (context.settings.render.window.geometry.z == 1280);
	CHECK (context.settings.render.window.geometry.w == 720);
}

TEST_CASE ("window and gnome-x11 flags are mutually exclusive") {
	ArgvBuilder argv ({ "lwe", "--window", "0x0x1280x720", "--gnome-x11", "--assets-dir", kAssetsDir, "2955458015" });
	ApplicationContext context (argv.argc (), argv.argv ());
	REQUIRE_THROWS (context.loadSettingsFromArgv ());
}

TEST_CASE ("fps flag stores the maximum FPS") {
	ArgvBuilder argv ({ "lwe", "--fps", "60", "--assets-dir", kAssetsDir, "2955458015" });
	ApplicationContext context (argv.argc (), argv.argv ());
	context.loadSettingsFromArgv ();
	CHECK (context.settings.render.maximumFPS == 60);
}

TEST_CASE ("cycle flag allows running without a background id") {
	ArgvBuilder argv ({ "lwe", "--cycle", "--assets-dir", kAssetsDir });
	ApplicationContext context (argv.argc (), argv.argv ());
	CHECK_NOTHROW (context.loadSettingsFromArgv ());
	CHECK (context.settings.general.cycleWallpapers);
}

TEST_CASE ("Missing background id without --cycle is rejected") {
	ArgvBuilder argv ({ "lwe", "--assets-dir", kAssetsDir });
	ApplicationContext context (argv.argc (), argv.argv ());
	REQUIRE_THROWS (context.loadSettingsFromArgv ());
}

TEST_CASE ("config file supplies defaults and CLI arguments override them") {
	const std::string configPath = "/tmp/lwe-test-config.json";
	{
		std::ofstream out (configPath);
		out << R"({
			"gnome-x11": true,
			"fps": 24,
			"silent": true,
			"assets-dir": "/tmp",
			"wallpaper": "2955458015"
		})";
	}

	// CLI only overrides fps; the rest comes from the config file.
	ArgvBuilder argv ({ "lwe", "--config", configPath, "--fps", "60" });
	ApplicationContext context (argv.argc (), argv.argv ());
	context.loadSettingsFromArgv ();
	CHECK (context.settings.render.mode == ApplicationContext::GNOME_X11_DESKTOP_WINDOW);
	CHECK (context.settings.render.maximumFPS == 60);
	CHECK (context.settings.general.defaultBackground.string ().find ("2955458015") != std::string::npos);

	std::remove (configPath.c_str ());
}
