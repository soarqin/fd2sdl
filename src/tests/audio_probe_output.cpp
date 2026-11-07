/* 测试专用C++17文件边界：canonical解析junction/symlink，禁止写原资源。
 * 不属于FD2语义或通用runtime接口；open始终exclusive，不以先查存在代替。 */
#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif
#include "audio_probe_output.h"
#include <cwchar>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#ifdef _WIN32
#include <cwctype>
#endif

namespace fs = std::filesystem;
struct fd2_probe_outputs {
    fs::path original;
    std::vector<fs::path> paths;
};

static bool same_name(const fs::path &a, const fs::path &b) {
#ifdef _WIN32
    return _wcsicmp(a.c_str(), b.c_str()) == 0;
#else
    return a == b;
#endif
}
static bool resource_parent(fs::path parent, const fs::path &original) {
    for (;;) {
        if (fs::equivalent(parent, original) || same_name(parent.filename(), fs::path("original_game")))
            return true;
        fs::path next = parent.parent_path();
        if (next == parent || next.empty()) return false;
        parent = next;
    }
}
static void new_leaf(const fs::path &path) {
    const auto name = path.filename().native();
    if (name.empty() || path.filename() == "." || path.filename() == "..")
        throw std::runtime_error("output needs a file name");
#ifdef _WIN32
    std::wstring stem;
    bool in_stem = true;
    for (wchar_t c : name) {
        if (c < 32 || std::wstring(L"<>:\"/\\|?*").find(c) != std::wstring::npos)
            throw std::runtime_error("invalid output file name");
        if (c == L'.') in_stem = false;
        if (in_stem) stem += static_cast<wchar_t>(std::towlower(c));
    }
    if (name.back() == L'.' || name.back() == L' ' || stem == L"con" || stem == L"prn" ||
        stem == L"aux" || stem == L"nul" || (stem.size() == 4 &&
        (stem.substr(0, 3) == L"com" || stem.substr(0, 3) == L"lpt") &&
        stem[3] >= L'0' && stem[3] <= L'9'))
        throw std::runtime_error("reserved output file name");
#endif
    std::error_code error;
    const auto status = fs::symlink_status(path, error);
    if (status.type() != fs::file_type::not_found ||
        (error && error != std::errc::no_such_file_or_directory))
        throw std::runtime_error("output already exists or cannot be checked");
}

extern "C" fd2_probe_outputs *fd2_probe_outputs_prepare(
        const char *original, const char *const *paths, size_t count) {
    try {
        if (!original || !*original || !paths || !count || count > 2)
            throw std::runtime_error("invalid output arguments");
        fd2_probe_outputs result;
        result.original = fs::canonical(fs::u8path(original));
        if (!fs::is_directory(result.original)) throw std::runtime_error("original must be a directory");
        for (size_t i = 0; i < count; ++i) {
            if (!paths[i] || !*paths[i]) throw std::runtime_error("missing output path");
            fs::path requested = fs::u8path(paths[i]);
            fs::path parent = fs::canonical(requested.has_parent_path()
                ? requested.parent_path() : fs::current_path());
            if (!fs::is_directory(parent) || resource_parent(parent, result.original))
                throw std::runtime_error("output parent is not outside original resources");
            fs::path path = parent / requested.filename();
            new_leaf(path);
            for (const auto &previous : result.paths)
                if (fs::equivalent(previous.parent_path(), parent) &&
                    same_name(previous.filename(), path.filename()))
                    throw std::runtime_error("outputs resolve to the same path");
            result.paths.push_back(path);
        }
        return new fd2_probe_outputs(std::move(result));
    } catch (const std::exception &error) {
        std::fprintf(stderr, "audio probe output rejected: %s\n", error.what());
        return nullptr;
    }
}

extern "C" FILE *fd2_probe_output_open(const fd2_probe_outputs *outputs, size_t index) {
    try {
        if (!outputs || index >= outputs->paths.size()) return nullptr;
        const fs::path &path = outputs->paths[index];
        if (resource_parent(fs::canonical(path.parent_path()), outputs->original)) return nullptr;
        new_leaf(path);
#ifdef _WIN32
        return _wfopen(path.c_str(), L"wbx");
#else
        return std::fopen(path.c_str(), "wbx");
#endif
    } catch (const std::exception &error) {
        std::fprintf(stderr, "audio probe output open failed: %s\n", error.what());
        return nullptr;
    }
}
extern "C" void fd2_probe_outputs_close(fd2_probe_outputs *outputs) { delete outputs; }
