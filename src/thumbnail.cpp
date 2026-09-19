#include "downloader.h"

namespace downloader {

int downloadThumbnail(const std::string& url) {
    if (!requireTool("yt-dlp")) return 1;

    std::string dir = downloadDir("thumbnail");
    if (dir.empty()) return 1;

    if (runYtDlp("--skip-download --write-thumbnail --convert-thumbnails jpg", dir, url) != 0) {
        std::cerr << "Failed to download thumbnail." << std::endl;
        return 1;
    }

    std::cout << "Thumbnail saved to: " << dir << std::endl;
    return 0;
}

}  // namespace downloader
