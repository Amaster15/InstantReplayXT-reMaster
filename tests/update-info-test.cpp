#include "../UpdatePanel.h"
#include <QApplication>
#include <cassert>
#include <QTemporaryDir>
int main(int argc, char **argv) {
    QApplication app(argc, argv);
    assert(updates::newer("v2.0.10", "2.0.9"));
    assert(!updates::newer("v2.0.7"));
    assert(!updates::newer("v2.0.8-beta"));
    assert(!updates::newer("v2.0.5"));
    QJsonObject asset{{"name", "InstantReplayXT-2.0.8-obs31-linux-x64-install.run"},
        {"browser_download_url", updates::repository + "/releases/download/v2.0.8/InstantReplayXT-2.0.8-obs31-linux-x64-install.run"},
        {"digest", "sha256:" + QString(64, 'a')}, {"size", 100}};
    QJsonObject object{{"tag_name", "v2.0.8"}, {"assets", QJsonArray{asset}}};
    auto parse = [&] { return updates::parse(QJsonDocument(object).toJson(), "linux-x64"); };
    assert(!parse().name.isEmpty());
    object["prerelease"] = true; assert(parse().version.isEmpty()); object["prerelease"] = false;
    asset["browser_download_url"] = "https://example.com/installer"; object["assets"] = QJsonArray{asset}; assert(parse().name.isEmpty());
    asset["browser_download_url"] = updates::repository + "/releases/download/v2.0.8/InstantReplayXT-2.0.8-obs31-linux-x64-install.run";
    asset["digest"] = ""; object["assets"] = QJsonArray{asset}; assert(parse().name.isEmpty());
    asset["digest"] = "sha256:" + QString(64, 'a'); asset["size"] = 200 * 1024 * 1024; object["assets"] = QJsonArray{asset}; assert(parse().name.isEmpty());
    UpdatePanel panel;
    QTemporaryDir temp; assert(temp.isValid());
    const auto target = temp.path() + "/custom plugins/InstantReplay";
    assert(QDir().mkpath(target + "/bin/64bit"));
    panel.modulePath = target + "/bin/64bit/InstantReplay.so";
    assert(panel.destination() == target);
    panel.modulePath = "/usr/lib/obs-plugins/InstantReplay.so"; assert(panel.destination().isEmpty());
    assert(!panel.install->isEnabled());
}
