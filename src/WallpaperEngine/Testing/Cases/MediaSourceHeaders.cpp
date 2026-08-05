// Regression test for the Ubuntu 22.04 build fix (docs 3.1): MediaSource headers
// must be self-contained. GCC 11 previously failed to compile them because the types used
// (std::optional, std::shared_ptr, std::uint32_t) relied on transitive includes.
#include "WallpaperEngine/Media/MediaSource.h"
#include "WallpaperEngine/Media/DBusMediaSource.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE ("MediaSource headers are self-contained") {
	// Referencing the types only requires complete declarations, so any missing
	// standard-library include in the headers themselves fails to compile.
	std::optional<WallpaperEngine::Media::MediaSource::MediaInfo> info;
	CHECK_FALSE (info.has_value ());

	std::shared_ptr<WallpaperEngine::Media::MediaSource> source = nullptr;
	CHECK (source == nullptr);

	WallpaperEngine::Media::MediaSource::PlaybackState state = WallpaperEngine::Media::MediaSource::PlaybackState::Stopped;
	CHECK (state == WallpaperEngine::Media::MediaSource::PlaybackState::Stopped);
}
