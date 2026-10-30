#include "../ReplayTrim.h"
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDir>
#include <cassert>

static QByteArray run(const QString &program, const QStringList &args) {
    QProcess p; p.start(program, args);
    assert(p.waitForStarted(5000)); assert(p.waitForFinished(30000));
    if (p.exitCode() != 0) qFatal("%s", p.readAllStandardError().constData());
    return p.readAllStandardOutput();
}
static QJsonObject probe(const QString &program, const QString &file) {
    return QJsonDocument::fromJson(run(program, {"-v", "error", "-show_format", "-show_streams",
        "-show_packets", "-show_data_hash", "sha256", "-of", "json", file})).object();
}
static QStringList hashes(const QJsonObject &data, int stream) {
    QStringList result;
    for (const auto &entry : data["packets"].toArray()) {
        const auto packet = entry.toObject();
        if (packet["stream_index"].toInt() == stream) result.append(packet["data_hash"].toString());
    }
    return result;
}
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv); assert(argc == 4);
    const QString helper = QString::fromLocal8Bit(argv[1]), inspector = QString::fromLocal8Bit(argv[2]);
    QTemporaryDir temp; assert(temp.isValid());
    // 80 seconds with two distinct audio stream indexes; packet content must survive unchanged.
    const auto longClip = temp.filePath("long.mp4");
    run(helper, {"-v", "error", "-nostdin", "-stream_loop", "3", "-i", QString::fromLocal8Bit(argv[3]),
        "-map", "0:v:0", "-map", "0:a:0", "-map", "0:a:0", "-c", "copy", longClip});
    const auto original = probe(inspector, longClip);
    assert(original["streams"].toArray().size() == 3);
    const auto backup = temp.filePath("backup 雪");
    for (const auto &extension : {QString("mp4"), QString("mkv"), QString("mov"), QString("ts")}) {
        const auto input = temp.filePath("OBS réplay with spaces." + extension);
        for (int seconds : {15, 30, 60, 120}) {
            run(helper, {"-v", "error", "-nostdin", "-y", "-i", longClip, "-map", "0", "-c", "copy", input});
            const auto before = probe(inspector, input);
            const auto inputDuration = before["format"].toObject()["duration"].toString().toDouble();
            const auto result = replay_trim::publish(input.toUtf8().toStdString(), "", backup.toUtf8().toStdString(), seconds, helper);
            assert(result.error.empty() && result.warning.empty());
            assert(!QFile::exists(input));
            const auto output = QString::fromUtf8(result.path.c_str());
            // std::filesystem uses native separators on Windows; compare actual
            // files while still rejecting publication inside the trim directory.
            const auto expectedOutput = QFileInfo(temp.filePath("InstantReplay." + extension));
            assert(expectedOutput.exists());
            assert(QFileInfo(output).canonicalFilePath() == expectedOutput.canonicalFilePath());
            const auto after = probe(inspector, output);
            assert(after["streams"].toArray().size() == 3);
            const auto duration = after["format"].toObject()["duration"].toString().toDouble();
            const double expected = std::min(double(seconds), inputDuration);
            std::fprintf(stderr, "%s preset %ds: %.3fs (input %.3fs)\n", qPrintable(extension), seconds, duration, inputDuration);
            double previousKey = 0, maxGop = 0;
            for (const auto &entry : before["packets"].toArray()) {
                const auto packet = entry.toObject();
                if (packet["stream_index"].toInt() == 0 && packet["flags"].toString().contains('K')) {
                    const double time = packet["pts_time"].toString().toDouble();
                    maxGop = std::max(maxGop, time - previousKey); previousKey = time;
                }
            }
            assert(duration >= expected - 0.2 && duration <= expected + maxGop + 0.2);
            for (int stream = 0; stream < 3; ++stream) {
                const auto sourcePackets = hashes(before, stream), outputPackets = hashes(after, stream);
                assert(!outputPackets.empty() && outputPackets.size() <= sourcePackets.size());
                assert(outputPackets == sourcePackets.mid(sourcePackets.size() - outputPackets.size()));
            }
            for (const auto &entry : after["packets"].toArray()) {
                const auto packet = entry.toObject();
                if (packet["stream_index"].toInt() == 0) {
                    assert(packet["flags"].toString().contains('K')); break;
                }
            }
            QFile mainFile(output), backupFile(QString::fromUtf8(result.backupFile.c_str()));
            assert(mainFile.open(QIODevice::ReadOnly) && backupFile.open(QIODevice::ReadOnly));
            assert(mainFile.readAll() == backupFile.readAll());
        }
    }
    const auto input = temp.filePath("original.mp4"); assert(QFile::copy(longClip, input));
    const auto blocked = temp.filePath("not a directory"); QFile file(blocked); assert(file.open(QIODevice::WriteOnly)); file.close();
    auto failed = replay_trim::publish(input.toUtf8().toStdString(), blocked.toStdString(), "", 15, helper);
    assert(!failed.error.empty() && QFile::exists(input));
    failed = replay_trim::publish(input.toUtf8().toStdString(), "", blocked.toStdString(), 15, helper);
    assert(!failed.warning.empty() && QFile::exists(input));
    bool caught = false;
    try { replay_trim::publish(input.toStdString(), "", "", 15, "missing-helper"); }
    catch (const std::exception &) { caught = true; }
    assert(caught && QFile::exists(input));
    auto cancelled = std::make_shared<std::atomic<bool>>(true); caught = false;
    try { replay_trim::publish(input.toStdString(), "", "", 15, helper, cancelled); }
    catch (const std::exception &) { caught = true; }
    assert(caught && QFile::exists(input));
    const auto invalid = temp.filePath("broken.mp4"); QFile bad(invalid); assert(bad.open(QIODevice::WriteOnly)); bad.write("bad video"); bad.close();
    caught = false;
    try { replay_trim::publish(invalid.toStdString(), "", "", 15, helper); }
    catch (const std::exception &) { caught = true; }
    assert(caught && QFile::exists(invalid));
    // OBS can itself use the fixed filename: don't delete the published clip afterwards.
    const auto fixed = temp.filePath("InstantReplay.mp4"); QFile::remove(fixed); assert(QFile::copy(longClip, fixed));
    const auto same = replay_trim::publish(fixed.toStdString(), "", "", 15, helper);
    assert(same.error.empty() && QFileInfo(fixed).size() > 256);
    // A container without duration must not silently publish the entire buffer as a short clip.
    const auto noDuration = temp.filePath("unfinished.mkv");
    run(helper, {"-v", "error", "-i", longClip, "-map", "0", "-c", "copy", "-live", "1", noDuration});
    caught = false;
    try { replay_trim::publish(noDuration.toStdString(), "", "", 15, helper); }
    catch (const std::exception &) { caught = true; }
    assert(caught && QFile::exists(noDuration));
    assert(QDir(temp.path()).entryList({".irxt-trim-*"}, QDir::Dirs | QDir::Hidden | QDir::NoDotAndDotDot).isEmpty());
}
