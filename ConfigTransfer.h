#pragma once
#include <QFile>
#include <QSaveFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <cmath>

// Portable settings for one collection, independent of the local recovery store.
class ConfigTransfer {
public:
    static bool write(const QString &path, const QJsonObject &settings, QString &error) {
        const auto bytes = QJsonDocument(QJsonObject{{"format", "InstantReplayXT-config"},
            {"version", 1}, {"settings", settings}}).toJson();
        QSaveFile file(path);
        if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
            error = "Cannot export config: " + file.errorString(); return false;
        }
        return true;
    }
    static bool read(const QString &path, QJsonObject &settings, QString &error) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) { error = "Cannot open config: " + file.errorString(); return false; }
        const auto bytes = file.read(4 * 1024 * 1024 + 1);
        QJsonParseError parse;
        const auto doc = QJsonDocument::fromJson(bytes, &parse);
        const auto root = doc.object();
        if (bytes.size() > 4 * 1024 * 1024 || parse.error != QJsonParseError::NoError || !doc.isObject() ||
            root["format"] != "InstantReplayXT-config" || root["version"] != 1 || !root["settings"].isObject()) {
            error = "Choose a supported InstantReplayXT exported config (version 1, up to 4 MB)."; return false;
        }
        const auto candidate = root["settings"].toObject();
        const auto value = candidate["replayCallout_data"];
        if (!value.isObject() || value.toObject().isEmpty()) { error = "The config contains no replay settings."; return false; }
        const auto obj = value.toObject();
        auto invalid = [&error] { error = "The config contains invalid setting values. Nothing was imported."; return false; };
        for (const auto *key : {"enable", "autoSwitch", "Dmute", "Amute", "backup", "matchMode", "vlcAutoRefresh", "vlcReplayPlayer", "librarySwitchScene", "returnSceneOnEnd"})
            if (obj.contains(key) && !obj[key].isBool()) return invalid();
        for (const auto *key : {"scene", "sourceName", "replayFolder", "backupPath", "vlcSource", "desktopMuteSource", "micMuteSource"})
            if (obj.contains(key) && !obj[key].isString()) return invalid();
        for (const auto *key : {"wait", "pause", "replay", "speedPercent", "activeMatch", "playerModeVersion"}) {
            if (!obj.contains(key)) continue;
            const auto v = obj[key]; const auto n = v.toDouble();
            if (!v.isDouble() || !std::isfinite(n) || std::floor(n) != n || n < -1 || n > 2147483647) return invalid();
        }
        for (const auto *key : {"clipSeconds1", "clipSeconds2", "clipSeconds3"}) {
            if (!obj.contains(key)) continue;
            const auto v = obj[key]; const auto n = v.toDouble();
            if (!v.isDouble() || !std::isfinite(n) || std::floor(n) != n || n < 1 || n > 3600) return invalid();
        }
        if (obj.contains("matches")) {
            if (!obj["matches"].isArray() || obj["matches"].toArray().size() > 1000) return invalid();
            for (const auto &row : obj["matches"].toArray()) {
                const auto match = row.toObject();
                if (!row.isObject() || !match["enabled"].isBool() || !match["name"].isString() || !match["path"].isString()) return invalid();
            }
        }
        settings = QJsonObject{{"replayCallout_data", obj}}; return true;
    }
};
