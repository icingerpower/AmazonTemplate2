#include <QHash>
#include <QFile>

#include <xlsxcell.h>
#include <xlsxcellrange.h>

#include "model/ColMapping.h"

#include "TemplateMerger.h"


const QStringList TemplateMerger::SHEETS_TEMPLATE{
        "Template",
        "Vorlage",
        "Modèle",
        "Sjabloon",
        "Mall",
        "Szablon",
        "Plantilla",
        "Modello",
        "Şablon"
    };

const QStringList TemplateMerger::SHEETS_VALID_VALUES{
    "Valeurs valides"
    , "Valid Values"
    , "Valori validi"
    , "Geldige waarden"
    , "Gültige Werte"
    , "Giltiga värden"
    , "Valores válidos"
    , "Poprawne wartości"
};

const QHash<QString, QString> TemplateMerger::SHEETS_MANDATORY{
    {"Définitions des données", "Obligatoire"}
    , {"Data Definitions", "Required"}
    , {"Datendefinitionen", "Erforderlich"}
    , {"Definizioni dati", "Obbligatorio"}
    , {"Gegevensdefinities", "Verplicht"}
    , {"Definitioner av data", "Krävs"}
    , {"Veri Tanımları", "Zorunlu"}
    , {"Definicje danych", "Wymagane"}
};

TemplateMerger::TemplateMerger(const QString &filePathTo)
{
    m_filePathTo = filePathTo;
}

void TemplateMerger::selectTemplateSheet(QXlsx::Document &doc)
{
    // List of candidate sheet names for different locales:
    // English, French (FR/BE), Dutch, Swedish, Polish, Spanish, Italian, Turkish.
    QStringList candidateSheetNames = {
        "Template",
        "Vorlage",
        "Modèle",
        "Sjabloon",
        "Mall",
        "Szablon",
        "Plantilla",
        "Modello",
        "Şablon"
    };

    bool sheetSelected = false;
    for (const QString &sheetName : candidateSheetNames) {
        if (doc.selectSheet(sheetName)) {
            sheetSelected = true;
            break;
        }
    }

    if (!sheetSelected) {
        // If none of the candidate names are found,
        // try selecting the 5th sheet (index 4) if it exists.
        QStringList sheets = doc.sheetNames();
        if (sheets.size() >= 5) {
            doc.selectSheet(sheets.at(4)); // 5th sheet
        } else if (!sheets.isEmpty()) {
            // Fallback: select the first sheet available.
            doc.selectSheet(sheets.first());
        }
    }
}

/*
void TemplateMerger::exportTo(const QString &filePath)
{
    struct FromInfo{
        QStringList colNames; //Each col names of the third row from the template sheet
        QHash<QString, int> nameToIndex; // Col name indexes
        QList<QStringList> lines; //Each celle value of each line
    };
    QList<FromInfo> fromInfos;
    const QHash<QString, QString> &colNameFromTo
        = ColMapping::instance()->colNameFromTo();

    // Process each old template file.
    for (const auto &filePathFrom : m_filePathsFrom) {
        QXlsx::Document xlsx(filePathFrom);
        selectTemplateSheet(xlsx);
        QXlsx::CellRange range = xlsx.dimension();
        if (!range.isValid())
            continue; // Skip if file is empty or invalid.

        const int headerRow = 3;  // Assume header is in row 3.
        FromInfo info;

        // Read header row and update each header name if a mapping exists.
        for (int col = range.firstColumn(); col <= range.lastColumn(); ++col) {
            auto cell = xlsx.cellAt(headerRow, col);
            QString headerValue = cell ? cell->value().toString() : QString();

            // Update header if the mapping defines an updated name.
            if (colNameFromTo.contains(headerValue))
                headerValue = colNameFromTo.value(headerValue);

            info.colNames << headerValue;
            // Save the index relative to the header row (0-based).
            info.nameToIndex[headerValue] = col - range.firstColumn();
        }

        // Read data rows (starting from the row after the header).
        for (int row = headerRow + 1; row <= range.lastRow(); ++row) {
            bool allEmpty = true;
            QStringList rowData;
            for (int col = range.firstColumn(); col <= range.lastColumn(); ++col) {
                auto cell = xlsx.cellAt(row, col);
                rowData << (cell ? cell->value().toString() : QString());
                if (!rowData.last().trimmed().isEmpty())
                {
                    allEmpty = false;
                }
            }
            if (allEmpty)
            {
                break;
            }
            info.lines.append(rowData);
        }
        fromInfos.append(info);
    }

    // Open the latest template file (m_filePathTo) to use its header and formatting.
    QXlsx::Document newDoc(m_filePathTo);
    selectTemplateSheet(newDoc);
    QXlsx::CellRange rangeTo = newDoc.dimension();
    const int headerRowTo = 3;  // Assume the new template header is in row 3.
    QStringList colNamesTo;
    for (int col = rangeTo.firstColumn(); col <= rangeTo.lastColumn(); ++col) {
        auto cell = newDoc.cellAt(headerRowTo, col);
        QString headerValue = cell ? cell->value().toString() : QString();
        colNamesTo << headerValue;
    }

    // Determine where to start writing data rows (immediately after the header row).
    int currentRow = headerRowTo + 1;

    // For each file's data, map the (now updated) old columns to the new template columns.
    for (const FromInfo &info : fromInfos) {
        for (const QStringList &line : info.lines) {
            // Create a new row matching the new template columns.
            QStringList newRow;
            newRow.resize(colNamesTo.size());
            for (int i = 0; i < colNamesTo.size(); ++i) {
                const QString &colName = colNamesTo.at(i);
                // If the old file has this column (by its updated name), copy its value.
                if (info.nameToIndex.contains(colName)) {
                    int oldIndex = info.nameToIndex[colName];
                    if (oldIndex >= 0 && oldIndex < line.size())
                        newRow[i] = line.at(oldIndex);
                }
                // Otherwise, leave the cell empty.
            }
            // Write the merged row into newDoc.
            for (int col = 0; col < newRow.size(); ++col) {
                newDoc.write(currentRow, col + 1, newRow.at(col));
            }
            ++currentRow;
        }
    }

    // Save the new document (with merged data) to the target file.
    newDoc.saveAs(filePath);
}
//*/

