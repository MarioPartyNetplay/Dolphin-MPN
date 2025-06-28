/*
*  Dolphin for Mario Party Netplay
*  Copyright (C) 2025 Tabitha Hanegan <tabithahanegan.com>
*/

#include "DownloadWorker.h"
#include "Common/HttpRequest.h"
#include <QFile>
#include <QDebug>

DownloadWorker::DownloadWorker(const QString& url, const QString& filename)
    : url(url), filename(filename) {}

void DownloadWorker::startDownload()
{
    Common::HttpRequest httpRequest;

    // Set the progress callback
    httpRequest.SetProgressCallback([this](s64 dltotal, s64 dlnow, s64 ultotal, s64 ulnow) {
        // Debug output to see if callback is being called
        qDebug() << "Progress callback called:" << dlnow << "/" << dltotal << "bytes";
        
        // Emit the progress signal directly instead of calling updateProgress
        emit progressUpdated(static_cast<qint64>(dlnow), static_cast<qint64>(dltotal));
        return true; // Continue the download
    });

    httpRequest.FollowRedirects();
    
    Common::HttpRequest::Headers headers;
    headers["User-Agent"] = "Dolphin-MPN/1.0";

    // Perform the GET request
    auto response = httpRequest.Get(url.toStdString(), headers);

    // Check if the final URL is the expected GitHub objects CDN
    std::string final_url = httpRequest.GetFinalUrl();
    if (final_url.find("objects.githubusercontent.com") == std::string::npos) {
        emit errorOccurred(QStringLiteral("Did not reach the expected GitHub objects URL. Final URL: %1").arg(QString::fromStdString(final_url)));
        return;
    }

    // Check the response
    if (response)
    {
        QFile file(filename);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        {
            emit errorOccurred(QStringLiteral("Failed to open file for writing."));
            return;
        }
        QByteArray byteArray(reinterpret_cast<const char*>(response->data()), response->size());
        file.write(byteArray);
        file.close();
        emit finished(); // Emit finished signal
    }
    else
    {
        emit errorOccurred(QStringLiteral("Failed to download update file.")); // This should also work
    }
}

void DownloadWorker::updateProgress(qint64 dlnow, qint64 dltotal)
{
    emit progressUpdated(dlnow, dltotal);
}
