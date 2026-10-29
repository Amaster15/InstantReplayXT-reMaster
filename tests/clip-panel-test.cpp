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
    assert(QFile::remove(two)); played.clear(); panel.playSelected(); assert(played.isEmpty());
    panel.refreshClips(); assert(panel.clips->rowCount() == 1 && panel.selectedPath().isEmpty());
}
