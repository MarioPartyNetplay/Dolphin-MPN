/*
*  Dolphin for Mario Party Netplay
*  Copyright (C) 2025 Tabitha Hanegan <tabithahanegan.com>
*/

#include "Common/MinizipUtil.h"
#include "InstallUpdateDialog.h"
#include "DownloadWorker.h"

#include <QCoreApplication>
#include <QProcess>
#include <QDir>
#include <QTextStream>
#include <QVBoxLayout>
#include <QDialog>
#include <QLabel>
#include <QProgressBar>
#include <QMessageBox>
#include <QThread>

#include <mz.h>
#include <mz_zip.h>
#include <mz_zip_rw.h>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

// Constructor implementation
InstallUpdateDialog::InstallUpdateDialog(QWidget *parent, QString installationDirectory, QString temporaryDirectory, QString filename, QString downloadUrl)
    : QDialog(parent), // Only pass the parent
      installationDirectory(installationDirectory),
      temporaryDirectory(temporaryDirectory),
      filename(filename),
      downloadUrl(downloadUrl) // Initialize member variables
{
    setWindowTitle(QStringLiteral("Installing %1...").arg(this->filename));
    
    // Create UI components
    QVBoxLayout* layout = new QVBoxLayout(this);
    label = new QLabel(QStringLiteral("Preparing installation..."), this);
    progressBar = new QProgressBar(this);
    stepLabel = new QLabel(QStringLiteral("Preparing..."), this);
    stepProgressBar = new QProgressBar(this);

    // Always show both bars and the step label
    progressBar->setVisible(true);
    stepLabel->setVisible(true);
    stepProgressBar->setVisible(true);

    // Add widgets in order: label, master bar, step label, step bar
    layout->addWidget(label);
    layout->addWidget(progressBar);
    layout->addWidget(stepLabel);
    layout->addWidget(stepProgressBar);

    setLayout(layout);

    // Set a minimum size to ensure both bars are visible
    setMinimumSize(400, 150);

    // If we have a download URL, start with download, otherwise start with installation
    if (!downloadUrl.isEmpty()) {
        startTimer(100); // Start download process
    } else {
        startTimer(100); // Start installation process
    }
}

// Destructor implementation
InstallUpdateDialog::~InstallUpdateDialog(void)
{
}

void InstallUpdateDialog::download()
{
    this->label->setText(QStringLiteral("Step 1/3: Downloading"));
    this->progressBar->setValue(0);
    this->progressBar->setMinimum(0);
    this->progressBar->setMaximum(100);
    
    // Step bar for download
    this->stepLabel->setText(QStringLiteral("0% Downloaded ..."));
    this->stepProgressBar->setValue(0);
    this->stepProgressBar->setMinimum(0);
    this->stepProgressBar->setMaximum(100);
    
    this->layout()->update();
    this->updateGeometry();
    
    // Create the worker and thread for download
    DownloadWorker* worker = new DownloadWorker(downloadUrl, (temporaryDirectory + QDir::separator() + filename));
    QThread* thread = new QThread;

    // Move the worker to the thread
    worker->moveToThread(thread);

    // Connect signals and slots
    connect(thread, &QThread::started, worker, &DownloadWorker::startDownload, Qt::UniqueConnection);
    connect(worker, &DownloadWorker::progressUpdated, this, [this](qint64 size, qint64 total) {
        if (total <= 0) {
            this->stepProgressBar->setValue(0);
            this->progressBar->setValue(0);
            return;
        }
        int downloadProgress = (size * 100) / total;
        this->stepProgressBar->setValue(downloadProgress);
        
        int mainProgress = (size * 50) / total;
        this->progressBar->setValue(mainProgress);
        
        this->stepLabel->setText(QStringLiteral("(%0%) Downloaded...").arg(downloadProgress));
    }, Qt::QueuedConnection);
    connect(worker, &DownloadWorker::finished, thread, &QThread::quit, Qt::UniqueConnection);
    connect(worker, &DownloadWorker::finished, worker, &DownloadWorker::deleteLater, Qt::UniqueConnection);
    connect(worker, &DownloadWorker::finished, this, [this]() {
        this->install();
    }, Qt::QueuedConnection);
    connect(worker, &DownloadWorker::errorOccurred, this, [this](const QString& errorMsg) {
        QMessageBox::critical(this, QStringLiteral("Error"), errorMsg);
        this->reject();
    }, Qt::QueuedConnection);
    connect(thread, &QThread::finished, thread, &QThread::deleteLater, Qt::UniqueConnection);

    // Start the thread
    thread->start();
}

