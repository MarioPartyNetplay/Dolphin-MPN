/*
*  Dolphin for Mario Party Netplay
*  Copyright (C) 2025 Tabitha Hanegan <tabithahanegan.com>
*/

#include "Common/Hash.h"
#include "Common/MinizipUtil.h"
#include "InstallUpdateDialog.h"
#include "DownloadWorker.h"

#include <array>
#include <cstring>
#include <optional>

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QDir>
#include <QTextStream>
#include <QVBoxLayout>
#include <QDialog>
#include <QLabel>
#include <QProgressBar>
#include <QMessageBox>
#include <QProcess>
#include <QThread>
#include <QMetaObject>

#include <mz.h>
#include <mz_strm.h>
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
    setWindowTitle(QStringLiteral("Dolphin MPN - Updater"));
    
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
    return;
  }
#endif

  this->label->setText(QStringLiteral("Step 2/3: Extracting").arg(this->filename));
  this->progressBar->setValue(50);
  
  // Step bar for extraction
  this->stepLabel->setText(QStringLiteral("0 files extracted..."));
  this->stepProgressBar->setValue(0);
  this->stepProgressBar->setMinimum(0);
  this->stepProgressBar->setMaximum(100);
  this->layout()->update();
  this->updateGeometry();

  const QString extract_directory =
      this->temporaryDirectory + QDir::separator() + QStringLiteral("Dolphin-MPN");

  if (this->filename.endsWith(QStringLiteral(".zip")))
  {
    QDir extract_directory_hack(extract_directory);
    if (extract_directory_hack.exists())
      extract_directory_hack.removeRecursively();

    QDir dir(this->temporaryDirectory);
    if (!QDir(extract_directory).exists())
    {
      if (!dir.mkdir(QStringLiteral("Dolphin-MPN")))
      {
        QMessageBox::critical(this, QStringLiteral("Error"),
                              QStringLiteral("Failed to create extract directory."));
        reject();
        return;
      }
    }

    startZipExtraction(fullFilePath, extract_directory, appPath);
    return;
  }

  QMessageBox::critical(this, QStringLiteral("Error"),
                        QStringLiteral("Unsupported update file format: %1").arg(this->filename));
  reject();
}

namespace
{
bool IsUserDataPath(const char* filename)
{
  if (!filename)
    return false;

  return strncmp(filename, "User/", 5) == 0 || strncmp(filename, "User\\", 5) == 0;
}

std::optional<u32> ComputeFileCRC32(const QString& path)
{
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly))
    return std::nullopt;

  u32 crc = Common::StartCRC32();
  std::array<char, 256 * 1024> buffer{};

  while (true)
  {
    const qint64 bytes_read = file.read(buffer.data(), static_cast<qint64>(buffer.size()));
    if (bytes_read < 0)
      return std::nullopt;
    if (bytes_read == 0)
      break;

    crc = Common::UpdateCRC32(crc, reinterpret_cast<const u8*>(buffer.data()),
                              static_cast<size_t>(bytes_read));
  }

  return crc;
}

bool InstalledFileMatchesZipEntry(const std::string& install_dir, const mz_zip_file* file_info)
{
  if (!file_info || !file_info->filename)
    return false;

  const size_t name_len = strlen(file_info->filename);
  if (name_len == 0 || file_info->filename[name_len - 1] == '/')
    return false;

  const QString install_path =
      QString::fromStdString(install_dir + "/" + file_info->filename);
  const QFileInfo installed_file(install_path);
  if (!installed_file.exists() || !installed_file.isFile())
    return false;

  if (static_cast<uint64_t>(installed_file.size()) != file_info->uncompressed_size)
    return false;

  const std::optional<u32> crc = ComputeFileCRC32(installed_file.absoluteFilePath());
  return crc && *crc == file_info->crc;
}
}  // namespace

