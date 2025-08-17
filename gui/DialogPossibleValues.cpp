#include <QMessageBox>
#include <QClipboard>
#include <QApplication>

#include "model/TemplateMergerFiller.h"

#include "DialogPossibleValues.h"
#include "ui_DialogPossibleValues.h"

DialogPossibleValues::DialogPossibleValues(
        TemplateMergerFiller *templateMergerFille, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::DialogPossibleValues)
{
    ui->setupUi(this);
    auto countryCode_langCode_fieldId_possibleValues
            = templateMergerFille->countryCode_langCode_fieldId_possibleValues();
    for (auto itCountryCode = countryCode_langCode_fieldId_possibleValues.begin();
         itCountryCode != countryCode_langCode_fieldId_possibleValues.end(); ++itCountryCode)
    {
        for (auto itLangCode = itCountryCode.value().begin();
             itLangCode != itCountryCode.value().end(); ++itLangCode)
        {
            const auto &countryLang
                    = itCountryCode.key() + "-" + itLangCode.key().toLower();
            m_countryLangs << countryLang;
            for (auto itFieldId = itLangCode.value().begin();
                 itFieldId != itLangCode.value().end(); ++itFieldId)
            {
                const auto &fieldId = itFieldId.key();
                m_fieldIds_countryLang_possibleValues[fieldId][countryLang]
                        = itFieldId.value();
                if (TemplateMergerFiller::MAPPING_FIELD_ID.contains(fieldId))
                {
                    const auto &mappedFieldId = TemplateMergerFiller::MAPPING_FIELD_ID[fieldId];
                    if (mappedFieldId != fieldId)
                    {
                        m_fieldIds_countryLang_possibleValues[mappedFieldId][countryLang]
                                = itFieldId.value();
                    }
                }
            }
        }
    }
    const auto &fieldIds = m_fieldIds_countryLang_possibleValues.keys();
    ui->listWidgetFieldIds->addItems(fieldIds);
    ui->tableWidgetPossibleValues->setColumnCount(m_countryLangs.size());
    ui->tableWidgetPossibleValues->setHorizontalHeaderLabels(m_countryLangs);
    _connectSlots();
}

DialogPossibleValues::~DialogPossibleValues()
{
    delete ui;
}

void DialogPossibleValues::onPromptSelected(
        const QItemSelection &selected, const QItemSelection &)
{
    ui->tableWidgetPossibleValues->clearContents();
    ui->tableWidgetPossibleValues->setRowCount(0);
    if (selected.size() > 0)
    {
        int row = selected.indexes().first().row();
        const auto &fieldId = ui->listWidgetFieldIds->item(row)->text();
        for (int col=0; col<m_countryLangs.size(); ++col)
        {
            const auto &countryLang = m_countryLangs[col];
            if (m_fieldIds_countryLang_possibleValues[fieldId].contains(countryLang))
            {
                const auto &possibleValues = m_fieldIds_countryLang_possibleValues[fieldId][countryLang];
                int curRowCount = ui->tableWidgetPossibleValues->rowCount();
                int rowCount = qMax(curRowCount, possibleValues.size());
                ui->tableWidgetPossibleValues->setRowCount(rowCount);
                for (int i=0; i<possibleValues.size(); ++i)
                {
                    ui->tableWidgetPossibleValues->setItem(
                                i, col, new QTableWidgetItem{possibleValues[i]});
                }
            }
        }
    }
}

void DialogPossibleValues::copyColumn()
{
    const auto &selIndexes = ui->tableWidgetPossibleValues->selectionModel()->selectedIndexes();
    if (selIndexes.size() == 0)
    {
        QMessageBox::information(
            this,
            tr("No sleection"),
            tr("You need to select a column"));
    }
    else
    {
        QStringList columns;
        int col = selIndexes.first().column();
        for (int i=0; i<ui->tableWidgetPossibleValues->rowCount(); ++i)
        {
            auto item = ui->tableWidgetPossibleValues->item(i, col);
            if (item != nullptr)
            {
                columns << item->text();
            }
        }
        auto clipboard = QApplication::clipboard();
        clipboard->setText(columns.join("\n"));
    }
}

void DialogPossibleValues::copyAll()
{
    auto *currentItem = ui->listWidgetFieldIds->currentItem();
    if (currentItem != nullptr)
    {
        const auto &fieldId = currentItem->text();
        QStringList lines;
        lines
              << "- For fieldId \"" + fieldId + "\" with value TODO, tell me, for each country-lang below, the exact value to select."
              << "- Use ONLY one of the allowed values listed for that country-lang (case-insensitive match, but return the exact string)."
              << "- If no good match exists, suggest the value the closest and add a comment with probability of being wrong next to the selected word."
              << ""
              << "Allowed values per country-lang:";

        const auto it = m_fieldIds_countryLang_possibleValues.constFind(fieldId);
        if (it != m_fieldIds_countryLang_possibleValues.constEnd())
        {
            const auto &byCountryLang = it.value();
            for (const auto &countryLang : m_countryLangs)
            {
                const auto &values = byCountryLang.value(countryLang);
                QString line = " - " + countryLang + ": ";
                line += values.isEmpty() ? "(none)" : values.join(", ");
                lines << line;
            }
        }

        lines << ""
              << "Exemple of format:"
              << "pattern_possibleValues.insert("
              << "\"" + fieldId + "\","
              << "QSet<QString>{"
              << "                    \"Parent\""
              << "                    , \"Superior\""
              << "                    , \"Üst Ürün\""
              << "                    , \"Principal\""
              << "                    , \"Ouder\""
              << "                    , \"Ana\" // Comment if needed"
              << "                    , \"Bovenliggend\""
              << "                    , \"Förälder\""
              << "                    , \"Lverordnad\""
              << "}";


        const auto &prompt = lines.join("\n");

        auto clipboard = QApplication::clipboard();
        clipboard->setText(prompt);
    }
    else
    {
        QMessageBox::information(
            this,
            tr("No sleection"),
            tr("You need to select a column"));
    }
}

void DialogPossibleValues::_connectSlots()
{
    connect(ui->listWidgetFieldIds->selectionModel(),
            &QItemSelectionModel::selectionChanged,
            this,
            &DialogPossibleValues::onPromptSelected);
    connect(ui->buttonCopyColumn,
            &QPushButton::clicked,
            this,
            &DialogPossibleValues::copyColumn);
    connect(ui->buttonCopyAll,
            &QPushButton::clicked,
            this,
            &DialogPossibleValues::copyAll);
}
