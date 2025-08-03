#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSettings>
#include <QDir>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class FileModelSources;
class FileModelToFill;
class TemplateMergerFiller;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    QSharedPointer<QSettings> settings() const;
    FileModelSources *getFileModelSources() const;
    FileModelToFill *getFileModelToFill() const;

public slots:
    void browseSourceMain();
    void extractProductInfos();
    void viewFormatExtraInfosGpt();
    void generate();
    void clearPreviousChatgptReplies();
    void displayLog(const QString &logMEssage);

private slots:
    void onApiKeyChanged(const QString &key);
    void _enableGenerateButtonIfValid();

private:
    Ui::MainWindow *ui;
    void _connectslots();
    QString m_settingsFilePath;
    QDir m_workingDir;
    QString m_settingsKeyExtraInfos;
    QString m_settingsKeyApi;
    TemplateMergerFiller *m_templateMergerFiller;
};
#endif // MAINWINDOW_H
