#pragma once

#include <atomic>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <utility>

namespace WallpaperEngine::Application {

/**
 * Lightweight Unix-domain-socket server that accepts JSON control commands
 * from external tools (e.g. the system-tray applet).
 *
 * The network thread only parses and *enqueues* commands; it never runs the
 * handler directly. The render loop consumes commands via tryPopCommand() on the
 * main thread, so application state is never mutated from a worker thread.
 */
class ControlServer {
public:
	using Command = std::pair<std::string, std::string>; // <cmd, payload>

	ControlServer (const std::string& socketPath);
	~ControlServer ();

	ControlServer (const ControlServer&) = delete;
	ControlServer& operator= (const ControlServer&) = delete;

	/**
	 * Pops the next queued command, if any.
	 *
	 * @param command Receives <cmd, payload> on success
	 * @return true if a command was popped
	 */
	bool tryPopCommand (Command& command);

private:
	void run ();
	void pushCommand (const std::string& cmd, const std::string& payload);

	std::string m_socketPath;
	std::deque<Command> m_commands;
	std::mutex m_mutex;
	std::atomic<bool> m_running {true};
	std::thread m_thread;
};

} // namespace WallpaperEngine::Application
