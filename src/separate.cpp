#include "downloader.h"

#include <chrono>

namespace downloader {

namespace {

namespace fs = std::filesystem;

// The only .wav inside dir, or an empty path if there isn't exactly one.
fs::path singleWav(const fs::path& dir) {
    fs::path found;
    for (const auto& entry : fs::directory_iterator(dir)) {
        if (entry.path().extension() != ".wav") continue;
        if (!found.empty()) return {};  // more than one: ambiguous
        found = entry.path();
    }
    return found;
}

}  // namespace

// Downloads the audio as .wav, then splits it into vocals + instrumental:
//   downloads/separate/<song>/<song>.wav
//   downloads/separate/<song>/<song> (Vocals).wav
//   downloads/separate/<song>/<song> (Instrumental).wav
// The split itself is delegated to separate.sh next to the executable, so the
// model can be swapped by editing stem-model.conf without rebuilding.
int downloadSeparate(const std::string& url) {
    if (!requireTool("yt-dlp")) return 1;
    if (!requireTool("ffmpeg", "yt-dlp needs it to convert audio to wav")) return 1;

    std::string base = downloadDir("separate");
    if (base.empty()) return 1;

    fs::path script = fs::path(exeDir()) / "separate.sh";
    if (!fs::exists(script)) {
        std::cerr << "Missing " << script << " (the separation script)." << std::endl;
        return 1;
    }

    // yt-dlp names the file after the video, which we only learn afterwards.
    // Downloading into an empty staging folder lets us find it without guessing.
    fs::path staging = fs::path(base) / (".staging-" + std::to_string(getpid()));
    std::error_code ec;
    fs::create_directories(staging, ec);
    if (ec) {
        std::cerr << "Failed to create " << staging << ": " << ec.message() << std::endl;
        return 1;
    }

    // The stems come out at 44.1 kHz (the models' rate), so ask ffmpeg for the
    // same rate here and all three files in the folder match.
    if (runYtDlp("-x --audio-format wav --postprocessor-args \"ExtractAudio:-ar 44100\"", staging.string(), url) != 0) {
        std::cerr << "Failed to download audio." << std::endl;
        fs::remove_all(staging, ec);
        return 1;
    }

    fs::path wav = singleWav(staging);
    if (wav.empty()) {
        std::cerr << "Expected exactly one .wav in " << staging << "." << std::endl;
        return 1;
    }

    std::string name = wav.stem().string();
    fs::path songDir = fs::path(base) / name;
    fs::create_directories(songDir, ec);
    if (ec) {
        std::cerr << "Failed to create " << songDir << ": " << ec.message() << std::endl;
        return 1;
    }

    fs::path original = songDir / wav.filename();
    fs::rename(wav, original, ec);
    if (ec) {
        std::cerr << "Failed to move " << wav << " to " << original << ": " << ec.message() << std::endl;
        return 1;
    }
    fs::remove_all(staging, ec);

    std::cout << "Downloaded: " << original << "\n"
              << "Separating vocals and instrumental..." << std::endl;

    auto started = std::chrono::steady_clock::now();
    std::string command =
        shellQuote(script.string()) + " " + shellQuote(original.string()) + " " + shellQuote(songDir.string());
    if (std::system(command.c_str()) != 0) {
        std::cerr << "Separation failed. The original wav is still in " << songDir << std::endl;
        return 1;
    }
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - started).count();

    std::cout << "Separation took " << seconds << " s. Files in " << songDir << ":" << std::endl;
    for (const auto& entry : fs::directory_iterator(songDir)) {
        std::cout << "  " << entry.path().filename().string() << std::endl;
    }
    return 0;
}

}  // namespace downloader
