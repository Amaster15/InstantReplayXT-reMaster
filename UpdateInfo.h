#pragma once
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QRegularExpression>
#include <QVersionNumber>
#include <QUrl>
#include <QVariant>

namespace updates {
inline const QString currentVersion = "2.0.19";
inline const QString repository = "https://github.com/Amaster15/InstantReplayXT-reMaster";
struct Release { QString version, name, digest; QUrl url; qint64 size = 0; };
inline bool newer(const QString &tag, const QString &current = currentVersion) {
    static const QRegularExpression pattern("^v([0-9]+\\.[0-9]+\\.[0-9]+)$");
    const auto match = pattern.match(tag);
    return match.hasMatch() && QVersionNumber::fromString(match.captured(1)) > QVersionNumber::fromString(current);
}
inline Release parse(const QByteArray &json, const QString &platform) {
    const auto object = QJsonDocument::fromJson(json).object();
    const auto tag = object.value("tag_name").toString();
    if (object.value("draft").toBool() || object.value("prerelease").toBool() || !newer(tag)) return {};
    Release result; result.version = tag.mid(1);
    const QString name = "InstantReplayXT-" + result.version + "-obs31-" + platform +
        (platform == "windows-x64" ? "-setup.exe" : "-install.run");
    for (const auto &value : object.value("assets").toArray()) {
        const auto asset = value.toObject();
        if (asset.value("name").toString() != name) continue;
        const QString url = repository + "/releases/download/" + tag + "/" + name;
        const auto digest = asset.value("digest").toString();
        const qint64 size = asset.value("size").toVariant().toLongLong();
        if (asset.value("browser_download_url").toString() != url ||
            !QRegularExpression("^sha256:[0-9a-f]{64}$").match(digest).hasMatch() || size <= 0 || size > 100 * 1024 * 1024) break;
        result.name = name; result.url = QUrl(url); result.digest = digest.mid(7); result.size = size;
        break;
    }
    return result;
}
}