void InstallUpdateDialog::install()
{
  QString fullFilePath = this->temporaryDirectory + QDir::separator() + this->filename;
  
  #ifdef __APPLE__
  QString appPath = QCoreApplication::applicationDirPath() + QStringLiteral("/../../../"); // Set the installation directory
  #else
  QString appPath = QCoreApplication::applicationDirPath();
  #endif

  QString appPid = QString::number(QCoreApplication::applicationPid());
  // Convert paths to native format
  this->temporaryDirectory = QDir::toNativeSeparators(this->temporaryDirectory);
  fullFilePath = QDir::toNativeSeparators(fullFilePath);
  appPath = QDir::toNativeSeparators(appPath);

  if (this->filename.endsWith(QStringLiteral(".exe")))
  {
    this->label->setText(QStringLiteral("Step 3/3: Finishing up"));
    this->progressBar->setValue(50);
    this->stepLabel->setText(QStringLiteral("Update complete. Restarting..."));
    this->stepProgressBar->setValue(100);

#ifdef _WIN32
    QStringList scriptLines = {
        QStringLiteral("@echo off"),
        QStringLiteral("("),
        QStringLiteral("   echo == Attempting to kill PID ") + appPid,
        QStringLiteral("   taskkill /F /PID:") + appPid,
        QStringLiteral("   echo == Attempting to start '") + fullFilePath + QStringLiteral("'"),
        QStringLiteral("   \"") + fullFilePath +
            QStringLiteral(
                "\" /CLOSEAPPLICATIONS /NOCANCEL /MERGETASKS=\"!desktopicon\" /SILENT /DIR=\"") +
            appPath + QStringLiteral("\""),
        QStringLiteral(")"),
        QStringLiteral("IF NOT ERRORLEVEL 0 ("),
        QStringLiteral("   start \"\" cmd /c \"echo Update failed, check the log for more information && pause\""),
        QStringLiteral(")"),
        QStringLiteral("rmdir /S /Q \"") + this->temporaryDirectory + QStringLiteral("\""),
        QStringLiteral("exit") + QStringLiteral("\""),

    };
    this->writeAndRunScript(scriptLines);
    this->accept();
#endif
    return;
  }

#ifdef __APPLE__
  if (this->filename.endsWith(QStringLiteral(".dmg")))
  {
    QString appPath = QCoreApplication::applicationDirPath() + QStringLiteral("/../../../");
    QString appPid = QString::number(QCoreApplication::applicationPid());
    this->temporaryDirectory = QDir::toNativeSeparators(this->temporaryDirectory);
    fullFilePath = QDir::toNativeSeparators(fullFilePath);
    appPath = QDir::toNativeSeparators(appPath);

    // DMG extraction logic for macOS
    this->label->setText(QStringLiteral("Step 3/3: Finishing up"));
    this->progressBar->setValue(50);

    QString mountPoint = QStringLiteral("/Volumes/Dolphin-MPN-Update");
    QString dmgPath = fullFilePath;
    QString appBundleName = QStringLiteral("Dolphin-MPN.app");
    QString appSource = mountPoint + QDir::separator() + appBundleName;
    QString appDest = appPath + QDir::separator() + appBundleName;

    QStringList scriptLines = {
        QStringLiteral("#!/bin/bash"),
        QStringLiteral("set -e"),
        QStringLiteral("echo '== Terminating application with PID ") + appPid + QStringLiteral("'"),
        QStringLiteral("kill -9 ") + appPid,
        QStringLiteral("echo '== Mounting DMG'"),
        QStringLiteral("hdiutil attach \"") + dmgPath + QStringLiteral("\" -mountpoint \"") + mountPoint + QStringLiteral("\""),
        QStringLiteral("echo '== Removing old application files'"),
        QStringLiteral("rm -rf \"") + appDest + QStringLiteral("\""),
        QStringLiteral("echo '== Copying new app bundle to ") + appPath + QStringLiteral("'"),
        QStringLiteral("cp -R \"") + appSource + QStringLiteral("\" \"") + appPath + QStringLiteral("\""),
        QStringLiteral("echo '== Unmounting DMG'"),
        QStringLiteral("hdiutil detach \"") + mountPoint + QStringLiteral("\""),
        QStringLiteral("echo '== Launching the updated application'"),
        QStringLiteral("open \"") + appDest + QStringLiteral("\""),
        QStringLiteral("echo '== Cleaning up temporary files'"),
        QStringLiteral("rm -rf \"") + this->temporaryDirectory + QStringLiteral("\""),
        QStringLiteral("exit 0")
    };
    this->writeAndRunScript(scriptLines);
    this->progressBar->setValue(50);
    return;
  }
#endif

  this->label->setText(QStringLiteral("Step 2/3: Extracting").arg(this->filename));
  this->progressBar->setValue(50);
  
  // Step bar for extraction
  this->stepLabel->setText(QStringLiteral("0-0 files extracted..."));
  this->stepProgressBar->setValue(0);
  this->stepProgressBar->setMinimum(0);
  this->stepProgressBar->setMaximum(100);
  this->layout()->update();
  this->updateGeometry();
  QThread::msleep(100);
  
  QString extractDirectory = this->temporaryDirectory + QDir::separator() + QStringLiteral("Dolphin-MPN");

  if (this->filename.endsWith(QStringLiteral(".zip")))
  {
    // Hack to remove stuck directory
    QDir extractDirectoryHack(extractDirectory);
    if (extractDirectoryHack.exists()) {
      extractDirectoryHack.removeRecursively();
    }

    // Ensure the extract directory exists before attempting to unzip
    QDir dir(this->temporaryDirectory);
    if (!QDir(extractDirectory).exists())
    {
      if (!dir.mkdir(QStringLiteral("Dolphin-MPN")))
      {
        QMessageBox::critical(this, QStringLiteral("Error"),
                              QStringLiteral("Failed to create extract directory."));
        this->reject();
        return;
      }
    }

    // Attempt to unzip files into the extract directory
    if (!unzipFile(fullFilePath.toStdString(), extractDirectory.toStdString(), 
                   [this](int current, int total) {
                       // Update step progress bar (0-100%)
                       int extractionProgress = (current * 100) / total;
                       this->stepProgressBar->setValue(extractionProgress);
                       
                       // Update master progress bar (50-95% range for extraction)
                       int mainProgress = 50 + (current * 45 / total); // 50% to 95%
                       this->progressBar->setValue(mainProgress);
                       
                       this->stepLabel->setText(QStringLiteral("(%2/%3) files extracted... ")
                                               .arg(current)
                                               .arg(total));
                   }))
    {
      QMessageBox::critical(this, QStringLiteral("Error"),
                            QStringLiteral("Unzip failed: Unable to extract files."));
      this->reject();
      return;
    }
  }
  else
  {
    // If not a zip or dmg, show error and abort
    QMessageBox::critical(this, QStringLiteral("Error"),
                          QStringLiteral("Unsupported update file format: %1").arg(this->filename));
    this->reject();
    return;
  }

  this->label->setText(QStringLiteral("Step 3/3: Finishing up..."));
  this->progressBar->setValue(95); // Start final steps at 95%
  this->stepLabel->setText(QStringLiteral("Finishing up..."));
  this->stepProgressBar->setValue(100);

  extractDirectory = QDir::toNativeSeparators(extractDirectory);

#ifdef __APPLE__
  QStringList scriptLines = {
      QStringLiteral("#!/bin/bash"),
      QStringLiteral("echo '== Terminating application with PID ") + appPid + QStringLiteral("'"),
      QStringLiteral("kill -9 ") + appPid,
      QStringLiteral("echo '== Removing old application files'"),
      QStringLiteral("rm -f \"") + appPath + QStringLiteral("\""),
      QStringLiteral("echo '== Copying new files to ") + appPath + QStringLiteral("'"),
      QStringLiteral("cp -r \"") + extractDirectory + QStringLiteral("/\"* \"") + appPath +
          QStringLiteral("\""),
      QStringLiteral("echo '== Launching the updated application'"),
      QStringLiteral("open \"") + appPath + QStringLiteral("/Dolphin-MPN.app\""),
      QStringLiteral("echo '== Cleaning up temporary files'"),
      QStringLiteral("rm -rf \"") + this->temporaryDirectory + QStringLiteral("\""),
      QStringLiteral("exit 0")};
  this->writeAndRunScript(scriptLines);
  this->progressBar->setValue(50); // Complete at 50%
#endif

#ifdef _WIN32
  QStringList scriptLines = {
      QStringLiteral("@echo off"),
      QStringLiteral("("),
      QStringLiteral("   echo == Attempting to remove '") + fullFilePath + QStringLiteral("'"),
      QStringLiteral("   del /F /Q \"") + fullFilePath + QStringLiteral("\""),
      QStringLiteral("   echo == Attempting to kill PID ") + appPid,
      QStringLiteral("   taskkill /F /PID:") + appPid,
      QStringLiteral("   echo == Attempting to copy '") + extractDirectory +
          QStringLiteral("' to '") + appPath + QStringLiteral("'"),
      QStringLiteral("   xcopy /S /Y /I \"") + extractDirectory + QStringLiteral("\\*\" \"") +
          appPath + QStringLiteral("\""),
      QStringLiteral("   echo == Attempting to start '") + appPath +
          QStringLiteral("\\Dolphin-MPN.exe'"),
      QStringLiteral("   start \"\" \"") + appPath + QStringLiteral("\\Dolphin-MPN.exe\""),
      QStringLiteral(")"),
      QStringLiteral("IF NOT ERRORLEVEL 0 ("),
      QStringLiteral("   start \"\" cmd /c \"echo Update failed && pause\""),
      QStringLiteral(")"),
      QStringLiteral("rmdir /S /Q \"") + this->temporaryDirectory + QStringLiteral("\""),
      QStringLiteral("exit") + QStringLiteral("\""),

  };
  this->writeAndRunScript(scriptLines);
  this->progressBar->setValue(50); // Complete at 50%
#endif
}


