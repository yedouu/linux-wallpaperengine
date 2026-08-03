#include <algorithm>

#include "VideoFactories.h"
#include "WallpaperEngine/Logging/Log.h"
#include <cassert>

using namespace WallpaperEngine::Render::Drivers;

VideoFactories::VideoFactories () { assert (this->sInstance == nullptr); }

VideoFactories& VideoFactories::get () {
    if (sInstance == nullptr) {
	sInstance = std::make_unique<VideoFactories> ();
    }

    return *sInstance;
}

void VideoFactories::registerDriver (
    ApplicationContext::WINDOW_MODE forMode, std::string xdgSessionType,
    WallpaperEngine::Render::Drivers::VideoFactories::DriverConstructionFunc factory
) {
    const auto cur = this->m_driverFactories.find (forMode);

    if (cur == this->m_driverFactories.end ()) {
	SessionTypeToFactoryType map;

	map.emplace (xdgSessionType, factory);
	this->m_driverFactories.emplace (forMode, map);
    } else {
	cur->second.emplace (xdgSessionType, factory);
    }
}

void VideoFactories::registerFullscreenDetector (
    std::string xdgSessionType, FullscreenDetectorConstructionFunc factory
) {
    this->m_fullscreenFactories.emplace (xdgSessionType, factory);
}

std::vector<std::string> VideoFactories::getRegisteredDrivers () const {
    std::vector<std::string> result;

    for (const auto& sessionTypeToFactory : this->m_driverFactories | std::views::values) {
	for (const auto& xdgSessionType : sessionTypeToFactory | std::views::keys) {
	    result.push_back (xdgSessionType);
	}
    }

    const auto last = std::ranges::unique (result).begin ();

    result.erase (last, result.end ());

    return result;
}

std::unique_ptr<VideoDriver> VideoFactories::createVideoDriver (
    ApplicationContext::WINDOW_MODE mode, const std::string& xdgSessionType, ApplicationContext& context,
    WallpaperApplication& application
) {
    const auto sessionTypeToFactory = this->m_driverFactories.find (mode);

    if (sessionTypeToFactory == this->m_driverFactories.end ()) {
	sLog.exception ("Cannot find a driver for window mode ", mode, " and XDG_SESSION_TYPE ", xdgSessionType);
    }

    // windows are a bit special, there's just one handler
    // and it's not like the current map properly allows for storing this
    // so hijacking the detection is probably best for now
    const auto factory = mode != Application::ApplicationContext::DESKTOP_BACKGROUND
	? sessionTypeToFactory->second.find (DEFAULT_WINDOW_NAME)
	: sessionTypeToFactory->second.find (xdgSessionType);

    if (factory == sessionTypeToFactory->second.end ()) {
	sLog.exception ("Cannot find a driver for window mode ", mode, " and XDG_SESSION_TYPE ", xdgSessionType);
    }

    return factory->second (context, application);
}

std::unique_ptr<Detectors::FullScreenDetector> VideoFactories::createFullscreenDetector (
    const std::string& xdgSessionType, ApplicationContext& context, VideoDriver& driver
) {
    const auto it = this->m_fullscreenFactories.find (xdgSessionType);

    // Fullscreen pausing is a desktop-wallpaper optimization. Applying it to a
    // normal or explicit preview window stops that window's event loop whenever
    // another application is fullscreen, leaving resize events unprocessed and
    // newly exposed back-buffer areas unpainted.
    if (context.settings.render.mode != ApplicationContext::DESKTOP_BACKGROUND
	|| it == this->m_fullscreenFactories.end () || !context.settings.render.pauseOnFullscreen) {
	return std::make_unique<Detectors::FullScreenDetector> (context);
    }

    return it->second (context, driver);
}

std::unique_ptr<VideoFactories> VideoFactories::sInstance = nullptr;
