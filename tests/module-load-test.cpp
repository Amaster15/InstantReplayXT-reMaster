// Exercise the real shared module and Qt dialog with a minimal OBS frontend shim.
#include <obs-module.h>
#include <obs-frontend-api.h>
#include <QApplication>
#include <QMainWindow>
#include <QAction>
#include <QLibrary>
#include <cassert>
static QMainWindow *window;
static int events, saves;
extern "C" {
void *obs_frontend_get_main_window() { return window; }
void *obs_frontend_add_tools_menu_qaction(const char *name) { return new QAction(name, window); }
void obs_frontend_add_event_callback(obs_frontend_event_cb, void *) { ++events; }
void obs_frontend_remove_event_callback(obs_frontend_event_cb, void *) { --events; }
void obs_frontend_add_save_callback(obs_frontend_save_cb, void *) { ++saves; }
void obs_frontend_remove_save_callback(obs_frontend_save_cb, void *) { --saves; }
void obs_frontend_push_ui_translation(obs_frontend_translate_ui_cb) {}
void obs_frontend_pop_ui_translation() {}
}
int main(int argc, char **argv) {
    QApplication app(argc, argv);
    assert(argc == 2);
    assert(obs_startup("en-US", nullptr, nullptr));
    QLibrary library(QString::fromLocal8Bit(argv[1]));
    assert(library.load());
    auto load = reinterpret_cast<bool (*)()>(library.resolve("obs_module_load"));
    auto unload = reinterpret_cast<void (*)()>(library.resolve("obs_module_unload"));
    auto version = reinterpret_cast<uint32_t (*)()>(library.resolve("obs_module_ver"));
    assert(load && unload && version);
    assert(version() == LIBOBS_API_VER);
    for (bool parentFirst : {false, true}) {
        window = new QMainWindow;
        assert(load());
        assert(events == 1 && saves == 2);
        assert(!window->findChildren<QAction *>().empty());
        if (parentFirst) { delete window; window = nullptr; }
        unload();
        assert(events == 0 && saves == 0);
        delete window; window = nullptr;
    }
    assert(library.unload());
    obs_shutdown();
}
