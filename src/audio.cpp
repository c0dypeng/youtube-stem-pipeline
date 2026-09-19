#include "downloader.h"

namespace downloader {

int downloadAudio(const std::string& url) {
    if (!requireTool("yt-dlp")) return 1;

    std::string dir = downloadDir("audio");
    if (dir.empty()) return 1;

    if (runYtDlp("-x --audio-format wav", dir, url) != 0) {
        std::cerr << "Failed to download audio." << std::endl;
        return 1;
    }

    std::cout << "Audio saved to: " << dir << std::endl;
    return 0;
}

}  // namespace downloader
