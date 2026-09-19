#include "downloader.h"

namespace downloader {

int downloadVideo(const std::string& url) {
    if (!requireTool("yt-dlp")) return 1;
    if (!requireTool("ffmpeg", "required for merging video+audio")) return 1;

    std::string dir = downloadDir("video");
    if (dir.empty()) return 1;

    // Merge into mp4 first, then recode to mov.
    const std::string options =
        "-f \"bestvideo+bestaudio/best\" --merge-output-format mp4 --recode-video mov";

    if (runYtDlp(options, dir, url) != 0) {
        std::cerr << "Failed to download video." << std::endl;
        return 1;
    }

    std::cout << "Video saved to: " << dir << std::endl;
    return 0;
}

}  // namespace downloader
