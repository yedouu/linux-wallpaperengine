#include <csignal>
#include <fcntl.h>
#include <iostream>
#include <string>
#include <sys/file.h>
#include <unistd.h>

#include "WallpaperEngine/Application/ApplicationContext.h"
#include "WallpaperEngine/Application/WallpaperApplication.h"
#include "WallpaperEngine/Logging/Log.h"

WallpaperEngine::Application::WallpaperApplication* app;

// ---- Single-instance guard ----
// The crash-and-restart pattern can otherwise leave several instances alive at once,
// each with its own render loop, socket thread and audio threads, which shows up as a
// pile of threads in htop. flock() is released automatically when the process dies, so a
// stale lock file can never block a restart.
static int g_singleInstanceFd = -1;

static bool acquireSingleInstanceLock () {
	const char* runtime = getenv ("XDG_RUNTIME_DIR");
	const std::string lockPath
	    = runtime != nullptr && runtime[0] != '\0' ? std::string (runtime) + "/linux-wallpaperengine.lock"
							: "/tmp/linux-wallpaperengine.lock";

	g_singleInstanceFd = open (lockPath.c_str (), O_CREAT | O_RDWR, 0600);
	if (g_singleInstanceFd < 0) {
		// Cannot create the lock file; don't block normal use.
		return true;
	}

	if (flock (g_singleInstanceFd, LOCK_EX | LOCK_NB) != 0) {
		close (g_singleInstanceFd);
		g_singleInstanceFd = -1;
		std::cerr
		    << "Another linux-wallpaperengine instance is already running. Stop it first "
		       "(systemctl --user stop linux-wallpaperengine) before starting a new one."
		    << std::endl;
		return false;
	}
	return true;
}

void signalhandler (const int sig) {
    if (app == nullptr) {
	return;
    }

    app->signal (sig);
}

void initLogging () {
    sLog.addOutput (new std::ostream (std::cout.rdbuf ()));
    sLog.addError (new std::ostream (std::cerr.rdbuf ()));
}

int main (int argc, char* argv[]) {
    try {
	// if type parameter is specified, this is a subprocess, so no logging should be enabled from our side
	bool enableLogging = true;
	const std::string typeZygote = "--type=zygote";
	const std::string typeUtility = "--type=utility";

	for (int i = 1; i < argc; i++) {
	    if (strncmp (typeZygote.c_str (), argv[i], typeZygote.size ()) == 0) {
		enableLogging = false;
		break;
	    }

	    if (strncmp (typeUtility.c_str (), argv[i], typeUtility.size ()) == 0) {
		enableLogging = false;
		break;
	    }
	}

	if (enableLogging) {
	    initLogging ();
	}

	WallpaperEngine::Application::ApplicationContext appContext (argc, argv);

	appContext.loadSettingsFromArgv ();

	// Only enforce single-instance after parsing so --help / --list-properties still work.
	if (!acquireSingleInstanceLock ()) {
	    return 1;
	}

	app = new WallpaperEngine::Application::WallpaperApplication (appContext);

	// halt if the list-properties option was specified
	if (appContext.settings.general.onlyListProperties) {
	    delete app;
	    return 0;
	}

	// attach signals to gracefully stop
	std::signal (SIGINT, signalhandler);
	std::signal (SIGTERM, signalhandler);
	std::signal (SIGKILL, signalhandler);

	// show the wallpaper application
	app->show ();

	// remove signal handlers before destroying app
	std::signal (SIGINT, SIG_DFL);
	std::signal (SIGTERM, SIG_DFL);
	std::signal (SIGKILL, SIG_DFL);

	delete app;

	return 0;
    } catch (const std::exception& e) {
	std::cerr << e.what () << std::endl;
	return 1;
    }
}