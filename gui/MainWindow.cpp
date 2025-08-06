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
    ui->progressBar->setVisible(false);
    ui->progressBar->setMaximum(100);
    ui->progressBar->setValue(0);
    m_settingsKeyExtraInfos = "MainWindowExtraInfos";
    m_settingsKeyApi = "MainWindowKey";
    m_templateMergerFiller = nullptr;
    auto settings = WorkingDirectoryManager::instance()->settings();
    if (settings->contains(m_settingsKeyExtraInfos))
    {
        ui->textEditExtraInfos->setText(settings->value(m_settingsKeyExtraInfos).toString());
    }
    if (settings->contains(m_settingsKeyApi))
    {
        auto key = settings->value(m_settingsKeyApi).toString();
        ui->lineEditOpenAiKey->setText(key);
    }
    _connectslots();
}

MainWindow::~MainWindow()
{
    delete ui;
    delete m_templateMergerFiller;
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
        QString{"Xlsx (*TOFILL*.xlsx *TOFILL*.XLSX *TOFILL*.xlsm *TOFILL*.XLSM)"},
        nullptr,
        QFileDialog::DontUseNativeDialog);
    if (!filePath.isEmpty())
    {
        ui->buttonExtractProductInfos->setEnabled(true);
        m_workingDir = QFileInfo{filePath}.dir();
        const auto &workingDirPath = m_workingDir.path();
        settings.setValue(key, workingDirPath);
        ui->lineEditTo->setText(filePath);
        _enableGenerateButtonIfValid();
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
    connect(ui->buttonViewFormatExtraInfos,
            &QPushButton::clicked,
            this,
            &MainWindow::viewFormatExtraInfosGpt);
    connect(ui->buttonClearPreviousChatGptReplies,
            &QPushButton::clicked,
            this,
            &MainWindow::clearPreviousChatgptReplies);
    connect(ui->lineEditOpenAiKey,
            &QLineEdit::textChanged,
            this,
            &MainWindow::onApiKeyChanged);
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

void MainWindow::_enableGenerateButtonIfValid()
{
    if (!ui->lineEditOpenAiKey->text().isEmpty()
        && !ui->lineEditTo->text().isEmpty())
    {
        ui->buttonGenerate->setEnabled(true);
    }
    else
    {
        ui->buttonGenerate->setEnabled(false);
    }
}

void MainWindow::generate()
{
    if (m_templateMergerFiller != nullptr)
    {
        m_templateMergerFiller->stopChatGPT();
        auto *templateMergerFiller = m_templateMergerFiller;
        QTimer::singleShot(5000, this, [templateMergerFiller]{
            delete templateMergerFiller;
        });
    }
    m_templateMergerFiller = new TemplateMergerFiller{ui->lineEditTo->text(),
                                                      ui->textEditExtraInfos->toPlainText(),
                                                      ui->lineEditOpenAiKey->text(),
                                                      [this](const QString &logMessage)
                                                      {
                                                          displayLog(logMessage);
                                                      }};
    try
    {
        const auto &keywordsFileInfos = m_workingDir.entryInfoList(
            QStringList{"keywor*.txt", "Keywor*.txt"}, QDir::Files, QDir::Name);
        if (keywordsFileInfos.size() == 0)
        {
            QMessageBox::information(this,
                                     tr("Keywords file missing"),
                                     tr("The keywords.txt file is missing"));
        }
        else
        {
            const auto &keywordFilePath = keywordsFileInfos.last().absoluteFilePath();
            ui->progressBar->setVisible(true);
            m_templateMergerFiller->fillExcelFiles(
                keywordFilePath,
                getFileModelSources()->getFilePaths()
                , getFileModelToFill()->getFilePaths()
                , [this](int progress, int max){
                    ui->progressBar->setMaximum(max);
                    ui->progressBar->setValue(progress);
                }
                , [this](){
                    ui->progressBar->hide();
                    QMessageBox::information(
                        this,
                        tr("Files created"),
                        tr("The template files were created successfully"));
                    }
                , [this](const QString &error){
                    ui->progressBar->hide();
                    QMessageBox::information(
                        this,
                        tr("Error"),
                        tr("The template files were not created due to an error like ChatGpt. %1").arg(error));
                    }
                );
        }
    }
    catch (const ExceptionTemplateError &exception)
    {
        QMessageBox::warning(this,
                             exception.title(),
                             exception.error());
    }
}

void MainWindow::clearPreviousChatgptReplies()
{
    TemplateMergerFiller templateMergerFiller{ui->lineEditTo->text(),
                                              ui->textEditExtraInfos->toPlainText(),
                                              ui->lineEditOpenAiKey->text(),
                                              [this](const QString &logMessage)
                                              {
                                                  displayLog(logMessage);
                                              }};
    templateMergerFiller.clearPreviousChatgptReplies();
}

void MainWindow::displayLog(const QString &logMEssage)
{
    //TODO
}

void MainWindow::onApiKeyChanged(const QString &key)
{
    auto settings = WorkingDirectoryManager::instance()->settings();
    if (!key.isEmpty())
    {
        settings->setValue(m_settingsKeyApi, key);
    }
    else if (settings->contains(m_settingsKeyApi))
    {
        settings->remove(m_settingsKeyApi);
    }
    _enableGenerateButtonIfValid();
}

void MainWindow::extractProductInfos()
{
    DialogExtractInfos dialog{m_workingDir.path()};
    dialog.exec();
}

void MainWindow::viewFormatExtraInfosGpt()
{
    QMessageBox::information(
        this,
        tr("Format"),
        tr("Custom instructions for every products\n"
           "[SKUSTART1,SKUSTART2]\nCustom instructions for products that include the skus\n"
           "[SKUSTART3,SKUSTART4]\nCustom instructions for products that include the skus"));
}
