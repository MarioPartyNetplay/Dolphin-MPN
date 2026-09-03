/*
*  Dolphin for Mario Party Netplay
*  Copyright (C) 2025 Tabitha Hanegan <tabithahanegan.com>
*/

#include "Common/MinizipUtil.h"
#include "InstallUpdateDialog.h"
#include "DownloadWorker.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QLabel>
#include <QMessageBox>
#include <QMetaObject>
#include <QProcess>
#include <QProgressBar>
#include <QTextStream>
#include <QThread>
#include <QThreadPool>
#include <QRunnable>
#include <QVBoxLayout>

#include <atomic>
#include <algorithm>
#include <mutex>
#include <vector>

#include <mz.h>
#include <mz_strm.h>
#include <mz_zip.h>
#include <mz_zip_rw.h>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

namespace
{
QString ResolveInstallRoot()
{
#ifdef __APPLE__
  // .../Dolphin-MPN.app/Contents/MacOS -> directory that contains Dolphin-MPN.app
  return QFileInfo(QCoreApplication::applicationDirPath() + QStringLiteral("/../../.."))
      .absoluteFilePath();
#else
  return QFileInfo(QCoreApplication::applicationDirPath()).absoluteFilePath();
#endif
}

QString NativePathNoTrailingSep(QString path)
{
  path = QDir::toNativeSeparators(QFileInfo(path).absoluteFilePath());
  while (path.size() > 1 &&
         (path.endsWith(QLatin1Char('\\')) || path.endsWith(QLatin1Char('/'))))
  {
    path.chop(1);
  }
  return path;
}

bool IsUserDataRelativePath(const QString& relative_path)
{
  const QString normalized = QDir::fromNativeSeparators(relative_path);
  return normalized == QStringLiteral("User") || normalized.startsWith(QStringLiteral("User/"));
}

QByteArray FileSha256(const QString& path)
{
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly))
    return {};

  QCryptographicHash hash(QCryptographicHash::Sha256);
  if (!hash.addData(&file))
    return {};
  return hash.result();
}

bool FilesAreExactlyIdentical(const QString& source_path, const QString& dest_path)
{
  const QFileInfo source_info(source_path);
  const QFileInfo dest_info(dest_path);
  if (!source_info.isFile() || !dest_info.isFile())
    return false;
  if (source_info.size() != dest_info.size())
    return false;

  const QByteArray source_hash = FileSha256(source_path);
  const QByteArray dest_hash = FileSha256(dest_path);
  return !source_hash.isEmpty() && source_hash == dest_hash;
}

struct CopyCounters
{
  std::atomic<int> processed{0};
  std::atomic<int> copied{0};
  std::atomic<int> skipped{0};
  std::atomic<int> failed{0};
  std::mutex pending_mutex;
  QStringList pending_relative_paths;
};

class CopyFileTask final : public QRunnable
{
public:
  CopyFileTask(QString source_path, QString dest_path, QString relative_path, CopyCounters* counters)
      : m_source_path(std::move(source_path)), m_dest_path(std::move(dest_path)),
        m_relative_path(std::move(relative_path)), m_counters(counters)
  {
    setAutoDelete(true);
  }

  void run() override
  {
    if (FilesAreExactlyIdentical(m_source_path, m_dest_path))
    {
      m_counters->skipped.fetch_add(1, std::memory_order_relaxed);
      m_counters->processed.fetch_add(1, std::memory_order_relaxed);
      return;
    }

    QDir().mkpath(QFileInfo(m_dest_path).absolutePath());
    QFile::remove(m_dest_path);
    if (QFile::copy(m_source_path, m_dest_path))
    {
      m_counters->copied.fetch_add(1, std::memory_order_relaxed);
    }
    else
    {
      // Likely locked by the running process; finish after restart script kills us.
      m_counters->failed.fetch_add(1, std::memory_order_relaxed);
      std::lock_guard lock(m_counters->pending_mutex);
      m_counters->pending_relative_paths.push_back(m_relative_path);
    }

    m_counters->processed.fetch_add(1, std::memory_order_relaxed);
  }

private:
  QString m_source_path;
  QString m_dest_path;
  QString m_relative_path;
  CopyCounters* m_counters;
};

