#include "../MatchPanel.h"
#include <QApplication>
#include <QTemporaryDir>
#include <cassert>

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    assert(obs_startup("en-US", nullptr, nullptr));
    obs_source_info info = {};
    info.id = "vlc_source"; info.type = OBS_SOURCE_TYPE_INPUT;
    info.get_name = [](void *) { return "VLC test source"; };
    info.create = [](obs_data_t *, obs_source_t *) -> void * { return new int(0); };
    info.destroy = [](void *p) { delete static_cast<int *>(p); };
    info.update = [](void *, obs_data_t *) {};
    obs_register_source(&info);
    auto *initial = obs_data_create(); obs_data_set_bool(initial, "loop", true);
    auto *vlc = obs_source_create("vlc_source", "playlist clips", initial, nullptr);
    obs_data_release(initial);
    QTemporaryDir temp; assert(temp.isValid());
    QString first = temp.path() + "/Match 1 — Team A", second = temp.path() + "/Match 2 — Team B";
    assert(QDir().mkpath(first) && QDir().mkpath(second));
    {
        MatchPanel panel;
        panel.addRow("Team A", first, true);
        panel.addRow("Disabled", temp.path() + "/unused", false);
        panel.addRow("Team B", second, true);
        panel.enabled->setChecked(true);
        panel.selectNext(-1); assert(panel.activePath() == first);
        panel.backupSaved(); assert(panel.activePath() == first); // No automatic advancement.
        panel.findChild<QPushButton *>("nextMatchButton")->click();
        assert(panel.active == 2 && panel.activePath() == second);
        assert(!panel.usable(0));
        assert(panel.folders->item(0, 0)->text() == "Completed");
        assert(panel.folders->item(0, 1)->font().italic());
        assert(panel.folders->item(0, 1)->foreground() == panel.folders->palette().brush(QPalette::Disabled, QPalette::Text));
        assert(panel.folders->item(2, 0)->text() == "Active");
        assert(panel.folders->item(2, 1)->font().bold());
        panel.folders->selectRow(2); panel.move(-1);
        assert(panel.active == 1 && panel.activePath() == second); // Reordering retains active match.
        panel.vlcName->setText("playlist clips");
        panel.refreshVlc(true);
        auto *settings = obs_source_get_settings(vlc);
        assert(obs_data_get_bool(settings, "loop"));
        auto *playlist = obs_data_get_array(settings, "playlist");
        assert(obs_data_array_count(playlist) == 1);
        auto *item = obs_data_array_item(playlist, 0);
        assert(QString::fromUtf8(obs_data_get_string(item, "value")) == second);
        obs_data_release(item); obs_data_array_release(playlist); obs_data_release(settings);
        auto *saved = obs_data_create(); panel.save(saved);
        MatchPanel restored; restored.load(saved); obs_data_release(saved);
        assert(restored.activePath() == second && restored.folders->rowCount() == 3);
        assert(!restored.usable(2));
        assert(!restored.usable(0) && restored.folders->item(0, 0)->text() == "Completed");
        int saves = 0;
        restored.settingsChanged = [&] { ++saves; };
        restored.findChild<QPushButton *>("resetMatchesButton")->click();
        assert(saves == 1);
        assert(restored.usable(0) && !restored.usable(2));
        assert(restored.activePath() == second);
        assert(restored.folders->item(0, 0)->text() == "Ready");
        // Completion follows a row when reordered and can be cleared manually.
        restored.advanceMatch(); assert(restored.active == -1);
        restored.folders->selectRow(1); restored.move(-1);
        assert(restored.folders->item(0, 0)->text() == "Completed");
        restored.folders->item(0, 0)->setCheckState(Qt::Checked);
        assert(!restored.folders->item(0, 0)->data(Qt::UserRole).toBool());
        restored.folders->item(0, 0)->setCheckState(Qt::Unchecked);
        restored.resetMatches(); assert(!restored.usable(0)); // Manual Off stays Off.
        assert(restored.vlcName->text() == "playlist clips");
        panel.resetMatches();
        // A live/preview source defers automatic refresh until it is no longer showing.
        obs_source_inc_showing(vlc);
        panel.active = 0; panel.autoRefresh->setChecked(true);
        assert(panel.pendingRefresh);
        settings = obs_source_get_settings(vlc); playlist = obs_data_get_array(settings, "playlist");
        item = obs_data_array_item(playlist, 0);
        assert(QString::fromUtf8(obs_data_get_string(item, "value")) == second);
        obs_data_release(item); obs_data_array_release(playlist); obs_data_release(settings);
        obs_source_dec_showing(vlc); panel.refreshVlc(false); assert(!panel.pendingRefresh);
        settings = obs_source_get_settings(vlc); playlist = obs_data_get_array(settings, "playlist");
        item = obs_data_array_item(playlist, 0);
        assert(QString::fromUtf8(obs_data_get_string(item, "value")) == first);
        obs_data_release(item); obs_data_array_release(playlist); obs_data_release(settings);
        panel.active = 1; panel.advanceMatch(); assert(panel.active == -1);
        assert(panel.activePath().isEmpty());
        assert(panel.folders->item(1, 0)->text() == "Completed");
        panel.resetMatches();
        assert(panel.active == -1 && panel.usable(1) && !panel.usable(2));
    }
    obs_source_release(vlc);
    obs_queue_task(OBS_TASK_DESTROY, [](void *) {}, nullptr, true);
    obs_shutdown();
}
