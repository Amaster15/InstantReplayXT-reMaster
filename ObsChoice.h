#pragma once
#include <obs-module.h>
#include <QComboBox>
#include <QSignalBlocker>
#include <QStringList>
#include <functional>

// Keep the stored OBS name separate from the label for a missing source.
class ObsChoice : public QComboBox {
public:
    std::function<QStringList()> options;
    explicit ObsChoice(QWidget *parent = nullptr) : QComboBox(parent) { setEditable(false); }
    QString text() const { return currentData().toString(); }
    void setText(const QString &name) {
        int index = findData(name);
        if (index < 0) { addItem(name.isEmpty() ? "Choose…" : name + " (unavailable)", name); index = count() - 1; }
        setCurrentIndex(index);
    }
    void refresh() {
        const auto selected = text();
        const QSignalBlocker block(this);
        auto names = options ? options() : QStringList{};
        names.removeDuplicates(); names.sort(Qt::CaseInsensitive);
        clear(); addItem("Choose…", QString());
        for (const auto &name : names) addItem(name, name);
        setText(selected);
    }
    void showPopup() override { refresh(); QComboBox::showPopup(); }
};

inline QStringList obsNames(const char *type = nullptr) {
    struct List { QStringList names; const char *type; } list{{}, type};
    auto collect = [](void *data, obs_source_t *source) {
        auto &list = *static_cast<List *>(data);
        if (!list.type || QString::fromUtf8(obs_source_get_unversioned_id(source)) == list.type)
            list.names.append(QString::fromUtf8(obs_source_get_name(source)));
        return true;
    };
    if (!type) obs_enum_scenes(collect, &list);
    else obs_enum_sources(collect, &list);
    return list.names;
}
inline QStringList replaySourceNames(const QString &sceneName) {
    QStringList names;
    auto *source = obs_get_source_by_name(sceneName.toUtf8().constData());
    auto *scene = obs_scene_from_source(source);
    if (scene) obs_scene_enum_items(scene, [](obs_scene_t *, obs_sceneitem_t *item, void *data) {
        auto *source = obs_sceneitem_get_source(item);
        if (QString::fromUtf8(obs_source_get_unversioned_id(source)) == "ffmpeg_source")
            static_cast<QStringList *>(data)->append(QString::fromUtf8(obs_source_get_name(source)));
        return true;
    }, &names);
    obs_source_release(source);
    return names;
}
