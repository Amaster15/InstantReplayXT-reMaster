#pragma once
#include <obs-module.h>
#include <QWidget>
#include <QTableWidget>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QCheckBox>
#include <QLineEdit>
#include <QComboBox>
#include <QLabel>
#include <QFileDialog>
#include <QTimer>
#include <QDir>
#include <QSignalBlocker>
#include <functional>

// Scene-collection settings. Removing a row never deletes clips from disk.
class MatchPanel : public QWidget {
public:
    QTableWidget *folders;
    QCheckBox *enabled, *autoRefresh;
    QLineEdit *vlcName;
    QLabel *activeLabel, *status;
    int active = -1;
    bool pendingRefresh = false, loading = false;
    std::function<QString()> legacyFolder;

    MatchPanel(QWidget *parent = nullptr) : QWidget(parent) {
        auto *layout = new QVBoxLayout(this);
        enabled = new QCheckBox("Use match folders instead of the single backup folder", this);
        layout->addWidget(enabled);
        folders = new QTableWidget(0, 3, this);
        folders->setHorizontalHeaderLabels({"Enabled", "Match / teams", "Backup folder"});
        folders->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        folders->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
        folders->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
        folders->setSelectionBehavior(QAbstractItemView::SelectRows);
        folders->setSelectionMode(QAbstractItemView::SingleSelection);
        layout->addWidget(folders);
        auto *buttons = new QHBoxLayout;
        layout->addLayout(buttons);
        auto button = [&](const QString &name, auto fn) {
            auto *b = new QPushButton(name, this); buttons->addWidget(b);
            connect(b, &QPushButton::clicked, this, fn);
        };
        button("Add folder…", [this] {
            auto path = QFileDialog::getExistingDirectory(this, "Choose or create a match backup folder");
            if (path.isEmpty()) return;
            addRow(QDir(path).dirName(), path, true);
            if (active < 0) active = folders->rowCount() - 1;
            showActive(); requestRefresh();
        });
        button("Browse…", [this] {
            int row = folders->currentRow(); if (row < 0) return;
            auto path = QFileDialog::getExistingDirectory(this, "Match backup folder", folders->item(row, 2)->text());
            if (!path.isEmpty()) folders->item(row, 2)->setText(path);
        });
        button("Remove from list", [this] {
            int row = folders->currentRow(); if (row < 0) return;
            folders->removeRow(row);
            if (active == row) { active = -1; selectNext(row - 1); }
            else if (active > row) --active;
            showActive(); requestRefresh();
        });
        button("Up", [this] { move(-1); });
        button("Down", [this] { move(1); });
        auto *selection = new QHBoxLayout; layout->addLayout(selection);
        auto *use = new QPushButton("Use selected match", this);
        auto *next = new QPushButton("Next match", this);
        selection->addWidget(use); selection->addWidget(next);
        connect(use, &QPushButton::clicked, this, [this] {
            int row = folders->currentRow();
            if (usable(row)) { active = row; showActive(); requestRefresh(); }
            else status->setText("Select an enabled match with a folder.");
        });
        connect(next, &QPushButton::clicked, this, [this] { if (active >= 0) { selectNext(active); requestRefresh(); } else status->setText("Choose an enabled match with Use selected match."); });
        activeLabel = new QLabel(this); activeLabel->setTextFormat(Qt::PlainText); activeLabel->setWordWrap(true);
        layout->addWidget(activeLabel);
        auto *note = new QLabel("Normally keep one match active for all its clips, then click Next match. Disabled rows are skipped. At the end, select a match explicitly; the list does not wrap. Each folder has its own backup numbering. Removing a row keeps its files.", this);
        note->setWordWrap(true); layout->addWidget(note);
        auto *vlcRow = new QHBoxLayout; layout->addLayout(vlcRow);
        vlcRow->addWidget(new QLabel("BRB VLC source name", this));
        vlcName = new QLineEdit(this); vlcName->setPlaceholderText("Exact OBS VLC Video Source name"); vlcRow->addWidget(vlcName);
        autoRefresh = new QCheckBox("Refresh VLC after backups and match changes (wait while source is visible)", this); layout->addWidget(autoRefresh);
        auto *refresh = new QPushButton("Refresh VLC now (restarts playlist)", this); layout->addWidget(refresh);
        status = new QLabel("Choose a VLC Video Source to manage its playlist from these folders.", this);
        status->setTextFormat(Qt::PlainText); status->setWordWrap(true); layout->addWidget(status);
        connect(refresh, &QPushButton::clicked, this, [this] { refreshVlc(true); });
        connect(folders, &QTableWidget::itemChanged, this, [this] {
            if (loading) return;
            if (active >= 0 && !usable(active)) selectNext(active);
            showActive(); requestRefresh();
        });
        connect(enabled, &QCheckBox::toggled, this, [this] { showActive(); requestRefresh(); });
        connect(autoRefresh, &QCheckBox::toggled, this, [this](bool value) { if (value) requestRefresh(); else pendingRefresh = false; });
        auto *timer = new QTimer(this); timer->setInterval(1000);
        connect(timer, &QTimer::timeout, this, [this] { if (pendingRefresh) refreshVlc(false); }); timer->start();
        showActive();
    }
    bool usable(int row) const {
        return row >= 0 && row < folders->rowCount() && folders->item(row, 0) && folders->item(row, 2) &&
            folders->item(row, 0)->checkState() == Qt::Checked && !folders->item(row, 2)->text().trimmed().isEmpty();
    }
    QString activePath() const { return usable(active) ? folders->item(active, 2)->text().trimmed() : QString(); }
    void addRow(const QString &name, const QString &path, bool on) {
        QSignalBlocker block(folders);
        int row = folders->rowCount(); folders->insertRow(row);
        auto *check = new QTableWidgetItem;
        check->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
        check->setCheckState(on ? Qt::Checked : Qt::Unchecked);
        folders->setItem(row, 0, check);
        folders->setItem(row, 1, new QTableWidgetItem(name));
        folders->setItem(row, 2, new QTableWidgetItem(path));
        folders->selectRow(row);
    }
    void selectNext(int after) {
        for (int row = after + 1; row < folders->rowCount(); ++row)
            if (usable(row)) { active = row; showActive(); return; }
        active = -1; showActive();
        status->setText("No next enabled match. Choose a match or add a folder; backups are paused in match mode.");
    }
    void showActive() {
        activeLabel->setText(!enabled->isChecked() ? "Match mode is off: using the single backup folder." :
            usable(active) ? "Active: " + folders->item(active, 1)->text() + " — " + activePath() : "No active match — match backups paused.");
    }
    void move(int delta) {
        int from = folders->currentRow(), to = from + delta;
        if (from < 0 || to < 0 || to >= folders->rowCount()) return;
        QSignalBlocker block(folders);
        for (int col = 0; col < 3; ++col) {
            auto *a = folders->takeItem(from, col), *b = folders->takeItem(to, col);
            folders->setItem(from, col, b); folders->setItem(to, col, a);
        }
        if (active == from) active = to; else if (active == to) active = from;
        folders->selectRow(to); showActive();
    }
    void backupSaved() { requestRefresh(); }
    void requestRefresh() { if (!loading && autoRefresh->isChecked()) { pendingRefresh = true; refreshVlc(false); } }
    void refreshVlc(bool force) {
        if (loading) return;
        auto name = vlcName->text().trimmed().toUtf8();
        auto *source = obs_get_source_by_name(name.constData());
        if (!source || std::string(obs_source_get_unversioned_id(source)) != "vlc_source") {
            obs_source_release(source); pendingRefresh = false;
            status->setText("Select an existing OBS VLC Video Source by its exact name."); return;
        }
        if (!force && obs_source_showing(source)) {
            pendingRefresh = true; obs_source_release(source);
            status->setText("VLC refresh queued until the BRB source is off-screen. Refresh now will restart it."); return;
        }
        QStringList paths;
        if (enabled->isChecked()) {
            if (!activePath().isEmpty()) paths.append(activePath());
        } else if (legacyFolder && !legacyFolder().trimmed().isEmpty()) paths.append(legacyFolder().trimmed());
        paths.removeDuplicates();
        for (const auto &path : paths) {
            if (!QDir(path).exists()) {
                obs_source_release(source); pendingRefresh = false;
                status->setText("Folder does not exist; VLC playlist unchanged: " + path); return;
            }
        }
        auto *settings = obs_data_create();
        auto *list = obs_data_array_create();
        for (const auto &path : paths) {
            auto *item = obs_data_create(); obs_data_set_string(item, "value", path.toUtf8().constData());
            obs_data_array_push_back(list, item); obs_data_release(item);
        }
        obs_data_set_array(settings, "playlist", list);
        obs_source_update(source, settings);
        obs_data_array_release(list); obs_data_release(settings); obs_source_release(source);
        pendingRefresh = false;
        status->setText(QString("VLC playlist refreshed from %1 folder(s). Existing loop/shuffle settings kept.").arg(paths.size()));
    }
    void save(obs_data_t *obj) const {
        obs_data_set_bool(obj, "matchMode", enabled->isChecked());
        obs_data_set_int(obj, "activeMatch", active);
        obs_data_set_string(obj, "vlcSource", vlcName->text().toUtf8().constData());
        obs_data_set_bool(obj, "vlcAutoRefresh", autoRefresh->isChecked());
        auto *array = obs_data_array_create();
        for (int row = 0; row < folders->rowCount(); ++row) {
            auto *item = obs_data_create();
            obs_data_set_bool(item, "enabled", folders->item(row, 0)->checkState() == Qt::Checked);
            obs_data_set_string(item, "name", folders->item(row, 1)->text().toUtf8().constData());
            obs_data_set_string(item, "path", folders->item(row, 2)->text().toUtf8().constData());
            obs_data_array_push_back(array, item); obs_data_release(item);
        }
        obs_data_set_array(obj, "matches", array); obs_data_array_release(array);
    }
    void load(obs_data_t *obj) {
        loading = true; pendingRefresh = false;
        folders->setRowCount(0);
        auto *array = obs_data_get_array(obj, "matches");
        for (size_t i = 0; i < obs_data_array_count(array); ++i) {
            auto *item = obs_data_array_item(array, i);
            addRow(QString::fromUtf8(obs_data_get_string(item, "name")), QString::fromUtf8(obs_data_get_string(item, "path")), obs_data_get_bool(item, "enabled"));
            obs_data_release(item);
        }
        obs_data_array_release(array);
        enabled->setChecked(obs_data_get_bool(obj, "matchMode"));
        active = obs_data_has_user_value(obj, "activeMatch") ? int(obs_data_get_int(obj, "activeMatch")) : -1;
        if (!usable(active)) active = -1;
        vlcName->setText(QString::fromUtf8(obs_data_get_string(obj, "vlcSource")));
        autoRefresh->setChecked(obs_data_get_bool(obj, "vlcAutoRefresh"));
        loading = false; showActive();
    }
};
