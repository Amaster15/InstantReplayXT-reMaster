#pragma once
#include "UpdateInfo.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QProcess>
#include <QTimer>
#include <cstring>
#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace updates {
// Use the OS-supplied curl on Windows: OBS need not deploy Qt TLS plugins.
// Keep a QNetworkReply interface so size, digest, cancellation and install checks
// are shared with the Qt transport. No shell or PATH executable lookup is used.
inline QString systemCurl() {
#ifdef Q_OS_WIN
    wchar_t path[MAX_PATH + 1]{};
    const auto length = GetSystemDirectoryW(path, MAX_PATH + 1);
    if (!length || length > MAX_PATH) return {};
    return QString::fromWCharArray(path, int(length)) + "/curl.exe";
#else
    return QStringLiteral("/usr/bin/curl"); // Used by transport regression tests.
#endif
}
class CurlReply final : public QNetworkReply {
    QProcess process;
    QByteArray buffer, diagnostic;
    bool done = false;
    qint64 total = 0, limit;
    void finish(NetworkError code, const QString &message = {}) {
        if (done) return;
        done = true;
        if (code != NoError) {
            setError(code, message);
            process.kill();
        }
        setFinished(true);
        if (code != NoError) emit errorOccurred(code);
        emit finished();
    }
    void consume() {
        if (done) return;
        const auto bytes = process.readAllStandardOutput();
        total += bytes.size();
        if (total > limit) { finish(ContentAccessDenied, "Response exceeded its size limit"); return; }
        if (!bytes.isEmpty()) { buffer += bytes; emit readyRead(); }
    }
protected:
    qint64 readData(char *data, qint64 maximum) override {
        const auto size = qMin(maximum, qint64(buffer.size()));
        if (!size) return done ? -1 : 0;
        std::memcpy(data, buffer.constData(), size_t(size)); buffer.remove(0, size);
        return size;
    }
public:
    explicit CurlReply(const QUrl &url, qint64 maximumBytes, QObject *parent = nullptr,
                       const QString &executable = systemCurl()) : QNetworkReply(parent), limit(maximumBytes) {
        setUrl(url); setOperation(QNetworkAccessManager::GetOperation);
        open(QIODevice::ReadOnly | QIODevice::Unbuffered);
        connect(&process, &QProcess::readyReadStandardOutput, this, [this] { consume(); });
        connect(&process, &QProcess::readyReadStandardError, this, [this] {
            const auto bytes = process.readAllStandardError();
            diagnostic += bytes.left(qMax(0, 4096 - int(diagnostic.size())));
        });
        connect(&process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
            if (error == QProcess::FailedToStart)
                finish(UnknownNetworkError, "Windows HTTPS client could not start. Open GitHub releases to update manually.");
        });
        connect(&process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
            [this](int code, QProcess::ExitStatus exit) {
                consume();
                if (done) return;
                if (code || exit != QProcess::NormalExit)
                    finish(UnknownNetworkError, "HTTPS request failed: " + QString::fromUtf8(diagnostic).trimmed());
                else finish(NoError);
            });
        // Defer start/errors until the caller has connected the reply signals.
        QTimer::singleShot(0, this, [this, url, executable] {
            if (done) return;
            if (url.scheme() != "https" || executable.isEmpty()) {
                finish(ProtocolInvalidOperationError, "A secure HTTPS connection is required"); return;
            }
            // --disable must be first: ignore user curlrc files, including insecure
            // options or output overrides. Redirects may only use HTTPS as well.
            process.start(executable, {"--disable", "--fail", "--silent", "--show-error",
                "--location", "--max-redirs", "5", "--proto", "=https", "--proto-redir", "=https",
                "--connect-timeout", "30", "--max-time", "300", "--speed-limit", "1", "--speed-time", "30",
                "--user-agent", "InstantReplayXT/" + currentVersion, "--url", url.toString(QUrl::FullyEncoded)});
        });
    }
    ~CurlReply() override {
        process.disconnect(this);
        if (process.state() != QProcess::NotRunning) { process.kill(); process.waitForFinished(1000); }
    }
    void abort() override { finish(OperationCanceledError, "Request cancelled"); }
    bool isSequential() const override { return true; }
    qint64 bytesAvailable() const override { return buffer.size() + QNetworkReply::bytesAvailable(); }
};
}
