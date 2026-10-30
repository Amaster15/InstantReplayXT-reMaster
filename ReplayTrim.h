#pragma once
#include "ReplayFiles.h"
#include <QFileInfo>
#include <QProcess>
#include <QTemporaryDir>
#include <QElapsedTimer>
#include <QRegularExpression>
#include <atomic>
#include <memory>

namespace replay_trim {
// Run only on the save worker. Stream copy starts at the preceding keyframe,
// preserving all audio tracks and the end of the replay without re-encoding.
inline replay_files::Result publish(const std::string &saved, const std::string &folder,
    const std::string &backup, int seconds, const QString &executable,
    const std::shared_ptr<std::atomic<bool>> &cancelled = {})
{
    auto checkCancelled = [&] {
        if (cancelled && cancelled->load()) throw std::runtime_error("Clip save cancelled");
    };
    checkCancelled();
    if (seconds == 0) return replay_files::publish(saved, folder, backup);
    if (seconds < 1 || seconds > 3600) throw std::runtime_error("Clip length must be 1–3600 seconds");
    if (executable.isEmpty() || !QFileInfo(executable).isExecutable())
        throw std::runtime_error("Clip trimming helper is missing. Reinstall the plugin");
    const QFileInfo input(QString::fromUtf8(saved.c_str()));
    QTemporaryDir temp(input.absolutePath() + "/.irxt-trim-XXXXXX");
    if (!temp.isValid()) throw std::runtime_error("Cannot create a temporary clip in the OBS recording folder");
    const auto output = temp.filePath("clip." + input.suffix());
    QProcess process;
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start(executable, {"-hide_banner", "-loglevel", "warning", "-nostdin", "-n", "-nostats", "-progress", "pipe:1",
        "-sseof", QString::number(-seconds), "-i", input.absoluteFilePath(),
        "-map", "0:v:0", "-map", "0:a?", "-c", "copy", "-avoid_negative_ts", "make_zero", output});
    if (!process.waitForStarted(5000)) throw std::runtime_error("Cannot start the clip trimming helper");
    QElapsedTimer elapsed; elapsed.start();
    while (!process.waitForFinished(100) && process.state() != QProcess::NotRunning) {
        if ((cancelled && cancelled->load()) || elapsed.elapsed() > 120000) {
            process.kill(); process.waitForFinished();
            checkCancelled();
            throw std::runtime_error("Clip trimming timed out");
        }
    }
    checkCancelled();
    const auto report = process.readAll();
    // FFmpeg can otherwise return success after ignoring a failed EOF-relative seek.
    if (report.contains("Cannot use -sseof") || report.contains("could not seek to position"))
        throw std::runtime_error("The recording could not be seeked reliably; the original was kept");
    const bool hasVideo = QRegularExpression("(?:^|[\\r\\n])frame=[1-9][0-9]*(?:[\\r\\n]|$)").match(QString::fromUtf8(report)).hasMatch();
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0 || QFileInfo(output).size() < 256 || !hasVideo)
        throw std::runtime_error("Clip trimming failed: " + report.right(2000).toStdString());
    const auto destination = folder.empty() ? input.absolutePath().toUtf8().toStdString() : folder;
    auto result = replay_files::publish(output.toUtf8().toStdString(), destination, backup);
    // Never remove OBS's original until both publication and backup have succeeded.
    if (result.error.empty() && result.warning.empty()) {
        std::error_code ec;
        const bool same = replay_files::fs::equivalent(replay_files::fs::u8path(saved), replay_files::fs::u8path(result.path), ec);
        if (!same && !QFile::remove(input.absoluteFilePath()))
            result.warning = "Clip saved, but OBS's timestamped original could not be removed.";
    }
    return result;
}
}