QString ResolveCopySourceRoot(const QString& extract_directory)
{
#ifdef __APPLE__
  QDirIterator it(extract_directory,
                  QStringList{QStringLiteral("Dolphin-MPN.app")},
                  QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
  if (it.hasNext())
  {
    const QString bundle = it.next();
    // Copy into the existing .app by using the bundle as the source root's parent mapping:
    // source is the bundle itself; dest will be INSTALL_ROOT/Dolphin-MPN.app
    return NativePathNoTrailingSep(bundle);
  }
#endif
  return NativePathNoTrailingSep(extract_directory);
}

QString ResolveCopyDestRoot(const QString& install_root, const QString& source_root)
{
#ifdef __APPLE__
  if (source_root.endsWith(QStringLiteral(".app")))
    return NativePathNoTrailingSep(install_root + QDir::separator() + QStringLiteral("Dolphin-MPN.app"));
#endif
  return NativePathNoTrailingSep(install_root);
}

#ifdef _WIN32
QString BatchQuote(const QString& value)
{
  QString escaped = value;
  escaped.replace(QLatin1Char('"'), QStringLiteral("\"\""));
  return QLatin1Char('"') + escaped + QLatin1Char('"');
}
#endif

#ifdef __APPLE__
QString ShellQuote(const QString& value)
{
  QString escaped = value;
  escaped.replace(QLatin1Char('\''), QStringLiteral("'\\''"));
  return QLatin1Char('\'') + escaped + QLatin1Char('\'');
}
#endif
}  // namespace

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
  if (filename.isEmpty())
  {
    QMessageBox::critical(this, QStringLiteral("Error"),
                          QStringLiteral("No update package was selected for this platform."));
    reject();
    return;
  }

  temporaryDirectory = NativePathNoTrailingSep(temporaryDirectory);
  const QString full_file_path =
      NativePathNoTrailingSep(temporaryDirectory + QDir::separator() + filename);
  const QString app_path = NativePathNoTrailingSep(
      installationDirectory.isEmpty() ? ResolveInstallRoot() : installationDirectory);
  const QString app_pid = QString::number(QCoreApplication::applicationPid());

  if (filename.endsWith(QStringLiteral(".exe")))
  {
    label->setText(QStringLiteral("Step 3/3: Finishing up"));
    progressBar->setValue(50);
    stepLabel->setText(QStringLiteral("Update complete. Restarting..."));
    stepProgressBar->setValue(100);

#ifdef _WIN32
    // Wait for Dolphin to exit, run the Inno installer, then clean up.
    const QStringList script_lines = {
        QStringLiteral("setlocal EnableExtensions"),
        QStringLiteral("set \"PID=%1\"").arg(app_pid),
        QStringLiteral("set \"INSTALLER=%1\"").arg(full_file_path),
        QStringLiteral("set \"DIR=%1\"").arg(app_path),
        QStringLiteral("set \"TMPDIR=%1\"").arg(temporaryDirectory),
        QStringLiteral("echo == Waiting for PID %PID% to exit"),
        QStringLiteral("taskkill /F /PID %PID% >nul 2>&1"),
        QStringLiteral(":wait_exe"),
        QStringLiteral("tasklist /FI \"PID eq %PID%\" 2>nul | find \"%PID%\" >nul"),
        QStringLiteral("if not errorlevel 1 ("),
        QStringLiteral("  timeout /T 1 /NOBREAK >nul"),
        QStringLiteral("  goto wait_exe"),
        QStringLiteral(")"),
        QStringLiteral("echo == Running installer"),
        QStringLiteral(
            "\"%INSTALLER%\" /CLOSEAPPLICATIONS /NOCANCEL /MERGETASKS=\"!desktopicon\" /SILENT /DIR=\"%DIR%\""),
        QStringLiteral("if errorlevel 1 ("),
        QStringLiteral(
            "  start \"\" cmd /c \"echo Update failed. Check the installer log. ^& pause\""),
        QStringLiteral("  exit /B 1"),
        QStringLiteral(")"),
        QStringLiteral("echo == Cleaning up"),
        QStringLiteral("cd /d \"%TEMP%\""),
        QStringLiteral("rmdir /S /Q \"%TMPDIR%\""),
        QStringLiteral("endlocal"),
        QStringLiteral("exit /B 0"),
    };
    writeAndRunScript(script_lines);
#else
    QMessageBox::critical(this, QStringLiteral("Error"),
                          QStringLiteral("Installer updates are only supported on Windows."));
    reject();
#endif
    return;
  }

