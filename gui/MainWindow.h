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
    void generate();
    void extractProductInfos();

private:
    Ui::MainWindow *ui;
    void _connectslots();
    QString m_settingsFilePath;
    QDir m_workingDir;
};
#endif // MAINWINDOW_H
