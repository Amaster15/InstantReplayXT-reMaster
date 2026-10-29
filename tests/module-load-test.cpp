// Exercise the real shared module and Qt dialog with a minimal OBS frontend shim.
#include <obs-module.h>
#include <obs-frontend-api.h>
#include <QApplication>
#include <QMainWindow>
#include <QAction>
#include <QLibrary>
#include <cassert>
#include <QComboBox>
#include <QLineEdit>
#include <QLabel>
#include <QSlider>
#include <QEventLoop>
#include <QTimer>
#include "../ObsChoice.h"
#include "../SettingsStore.h"
#include <util/bmem.h>
#include <QPushButton>
#include <QDialog>
#include <QCheckBox>
#include <QSpinBox>
#include <QTemporaryDir>
#include <vector>
#include <algorithm>
static QMainWindow *window;
static int events, saves, saveRequests;
static std::vector<std::pair<obs_frontend_save_cb, void *>> saveCallbacks;
static obs_data_t *savedData;
static const char *collectionName = "Test collection";
static ObsChoice *choice(const char *name) { return static_cast<ObsChoice *>(window->findChild<QComboBox *>(name)); }
static void loadSettings(obs_data_t *data) {
    for (auto it = saveCallbacks.rbegin(); it != saveCallbacks.rend(); ++it) it->first(data, false, it->second);
}
extern "C" {
char *obs_frontend_get_current_scene_collection() { return bstrdup(collectionName); }
void *obs_frontend_get_main_window() { return window; }
void *obs_frontend_add_tools_menu_qaction(const char *name) { return new QAction(name, window); }
void obs_frontend_add_event_callback(obs_frontend_event_cb, void *) { ++events; }
void obs_frontend_remove_event_callback(obs_frontend_event_cb, void *) { --events; }
void obs_frontend_add_save_callback(obs_frontend_save_cb cb, void *data) { ++saves; saveCallbacks.emplace_back(cb, data); }
void obs_frontend_remove_save_callback(obs_frontend_save_cb cb, void *data) {
    --saves; saveCallbacks.erase(std::remove(saveCallbacks.begin(), saveCallbacks.end(), std::make_pair(cb, data)), saveCallbacks.end());
}
void obs_frontend_save() {
    ++saveRequests;
    for (auto it = saveCallbacks.rbegin(); it != saveCallbacks.rend(); ++it) it->first(savedData, true, it->second);
}
void obs_frontend_push_ui_translation(obs_frontend_translate_ui_cb) {}
void obs_frontend_pop_ui_translation() {}
}
int main(int argc, char **argv) {
    QApplication app(argc, argv);
    assert(argc == 2);
    assert(obs_startup("en-US", nullptr, nullptr));
    obs_source_info info{}; info.id = "ffmpeg_source"; info.type = OBS_SOURCE_TYPE_INPUT;
    info.output_flags = OBS_SOURCE_AUDIO;
    info.get_name = [](void *) { return "Test Media Source"; };
    info.create = [](obs_data_t *, obs_source_t *) -> void * { return new int(0); };
    info.destroy = [](void *p) { delete static_cast<int *>(p); };
    obs_register_source(&info);
    auto *privateData = obs_get_private_data();
    obs_data_set_bool(privateData, "AbsoluteCoordinates", true); obs_data_release(privateData);
    QLibrary library(QString::fromLocal8Bit(argv[1]));
    assert(library.load());
    auto load = reinterpret_cast<bool (*)()>(library.resolve("obs_module_load"));
    auto unload = reinterpret_cast<void (*)()>(library.resolve("obs_module_unload"));
    auto version = reinterpret_cast<uint32_t (*)()>(library.resolve("obs_module_ver"));
    assert(load && unload && version);
    assert(version() == LIBOBS_API_VER);
    savedData = obs_data_create();
    QTemporaryDir savedDir; assert(savedDir.isValid());
    for (bool parentFirst : {false, true}) {
        window = new QMainWindow;
        assert(load());
        window->findChild<QDialog *>("InstantReplay")->setProperty("irxtConfigPath", savedDir.filePath("config/settings.json"));
        if (!parentFirst) { auto *empty = obs_data_create(); loadSettings(empty); obs_data_release(empty); QCoreApplication::processEvents(); }
        assert(events == 1 && saves == 2);
        assert(!window->findChildren<QAction *>().empty());
        if (parentFirst) {
            const auto path = savedDir.filePath("settings.json").toUtf8();
            auto *restored = obs_data_create_from_json_file(path.constData()); assert(restored);
            int before = saveRequests;
            loadSettings(restored); obs_data_release(restored);
            assert(choice("sceneLineEdit")->text() == "Saved replay scene");
            assert(choice("replaySourceNameEdit")->text() == "Saved media source");
            assert(window->findChild<QCheckBox *>("enableCheck")->isChecked());
            assert(window->findChild<QSpinBox *>("waitSpinBox")->value() == 7);
            QEventLoop loop; QTimer::singleShot(650, &loop, &QEventLoop::quit); loop.exec();
            assert(saveRequests == before); // Loading must not queue an autosave.
            collectionName = "Other collection";
            auto *empty = obs_data_create(); loadSettings(empty); obs_data_release(empty);
            assert(choice("sceneLineEdit")->text() == "Replay Scene");
            collectionName = "Test collection";
            empty = obs_data_create(); loadSettings(empty); obs_data_release(empty);
            assert(choice("sceneLineEdit")->text() == "Saved replay scene");
            auto *recoveryScene = obs_scene_create("Recovered replay");
            auto *recoverySource = obs_source_create("ffmpeg_source", "Recovered media", nullptr, nullptr);
            obs_scene_add(recoveryScene, recoverySource);
            QString error;
            SettingsStore store(savedDir.filePath("config/settings.json"));
            assert(store.save("Recovery", QJsonObject{{"replayCallout_data", QJsonObject{
                {"scene", "Recovered replay"}, {"sourceName", "Recovered media"}, {"vlcReplayPlayer", true}, {"speedPercent", 50}}}}, error));
            collectionName = "Recovery";
            empty = obs_data_create(); loadSettings(empty); obs_data_release(empty);
            QCoreApplication::processEvents();
            auto *item = obs_scene_find_source(recoveryScene, "Recovered media (VLC live)");
            assert(item && obs_sceneitem_visible(item));
            assert(!obs_sceneitem_visible(obs_scene_find_source(recoveryScene, "Recovered media")));
            collectionName = "Test collection";
            empty = obs_data_create(); loadSettings(empty); obs_data_release(empty);
            QCoreApplication::processEvents();
            obs_source_release(recoverySource); obs_scene_release(recoveryScene);
        }
        if (!parentFirst) {
            QComboBox *mode = nullptr;
            for (auto *combo : window->findChildren<QComboBox *>())
                if (combo->accessibleName() == "Replay player mode") mode = combo;
            assert(mode);
            // VLC is the default. Invalid mode changes roll back with a visible error.
            assert(mode->currentData().toInt() == 1);
            mode->setCurrentIndex(mode->findData(0));
            assert(mode->currentData().toInt() == 1);
            bool errorShown = false;
            for (auto *label : window->findChildren<QLabel *>())
                if (label->text().contains("Choose a replay scene")) errorShown = true;
            assert(errorShown);
            auto *scene = obs_scene_create("Replay test");
            auto *source = obs_source_create("ffmpeg_source", "Media test", nullptr, nullptr);
            obs_scene_add(scene, source);
            choice("sceneLineEdit")->refresh();
            assert(choice("sceneLineEdit")->findData("Replay test") >= 0);
            assert(!choice("sceneLineEdit")->isEditable());
            choice("sceneLineEdit")->setText("Replay test");
            assert(choice("replaySourceNameEdit")->findData("Media test") >= 0);
            choice("replaySourceNameEdit")->setText("Media test");
            mode->setCurrentIndex(mode->findData(0));
            mode->setCurrentIndex(mode->findData(1));
            assert(mode->currentData().toInt() == 1);
            auto *vlc = obs_get_source_by_name("Media test (VLC live)"); assert(vlc);
            auto *slider = mode->parentWidget()->findChild<QSlider *>(); assert(slider);
            slider->setValue(50);
            QEventLoop loop; QTimer::singleShot(100, &loop, &QEventLoop::quit); loop.exec();
            auto *settings = obs_source_get_settings(vlc);
            assert(obs_data_get_int(settings, "speed_percent") == 50); obs_data_release(settings);
            mode->setCurrentIndex(mode->findData(0)); assert(mode->currentData().toInt() == 0);
            slider->setValue(75);
            QTimer::singleShot(250, &loop, &QEventLoop::quit); loop.exec();
            settings = obs_source_get_settings(source);
            assert(obs_data_get_int(settings, "speed_percent") == 75); obs_data_release(settings);
            // Editing schedules a save even if the user never presses Close.
            int before = saveRequests;
            choice("sceneLineEdit")->setText("Saved replay scene");
            const auto immediate = SettingsStore(savedDir.filePath("config/settings.json")).load("Test collection");
            assert(immediate["replayCallout_data"].toObject()["scene"].toString() == "Saved replay scene");
            choice("replaySourceNameEdit")->setText("Saved media source");
            window->findChild<QCheckBox *>("enableCheck")->setChecked(true);
            window->findChild<QSpinBox *>("waitSpinBox")->setValue(7);
            QTimer::singleShot(650, &loop, &QEventLoop::quit); loop.exec();
            assert(saveRequests > before);
            auto *data = obs_data_get_obj(savedData, "replayCallout_data");
            assert(std::string(obs_data_get_string(data, "scene")) == "Saved replay scene");
            assert(obs_data_get_int(data, "wait") == 7); obs_data_release(data);
            before = saveRequests;
            window->findChild<QPushButton *>("saveSettingsButton")->click();
            assert(saveRequests == before + 1);
            // Simulate OBS crashing before its own scene-collection file catches up.
            data = obs_data_get_obj(savedData, "replayCallout_data");
            obs_data_set_string(data, "scene", "Stale OBS scene"); obs_data_release(data);
            assert(obs_data_save_json(savedData, savedDir.filePath("settings.json").toUtf8().constData()));
            obs_source_release(vlc); obs_source_release(source); obs_scene_release(scene);
        }
        if (parentFirst) { delete window; window = nullptr; }
        unload();
        assert(events == 0 && saves == 0);
        delete window; window = nullptr;
    }
    // Recover from a damaged primary without crossing collection boundaries.
    const auto storePath = savedDir.filePath("store-test.json");
    SettingsStore store(storePath); QString error;
    assert(store.save("One", QJsonObject{{"value", 1}}, error));
    assert(store.save("Two", QJsonObject{{"value", 2}}, error));
    assert(store.load("One")["value"].toInt() == 1);
    assert(store.load("Two")["value"].toInt() == 2);
    QFile broken(storePath); assert(broken.open(QIODevice::WriteOnly)); broken.write("broken"); broken.close();
    assert(store.load("One")["value"].toInt() == 1);
    assert(store.save("Two", QJsonObject{{"value", 3}}, error));
    assert(store.load("One")["value"].toInt() == 1);
    assert(store.load("Two")["value"].toInt() == 3);
    SettingsStore unwritable(savedDir.path());
    assert(!unwritable.save("One", QJsonObject{{"value", 4}}, error));
    obs_data_release(savedData);
    assert(library.unload());
    obs_shutdown();
}