#ifdef __APPLE__
  if (filename.endsWith(QStringLiteral(".dmg")))
  {
    label->setText(QStringLiteral("Step 3/3: Finishing up"));
    progressBar->setValue(50);
    stepLabel->setText(QStringLiteral("Installing from DMG..."));
    stepProgressBar->setValue(100);

    const QString mount_point = QStringLiteral("/Volumes/Dolphin-MPN-Update");
    const QString app_dest =
        NativePathNoTrailingSep(app_path + QDir::separator() + QStringLiteral("Dolphin-MPN.app"));

    const QStringList script_lines = {
        QStringLiteral("set -euo pipefail"),
        QStringLiteral("PID=%1").arg(ShellQuote(app_pid)),
        QStringLiteral("DMG=%1").arg(ShellQuote(full_file_path)),
        QStringLiteral("MOUNT=%1").arg(ShellQuote(mount_point)),
        QStringLiteral("APP_DEST=%1").arg(ShellQuote(app_dest)),
        QStringLiteral("TMPDIR=%1").arg(ShellQuote(temporaryDirectory)),
        QStringLiteral("echo '== Waiting for process to exit'"),
        QStringLiteral("kill \"$PID\" 2>/dev/null || true"),
        QStringLiteral("while kill -0 \"$PID\" 2>/dev/null; do sleep 0.5; done"),
        QStringLiteral("echo '== Mounting DMG'"),
        QStringLiteral("hdiutil detach \"$MOUNT\" >/dev/null 2>&1 || true"),
        QStringLiteral("hdiutil attach \"$DMG\" -mountpoint \"$MOUNT\" -nobrowse"),
        QStringLiteral("echo '== Replacing application bundle'"),
        QStringLiteral("rm -rf \"$APP_DEST\""),
        QStringLiteral("cp -R \"$MOUNT/Dolphin-MPN.app\" \"$(dirname \"$APP_DEST\")/\""),
        QStringLiteral("echo '== Unmounting DMG'"),
        QStringLiteral("hdiutil detach \"$MOUNT\" || hdiutil detach \"$MOUNT\" -force"),
        QStringLiteral("echo '== Launching updated application'"),
        QStringLiteral("open \"$APP_DEST\""),
        QStringLiteral("echo '== Cleaning up'"),
        QStringLiteral("rm -rf \"$TMPDIR\""),
        QStringLiteral("exit 0"),
    };
    writeAndRunScript(script_lines);
    return;
  }
#endif

  label->setText(QStringLiteral("Step 2/3: Extracting"));
  progressBar->setValue(50);
  stepLabel->setText(QStringLiteral("0 files extracted..."));
  stepProgressBar->setValue(0);
  stepProgressBar->setMinimum(0);
  stepProgressBar->setMaximum(100);
  layout()->update();
  updateGeometry();

  const QString extract_directory =
      temporaryDirectory + QDir::separator() + QStringLiteral("Dolphin-MPN");

  if (filename.endsWith(QStringLiteral(".zip")))
  {
    QDir extract_directory_hack(extract_directory);
    if (extract_directory_hack.exists())
      extract_directory_hack.removeRecursively();

    if (!QDir().mkpath(extract_directory))
    {
      QMessageBox::critical(this, QStringLiteral("Error"),
                            QStringLiteral("Failed to create extract directory."));
      reject();
      return;
    }

    startZipExtraction(full_file_path, extract_directory);
    return;
  }

  QMessageBox::critical(this, QStringLiteral("Error"),
                        QStringLiteral("Unsupported update file format: %1").arg(filename));
  reject();
}

