#pragma once
#include <QWidget>
#include <QTableWidget>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QCheckBox>
#include <QSlider>
#include <QLabel>
#include <QDir>
#include <QDateTime>
#include <QSet>
#include <QTimer>
#include <functional>
#include <algorithm>

class ClipPanel : public QWidget {
public:
    QTableWidget *clips;
    QCheckBox *switchScene;
    QSlider *speed;
    QLabel *status;
    // Named folders include inactive/disabled matches; they remain part of the library.
    std::function<QList<QPair<QString, QString>>()> folders;
    std::function<QString()> latest;
    std::function<void(const QString &, bool)> play;
    std::function<void(int)> changeSpeed;
    explicit ClipPanel(QWidget *parent = nullptr) : QWidget(parent) {
        auto *layout = new QVBoxLayout(this);
        auto *note = new QLabel("Saved clips from all configured backup / match folders, plus the current replay file. Enable backups to keep older clips.");
        note->setWordWrap(true); layout->addWidget(note);
        clips = new QTableWidget(0, 3, this);
        clips->setHorizontalHeaderLabels({"Clip", "Match / folder", "Saved"});
        clips->setSelectionBehavior(QAbstractItemView::SelectRows);
        clips->setSelectionMode(QAbstractItemView::SingleSelection);
        clips->setEditTriggers(QAbstractItemView::NoEditTriggers);
        clips->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
        clips->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
        clips->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
        layout->addWidget(clips);
        auto *buttons = new QHBoxLayout; layout->addLayout(buttons);
        auto *refresh = new QPushButton("Refresh clips", this);
        auto *playButton = new QPushButton("Play selected clip", this);
        buttons->addWidget(refresh); buttons->addWidget(playButton);
        switchScene = new QCheckBox("Switch to replay scene when playing a clip", this);
        layout->addWidget(switchScene);
        auto *speedRow = new QHBoxLayout; layout->addLayout(speedRow);
        speedRow->addWidget(new QLabel("Playback speed", this));
        speed = new QSlider(Qt::Horizontal, this); speed->setRange(25, 200); speed->setValue(100);
        speed->setAccessibleName("Replay playback speed percent"); speedRow->addWidget(speed);
        auto *value = new QLabel("100%", this); value->setMinimumWidth(55); speedRow->addWidget(value);
        auto *normal = new QPushButton("Normal speed", this); speedRow->addWidget(normal);
        auto *hint = new QLabel("25% = quarter speed • 100% = normal • 200% = double speed. OBS reloads the clip on speed changes; playback resumes near the same position with a possible brief interruption. With scene switching off, enter the replay scene manually to show the selected clip.");
        hint->setWordWrap(true); layout->addWidget(hint);
        status = new QLabel(this); status->setWordWrap(true); status->setTextFormat(Qt::PlainText); layout->addWidget(status);
        auto *debounce = new QTimer(this); debounce->setSingleShot(true); debounce->setInterval(180);
        connect(speed, &QSlider::valueChanged, this, [=](int percent) { value->setText(QString::number(percent) + "%"); debounce->start(); });
        connect(debounce, &QTimer::timeout, this, [this] { if (changeSpeed) changeSpeed(speed->value()); });
        connect(normal, &QPushButton::clicked, this, [this] { speed->setValue(100); });
        connect(refresh, &QPushButton::clicked, this, [this] { refreshClips(); });
        connect(playButton, &QPushButton::clicked, this, [this] { playSelected(); });
        connect(clips, &QTableWidget::cellDoubleClicked, this, [this](int, int) { playSelected(); });
        auto *timer = new QTimer(this); timer->setInterval(2000);
        connect(timer, &QTimer::timeout, this, [this] { if (isVisible()) refreshClips(); }); timer->start();
    }
    QString selectedPath() const {
        auto *item = clips->item(clips->currentRow(), 0);
        return item ? item->data(Qt::UserRole).toString() : QString();
    }
    void playSelected() {
        const auto path = selectedPath();
        if (path.isEmpty()) { status->setText("Select a clip first."); return; }
        if (!QFileInfo::exists(path)) { status->setText("This clip has moved or been deleted. Refresh the list."); return; }
        if (changeSpeed) changeSpeed(speed->value());
        if (play) play(path, switchScene->isChecked());
    }
    void refreshClips() {
        struct Entry { QFileInfo file; QString label, path; };
        QList<Entry> entries; QSet<QString> seen;
        auto add = [&](const QFileInfo &file, const QString &label) {
            if (!file.isFile()) return;
            auto path = file.canonicalFilePath();
            if (path.isEmpty() || seen.contains(path)) return;
            seen.insert(path); entries.append({file, label, path});
        };
        if (latest) { auto path = latest(); if (!path.isEmpty()) add(QFileInfo(path), "Current replay"); }
        if (folders) for (const auto &folder : folders()) {
            if (folder.second.trimmed().isEmpty()) continue;
            for (const auto &file : QDir(folder.second).entryInfoList(QDir::Files | QDir::Readable)) {
                const auto ext = file.suffix().toLower();
                if (QStringList{"mkv", "mp4", "mov", "flv", "ts", "m4v", "avi", "webm"}.contains(ext)) add(file, folder.first);
            }
        }
        std::sort(entries.begin(), entries.end(), [](const Entry &a, const Entry &b) {
            return a.file.lastModified() == b.file.lastModified() ? a.path < b.path : a.file.lastModified() > b.file.lastModified();
        });
        auto selected = selectedPath();
        clips->clearSelection();
        clips->setCurrentCell(-1, -1);
        clips->setRowCount(entries.size());
        for (int row = 0; row < entries.size(); ++row) {
            const auto &e = entries[row];
            auto *name = new QTableWidgetItem(e.file.fileName()); name->setData(Qt::UserRole, e.path); name->setToolTip(e.path);
            clips->setItem(row, 0, name);
            auto *label = new QTableWidgetItem(e.label); label->setToolTip(e.path); clips->setItem(row, 1, label);
            clips->setItem(row, 2, new QTableWidgetItem(e.file.lastModified().toString("yyyy-MM-dd HH:mm:ss")));
            if (e.path == selected) clips->selectRow(row);
        }
    }
};
