#include "TextureCache.h"

#include "AlbumTexture.h"
#include "WallpaperEngine/FileSystem/Container.h"

#include "CTexture.h"
#include "WallpaperEngine/Assets/AssetLoadException.h"
#include "WallpaperEngine/Render/Helpers/ContextAware.h"

#include "WallpaperEngine/Data/Model/Project.h"
#include "WallpaperEngine/Data/Parsers/TextureParser.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>

using namespace WallpaperEngine::Render;
using namespace WallpaperEngine::FileSystem;
using namespace WallpaperEngine::Data::Parsers;
using namespace WallpaperEngine::Data::Assets;

TextureCache::TextureCache (RenderContext& context) : Helpers::ContextAware (context) {
    // these textures are special cases, so make sure they're created only upon request
    this->m_currentThumbnail = std::make_shared<AlbumTexture> (this->getContext ());

#if !NDEBUG
    glObjectLabel (GL_TEXTURE, this->m_currentThumbnail->getTextureID (0), -1, "$mediaThumbnail");
#endif

    this->m_previousThumbnail = std::make_shared<AlbumTexture> (this->getContext ());

#if !NDEBUG
    glObjectLabel (GL_TEXTURE, this->m_previousThumbnail->getTextureID (0), -1, "$mediaPreviousThumbnail");
#endif

    // load the latest texture (if available)
    this->m_currentThumbnail->load ();

    this->m_mediaCallback = this->getContext ().getMediaSource ().addAlbumArtListener (
	[this] (const Media::MediaSource::MediaInfo& data) {
	    if (this->m_currentThumbnail->isReady ()) {
		// copy over pixel data and setup the new texture with the new data
		this->m_previousThumbnail->copyContents (*this->m_currentThumbnail);
	    }

	    // load the next image
	    this->m_currentThumbnail->load ();
	}
    );
}

TextureCache::~TextureCache () { this->m_mediaCallback (); }

std::shared_ptr<const TextureProvider>
TextureCache::resolve (const std::string& filename, const Assets::AssetLocator& assetLocator) {
    // Media thumbnails are process-wide dynamic textures rather than project assets.
    if (filename == "$mediaThumbnail") {
	return this->m_currentThumbnail;
    }
    if (filename == "$mediaPreviousThumbnail") {
	return this->m_previousThumbnail;
    }

    const TextureKey key { &assetLocator, filename };
    if (const auto found = this->m_textureCache.find (key); found != this->m_textureCache.end ()) {
	if (auto texture = found->second.lock ()) {
	    return texture;
	}
	this->m_textureCache.erase (found);
    }

    const auto contents = assetLocator.texture (filename);
    auto stream = BinaryReader (contents);
    auto metadataLoader = [&assetLocator] (const std::string& metaFilename) -> std::string {
	const std::filesystem::path fullPath = std::filesystem::path ("materials") / metaFilename;
	return assetLocator.readString (fullPath);
    };

    auto parsedTexture = TextureParser::parse (stream, filename, metadataLoader);
    auto texture = std::make_shared<CTexture> (this->getContext (), std::move (parsedTexture));
#if !NDEBUG
	glObjectLabel (GL_TEXTURE, texture->getTextureID (0), -1, filename.c_str ());
#endif

    this->m_textureCache.insert_or_assign (key, texture);
    return texture;
}

void TextureCache::pruneExpired () {
    std::erase_if (this->m_textureCache, [] (const auto& entry) { return entry.second.expired (); });
}
