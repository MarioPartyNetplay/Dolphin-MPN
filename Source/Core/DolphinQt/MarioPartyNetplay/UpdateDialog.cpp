/*
*  Dolphin for Mario Party Netplay
*  Copyright (C) 2025 Tabitha Hanegan <tabithahanegan.com>
*/

#include "UpdateDialog.h"
#include "InstallUpdateDialog.h"

#include <QFileInfo>
#include <QPushButton>
#include <QJsonArray>
#include <QJsonObject>
#include <QDesktopServices>
#include <QMessageBox>
#include <QTemporaryDir>
#include <QUrl>
#include <QFile>
#include <QSysInfo>
#include <QProcess>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QWidget>
#include <QLabel>
#include <QTextEdit>
#include <QDialogButtonBox>
#include "../../Common/Logging/Log.h"
#include <QCoreApplication>
#include <QDir>

using namespace UserInterface::Dialog;

UpdateDialog::UpdateDialog(QWidget *parent, QJsonObject jsonObject, bool forced) 
    : QDialog(parent)
{
    this->jsonObject = jsonObject;

    // Create UI components
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    
    // Create and set up the label
    label = new QLabel(this);
    QString tagName = jsonObject.value(QStringLiteral("tag_name")).toString();
    label->setText(QStringLiteral("%1 Available").arg(tagName));
    mainLayout->addWidget(label);

    // Create and set up the text edit
    textEdit = new QTextEdit(this);
#if defined(__APPLE__) || defined(_WIN32)
    textEdit->setText(jsonObject.value(QStringLiteral("body")).toString());
#else
    textEdit->setText(
        QStringLiteral(
            "Automatic updates are not supported on Linux.\n\n"
            "Download the latest release from GitHub and replace your install manually.\n\n") +
        jsonObject.value(QStringLiteral("body")).toString());
#endif
    textEdit->setReadOnly(true);
    mainLayout->addWidget(textEdit);

    // Create and set up the button box
    buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    QPushButton* updateButton = buttonBox->button(QDialogButtonBox::Ok);
#if defined(__APPLE__) || defined(_WIN32)
    updateButton->setText(QStringLiteral("Update"));
#else
    updateButton->setText(QStringLiteral("Open Download Page"));
#endif
    mainLayout->addWidget(buttonBox);

    // Connect signals
    connect(buttonBox, &QDialogButtonBox::accepted, this, &UpdateDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &UpdateDialog::reject);

    // Set the layout
    setLayout(mainLayout);
    setWindowTitle(QStringLiteral("Update Available"));
    resize(400, 300);
}

UpdateDialog::~UpdateDialog()
{
}

void UpdateDialog::accept()
{
#if !defined(_WIN32) && !defined(__APPLE__)
    const QString release_url = jsonObject.value(QStringLiteral("html_url")).toString();
    if (!release_url.isEmpty())
      QDesktopServices::openUrl(QUrl(release_url));
    QDialog::accept();
#else
    QJsonArray jsonArray = jsonObject[QStringLiteral("assets")].toArray();
    QString filenameToDownload;
    QString urlToDownload;

    for (const QJsonValue& value : jsonArray)
    {
        QJsonObject object = value.toObject();

        QString filenameBlob = object.value(QStringLiteral("name")).toString();
        QString downloadUrl(object.value(QStringLiteral("browser_download_url")).toString());

#ifdef _WIN32
        if (filenameBlob.contains(QStringLiteral("win32"), Qt::CaseInsensitive) ||
            filenameBlob.contains(QStringLiteral("windows"), Qt::CaseInsensitive) ||
            filenameBlob.contains(QStringLiteral("win64"), Qt::CaseInsensitive))
        {
            filenameToDownload = filenameBlob;
            urlToDownload = downloadUrl;
            break;
        }
#endif
#ifdef __APPLE__
        if (filenameBlob.contains(QStringLiteral("darwin"), Qt::CaseInsensitive) ||
            filenameBlob.contains(QStringLiteral("macOS"), Qt::CaseInsensitive) ||
            filenameBlob.contains(QStringLiteral("osx"), Qt::CaseInsensitive))
        {
            filenameToDownload = filenameBlob;
            urlToDownload = downloadUrl;
            break;
        }
#endif
    }

    if (filenameToDownload.isEmpty() || urlToDownload.isEmpty())
    {
      QMessageBox::warning(
          this, QStringLiteral("Update"),
          QStringLiteral("No downloadable package for this platform was found in the release."));
      return;
    }

    this->url = urlToDownload;
    this->filename = filenameToDownload;
    QDialog::accept();

    // Dedicated temp dir — never point scripts at QDir::tempPath() itself (they delete it).
    QTemporaryDir temp_dir;
    temp_dir.setAutoRemove(false);
    if (!temp_dir.isValid())
    {
      QMessageBox::critical(this, QStringLiteral("Error"),
                            QStringLiteral("Failed to create a temporary update directory."));
      return;
    }

    const QString installationDirectory = QCoreApplication::applicationDirPath();
    InstallUpdateDialog installDialog(this, installationDirectory, temp_dir.path(),
                                      filenameToDownload, urlToDownload);
    installDialog.exec();
#endif
}
