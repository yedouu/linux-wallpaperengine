#include "ControlServer.h"
#include "WallpaperEngine/Logging/Log.h"

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cstring>
#include <sstream>

using namespace WallpaperEngine::Application;

ControlServer::ControlServer (const std::string& socketPath) : m_socketPath (socketPath) {
	unlink (m_socketPath.c_str ());
	m_thread = std::thread (&ControlServer::run, this);
}

ControlServer::~ControlServer () {
	m_running = false;
	// Connect briefly to unblock accept()
	int fd = socket (AF_UNIX, SOCK_STREAM, 0);
	if (fd >= 0) {
		struct sockaddr_un addr {};
		addr.sun_family = AF_UNIX;
		strncpy (addr.sun_path, m_socketPath.c_str (), sizeof (addr.sun_path) - 1);
		connect (fd, (struct sockaddr*) &addr, sizeof (addr));
		close (fd);
	}
	if (m_thread.joinable ()) m_thread.join ();
	unlink (m_socketPath.c_str ());
}

void ControlServer::pushCommand (const std::string& cmd, const std::string& payload) {
	std::lock_guard<std::mutex> lock (m_mutex);
	m_commands.emplace_back (cmd, payload);
}

bool ControlServer::tryPopCommand (Command& command) {
	std::lock_guard<std::mutex> lock (m_mutex);
	if (m_commands.empty ()) return false;
	command = std::move (m_commands.front ());
	m_commands.pop_front ();
	return true;
}

void ControlServer::run () {
	int listenFd = socket (AF_UNIX, SOCK_STREAM, 0);
	if (listenFd < 0) {
		sLog.error ("ControlServer: socket() failed");
		return;
	}

	struct sockaddr_un addr {};
	addr.sun_family = AF_UNIX;
	strncpy (addr.sun_path, m_socketPath.c_str (), sizeof (addr.sun_path) - 1);
	if (bind (listenFd, (struct sockaddr*) &addr, sizeof (addr)) < 0) {
		sLog.error ("ControlServer: bind() failed on ", m_socketPath);
		close (listenFd);
		return;
	}
	if (listen (listenFd, 1) < 0) {
		sLog.error ("ControlServer: listen() failed");
		close (listenFd);
		return;
	}

	sLog.out ("ControlServer: listening on ", m_socketPath);

	char buf[4096];
	while (m_running) {
		int clientFd = accept (listenFd, nullptr, nullptr);
		if (clientFd < 0) break;

		ssize_t n = read (clientFd, buf, sizeof (buf) - 1);
		if (n > 0) {
			buf[n] = '\0';
			std::string line (buf);

			// Simple JSON: {"cmd":"xxx","key":"val"}
			auto extract = [&](const std::string& key) -> std::string {
				auto pos = line.find ("\"" + key + "\":");
				if (pos == std::string::npos) return {};
				pos = line.find ('"', pos + key.length () + 3);
				if (pos == std::string::npos) return {};
				auto end = line.find ('"', pos + 1);
				if (end == std::string::npos) return {};
				return line.substr (pos + 1, end - pos - 1);
			};

			std::string cmd = extract ("cmd");
			std::string payload = extract ("path");
			if (payload.empty ()) payload = extract ("enabled");

			this->pushCommand (cmd, payload);
		}
		close (clientFd);
	}

	close (listenFd);
}
