#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace Steam::FileSystem {

struct WorkshopItem {
    std::filesystem::path path;
    std::string id;     // workshop content ID
    std::string type;   // "scene" / "video" / "web"
    std::string title;  // from project.json
};

std::filesystem::path workshopDirectory (int appID, const std::string& contentID);
std::filesystem::path appDirectory (const std::string& appDirectory, const std::string& path);

/** Scan the Steam Workshop content directory for a given app and return
 *  all wallpapers found, each with its type and title parsed from project.json.
 *  Items that fail to parse are silently skipped. */
std::vector<WorkshopItem> listWorkshopWallpapers (int appID);

} // namespace Steam::FileSystem