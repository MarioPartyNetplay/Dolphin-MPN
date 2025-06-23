/*
*  Dolphin for Mario Party Netplay
*  Copyright (C) 2025 Tabitha Hanegan <tabithahanegan.com>
*/

#include "InstallUpdateDialog.h"
#include <QCoreApplication>
#include <QProcess>
#include <QDir>
#include <QTextStream>
#include <QVBoxLayout>
#include <QDialog>
#include <QLabel>
#include <QProgressBar>
#include <QMessageBox>
#include "Common/MinizipUtil.h"
#include <Common/Logging/Log.h>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

// Constructor implementation
InstallUpdateDialog::InstallUpdateDialog(QWidget *parent, QString installationDirectory, QString temporaryDirectory, QString filename)
    : QDialog(parent), // Only pass the parent
      installationDirectory(installationDirectory),
      temporaryDirectory(temporaryDirectory),
      filename(filename) // Initialize member variables
{
    setWindowTitle(QStringLiteral("Installing %1...").arg(this->filename));
    
    // Create UI components
    QVBoxLayout* layout = new QVBoxLayout(this);
    label = new QLabel(QStringLiteral("Installing %1...").arg(this->filename), this);
    progressBar = new QProgressBar(this);

    // Set up the layout
    layout->addWidget(label);
    layout->addWidget(progressBar);

    setLayout(layout);

    startTimer(100);
}

// Destructor implementation
InstallUpdateDialog::~InstallUpdateDialog(void)
{
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
    this->label->setText(QStringLiteral("Executing %1...").arg(this->filename));

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
        QStringLiteral("   start \"\" cmd /c \"echo Update failed, check the log for more "
                       "information && pause\""),
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
    this->label->setText(QStringLiteral("Mounting DMG and installing %1...").arg(this->filename));
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

  this->label->setText(QStringLiteral("Extracting %1...").arg(this->filename));
  this->progressBar->setValue(50);
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
    if (!unzipFile(fullFilePath.toStdString(), extractDirectory.toStdString()))
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

  this->label->setText(QStringLiteral("Executing update script..."));
  this->progressBar->setValue(100);

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
#endif
}


bool InstallUpdateDialog::unzipFile(const std::string& zipFilePath, const std::string& destDir)
{
  unzFile zipFile = unzOpen(zipFilePath.c_str());
  if (!zipFile)
  {
    return false;  // Failed to open zip file
  }

  if (unzGoToFirstFile(zipFile) != UNZ_OK)
  {
    unzClose(zipFile);
    return false;  // No files in zip
  }

  do
  {
    char filename[256];
    unz_file_info fileInfo;
    if (unzGetCurrentFileInfo(zipFile, &fileInfo, filename, sizeof(filename), nullptr, 0, nullptr,
                              0) != UNZ_OK)
    {
      unzClose(zipFile);
      return false;  // Failed to get file info
    }

    // Skip User/GC and User/GameSettings directories
    if (std::string(filename).find("User/GC") == 0 || std::string(filename).find("User/GameSettings") == 0)
    {
        continue;  // Skip these directories
    }

    // Create full path for the extracted file
    std::string fullPath = destDir + "/" + std::string(filename);
    QString qFullPath = QString::fromStdString(fullPath);

    // Handle directories
    if (filename[std::strlen(filename) - 1] == '/')
    {
      QDir().mkpath(qFullPath);
      continue;
    }

    // Ensure the directory structure exists
    QDir().mkpath(QFileInfo(qFullPath).path());

    // Prepare a buffer to store file data
    std::vector<u8> fileData(fileInfo.uncompressed_size);
    if (!Common::ReadFileFromZip(zipFile, &fileData))
    {
      unzClose(zipFile);
      return false;  // Failed to read file from zip
    }

    // Write the file data to disk
    QFile outFile(qFullPath);
    if (!outFile.open(QIODevice::WriteOnly))
    {
      unzClose(zipFile);
      return false;  // Failed to create output file
    }

    outFile.write(reinterpret_cast<const char*>(fileData.data()), fileData.size());
    outFile.close();
  } while (unzGoToNextFile(zipFile) == UNZ_OK);

  unzClose(zipFile);
  return true;  // Successfully unzipped all files
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
    this->install();
}
