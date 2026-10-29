#include "../ClipPanel.h"
#include <QApplication>
#include <QTemporaryDir>
#include <QFile>
#include <cassert>
int main(int argc, char **argv) {
    QApplication app(argc, argv);
    QTemporaryDir temp; assert(temp.isValid());
    QDir dir(temp.path()); dir.mkdir("match one"); dir.mkdir("match two");
    const auto one = dir.filePath("match one/InstantReplay_1.mkv");
    const auto two = dir.filePath("match two/InstantReplay_2.MP4");
    const auto junk = dir.filePath("match one/readme.txt");
    for (const auto &path : {one, two, junk}) { QFile f(path); assert(f.open(QIODevice::WriteOnly)); f.write("clip"); }
    ClipPanel panel;
    panel.folders = [&] { return QList<QPair<QString, QString>>{{"Match 1", dir.filePath("match one")}, {"Disabled match", dir.filePath("match two")}, {"Duplicate", dir.filePath("match one")}, {"Blank", ""}}; };
    panel.latest = [&] { return one; };
    panel.refreshClips(); assert(panel.clips->rowCount() == 2);
    int row = panel.clips->item(0, 0)->data(Qt::UserRole).toString() == two ? 0 : 1;
    panel.clips->selectRow(row);
    QString played; bool switched = true; int speed = 0;
    panel.play = [&](const QString &path, bool change) { played = path; switched = change; };
    panel.changeSpeed = [&](int percent) { speed = percent; };
    panel.speed->setValue(50); panel.playSelected();
    assert(played == two && !switched && speed == 50);
    panel.refreshClips(); assert(panel.selectedPath() == two);
    panel.switchScene->setChecked(true); panel.playSelected(); assert(switched);
    // Row actions act on their own clip and override the global checkbox.
    auto *playHere = qobject_cast<QPushButton *>(panel.clips->cellWidget(row, 3));
    auto *playSwitch = qobject_cast<QPushButton *>(panel.clips->cellWidget(row, 4));
    assert(playHere && playSwitch);
    panel.clips->selectRow(1 - row);
    panel.switchScene->setChecked(true); playHere->click();
    assert(played == two && !switched);
    panel.switchScene->setChecked(false); playSwitch->click();
    assert(played == two && switched);
    panel.refreshClips(); assert(panel.clips->cellWidget(row, 3) == playHere);
    panel.clips->selectRow(row);
    assert(QFile::remove(two)); played.clear(); panel.playSelected(); assert(played.isEmpty());
    panel.refreshClips(); assert(panel.clips->rowCount() == 1 && panel.selectedPath().isEmpty());
    // Nested backups in a configured folder are found, with duplicate roots deduplicated.
    assert(dir.mkpath("match two/round 2"));
    QFile nested(dir.filePath("match two/round 2/InstantReplay_3.mkv"));
    assert(nested.open(QIODevice::WriteOnly)); nested.write("nested clip"); nested.close();
    panel.refreshClips(); assert(panel.clips->rowCount() == 2);
    bool found = false;
    for (int index = 0; index < panel.clips->rowCount(); ++index)
        if (panel.clips->item(index, 0)->data(Qt::UserRole).toString() == QFileInfo(nested).canonicalFilePath()) found = true;
    assert(found);
}