bool InstallUpdateDialog::unzipFile(const std::string& zipFilePath, const std::string& destDir, std::function<void(int, int)> progressCallback)
{
    void* reader = mz_zip_reader_create();
    if (!reader)
        return false;
    
    if (mz_zip_reader_open_file(reader, zipFilePath.c_str()) != MZ_OK)
    {
        mz_zip_reader_delete(&reader);
        return false;
    }
    
    // First pass: count total files
    int total_files = 0;
    int32_t count_status = mz_zip_reader_goto_first_entry(reader);
    while (count_status == MZ_OK)
    {
        total_files++;
        count_status = mz_zip_reader_goto_next_entry(reader);
    }
    
    // Reset to first entry for extraction
    int32_t entry_status = mz_zip_reader_goto_first_entry(reader);
    int current_file = 0;
    
    while (entry_status == MZ_OK)
    {
        mz_zip_file* file_info = nullptr;
        mz_zip_reader_entry_get_info(reader, &file_info);
        if (file_info == nullptr)
        {
            mz_zip_reader_close(reader);
            mz_zip_reader_delete(&reader);
            return false;
        }
        
        std::string out_path = destDir + "/" + file_info->filename;
        if (file_info->filename[strlen(file_info->filename) - 1] == '/')
        {
            // Directory
            QDir().mkpath(QString::fromStdString(out_path));
        }
        else
        {
            // File
            QDir().mkpath(QFileInfo(QString::fromStdString(out_path)).path());
            if (mz_zip_reader_entry_save_file(reader, out_path.c_str()) != MZ_OK)
            {
                mz_zip_reader_close(reader);
                mz_zip_reader_delete(&reader);
                return false;
            }
        }
        
        current_file++;
        if (progressCallback)
        {
            progressCallback(current_file, total_files);
        }
        
        entry_status = mz_zip_reader_goto_next_entry(reader);
    }
    
    mz_zip_reader_close(reader);
    mz_zip_reader_delete(&reader);
    return true;
}

