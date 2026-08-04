#pragma once

#include <atomic>
#include <functional>
#include <string>
#include <thread>

namespace WallpaperEngine::Application {

/**
 * Lightweight Unix-domain-socket server that accepts JSON control commands
 * from external tools (e.g. the system-tray applet).
 *
 * Runs in its own thread so it never blocks the render loop.
 */
class ControlServer {
public:
	using CommandHandler = std::function<void(const std::string& cmd, const std::string& payload)>;

	ControlServer (const std::string& socketPath, CommandHandler handler);
	~ControlServer ();

	ControlServer (const ControlServer&) = delete;
	ControlServer& operator= (const ControlServer&) = delete;

private:
	void run ();

	std::string m_socketPath;
	CommandHandler m_handler;
	std::atomic<bool> m_running {true};
	std::thread m_thread;
};

} // namespace WallpaperEngine::Application