void InstallUpdateDialog::startZipExtraction(const QString& full_file_path,
                                             const QString& extract_directory,
                                             const QString& install_directory)
{
  struct ExtractionContext
  {
    QString full_file_path;
    QString extract_directory;
    QString install_directory;
  };

  auto* context =
      new ExtractionContext{full_file_path, extract_directory, install_directory};
  auto* thread = new QThread;
  auto* worker = new QObject;

  worker->moveToThread(thread);

  connect(thread, &QThread::started, worker, [this, context, worker, thread] {
    const bool ok = unzipFile(
        context->full_file_path.toStdString(), context->extract_directory.toStdString(),
        context->install_directory.toStdString(),
        [this](const int current, const int total, const int skipped) {
          QMetaObject::invokeMethod(
              this,
              [this, current, total, skipped] {
                if (total <= 0)
                  return;

                const int extraction_progress = (current * 100) / total;
                stepProgressBar->setValue(extraction_progress);
                progressBar->setValue(50 + (current * 45 / total));
                if (skipped > 0)
                {
                  stepLabel->setText(QStringLiteral("(%1/%2) processed, %3 unchanged...")
                                         .arg(current)
                                         .arg(total)
                                         .arg(skipped));
                }
                else
                {
                  stepLabel->setText(
                      QStringLiteral("(%1/%2) files extracted...").arg(current).arg(total));
                }
              },
              Qt::QueuedConnection);
        });

    const QString extract_directory = context->extract_directory;
    delete context;

    QMetaObject::invokeMethod(
        this,
        [this, extract_directory, ok, worker, thread] {
          thread->quit();

          if (ok)
            finishInstallAfterExtract(extract_directory);
          else
          {
            QMessageBox::critical(this, QStringLiteral("Error"),
                                  QStringLiteral("Unzip failed: Unable to extract files."));
            reject();
          }

          worker->deleteLater();
        },
        Qt::QueuedConnection);
  });

  connect(thread, &QThread::finished, thread, &QThread::deleteLater);
  thread->start();
}

void InstallUpdateDialog::finishInstallAfterExtract(const QString& extract_directory)
{
  QString full_file_path = temporaryDirectory + QDir::separator() + filename;

#ifdef __APPLE__
  const QString app_path = QDir::toNativeSeparators(
      QCoreApplication::applicationDirPath() + QStringLiteral("/../../../"));
#else
  const QString app_path = QDir::toNativeSeparators(QCoreApplication::applicationDirPath());
#endif

  const QString app_pid = QString::number(QCoreApplication::applicationPid());
  full_file_path = QDir::toNativeSeparators(full_file_path);
  const QString native_extract_directory = QDir::toNativeSeparators(extract_directory);

  label->setText(QStringLiteral("Step 3/3: Finishing up..."));
  progressBar->setValue(95);
  stepLabel->setText(QStringLiteral("Finishing up..."));
  stepProgressBar->setValue(100);

#ifdef __APPLE__
  const QStringList script_lines = {
      QStringLiteral("#!/bin/bash"),
      QStringLiteral("echo '== Terminating application with PID ") + app_pid + QStringLiteral("'"),
      QStringLiteral("kill -9 ") + app_pid,
      QStringLiteral("echo '== Removing old application files'"),
      QStringLiteral("rm -f \"") + app_path + QStringLiteral("\""),
      QStringLiteral("echo '== Copying new files to ") + app_path + QStringLiteral("'"),
      QStringLiteral("cp -r \"") + native_extract_directory + QStringLiteral("/\"* \"") + app_path +
          QStringLiteral("\""),
      QStringLiteral("echo '== Launching the updated application'"),
      QStringLiteral("open \"") + app_path + QStringLiteral("/Dolphin-MPN.app\""),
      QStringLiteral("echo '== Cleaning up temporary files'"),
      QStringLiteral("rm -rf \"") + temporaryDirectory + QStringLiteral("\""),
      QStringLiteral("exit 0")};
  writeAndRunScript(script_lines);
#endif

#ifdef _WIN32
  const QStringList script_lines = {
      QStringLiteral("@echo off"),
      QStringLiteral("("),
      QStringLiteral("   echo == Attempting to remove '") + full_file_path + QStringLiteral("'"),
      QStringLiteral("   del /F /Q \"") + full_file_path + QStringLiteral("\""),
      QStringLiteral("   echo == Attempting to kill PID ") + app_pid,
      QStringLiteral("   taskkill /F /PID:") + app_pid,
      QStringLiteral("   echo == Attempting to copy '") + native_extract_directory +
          QStringLiteral("' to '") + app_path + QStringLiteral("'"),
      QStringLiteral("   xcopy /S /Y /I \"") + native_extract_directory + QStringLiteral("\\*\" \"") +
          app_path + QStringLiteral("\""),
      QStringLiteral("   echo == Attempting to start '") + app_path +
          QStringLiteral("\\Dolphin-MPN.exe'"),
      QStringLiteral("   start \"\" \"") + app_path + QStringLiteral("\\Dolphin-MPN.exe\""),
      QStringLiteral(")"),
      QStringLiteral("IF NOT ERRORLEVEL 0 ("),
      QStringLiteral("   start \"\" cmd /c \"echo Update failed && pause\""),
      QStringLiteral(")"),
      QStringLiteral("rmdir /S /Q \"") + temporaryDirectory + QStringLiteral("\""),
      QStringLiteral("exit") + QStringLiteral("\""),
  };
  writeAndRunScript(script_lines);
#endif
}


