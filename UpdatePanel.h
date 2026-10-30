#pragma once
#include "UpdateInfo.h"
#include "UpdateTransport.h"
#include <QWidget>
#include <QVBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QProgressBar>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSaveFile>
#include <QCryptographicHash>
#include <QStandardPaths>
#include <QDesktopServices>
#include <QCoreApplication>
#include <QProcess>
#include <QFileInfo>
#include <QDir>
#include <QUuid>
#include <QTimer>
#include <QPointer>
#include <memory>

class UpdatePanel : public QWidget {
public:
    QString modulePath, helperPath;
    QPushButton *check, *install, *cancel;
    QLabel *status;
    QProgressBar *progress;
    QNetworkAccessManager network;
    QPointer<QNetworkReply> reply;
    updates::Release release;
    QString cache, marker, installer;
    std::unique_ptr<QSaveFile> file;
    QCryptographicHash hash{QCryptographicHash::Sha256};
    qint64 received = 0;
    bool writeFailed = false;
#ifdef Q_OS_WIN
    const QString platform = "windows-x64";
#else
    const QString platform = "linux-x64";
#endif
    explicit UpdatePanel(QWidget *parent = nullptr) : QWidget(parent) {
        auto *layout = new QVBoxLayout(this);
        layout->addWidget(new QLabel("Installed version: " + updates::currentVersion, this));
        auto *note = new QLabel("Check GitHub for the latest stable release. Updates download only when requested. After downloading and verifying the installer, close OBS normally to start installation. Finish recordings or streams first. Restart OBS after setup finishes. Windows setup may ask for administrator permission.", this);
        note->setWordWrap(true); layout->addWidget(note);
        check = new QPushButton("Check for updates", this); layout->addWidget(check);
        install = new QPushButton("Download and install after OBS closes", this); install->setEnabled(false); layout->addWidget(install);
        cancel = new QPushButton("Cancel update", this); cancel->setEnabled(false); layout->addWidget(cancel);
        auto *page = new QPushButton("Open GitHub releases", this); layout->addWidget(page);
        auto *downloads = new QPushButton("Open update downloads / log", this); layout->addWidget(downloads);
        progress = new QProgressBar(this); progress->setRange(0, 100); progress->setValue(0); layout->addWidget(progress);
        status = new QLabel("Click Check for updates to contact GitHub.", this); status->setWordWrap(true); status->setTextFormat(Qt::PlainText); layout->addWidget(status); layout->addStretch();
        cache = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/InstantReplayXT-updates";
        QFile previous(cache + "/update.txt");
        if (previous.open(QIODevice::ReadOnly)) status->setText(QString::fromUtf8(previous.read(4096)));
        connect(check, &QPushButton::clicked, this, [this] { checkLatest(); });
        connect(install, &QPushButton::clicked, this, [this] { download(); });
        connect(cancel, &QPushButton::clicked, this, [this] {
            if (reply) { reply->abort(); return; }
            if (!marker.isEmpty()) QFile::remove(marker);
            marker.clear(); check->setEnabled(true); install->setEnabled(!release.name.isEmpty()); cancel->setEnabled(false);
            status->setText("Update cancelled."); writeStatus(status->text());
        });
        connect(page, &QPushButton::clicked, this, [] { QDesktopServices::openUrl(QUrl(updates::repository + "/releases/latest")); });
        connect(downloads, &QPushButton::clicked, this, [this] { QDir().mkpath(cache); QDesktopServices::openUrl(QUrl::fromLocalFile(cache)); });
        auto *poll = new QTimer(this); poll->setInterval(1000);
        connect(poll, &QTimer::timeout, this, [this] {
            if (!marker.isEmpty() && !QFileInfo::exists(marker)) {
                marker.clear(); cancel->setEnabled(false); check->setEnabled(true);
                QFile result(cache + "/update.txt"); if (result.open(QIODevice::ReadOnly)) status->setText(QString::fromUtf8(result.read(4096)));
            }
        }); poll->start();
    }
    ~UpdatePanel() override {
        // A queued installation survives OBS shutting down; only unfinished downloads are cancelled.
        if (reply) { reply->disconnect(this); reply->abort(); }
    }
    QString destination() const {
        QDir directory(QFileInfo(modulePath).absolutePath());
        if (directory.dirName() != "64bit") return {};
        directory.cdUp(); if (directory.dirName() != "bin") return {};
        directory.cdUp();
        if (directory.dirName().compare("InstantReplay", Qt::CaseInsensitive) != 0) return {};
        return directory.absolutePath();
    }
    void writeStatus(const QString &message) {
        QSaveFile out(cache + "/update.txt");
        if (out.open(QIODevice::WriteOnly)) { out.write(message.toUtf8()); out.commit(); }
    }
    void fail(const QString &message) {
        file.reset(); reply = nullptr;
        check->setEnabled(true); install->setEnabled(!release.name.isEmpty()); cancel->setEnabled(false);
        status->setText(message);
    }
    QNetworkReply *get(const QUrl &url, qint64 maximumBytes) {
#ifdef Q_OS_WIN
        return new updates::CurlReply(url, maximumBytes, this);
#else
        Q_UNUSED(maximumBytes);
        QNetworkRequest request(url);
        request.setRawHeader("User-Agent", "InstantReplayXT/2.0.20");
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
        request.setTransferTimeout(30000);
        return network.get(request);
#endif
    }
    void checkLatest() {
        if (reply || !marker.isEmpty()) return;
        release = {}; install->setEnabled(false); check->setEnabled(false); cancel->setEnabled(true);
        status->setText("Checking GitHub…"); progress->setValue(0);
        auto *request = get(QUrl("https://api.github.com/repos/Amaster15/InstantReplayXT-reMaster/releases/latest"), 1024 * 1024); reply = request;
        connect(request, &QNetworkReply::readyRead, this, [request] { if (request->bytesAvailable() > 1024 * 1024) request->abort(); });
        connect(request, &QNetworkReply::finished, this, [this, request] {
            request->deleteLater(); reply = nullptr; check->setEnabled(true); cancel->setEnabled(false);
            if (request->error() != QNetworkReply::NoError) { fail("Update check failed or was cancelled: " + request->errorString() + ". You can retry or open GitHub releases."); return; }
            const auto data = request->readAll();
            const auto document = QJsonDocument::fromJson(data);
            const auto object = document.object();
            const auto tag = object.value("tag_name").toString();
            if (!QRegularExpression("^v[0-9]+\\.[0-9]+\\.[0-9]+$").match(tag).hasMatch()) { fail("GitHub returned an unsupported release format."); return; }
            release = updates::parse(data, platform);
            if (release.version.isEmpty()) { status->setText("You are up to date (" + updates::currentVersion + ")."); return; }
            if (release.name.isEmpty()) { fail("Version " + release.version + " is available, but a compatible installer with a GitHub SHA256 digest was not found. Open GitHub releases."); return; }
            if (destination().isEmpty() || !QFileInfo::exists(helperPath)) { fail("Version " + release.version + " is available. This installation uses an unsupported layout; update manually using GitHub releases."); install->setEnabled(false); return; }
            status->setText("Version " + release.version + " is available. Install into: " + destination()); install->setEnabled(true);
        });
    }
    void download() {
        if (reply || release.name.isEmpty() || !marker.isEmpty()) return;
        if (destination().isEmpty()) { fail("Cannot identify the installed plugin folder. Use GitHub releases to update manually."); install->setEnabled(false); return; }
        const auto job = cache + "/" + QUuid::createUuid().toString(QUuid::WithoutBraces);
        if (!QDir().mkpath(job)) { fail("Cannot create update download folder."); return; }
        installer = job + "/" + release.name;
        file = std::make_unique<QSaveFile>(installer);
        if (!file->open(QIODevice::WriteOnly)) { fail("Cannot write the installer download."); return; }
        received = 0; hash.reset(); writeFailed = false;
        check->setEnabled(false); install->setEnabled(false); cancel->setEnabled(true);
        status->setText("Downloading " + release.name + "…");
        auto *request = get(release.url, release.size); reply = request;
        auto *deadline = new QTimer(request); deadline->setSingleShot(true); deadline->start(300000);
        connect(deadline, &QTimer::timeout, request, &QNetworkReply::abort);
        connect(request, &QNetworkReply::readyRead, this, [this, request] {
            const auto bytes = request->readAll(); received += bytes.size();
            if (received > release.size || file->write(bytes) != bytes.size()) { writeFailed = true; request->abort(); return; }
            hash.addData(bytes); progress->setValue(int(received * 100 / release.size));
        });
        connect(request, &QNetworkReply::finished, this, [this, request] {
            request->deleteLater(); reply = nullptr;
            if (request->error() != QNetworkReply::NoError || writeFailed) { fail("Download failed or was cancelled: " + request->errorString() + ". No installer was run."); return; }
            if (received != release.size || QString::fromLatin1(hash.result().toHex()) != release.digest) { fail("Installer verification failed. No installer was run."); return; }
            if (!file->commit()) { fail("Could not save the verified installer."); return; }
            file.reset(); queueInstall();
        });
    }
    void queueInstall() {
        // Copy the helper so upgrading the plugin cannot replace the running helper file.
        const auto job = QFileInfo(installer).absolutePath();
        const auto helper = job + "/" + QFileInfo(helperPath).fileName();
        if (!QFile::copy(helperPath, helper)) { fail("Could not prepare update helper. Open the download folder to install manually after closing OBS."); return; }
        marker = job + "/pending";
        QFile pending(marker);
        if (!pending.open(QIODevice::WriteOnly)) { marker.clear(); fail("Cannot queue installation."); return; } pending.close();
        QStringList args;
#ifdef Q_OS_WIN
        const auto executable = QStandardPaths::findExecutable("powershell.exe");
        args << "-NoProfile" << "-NonInteractive" << "-WindowStyle" << "Hidden" << "-ExecutionPolicy" << "Bypass" << "-File" << helper;
#else
        const auto executable = QStandardPaths::findExecutable("python3");
        args << helper;
#endif
        args << installer << release.digest << QString::number(QCoreApplication::applicationPid()) << destination() << marker << cache + "/update.txt";
        if (executable.isEmpty() || !QProcess::startDetached(executable, args, job)) {
            QFile::remove(marker); marker.clear(); fail("Could not launch update helper. Install the downloaded file manually after closing OBS."); return;
        }
        status->setText("Verified version " + release.version + ". Close OBS normally within 30 minutes to start installation, then restart OBS after setup completes. Cancel update to keep the current version.");
        writeStatus(status->text()); cancel->setEnabled(true);
    }
};
