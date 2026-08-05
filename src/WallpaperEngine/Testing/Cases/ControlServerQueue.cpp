// Regression test for the ControlServer thread-safety fix: the network thread must
// only enqueue commands, and tryPopCommand() must hand them back to the caller
// (the render loop) in order, without executing anything on the worker thread.
#include "WallpaperEngine/Application/ControlServer.h"

#include <catch2/catch_test_macros.hpp>

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cstring>

using WallpaperEngine::Application::ControlServer;

namespace {
constexpr const char* kSocketPath = "/tmp/lwe-test-control.sock";
constexpr int kRetries = 200;
constexpr useconds_t kRetryDelayUs = 10000; // 10 ms

int connectClient () {
	int fd = socket (AF_UNIX, SOCK_STREAM, 0);
	if (fd < 0) return -1;

	struct sockaddr_un addr {};
	addr.sun_family = AF_UNIX;
	std::strncpy (addr.sun_path, kSocketPath, sizeof (addr.sun_path) - 1);

	// The server thread needs a moment to bind()/listen(); retry until it is up.
	for (int i = 0; i < kRetries; i++) {
		if (connect (fd, reinterpret_cast<struct sockaddr*> (&addr), sizeof (addr)) == 0) return fd;
		usleep (kRetryDelayUs);
	}

	close (fd);
	return -1;
}
} // namespace

TEST_CASE ("ControlServer enqueues commands for the main thread to consume") {
	{
		ControlServer server (kSocketPath);

		// The server processes one command per connection (one read per accept).
		{
			int fd = connectClient ();
			REQUIRE (fd >= 0);
			const char* next = "{\"cmd\":\"next\"}";
			REQUIRE (write (fd, next, std::strlen (next)) == static_cast<ssize_t> (std::strlen (next)));
			close (fd);
		}
		{
			int fd = connectClient ();
			REQUIRE (fd >= 0);
			const char* cycle = "{\"cmd\":\"cycle\",\"enabled\":\"1\"}";
			REQUIRE (write (fd, cycle, std::strlen (cycle)) == static_cast<ssize_t> (std::strlen (cycle)));
			close (fd);
		}

		ControlServer::Command command;
		bool gotFirst = false;
		bool gotSecond = false;
		for (int i = 0; i < kRetries && !(gotFirst && gotSecond); i++) {
			while (server.tryPopCommand (command)) {
				if (command.first == "next") gotFirst = true;
				else if (command.first == "cycle" && command.second == "1") gotSecond = true;
			}
			usleep (kRetryDelayUs);
		}

		CHECK (gotFirst);
		CHECK (gotSecond);
	}
	// server destructor must have cleaned up the socket
	CHECK (access (kSocketPath, F_OK) != 0);
}

TEST_CASE ("ControlServer tryPopCommand returns false on an empty queue") {
	ControlServer server (kSocketPath);
	ControlServer::Command command;
	CHECK_FALSE (server.tryPopCommand (command));
}
