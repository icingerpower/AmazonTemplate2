#include <QFileDialog>
#include <QClipboard>
#include <QApplication>
#include <QMessageBox>
#include <QInputDialog>
#include <QSettings>
#include <QDir>

#include "model/TableInfoExtractor.h"

#include "DialogExtractInfos.h"
#include "ui_DialogExtractInfos.h"

DialogExtractInfos::DialogExtractInfos(
    const QString &workingDir, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::DialogExtractInfos)
{
    ui->setupUi(this);
    ui->tableViewInfos->setModel(new TableInfoExtractor{ui->tableViewInfos});
    ui->tableViewInfos->horizontalHeader()->resizeSection(TableInfoExtractor::IND_TITLE, 300);
    QDir dir{workingDir};
    m_workingDir = workingDir;
    const auto &fileInfos = dir.entryInfoList(
        QStringList{"Mod*.xlsx"}, QDir::Files, QDir::Name);
    if (fileInfos.size() > 0)
    {
        ui->lineEditGtinTemplate->setText(fileInfos.first().absoluteFilePath());
    }
    _connectSlots();
}

void DialogExtractInfos::_connectSlots()
{
    connect(ui->buttonPasteSKUs,
            &QPushButton::clicked,
            this,
            &DialogExtractInfos::pasteSkus);
    connect(ui->buttonPasteTitles,
            &QPushButton::clicked,
            this,
            &DialogExtractInfos::pasteTitles);
    connect(ui->buttonBrowseGtinTemplate,
            &QPushButton::clicked,
            this,
            &DialogExtractInfos::browseGtinTemplateFile);
    connect(ui->buttonFillGtin,
            &QPushButton::clicked,
            this,
            &DialogExtractInfos::fillGtinTemplateFile);
    connect(ui->buttonClear,
            &QPushButton::clicked,
            getTableInfoExtractor(),
            &TableInfoExtractor::clear);
    connect(ui->buttonCopyColumn,
            &QPushButton::clicked,
            this,
            &DialogExtractInfos::copyColumn);
    connect(ui->buttonReadGtin,
            &QPushButton::clicked,
            this,
            &DialogExtractInfos::readGtinCodes);
    connect(ui->buttonGenerateModelNames,
            &QPushButton::clicked,
            this,
            &DialogExtractInfos::generateModelNames);
}

DialogExtractInfos::~DialogExtractInfos()
{
    delete ui;
}

TableInfoExtractor *DialogExtractInfos::getTableInfoExtractor() const
{
    return static_cast<TableInfoExtractor *>(
        ui->tableViewInfos->model());
}

void DialogExtractInfos::browseGtinTemplateFile()
{
    const QString &filePath = QFileDialog::getOpenFileName(
        this
        , tr("GTIN empty template")
        , m_workingDir
        , QString{"Xlsx (*.xlsx *.XLSX)"}
        //, nullptr
        //, QFileDialog::DontUseNativeDialog
        );
    if (!filePath.isEmpty())
    {
        ui->lineEditGtinTemplate->setText(filePath);
    }
}

void DialogExtractInfos::fillGtinTemplateFile()
{
    const auto &brand = ui->lineEditBrand->text();
    if (brand.isEmpty())
    {
        QMessageBox::information(
            this,
            tr("No brand"),
            tr("You need to enter the brand"));
        return;
    }
    const auto &category = ui->lineEditCategoryCode->text();
    if (category.isEmpty())
    {
        QMessageBox::information(
            this,
            tr("No category code"),
            tr("You need to enter the GTIN category code"));
        return;
    }
    const auto &fileGtinTempalteFrom = ui->lineEditGtinTemplate->text();
    auto fileGtinTempalteTo = fileGtinTempalteFrom;
    fileGtinTempalteTo.replace(".xlsx", "FILLED.xlsx");
    getTableInfoExtractor()->fillGtinTemplate(
        fileGtinTempalteFrom
        , fileGtinTempalteTo
        , brand
        , category
        );
}

void DialogExtractInfos::copyColumn()
{
    const auto &selIndexes = ui->tableViewInfos->selectionModel()->selectedIndexes();
    if (selIndexes.size() == 0)
    {
        QMessageBox::information(
            this,
            tr("No sleection"),
            tr("You need to select a column"));
    }
    else
    {
        getTableInfoExtractor()->copyColumn(selIndexes.first());
    }
}

void DialogExtractInfos::pasteSkus()
{
    const QString &error = getTableInfoExtractor()->pasteSKUs();
    if (!error.isEmpty())
    {
        QMessageBox::warning(
            this,
            tr("Erreur"),
            error);
    }
}

void DialogExtractInfos::pasteTitles()
{
    const QString &error = getTableInfoExtractor()->pasteTitles();
    if (!error.isEmpty())
    {
        QMessageBox::warning(
            this,
            tr("Erreur"),
            error);
    }
}

void DialogExtractInfos::generateModelNames()
{
    getTableInfoExtractor()->generateModelNames();
}

void DialogExtractInfos::readGtinCodes()
{
    QDir dir{m_workingDir};
    const auto &fileInfos = dir.entryInfoList(
        QStringList{"Mes-derniers-produits*.xlsx"}, QDir::Files, QDir::Name);
    if (fileInfos.size() > 0)
    {
        const auto &fileInfo = fileInfos.last();
        const auto &GTINs = getTableInfoExtractor()->readGtin(fileInfo.absoluteFilePath());
        if (GTINs.size() == 0)
        {
            QMessageBox::information(
                this,
                tr("No GTIN found"),
                tr("No GTIN could be rode in the report file Mes-derniers-produits*.xlsx"));
        }
        else
        {
            auto clipboard = QApplication::clipboard();
            clipboard->setText(GTINs.join("\n"));
            QMessageBox::information(
                this,
                tr("GTINs copied"),
                tr("%1 GTINs have been copied in the clipboard").arg(GTINs.size()));
        }
    }
    else
    {
        QMessageBox::information(
            this,
            tr("File not found"),
            tr("Can't find the GTIN GS1 report file Mes-derniers-produits*.xlsx"));
    }
}

