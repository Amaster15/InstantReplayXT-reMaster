#pragma once
#include <QFile>
#include <QSaveFile>
#include <filesystem>
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <string>

namespace replay_files {
namespace fs = std::filesystem;
inline void copyAtomic(const fs::path &source, const fs::path &destination)
{
    QFile input(QString::fromUtf8(source.u8string().c_str()));
    QSaveFile output(QString::fromUtf8(destination.u8string().c_str()));
    if (!input.open(QIODevice::ReadOnly) || !output.open(QIODevice::WriteOnly))
        throw std::runtime_error("Cannot open replay or destination: " + destination.u8string());
    while (!input.atEnd()) {
        auto bytes = input.read(1024 * 1024);
        if (bytes.isEmpty() && input.error() != QFileDevice::NoError)
            throw std::runtime_error("Cannot read saved replay");
        if (output.write(bytes) != bytes.size())
            throw std::runtime_error("Cannot write replay (check free disk space)");
    }
    if (!output.commit()) throw std::runtime_error("Cannot replace replay: " + output.errorString().toStdString());
}
inline fs::path nextBackup(const fs::path &directory, const fs::path &extension)
{
    unsigned long long highest = 0;
    const std::string prefix = "InstantReplay_";
    for (const auto &entry : fs::directory_iterator(directory)) {
        const auto path = entry.path();
        const auto stem = path.stem().u8string();
        if (path.extension() != extension || stem.compare(0, prefix.size(), prefix) != 0) continue;
        auto digits = stem.substr(prefix.size());
        if (digits.empty() || digits.find_first_not_of("0123456789") != std::string::npos) continue;
        try { highest = std::max(highest, std::stoull(digits)); }
        catch (const std::out_of_range &) { throw std::runtime_error("Backup number is too large"); }
    }
    if (highest == std::numeric_limits<unsigned long long>::max())
        throw std::runtime_error("Backup numbers exhausted");
    return directory / fs::u8path(prefix + std::to_string(highest + 1) + extension.u8string());
}
struct Result { std::string path; std::string warning; };
inline Result publish(const std::string &saved, const std::string &folder, const std::string &backup)
{
    auto source = fs::absolute(fs::u8path(saved));
    auto directory = folder.empty() ? source.parent_path() : fs::absolute(fs::u8path(folder));
    fs::create_directories(directory);
    auto destination = directory / fs::u8path("InstantReplay" + source.extension().u8string());
    Result result{destination.u8string(), ""};
    if (!backup.empty()) {
        try {
            auto backupDirectory = fs::u8path(backup);
            fs::create_directories(backupDirectory);
            // Never overwrite an existing numbered backup, even if another process creates it.
            auto target = nextBackup(backupDirectory, source.extension());
            fs::copy_file(source, target, fs::copy_options::none);
        } catch (const std::exception &e) { result.warning = std::string("Backup failed: ") + e.what(); }
    }
    bool same = fs::exists(destination) && fs::equivalent(source, destination);
    if (!same) {
        copyAtomic(source, destination);
        // Keep OBS's original on backup failure; otherwise only the fixed main clip remains.
        if (result.warning.empty()) {
            std::error_code error;
            fs::remove(source, error);
            if (error) result.warning = "Saved fixed replay, but could not remove OBS's timestamped original.";
        }
    }
    return result;
}
}
