#pragma once

#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <iostream>
#include <string>

#include <libgen.h>     // For dirname
#include <limits.h>     // For PATH_MAX
#include <unistd.h>     // For readlink

#ifdef __APPLE__
#include <mach-o/dyld.h>  // For _NSGetExecutablePath
#endif

namespace downloader {

// Directory holding the running executable. Empty string on failure.
inline std::string exeDir() {
    char buf[PATH_MAX];

#ifdef __APPLE__
    // macOS has no /proc/self/exe, so ask dyld where we were loaded from.
    uint32_t size = sizeof(buf);
    if (_NSGetExecutablePath(buf, &size) != 0) return "";
#else
    ssize_t count = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (count == -1) return "";
    buf[count] = '\0';
#endif

    return std::string(dirname(buf));
}

// Creates <exe dir>/downloads/<name> if needed. Empty string on failure.
inline std::string downloadDir(const std::string& name) {
    std::string base = exeDir();
    if (base.empty()) {
        std::cerr << "Failed to get executable path." << std::endl;
        return "";
    }

    std::string dir = base + "/downloads/" + name;
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    if (ec) {
        std::cerr << "Failed to create " << dir << ": " << ec.message() << std::endl;
        return "";
    }
    return dir;
}

// Wraps s in single quotes so the shell sees exactly one literal argument,
// even when it contains spaces, quotes or &.
inline std::string shellQuote(const std::string& s) {
    std::string out = "'";
    for (char c : s) {
        if (c == '\'') out += "'\\''";  // close, escaped quote, reopen
        else out += c;
    }
    return out + "'";
}

// Reports a missing dependency instead of failing later inside yt-dlp.
// Uses `command -v` rather than a version flag: those are not consistent
// across tools (ffmpeg wants -version and errors on --version).
inline bool requireTool(const std::string& tool, const std::string& why = "") {
    if (std::system(("command -v " + shellQuote(tool) + " > /dev/null 2>&1").c_str()) == 0) return true;

    std::cerr << tool << " is not installed. Please install it first";
    if (!why.empty()) std::cerr << " (" << why << ")";
    std::cerr << "." << std::endl;
    return false;
}

// Runs cmd and returns its stdout with trailing whitespace stripped.
inline std::string commandOutput(const std::string& cmd) {
    std::string out;
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return out;

    std::array<char, 256> buf;
    while (fgets(buf.data(), buf.size(), pipe)) out += buf.data();
    pclose(pipe);

    while (!out.empty() && (out.back() == '\n' || out.back() == '\r' || out.back() == ' ')) out.pop_back();
    return out;
}

// yt-dlp versions are release dates (2026.08.19). YouTube changes often
// enough that a build older than this usually fails, so refresh it first.
constexpr int kYtDlpMaxAgeDays = 30;

// Days since the installed yt-dlp was released, or -1 if unknown.
inline int ytDlpAgeDays() {
    std::string version = commandOutput("yt-dlp --version 2>/dev/null");

    int year = 0, month = 0, day = 0;
    if (std::sscanf(version.c_str(), "%d.%d.%d", &year, &month, &day) != 3) return -1;

    std::tm released{};
    released.tm_year = year - 1900;
    released.tm_mon = month - 1;
    released.tm_mday = day;
    released.tm_hour = 12;
    std::time_t releasedAt = std::mktime(&released);
    if (releasedAt == -1) return -1;

    return static_cast<int>(std::difftime(std::time(nullptr), releasedAt) / 86400);
}

// Upgrades yt-dlp through Homebrew. True if brew reported success.
inline bool upgradeYtDlp() {
    if (std::system("command -v brew > /dev/null 2>&1") != 0) {
        std::cerr << "Homebrew not found, so yt-dlp can't be updated automatically. "
                     "Try: pip install -U yt-dlp" << std::endl;
        return false;
    }

    std::cout << "Updating yt-dlp with Homebrew..." << std::endl;
    // `brew upgrade` can exit non-zero when nothing is outdated; `brew install`
    // then confirms it's present so a no-op update still counts as success.
    return std::system("brew upgrade yt-dlp || brew install yt-dlp") == 0;
}

// True at most once per day: Homebrew may not have a newer yt-dlp yet, and
// running `brew upgrade` (which also runs `brew update`) before every single
// download would make each one noticeably slower.
inline bool upgradeCheckedRecently() {
    std::filesystem::path stamp = std::filesystem::path(exeDir()) / ".yt-dlp-checked";
    std::error_code ec;
    auto last = std::filesystem::last_write_time(stamp, ec);
    if (!ec) {
        auto age = std::filesystem::file_time_type::clock::now() - last;
        if (age < std::chrono::hours(24)) return true;
    }
    std::FILE* f = std::fopen(stamp.string().c_str(), "w");
    if (f) std::fclose(f);
    return false;
}

// Runs yt-dlp with mode-specific options, saving into dir.
// A stale yt-dlp is upgraded before the first attempt (checked once a day).
// If the download still fails and we haven't upgraded yet, upgrade once and
// retry, since an out-of-date yt-dlp is by far the most common failure.
inline int runYtDlp(const std::string& options, const std::string& dir, const std::string& url) {
    std::string command = "yt-dlp " + options + " -P " + shellQuote(dir) + " " + shellQuote(url);

    bool upgraded = false;
    int age = ytDlpAgeDays();
    if (age > kYtDlpMaxAgeDays && !upgradeCheckedRecently()) {
        std::cout << "Installed yt-dlp is " << age << " days old." << std::endl;
        upgraded = upgradeYtDlp();
    }

    int rc = std::system(command.c_str());
    if (rc != 0 && !upgraded) {
        std::cerr << "yt-dlp failed. Updating it and trying once more..." << std::endl;
        if (upgradeYtDlp()) rc = std::system(command.c_str());
    }
    return rc;
}

// One per mode, defined in audio.cpp / video.cpp / thumbnail.cpp / separate.cpp.
int downloadAudio(const std::string& url);
int downloadVideo(const std::string& url);
int downloadThumbnail(const std::string& url);
int downloadSeparate(const std::string& url);

}  // namespace downloader
