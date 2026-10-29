#include "../UpdateTransport.h"
#include <QCoreApplication>
#include <QEventLoop>
#include <QCryptographicHash>
#include <QFile>
#include <cstdio>
#include <cassert>

static void waitFor(QNetworkReply &reply) {
    QEventLoop loop;
    bool timedOut = false;
    QTimer timeout; timeout.setSingleShot(true);
    QObject::connect(&timeout, &QTimer::timeout, &loop, [&] { timedOut = true; reply.abort(); loop.quit(); });
    QObject::connect(&reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    timeout.start(60000); loop.exec(); assert(!timedOut);
}
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    const auto args = app.arguments();
    // The test executable doubles as a deterministic child HTTPS client.
    if (args.contains("--disable")) {
        assert(args.at(1) == "--disable");
        assert(args.at(args.indexOf("--proto") + 1) == "=https");
        assert(args.at(args.indexOf("--proto-redir") + 1) == "=https");
        assert(!args.contains("--insecure"));
        const auto url = args.last();
        if (url.endsWith("/fail")) { std::fputs("certificate verification failed", stderr); return 60; }
        if (url.endsWith("/wait")) { QTimer::singleShot(10000, &app, &QCoreApplication::quit); return app.exec(); }
        std::fputs("abc", stdout); std::fflush(stdout);
        QTimer::singleShot(20, &app, [&] { std::fputs("def", stdout); std::fflush(stdout); app.quit(); });
        return app.exec();
    }
    // Deliberately remove Qt plugin search paths. The Windows transport must
    // check AND download without Qt's TLS plugins, matching packaged OBS.
    QCoreApplication::setLibraryPaths({});
    const auto fake = QCoreApplication::applicationFilePath();
    {
        updates::CurlReply reply(QUrl("https://example.test/ok"), 6, nullptr, fake);
        QByteArray bytes; int finishes = 0;
        QObject::connect(&reply, &QNetworkReply::readyRead, &app, [&] { bytes += reply.readAll(); });
        QObject::connect(&reply, &QNetworkReply::finished, &app, [&] { ++finishes; });
        waitFor(reply); assert(reply.error() == QNetworkReply::NoError); assert(bytes == "abcdef"); assert(finishes == 1);
    }
    {
        updates::CurlReply reply(QUrl("https://example.test/ok"), 5, nullptr, fake);
        waitFor(reply); assert(reply.error() != QNetworkReply::NoError);
    }
    {
        updates::CurlReply reply(QUrl("https://example.test/fail"), 100, nullptr, fake);
        waitFor(reply); assert(reply.error() != QNetworkReply::NoError); assert(reply.errorString().contains("certificate"));
    }
    {
        updates::CurlReply reply(QUrl("https://example.test/wait"), 100, nullptr, fake);
        QTimer::singleShot(50, &reply, &QNetworkReply::abort);
        waitFor(reply); assert(reply.error() == QNetworkReply::OperationCanceledError);
    }
    {
        updates::CurlReply reply(QUrl("http://example.test/ok"), 100, nullptr, fake);
        waitFor(reply); assert(reply.error() == QNetworkReply::ProtocolInvalidOperationError);
    }
    {
        updates::CurlReply reply(QUrl("https://example.test/ok"), 100, nullptr, fake + ".missing");
        waitFor(reply); assert(reply.error() != QNetworkReply::NoError);
    }
    if (args.contains("--live")) {
        updates::CurlReply metadata(QUrl("https://api.github.com/repos/Amaster15/InstantReplayXT-reMaster/releases/tags/v2.0.7"), 1024 * 1024);
        waitFor(metadata);
        if (metadata.error() != QNetworkReply::NoError) { qCritical("%s", qPrintable(metadata.errorString())); return 1; }
        const auto object = QJsonDocument::fromJson(metadata.readAll()).object();
        assert(object.value("tag_name").toString() == "v2.0.7");
        bool found = false;
        for (const auto &value : object.value("assets").toArray()) {
            const auto asset = value.toObject();
            if (!asset.value("name").toString().endsWith("-setup.exe")) continue;
            found = true;
            const auto size = asset.value("size").toVariant().toLongLong();
            updates::CurlReply download(QUrl(asset.value("browser_download_url").toString()), size);
            QCryptographicHash hash(QCryptographicHash::Sha256); qint64 received = 0;
            QObject::connect(&download, &QNetworkReply::readyRead, &app, [&] {
                const auto data = download.readAll(); received += data.size(); hash.addData(data);
            });
            waitFor(download);
            if (download.error() != QNetworkReply::NoError) { qCritical("%s", qPrintable(download.errorString())); return 1; }
            assert(received == size);
            assert("sha256:" + QString::fromLatin1(hash.result().toHex()) == asset.value("digest").toString());
        }
        assert(found);
    }
    std::puts("HTTPS transport tests passed (Qt plugin search paths disabled).");
}
