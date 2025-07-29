#include <QSettings>
#include <QMessageBox>
#include <QFileDialog>

#include "model/FileModelSources.h"
#include "model/FileModelToFill.h"
#include "DialogExtractInfos.h"

#include "MainWindow.h"
#include "./ui_MainWindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    ui->buttonGenerate->setEnabled(false);
    _connectslots();
}

MainWindow::~MainWindow()
{
    delete ui;
}

QSharedPointer<QSettings> MainWindow::settings() const
{
    return QSharedPointer<QSettings>{new QSettings{m_settingsFilePath, QSettings::IniFormat}};
}

FileModelSources *MainWindow::getFileModelSources() const
{
    return static_cast<FileModelSources *>(
        ui->treeViewSources->model());
}

FileModelToFill *MainWindow::getFileModelToFill() const
{
    return static_cast<FileModelToFill *>(
        ui->treeViewToFill->model());
}

void MainWindow::browseSourceMain()
{
    QSettings settings;
    const QString key{"MainWindow__browseSourceMain"};
    QDir lastDir{settings.value(key, QDir{}.path()).toString()};
    const QString &filePath = QFileDialog::getOpenFileName(
        this,
        tr("Template file pre-filled"),
        lastDir.path(),
        QString{"Xlsx (*.xlsx *.XLSX *.xlsm *.XLSM)"},
        nullptr,
        QFileDialog::DontUseNativeDialog);
    if (!filePath.isEmpty())
    {
        m_workingDir = QFileInfo{filePath}.dir();
        const auto &workingDirPath = m_workingDir.path();
        settings.setValue(key, workingDirPath);
        ui->lineEditTo->setText(filePath);
        ui->buttonGenerate->setEnabled(true);
        auto *curModelSource = ui->treeViewSources->model();
        auto *fileModelSources
            = new FileModelSources{workingDirPath, ui->treeViewSources};
        ui->treeViewSources->setModel(fileModelSources);
        ui->treeViewSources->setRootIndex(
            fileModelSources->index(workingDirPath));
        ui->treeViewSources->header()->resizeSection(0, 300);
        auto *curModelToFill = ui->treeViewToFill->model();
        auto *fileModelToFill
            = new FileModelToFill{workingDirPath, ui->treeViewToFill};
        ui->treeViewToFill->setModel(fileModelToFill);
        ui->treeViewToFill->setRootIndex(fileModelToFill->index(workingDirPath));
        ui->treeViewToFill->header()->resizeSection(0, 300);
        if (curModelToFill != nullptr)
        {
            curModelToFill->deleteLater();
            curModelSource->deleteLater();
        }
    }
}

void MainWindow::_connectslots()
{
    connect(ui->buttonBrowseSource,
            &QPushButton::clicked,
            this,
            &MainWindow::browseSourceMain);
    connect(ui->buttonGenerate,
            &QPushButton::clicked,
            this,
            &MainWindow::generate);
    connect(ui->buttonExtractProductInfos,
            &QPushButton::clicked,
            this,
            &MainWindow::extractProductInfos);
}

void MainWindow::generate()
{
    /*
    const auto &fromFilePaths = filePathsFrom();
    //TOTO create a class that will help
    TemplateMerger templateMerger{
        fromFilePaths,
        ui->lineEditTo->text()
    };
    QFileInfo fileInfoTo(ui->lineEditTo->text());
    const auto &fileNameTo = fileInfoTo.baseName() + "-FILLED.xlsx";
    const auto &filePathTo = QDir{fileInfoTo.path()}.absoluteFilePath(fileNameTo);
    templateMerger.exportTo(filePathTo);
//*/
}

void MainWindow::extractProductInfos()
{
    DialogExtractInfos dialog{m_workingDir.path()};
    dialog.exec();
}
