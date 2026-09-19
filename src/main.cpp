#include "downloader.h"

namespace {

void usage(const char* prog) {
    std::cout << "Usage: " << prog << " [audio|video|thumbnail|separate] [URL]\n"
              << "\n"
              << "  audio      extract audio as .wav                -> downloads/audio\n"
              << "  video      full video as .mov                   -> downloads/video\n"
              << "  thumbnail  cover art as .jpg                    -> downloads/thumbnail\n"
              << "  separate   .wav + vocals + instrumental stems   -> downloads/separate/<song>/\n"
              << "\n"
              << "Run with no arguments to be prompted instead.\n"
              << "The separation model is chosen in stem-model.conf (see separate.sh)." << std::endl;
}

// Accepts either the menu number or the mode name.
std::string parseMode(const std::string& in) {
    if (in == "1" || in == "audio") return "audio";
    if (in == "2" || in == "video") return "video";
    if (in == "3" || in == "thumbnail") return "thumbnail";
    if (in == "4" || in == "separate" || in == "seperate") return "separate";
    return "";
}

std::string prompt(const std::string& question) {
    std::cout << question;
    std::string answer;
    std::getline(std::cin, answer);
    return answer;
}

}  // namespace

int main(int argc, char* argv[]) {
    std::string arg = argc > 1 ? argv[1] : "";
    std::string url = argc > 2 ? argv[2] : "";

    if (arg == "-h" || arg == "--help") {
        usage(argv[0]);
        return 0;
    }

    if (arg.empty()) {
        arg = prompt("What do you want to download?\n"
                     "  1) audio      (.wav)\n"
                     "  2) video      (.mov)\n"
                     "  3) thumbnail  (.jpg)\n"
                     "  4) separate   (.wav + vocals + instrumental)\n"
                     "Choice: ");
    }

    std::string mode = parseMode(arg);
    if (mode.empty()) {
        std::cerr << "Unknown mode: " << arg << "\n" << std::endl;
        usage(argv[0]);
        return 1;
    }

    if (url.empty()) url = prompt("Enter the YouTube URL: ");
    if (url.empty()) {
        std::cerr << "No URL given." << std::endl;
        return 1;
    }

    if (mode == "audio") return downloader::downloadAudio(url);
    if (mode == "video") return downloader::downloadVideo(url);
    if (mode == "separate") return downloader::downloadSeparate(url);
    return downloader::downloadThumbnail(url);
}