void InstallUpdateDialog::writeAndRunScript(QStringList stringList)
{
#ifdef __APPLE__
  QString scriptPath = this->temporaryDirectory + QStringLiteral("/update.sh");
#else
  QString scriptPath = this->temporaryDirectory + QStringLiteral("/update.bat");
#endif

  QFile scriptFile(scriptPath);
  if (!scriptFile.open(QIODevice::WriteOnly | QIODevice::Text))
  {
    QMessageBox::critical(this, tr("Error"),
                          tr("Failed to open file for writing: %1").arg(filename));
    return;
  }

  QTextStream textStream(&scriptFile);

#ifdef __APPLE__
  // macOS: Write shell script
  textStream << QStringLiteral("#!/bin/bash\n");
#else
  // Windows: Write batch script
  textStream << QStringLiteral("@echo off\n");
#endif

  for (const QString& str : stringList)
  {
    textStream << str << "\n";
  }

#ifdef __APPLE__
  scriptFile.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner |
                            QFileDevice::ExeOwner);
#else
  scriptFile.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
#endif
  scriptFile.close();

  this->launchProcess(scriptPath, {});
}

void InstallUpdateDialog::launchProcess(QString file, QStringList arguments)
{
#ifdef _WIN32
    #include <windows.h>
    #include <QMessageBox>

    QString argumentsString = arguments.join(QStringLiteral(" "));
    std::wstring fileW = file.toStdWString();
    std::wstring argumentsW = argumentsString.toStdWString();

    SHELLEXECUTEINFO sei = {0};
    sei.cbSize = sizeof(SHELLEXECUTEINFO);
    sei.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NO_CONSOLE;
    sei.hwnd = nullptr;
    sei.lpVerb = L"runas"; // Request admin privileges
    sei.lpFile = fileW.c_str(); // Path to batch file
    sei.lpParameters = argumentsW.c_str(); // Arguments
    sei.lpDirectory = nullptr;
    sei.nShow = SW_HIDE; // Hide the window

    if (ShellExecuteEx(&sei)) {
        WaitForSingleObject(sei.hProcess, INFINITE);
        CloseHandle(sei.hProcess);
    }
    
    if (!ShellExecuteEx(&sei))
    {
        QMessageBox::critical(nullptr, QStringLiteral("Error"), QStringLiteral("Failed to launch %1 as administrator.").arg(file));
    }
#else
    #include <QProcess>
    
    QProcess process;
    process.setProgram(file);
    process.setArguments(arguments);
    process.startDetached();
#endif
}

void InstallUpdateDialog::timerEvent(QTimerEvent *event)
{
    this->killTimer(event->timerId());
    
    // If we have a download URL, start with download, otherwise start with installation
    if (!downloadUrl.isEmpty()) {
        this->download();
    } else {
        this->install();
    }
}

