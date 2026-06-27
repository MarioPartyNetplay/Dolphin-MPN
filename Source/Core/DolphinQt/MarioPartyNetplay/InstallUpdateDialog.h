/*
*  Dolphin for Mario Party Netplay
*  Copyright (C) 2025 Tabitha Hanegan <tabithahanegan.com>
*/

#include <QDialog>
#include <QString>
#include <QLabel>
#include <QProgressBar>
#include <functional>

// Forward declarations
QT_BEGIN_NAMESPACE
class QString;
class QLabel;
class QProgressBar;
QT_END_NAMESPACE

class InstallUpdateDialog : public QDialog 
{
    Q_OBJECT

public:
    InstallUpdateDialog(QWidget *parent, QString installationDirectory, QString temporaryDirectory, QString filename, QString downloadUrl = QString());
    ~InstallUpdateDialog();

    void install(void);

private:
    QString installationDirectory;
    QString temporaryDirectory;    
    QString filename;              
    QString downloadUrl;
    QLabel* label;                
    QProgressBar* progressBar;     // Master progress bar
    QLabel* stepLabel;             // Unified step label
    QProgressBar* stepProgressBar; // Unified step progress bar

    void writeAndRunScript(QStringList stringList);
    void launchProcess(QString file, QStringList arguments);
    void timerEvent(QTimerEvent* event);
    void download();
    void startZipExtraction(const QString& full_file_path, const QString& extract_directory,
                            const QString& install_directory);
    void finishInstallAfterExtract(const QString& extract_directory);
    bool unzipFile(const std::string& zipFilePath, const std::string& destDir,
                   const std::string& installDir,
                   std::function<void(int current, int total, int skipped)> progressCallback =
                       nullptr);
};
