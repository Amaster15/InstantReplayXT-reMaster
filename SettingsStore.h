#pragma once
#include <QFile>
#include <QSaveFile>
#include <QFileInfo>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

// A separate, atomically replaced document survives OBS exiting without saving
// its scene collection. Never mix settings from different scene collections.
class SettingsStore {
    static bool readFile(const QString &path, QJsonObject &root) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) return false;
        QJsonParseError error;
        const auto doc = QJsonDocument::fromJson(file.readAll(), &error);
        if (error.error != QJsonParseError::NoError || !doc.isObject() || doc.object()["version"].toInt() != 1 || !doc.object()["collections"].isObject()) return false;
        root = doc.object(); return true;
    }
    static bool writeFile(const QString &path, const QByteArray &bytes) {
        QSaveFile file(path); // No direct-write fallback: leave the old file intact on failure.
        return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size() && file.commit();
    }
public:
    QString path;
    explicit SettingsStore(QString filename = {}) : path(std::move(filename)) {}
    QJsonObject load(const QString &collection) const {
        QJsonObject root;
        if (!readFile(path, root) && !readFile(path + ".bak", root)) return {};
        return root["collections"].toObject()[collection].toObject();
    }
    bool save(const QString &collection, const QJsonObject &settings, QString &error) const {
        if (path.isEmpty() || collection.isEmpty()) { error = "OBS configuration location or scene collection is not available yet."; return false; }
        QJsonObject root;
        const bool valid = readFile(path, root) || readFile(path + ".bak", root);
        if (!valid && (QFileInfo::exists(path) || QFileInfo::exists(path + ".bak"))) {
            error = "Both settings files are unreadable; existing files were preserved: " + path; return false;
        }
        if (!QDir().mkpath(QFileInfo(path).absolutePath())) { error = "Cannot create settings directory: " + QFileInfo(path).absolutePath(); return false; }
        if (valid && !writeFile(path + ".bak", QJsonDocument(root).toJson())) { error = "Cannot write settings backup: " + path + ".bak"; return false; }
        auto collections = root["collections"].toObject(); collections[collection] = settings;
        root["version"] = 1; root["collections"] = collections;
        if (!writeFile(path, QJsonDocument(root).toJson())) { error = "Cannot save settings: " + path; return false; }
        return true;
    }
};