void InstallUpdateDialog::startZipExtraction(const QString& full_file_path,
                                             const QString& extract_directory)
{
  struct ExtractionContext
  {
    QString full_file_path;
    QString extract_directory;
  };

  auto* context = new ExtractionContext{full_file_path, extract_directory};
  auto* thread = new QThread;
  auto* worker = new QObject;

  worker->moveToThread(thread);

  connect(thread, &QThread::started, worker, [this, context, worker, thread] {
    const bool ok = unzipFile(
        context->full_file_path.toStdString(), context->extract_directory.toStdString(),
        [this](const int current, const int total) {
          QMetaObject::invokeMethod(
              this,
              [this, current, total] {
                if (total <= 0)
                  return;

                const int extraction_progress = (current * 100) / total;
                stepProgressBar->setValue(extraction_progress);
                progressBar->setValue(50 + (current * 45 / total));
                stepLabel->setText(
                    QStringLiteral("(%1/%2) files extracted...").arg(current).arg(total));
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
  const QString install_root = NativePathNoTrailingSep(
      installationDirectory.isEmpty() ? ResolveInstallRoot() : installationDirectory);
  const QString source_root = ResolveCopySourceRoot(extract_directory);
  const QString dest_root = ResolveCopyDestRoot(install_root, source_root);

  label->setText(QStringLiteral("Step 3/3: Copying changed files..."));
  progressBar->setValue(95);
  stepLabel->setText(QStringLiteral("Comparing and copying..."));
  stepProgressBar->setValue(0);
  stepProgressBar->setMaximum(100);

  startThreadedInstallCopy(source_root, dest_root);
}

void InstallUpdateDialog::startThreadedInstallCopy(const QString& source_directory,
                                                   const QString& dest_directory)
{
  struct CopyContext
  {
    QString source_directory;
    QString dest_directory;
  };

  auto* context = new CopyContext{source_directory, dest_directory};
  auto* thread = new QThread;
  auto* worker = new QObject;
  worker->moveToThread(thread);

  connect(thread, &QThread::started, worker, [this, context, worker, thread] {
    QStringList relative_paths;
    QDirIterator iterator(context->source_directory, QDir::Files | QDir::Hidden,
                          QDirIterator::Subdirectories);
    while (iterator.hasNext())
    {
      iterator.next();
      const QString absolute = iterator.filePath();
      const QString relative = QDir(context->source_directory).relativeFilePath(absolute);
      if (IsUserDataRelativePath(relative))
        continue;
      relative_paths.push_back(relative);
    }

    const int total = relative_paths.size();
    auto* counters = new CopyCounters;

    QMetaObject::invokeMethod(
        this,
        [this, total] {
          stepProgressBar->setMaximum(std::max(total, 1));
          stepProgressBar->setValue(0);
          stepLabel->setText(QStringLiteral("0/%1 files processed...").arg(total));
        },
        Qt::QueuedConnection);

    QThreadPool pool;
    pool.setMaxThreadCount(std::max(2, QThread::idealThreadCount()));

    for (const QString& relative : relative_paths)
    {
      const QString source_path = context->source_directory + QDir::separator() + relative;
      const QString dest_path = context->dest_directory + QDir::separator() + relative;
      pool.start(new CopyFileTask(source_path, dest_path, relative, counters));
    }

    while (counters->processed.load(std::memory_order_relaxed) < total)
    {
      const int processed = counters->processed.load(std::memory_order_relaxed);
      const int copied = counters->copied.load(std::memory_order_relaxed);
      const int skipped = counters->skipped.load(std::memory_order_relaxed);
      QMetaObject::invokeMethod(
          this,
          [this, processed, total, copied, skipped] {
            stepProgressBar->setValue(processed);
            progressBar->setValue(95 + (total > 0 ? (processed * 4 / total) : 0));
            stepLabel->setText(QStringLiteral("(%1/%2) copied %3, skipped identical %4")
                                   .arg(processed)
                                   .arg(total)
                                   .arg(copied)
                                   .arg(skipped));
          },
          Qt::QueuedConnection);
      QThread::msleep(50);
    }

    pool.waitForDone();

    QStringList pending;
    {
      std::lock_guard lock(counters->pending_mutex);
      pending = counters->pending_relative_paths;
    }

    const int copied = counters->copied.load();
    const int skipped = counters->skipped.load();
    const int failed = counters->failed.load();
    delete counters;
    delete context;

    QMetaObject::invokeMethod(
        this,
        [this, worker, thread, pending, copied, skipped, failed, total] {
          thread->quit();
          stepProgressBar->setValue(total);
          stepLabel->setText(QStringLiteral("Copied %1, skipped %2 identical, deferred %3")
                                 .arg(copied)
                                 .arg(skipped)
                                 .arg(failed));
          finishInstallAfterCopy(pending);
          worker->deleteLater();
        },
        Qt::QueuedConnection);
  });

  connect(thread, &QThread::finished, thread, &QThread::deleteLater);
  thread->start();
}

void InstallUpdateDialog::finishInstallAfterCopy(const QStringList& pending_relative_paths)
{
#if !defined(_WIN32) && !defined(__APPLE__)
  QMessageBox::critical(this, QStringLiteral("Error"),
                        QStringLiteral("Automatic install is not supported on this platform."));
  reject();
  return;
#else
  const QString install_root = NativePathNoTrailingSep(
      installationDirectory.isEmpty() ? ResolveInstallRoot() : installationDirectory);
  const QString extract_directory =
      NativePathNoTrailingSep(temporaryDirectory + QDir::separator() + QStringLiteral("Dolphin-MPN"));
  const QString source_root = ResolveCopySourceRoot(extract_directory);
  const QString dest_root = ResolveCopyDestRoot(install_root, source_root);
  const QString app_pid = QString::number(QCoreApplication::applicationPid());
  const QString native_temp_directory = NativePathNoTrailingSep(temporaryDirectory);
  const QString pending_list_path = NativePathNoTrailingSep(
      temporaryDirectory + QDir::separator() + QStringLiteral("pending_copy.txt"));

  {
    QFile pending_file(pending_list_path);
    if (!pending_file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
    {
      QMessageBox::critical(this, QStringLiteral("Error"),
                            QStringLiteral("Failed to write pending copy list."));
      reject();
      return;
    }
    QTextStream out(&pending_file);
    for (const QString& relative : pending_relative_paths)
      out << QDir::fromNativeSeparators(relative) << QLatin1Char('\n');
  }

  label->setText(QStringLiteral("Step 3/3: Finishing up..."));
  progressBar->setValue(99);
  stepLabel->setText(pending_relative_paths.isEmpty() ?
                         QStringLiteral("Restarting...") :
                         QStringLiteral("Restarting to finish %1 locked files...")
                             .arg(pending_relative_paths.size()));
  stepProgressBar->setValue(stepProgressBar->maximum());

#ifdef __APPLE__
  const QString app_dest =
      NativePathNoTrailingSep(install_root + QDir::separator() + QStringLiteral("Dolphin-MPN.app"));
  const QStringList script_lines = {
      QStringLiteral("set -euo pipefail"),
      QStringLiteral("PID=%1").arg(ShellQuote(app_pid)),
      QStringLiteral("SRC=%1").arg(ShellQuote(source_root)),
      QStringLiteral("DST=%1").arg(ShellQuote(dest_root)),
      QStringLiteral("APP_DEST=%1").arg(ShellQuote(app_dest)),
      QStringLiteral("TMPDIR=%1").arg(ShellQuote(native_temp_directory)),
      QStringLiteral("PENDING=%1").arg(ShellQuote(pending_list_path)),
      QStringLiteral("echo '== Waiting for process to exit'"),
      QStringLiteral("kill \"$PID\" 2>/dev/null || true"),
      QStringLiteral("while kill -0 \"$PID\" 2>/dev/null; do sleep 0.5; done"),
      QStringLiteral("if [ -f \"$PENDING\" ]; then"),
      QStringLiteral("  echo '== Copying files that were locked'"),
      QStringLiteral("  while IFS= read -r rel || [ -n \"$rel\" ]; do"),
      QStringLiteral("    [ -z \"$rel\" ] && continue"),
      QStringLiteral("    mkdir -p \"$(dirname \"$DST/$rel\")\""),
      QStringLiteral("    cp -f \"$SRC/$rel\" \"$DST/$rel\""),
      QStringLiteral("  done < \"$PENDING\""),
      QStringLiteral("fi"),
      QStringLiteral("echo '== Launching updated application'"),
      QStringLiteral("open \"$APP_DEST\""),
      QStringLiteral("echo '== Cleaning up'"),
      QStringLiteral("rm -rf \"$TMPDIR\""),
      QStringLiteral("exit 0"),
  };
  writeAndRunScript(script_lines);
#elif defined(_WIN32)
  const QString exe_path =
      NativePathNoTrailingSep(install_root + QDir::separator() + QStringLiteral("Dolphin-MPN.exe"));
  const QStringList script_lines = {
      QStringLiteral("setlocal EnableExtensions EnableDelayedExpansion"),
      QStringLiteral("set \"PID=%1\"").arg(app_pid),
      QStringLiteral("set \"SRC=%1\"").arg(source_root),
      QStringLiteral("set \"DST=%1\"").arg(dest_root),
      QStringLiteral("set \"EXE=%1\"").arg(exe_path),
      QStringLiteral("set \"TMPDIR=%1\"").arg(native_temp_directory),
      QStringLiteral("set \"PENDING=%1\"").arg(pending_list_path),
      QStringLiteral("echo == Waiting for PID %PID% to exit"),
      QStringLiteral("taskkill /F /PID %PID% >nul 2>&1"),
      QStringLiteral(":wait_copy"),
      QStringLiteral("tasklist /FI \"PID eq %PID%\" 2>nul | find \"%PID%\" >nul"),
      QStringLiteral("if not errorlevel 1 ("),
      QStringLiteral("  timeout /T 1 /NOBREAK >nul"),
      QStringLiteral("  goto wait_copy"),
      QStringLiteral(")"),
      QStringLiteral("if exist \"%PENDING%\" ("),
      QStringLiteral("  echo == Copying files that were locked"),
      QStringLiteral("  for /f \"usebackq delims=\" %%F in (\"%PENDING%\") do ("),
      QStringLiteral("    if not \"%%F\"==\"\" ("),
      QStringLiteral("      set \"REL=%%F\""),
      QStringLiteral("      set \"REL=!REL:/=\\!\""),
      QStringLiteral("      for %%D in (\"%DST%\\!REL!\") do if not exist \"%%~dpD\" mkdir \"%%~dpD\""),
      QStringLiteral("      copy /Y \"%SRC%\\!REL!\" \"%DST%\\!REL!\" >nul"),
      QStringLiteral("    )"),
      QStringLiteral("  )"),
      QStringLiteral(")"),
      QStringLiteral("echo == Starting Dolphin-MPN"),
      QStringLiteral("if not exist \"%EXE%\" ("),
      QStringLiteral("  start \"\" cmd /c \"echo Updated executable not found: %EXE% ^& pause\""),
      QStringLiteral("  exit /B 1"),
      QStringLiteral(")"),
      QStringLiteral("start \"\" \"%EXE%\""),
      QStringLiteral("echo == Cleaning up"),
      QStringLiteral("cd /d \"%TEMP%\""),
      QStringLiteral("rmdir /S /Q \"%TMPDIR%\""),
      QStringLiteral("endlocal"),
      QStringLiteral("exit /B 0"),
  };
  writeAndRunScript(script_lines);
#endif
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
        
        // Skip files in the User/ directory to preserve user data
        std::string filename_str(file_info->filename);
        bool is_user_file = (filename_str.find("User/") == 0 || 
                            filename_str.find("User\\") == 0);
        
        if (!is_user_file)
        {
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
  const QString script_path =
      QDir::toNativeSeparators(temporaryDirectory + QStringLiteral("/update.sh"));
#else
  const QString script_path =
      QDir::toNativeSeparators(temporaryDirectory + QStringLiteral("/update.bat"));
#endif

  QFile script_file(script_path);
  if (!script_file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
  {
    QMessageBox::critical(this, tr("Error"),
                          tr("Failed to open file for writing: %1").arg(script_path));
    return;
  }

  QTextStream out(&script_file);
#ifdef __APPLE__
  out << QStringLiteral("#!/bin/bash\n");
#else
  out << QStringLiteral("@echo off\r\n");
#endif

  for (const QString& line : stringList)
  {
#ifdef __APPLE__
    out << line << QLatin1Char('\n');
#else
    // CRLF so cmd.exe reliably parses the batch file.
    out << line << QStringLiteral("\r\n");
#endif
  }

  script_file.close();

#ifdef __APPLE__
  script_file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner |
                             QFileDevice::ExeOwner | QFileDevice::ReadUser |
                             QFileDevice::ExeUser);
#endif

  launchProcess(script_path, {});
}

void InstallUpdateDialog::launchProcess(QString file, QStringList arguments)
{
#ifdef _WIN32
  // Elevate and run through cmd.exe so .bat files execute correctly under ShellExecute.
  const QString params =
      QStringLiteral("/c ") + BatchQuote(file) +
      (arguments.isEmpty() ? QString{} :
                             (QStringLiteral(" ") + arguments.join(QLatin1Char(' '))));
  const std::wstring cmd_w = QStringLiteral("cmd.exe").toStdWString();
  const std::wstring params_w = params.toStdWString();

  SHELLEXECUTEINFOW sei{};
  sei.cbSize = sizeof(sei);
  sei.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NO_CONSOLE;
  sei.lpVerb = L"runas";
  sei.lpFile = cmd_w.c_str();
  sei.lpParameters = params_w.c_str();
  sei.nShow = SW_HIDE;

  if (!ShellExecuteExW(&sei))
  {
    QMessageBox::critical(this, QStringLiteral("Error"),
                          QStringLiteral("Failed to launch updater script as administrator."));
    return;
  }

  // Do not wait: the script kills this process.
  if (sei.hProcess != nullptr)
    CloseHandle(sei.hProcess);
#elif defined(__APPLE__)
  if (!QProcess::startDetached(QStringLiteral("/bin/bash"), QStringList{file} + arguments))
  {
    QMessageBox::critical(this, QStringLiteral("Error"),
                          QStringLiteral("Failed to launch %1.").arg(file));
    return;
  }
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

