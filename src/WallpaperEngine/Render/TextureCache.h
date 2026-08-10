#pragma once

#include <functional>
#include <map>
#include <memory>
#include <string>

#include "TextureProvider.h"
#include "WallpaperEngine/Assets/AssetLocator.h"
#include "WallpaperEngine/Render/Helpers/ContextAware.h"
#include "WallpaperEngine/Render/RenderContext.h"

using namespace WallpaperEngine::Render;

namespace WallpaperEngine::Render {
class AlbumTexture;
namespace Helpers {
    class ContextAware;
}

class RenderContext;

class TextureCache final : Helpers::ContextAware {
public:
    explicit TextureCache (RenderContext& context);
    ~TextureCache () override;

    /**
     * Checks if the given texture was already loaded and returns it
     * If the texture was not loaded yet, it tries to load it from the container
     *
     * @param filename
     * @return
     */
    std::shared_ptr<const TextureProvider>
    resolve (const std::string& filename, const Assets::AssetLocator& assetLocator);

    /**
     * Removes expired weak cache entries after a wallpaper switch.
     */
    void pruneExpired ();

private:
    /** The previous album thumbnail texture */
    std::shared_ptr<const AlbumTexture> m_previousThumbnail = nullptr;
    /** The current album thumbnail texture */
    std::shared_ptr<const AlbumTexture> m_currentThumbnail = nullptr;
    /**
     * Cached textures, isolated by the project AssetLocator that resolved them.
     * Weak ownership lets wallpaper objects control texture lifetime, so switching
     * wallpapers cannot retain every texture loaded earlier in the process.
     */
    using TextureKey = std::pair<const Assets::AssetLocator*, std::string>;
    std::map<TextureKey, std::weak_ptr<const TextureProvider>> m_textureCache = {};
    /** The callback to de-register media events */
    std::function<void ()> m_mediaCallback;
};
} // namespace WallpaperEngine::Render