bool InstallUpdateDialog::unzipFile(const std::string& zipFilePath, const std::string& destDir,
                                    const std::string& installDir,
                                    std::function<void(int, int, int)> progressCallback)
{
  void* reader = mz_zip_reader_create();
  if (!reader)
    return false;

  if (mz_zip_reader_open_file(reader, zipFilePath.c_str()) != MZ_OK)
  {
    mz_zip_reader_delete(&reader);
    return false;
  }

  int total_files = 0;
  for (int32_t count_status = mz_zip_reader_goto_first_entry(reader); count_status == MZ_OK;
       count_status = mz_zip_reader_goto_next_entry(reader))
  {
    total_files++;
  }

  int32_t entry_status = mz_zip_reader_goto_first_entry(reader);
  int current_file = 0;
  int skipped_files = 0;
  int last_reported = 0;

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

    const bool is_user_file = IsUserDataPath(file_info->filename);
    const size_t name_len = strlen(file_info->filename);
    const bool is_directory = name_len > 0 && file_info->filename[name_len - 1] == '/';

    if (!is_user_file)
    {
      const bool already_installed =
          !is_directory && InstalledFileMatchesZipEntry(installDir, file_info);

      if (already_installed)
      {
        skipped_files++;
      }
      else
      {
        const std::string out_path = destDir + "/" + file_info->filename;
        if (is_directory)
        {
          QDir().mkpath(QString::fromStdString(out_path));
        }
        else
        {
          QDir().mkpath(QFileInfo(QString::fromStdString(out_path)).path());
          if (mz_zip_reader_entry_save_file(reader, out_path.c_str()) != MZ_OK)
          {
            mz_zip_reader_close(reader);
            mz_zip_reader_delete(&reader);
            return false;
          }
        }
      }
    }

    current_file++;
    if (progressCallback &&
        (current_file - last_reported >= 50 || current_file == total_files))
    {
      last_reported = current_file;
      progressCallback(current_file, total_files, skipped_files);
    }

    entry_status = mz_zip_reader_goto_next_entry(reader);
  }

  if (progressCallback)
    progressCallback(total_files, total_files, skipped_files);

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
  const std::wstring file_w = file.toStdWString();
  const std::wstring args_w = arguments.join(QStringLiteral(" ")).toStdWString();

  SHELLEXECUTEINFOW sei{};
  sei.cbSize = sizeof(sei);
  sei.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NO_CONSOLE;
  sei.lpVerb = L"runas";
  sei.lpFile = file_w.c_str();
  sei.lpParameters = args_w.empty() ? nullptr : args_w.c_str();
  sei.nShow = SW_HIDE;

  if (!ShellExecuteExW(&sei))
  {
    QMessageBox::critical(this, QStringLiteral("Error"),
                          QStringLiteral("Failed to launch %1 as administrator.").arg(file));
    return;
  }

  // Do not wait for the updater script: it terminates this process, which would deadlock the UI.
  if (sei.hProcess != nullptr)
    CloseHandle(sei.hProcess);
#else
  if (!QProcess::startDetached(file, arguments))
  {
    QMessageBox::critical(this, QStringLiteral("Error"),
                          QStringLiteral("Failed to launch %1.").arg(file));
    return;
  }
#endif

  accept();
  QCoreApplication::quit();
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

