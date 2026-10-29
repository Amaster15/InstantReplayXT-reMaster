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
#include <QDirIterator>
#include <QDateTime>
#include <QSet>
#include <QTimer>
#include <QPainter>
#include <QIcon>
#include <QComboBox>
#include <functional>
#include <algorithm>

class ClipPanel : public QWidget {
public:
    QTableWidget *clips;
    QCheckBox *switchScene;
    QSlider *speed;
    QComboBox *playbackMode;
    QLabel *modeHint;
    QLabel *status;
    // Named folders include inactive/disabled matches; they remain part of the library.
    std::function<QList<QPair<QString, QString>>()> folders;
    std::function<QString()> latest;
    std::function<void(const QString &, bool)> play;
    std::function<void(int)> changeSpeed;
    QIcon playbackIcon(bool showScreen) const {
        QIcon icon;
        for (const auto mode : {QIcon::Normal, QIcon::Disabled}) {
            QPixmap image(48, 48); image.setDevicePixelRatio(2); image.fill(Qt::transparent);
            QPainter painter(&image); painter.setRenderHint(QPainter::Antialiasing);
            const auto color = palette().color(mode == QIcon::Disabled ? QPalette::Disabled : QPalette::Active, QPalette::ButtonText);
            painter.setPen(QPen(color, 1.6));
            if (showScreen) {
                painter.drawRoundedRect(QRectF(2, 3, 20, 15), 2, 2);
                painter.drawLine(QPointF(12, 18), QPointF(12, 21));
                painter.drawLine(QPointF(8, 21), QPointF(16, 21));
            }
            painter.setPen(Qt::NoPen); painter.setBrush(color);
            painter.drawPolygon(showScreen ? QPolygonF{QPointF(9, 6), QPointF(9, 15), QPointF(16, 10.5)}
                                            : QPolygonF{QPointF(7, 4), QPointF(7, 20), QPointF(20, 12)});
            painter.end(); icon.addPixmap(image, mode);
        }
        return icon;
    }
    explicit ClipPanel(QWidget *parent = nullptr) : QWidget(parent) {
        auto *layout = new QVBoxLayout(this);
        auto *note = new QLabel("Saved clips from all configured backup / match folders (including subfolders), plus the current replay file. Enable backups to keep older clips.");
        note->setWordWrap(true); layout->addWidget(note);
        clips = new QTableWidget(0, 5, this);
        clips->setHorizontalHeaderLabels({"Clip", "Match / folder", "Saved", "", ""});
        clips->setSelectionBehavior(QAbstractItemView::SelectRows);
        clips->setSelectionMode(QAbstractItemView::SingleSelection);
        clips->setEditTriggers(QAbstractItemView::NoEditTriggers);
        clips->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
        clips->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
        clips->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
        clips->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
        clips->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
        layout->addWidget(clips);
        auto *buttons = new QHBoxLayout; layout->addLayout(buttons);
        auto *refresh = new QPushButton("Refresh clips", this);
        auto *playButton = new QPushButton("Play selected clip", this);
        buttons->addWidget(refresh); buttons->addWidget(playButton);
        switchScene = new QCheckBox("Switch scene for Play selected clip / double-click", this);
        layout->addWidget(switchScene);
        auto *modeRow = new QHBoxLayout; layout->addLayout(modeRow);
        modeRow->addWidget(new QLabel("Replay player", this));
        playbackMode = new QComboBox(this);
        playbackMode->addItems({"Standard Media Source", "VLC live slow motion"});
        playbackMode->setAccessibleName("Replay player mode"); modeRow->addWidget(playbackMode, 1);
        auto *speedRow = new QHBoxLayout; layout->addLayout(speedRow);
        speedRow->addWidget(new QLabel("Playback speed", this));
        speed = new QSlider(Qt::Horizontal, this); speed->setRange(25, 200); speed->setValue(100);
        speed->setAccessibleName("Replay playback speed percent"); speedRow->addWidget(speed);
        auto *value = new QLabel("100%", this); value->setMinimumWidth(55); speedRow->addWidget(value);
        auto *normal = new QPushButton("Normal speed", this); speedRow->addWidget(normal);
        modeHint = new QLabel(this); modeHint->setWordWrap(true); layout->addWidget(modeHint);
        auto updateHint = [this] {
            modeHint->setText(playbackMode->currentIndex() == 0
                ? "Standard Media Source reloads the clip when speed changes. Try VLC live slow motion for speed changes without reloading."
                : "VLC changes speed during playback without reloading the clip. Requires VLC 3.x (64-bit on Windows). 25% = quarter speed; 100% = normal; 200% = double speed.");
        };
        connect(playbackMode, qOverload<int>(&QComboBox::currentIndexChanged), this, [updateHint](int) { updateHint(); }); updateHint();
        status = new QLabel(this); status->setWordWrap(true); status->setTextFormat(Qt::PlainText); layout->addWidget(status);
        auto *debounce = new QTimer(this); debounce->setSingleShot(true); debounce->setInterval(180);
        connect(speed, &QSlider::valueChanged, this, [=](int percent) { value->setText(QString::number(percent) + "%");
            if (playbackMode->currentIndex() == 1) { if (!debounce->isActive()) debounce->start(50); }
            else debounce->start(180); });
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
        playClip(selectedPath(), switchScene->isChecked());
    }
    void playClip(const QString &path, bool changeScene) {
        if (path.isEmpty()) { status->setText("Select a clip first."); return; }
        if (!QFileInfo::exists(path)) { status->setText("This clip has moved or been deleted. Refresh the list."); return; }
        if (changeSpeed) changeSpeed(speed->value());
        if (play) play(path, changeScene);
    }
    void refreshClips() {
        struct Entry { QFileInfo file; QString label, path; };
        QList<Entry> entries; QSet<QString> seen, scannedFolders;
        auto add = [&](const QFileInfo &file, const QString &label) {
            if (!file.isFile()) return;
            auto path = file.canonicalFilePath();
            if (path.isEmpty() || seen.contains(path)) return;
            seen.insert(path); entries.append({file, label, path});
        };
        if (latest) { auto path = latest(); if (!path.isEmpty()) add(QFileInfo(path), "Current replay"); }
        if (folders) for (const auto &folder : folders()) {
            if (folder.second.trimmed().isEmpty()) continue;
            const auto directory = QFileInfo(folder.second).canonicalFilePath();
            if (directory.isEmpty() || scannedFolders.contains(directory)) continue;
            scannedFolders.insert(directory);
            QDirIterator iterator(directory, QDir::Files | QDir::Readable, QDirIterator::Subdirectories);
            while (iterator.hasNext()) {
                iterator.next();
                const auto file = iterator.fileInfo();
                const auto ext = file.suffix().toLower();
                if (QStringList{"mkv", "mp4", "mov", "flv", "ts", "m4v", "avi", "webm"}.contains(ext)) add(file, folder.first);
            }
        }
        std::sort(entries.begin(), entries.end(), [](const Entry &a, const Entry &b) {
            return a.file.lastModified() == b.file.lastModified() ? a.path < b.path : a.file.lastModified() > b.file.lastModified();
        });
        // Keep existing row buttons alive during periodic scans when nothing changed.
        bool unchanged = clips->rowCount() == entries.size();
        for (int row = 0; unchanged && row < entries.size(); ++row) {
            const auto &e = entries[row];
            unchanged = clips->item(row, 0) && clips->item(row, 0)->data(Qt::UserRole).toString() == e.path &&
                clips->item(row, 1)->text() == e.label &&
                clips->item(row, 2)->text() == e.file.lastModified().toString("yyyy-MM-dd HH:mm:ss");
        }
        if (unchanged) return;
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
            auto *playHere = new QPushButton(clips);
            playHere->setIcon(playbackIcon(false)); playHere->setIconSize(QSize(24, 24));
            playHere->setMinimumSize(36, 30);
            playHere->setToolTip("Play / prepare this clip without changing the current scene");
            playHere->setAccessibleName("Play " + e.file.fileName());
            auto *playSwitch = new QPushButton(clips);
            playSwitch->setIcon(playbackIcon(true)); playSwitch->setIconSize(QSize(24, 24));
            playSwitch->setMinimumSize(36, 30);
            playSwitch->setToolTip("Switch to the replay scene and play this clip");
            playSwitch->setAccessibleName("Play and switch scene: " + e.file.fileName());
            connect(playHere, &QPushButton::clicked, this, [this, path = e.path] { playClip(path, false); });
            connect(playSwitch, &QPushButton::clicked, this, [this, path = e.path] { playClip(path, true); });
            clips->setCellWidget(row, 3, playHere);
            clips->setCellWidget(row, 4, playSwitch);
            if (e.path == selected) clips->selectRow(row);
        }
    }
};
