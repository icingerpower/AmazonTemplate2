#include <QSettings>
#include <QTimer>
#include <QMessageBox>
#include <QFileDialog>

#include "../common/workingdirectory/WorkingDirectoryManager.h"

#include "model/FileModelSources.h"
#include "model/FileModelToFill.h"
#include "model/TemplateMergerFiller.h"
#include "model/ExceptionTemplateError.h"

#include "DialogExtractInfos.h"

#include "MainWindow.h"
#include "./ui_MainWindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    ui->buttonGenerate->setEnabled(false);
    ui->buttonExtractProductInfos->setEnabled(false);
    m_settingsKeyExtraInfos = "MainWindowExtraInfos";
    auto settings = WorkingDirectoryManager::instance()->settings();
    if (settings->contains(m_settingsKeyExtraInfos))
    {
        ui->textEditExtraInfos->setText(settings->value(m_settingsKeyExtraInfos).toString());
    }
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
        ui->buttonExtractProductInfos->setEnabled(true);
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
    connect(ui->textEditExtraInfos,
            &QTextEdit::textChanged,
            this,
            [this](){
                static QDateTime nextDateTime = QDateTime::currentDateTime().addSecs(-1);
                const QDateTime &currentDateTime = QDateTime::currentDateTime();
                if (nextDateTime.secsTo(currentDateTime) > 0)
                {
                    int nSecs = 3;
                    nextDateTime = currentDateTime.addSecs(nSecs);
                    QTimer::singleShot(nSecs * 1000 + 20, this, [this]{
                        auto settings = WorkingDirectoryManager::instance()->settings();
                        settings->setValue(m_settingsKeyExtraInfos,
                                           ui->textEditExtraInfos->toPlainText());
                    });
                }
            });
}

void MainWindow::generate()
{
    TemplateMergerFiller templateMergerFiller{ui->lineEditTo->text()};
    try
    {
        templateMergerFiller.fillExcelFiles(
            getFileModelSources()->getFilePaths()
            , getFileModelToFill()->getFilePaths());
    }
    catch (const ExceptionTemplateError &exception)
    {
        QMessageBox::warning(this,
                             exception.title(),
                             exception.error());
    }

    /*
    const auto &fromFilePaths = filePathsFrom();
    //TOTO create a class that will help
    TemplateMergerFiller TemplateMergerFiller{
        fromFilePaths,
        ui->lineEditTo->text()
    };
    QFileInfo fileInfoTo(ui->lineEditTo->text());
    const auto &fileNameTo = fileInfoTo.baseName() + "-FILLED.xlsx";
    const auto &filePathTo = QDir{fileInfoTo.path()}.absoluteFilePath(fileNameTo);
    TemplateMergerFiller.exportTo(filePathTo);
//*/
}

void MainWindow::extractProductInfos()
{
    DialogExtractInfos dialog{m_workingDir.path()};
    dialog.exec();
}
