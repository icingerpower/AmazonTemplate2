#include <QHash>
#include <QFile>
#include <QFileInfo>
#include <QObject>

#include <xlsxcell.h>
#include <xlsxcellrange.h>

#include "ExceptionTemplateError.h"

#include "TemplateMergerFiller.h"


const QStringList TemplateMergerFiller::SHEETS_TEMPLATE{
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

const QStringList TemplateMergerFiller::SHEETS_VALID_VALUES{
    "Valeurs valides"
    , "Valid Values"
    , "Valori validi"
    , "Geldige waarden"
    , "Gültige Werte"
    , "Giltiga värden"
    , "Valores válidos"
    , "Poprawne wartości"
};

const QHash<QString, QString> TemplateMergerFiller::SHEETS_MANDATORY{
    {"Définitions des données", "Obligatoire"}
    , {"Data Definitions", "Required"}
    , {"Datendefinitionen", "Erforderlich"}
    , {"Definizioni dati", "Obbligatorio"}
    , {"Gegevensdefinities", "Verplicht"}
    , {"Definitioner av data", "Krävs"}
    , {"Veri Tanımları", "Zorunlu"}
    , {"Definicje danych", "Wymagane"}
};

const QSet<QString> TemplateMergerFiller::FIELD_IDS_NOT_AI{
    "feed_product_type"
    , "item_sku"
    , "brand_name"
    , "external_product_id"
    , "external_product_id_type"
    , "recommended_browse_nodes"
    , "manufacturer"
    , "standard_price"
    , "parent_child"
    , "parent_sku"
};

const QSet<QString> TemplateMergerFiller::FIELD_IDS_PUT_FIRST_VALUE{
    "feed_product_type"
    , "fulfillment_center_id"
    , "recommended_browse_nodes"
};

TemplateMergerFiller::FuncFiller TemplateMergerFiller::FUNC_FILLER_COPY
    = [](const QString &,
         const QString &,
         Gender,
         Age,
         const QVariant &origValue) -> QVariant{
    return origValue;
};

TemplateMergerFiller::FuncFiller TemplateMergerFiller::FUNC_FILLER_CONVERT_CLOTHE_SIZE
    = [](const QString &countryFrom,
         const QString &countryTo,
         Gender targetGender,
         Age age_range_description,
         const QVariant &origValue) -> QVariant{
    bool isNum = false;
    int num = origValue.toInt(&isNum);
    static const QList<QHash<QString, int>> list_countryCode_size
    = [targetGender, age_range_description]() -> QList<QHash<QString, int>>
    {
        QList<QHash<QString, int>> _list_countryCode_size;
        if (targetGender == Gender::UndefinedGender
            || age_range_description == Age::UndefinedAge)
        {
            ExceptionTemplateError exception;
            exception.setInfos(QObject::tr("No gender / age"),
                               QObject::tr("The gender and/or age_range_description was not defined in the template"));
            exception.raise();
        }
        if (targetGender == Gender::Female
            && age_range_description == Age::Adult)
        {
            _list_countryCode_size = QList<QHash<QString, int>> {
                {{{"FR", 32}
                    , {"BE", 32}
                    , {"ES", 32}
                    , {"TR", 32}
                    , {"DE", 30}
                    , {"NL", 30}
                    , {"SE", 30}
                    , {"PL", 30}
                    , {"IT", 36}
                    , {"IR", 4} // Irland
                    , {"UK", 4}
                    , {"AU", 4}
                    , {"COM", 0}
                    , {"CA", 0}
                    , {"JP", 5}
                    , {"AE", 0}
                    , {"MX", 0}
                    , {"SA", 0}
                    , {"SG", 0}
                }}
            };
        }
        else if (targetGender == Gender::Male
                 && age_range_description == Age::Adult)
        {
            _list_countryCode_size = QList<QHash<QString, int>> {
                {{{"FR", 36}
                    , {"BE", 36}
                    , {"ES", 36}
                    , {"TR", 36}
                    , {"DE", 36}
                    , {"NL", 36}
                    , {"SE", 36}
                    , {"PL", 36}
                    , {"IT", 36}
                    , {"IR", 26} // Irland
                    , {"UK", 26}
                    , {"AU", 26}
                    , {"COM", 26}
                    , {"CA", 26}
                    , {"JP", 28}
                    , {"AE", 26}
                    , {"MX", 26}
                    , {"SA", 26}
                    , {"SG", 26}
                }}
            };
        }
        else
        {
            Q_ASSERT(false);
        }
        for (int i=2; i<20; i+=2)
        {
            QHash<QString, int> curSizes;
            for (auto it = _list_countryCode_size[0].begin();
                 it != _list_countryCode_size[0].end(); ++it)
            {
                curSizes[it.key()] = it.value() + i;
            }
            _list_countryCode_size << curSizes;
        }
        return _list_countryCode_size;
    }();
    if (isNum)
    {
        for (const auto &countryCode_size : list_countryCode_size)
        {
            Q_ASSERT(countryCode_size.contains(countryTo)
                     && countryCode_size.contains(countryFrom));
            if (countryCode_size[countryFrom] == num)
            {
                return countryCode_size[countryTo];
            }
        }
    }
    return origValue;
};

TemplateMergerFiller::FuncFiller TemplateMergerFiller::FUNC_FILLER_CONVERT_SHOE_SIZE
    = [](const QString &countryFrom,
         const QString &countryTo,
         Gender targetGender,
         Age age_range_description,
         const QVariant &origValue) -> QVariant{
    bool isNum = false;
    double num = 0.;
    const auto &origValueString = origValue.toString();
    if (origValueString.contains(" "))
    {
        auto sizeElements = origValueString.split(" ");
        if (sizeElements.first().contains("CN", Qt::CaseInsensitive))
        {
            sizeElements << sizeElements.takeFirst();
        }
        for (const auto &sizeElement : sizeElements)
        {
            num = sizeElement.toDouble(&isNum);
            if (isNum)
            {
                break;
            }
        }
    }
    else
    {
        num = origValueString.toDouble(&isNum);
    }
    if (isNum)
    {
        static const QList<QHash<QString, double>> list_countryCode_size
            = [targetGender, age_range_description]() -> QList<QHash<QString, double>>
        {
            QList<QHash<QString, double>> _list_countryCode_size;
            if (targetGender == Gender::UndefinedGender
                || age_range_description == Age::UndefinedAge)
            {
                ExceptionTemplateError exception;
                exception.setInfos(QObject::tr("No gender / age"),
                                   QObject::tr("The gender and/or age_range_description was not defined in the template"));
                exception.raise();
            }
            if (targetGender == Gender::Female
                && age_range_description == Age::Adult)
            {
                QSet<QString> groupEu{"FR", "BE", "ES", "IT", "DE", "NL", "SE", "PL", "TR"};
                QSet<QString> groupUs{"COM", "CA", "MX", "SA", "SG", "AE"};
                QSet<QString> groupUk{"UK", "IR", "AU"};
                QSet<QString> groupJp{"Jp"};
                QSet<double> corEu_other{40., 41., 43., 44.};
                double firstSizeEu = 34.;
                double curSizeUs = 3.-1.;
                double curSizeUk = 1.-1.;
                double curSizeJp = 20.-1.;
                for (double curSizeEu=firstSizeEu; curSizeEu<55; ++curSizeEu)
                {
                    QHash<QString, double> countrycode_size;
                    for (const auto &countryCode : groupEu)
                    {
                        countrycode_size[countryCode] = curSizeEu;
                    }
                    double otherToAdd = corEu_other.contains(curSizeEu) ? 0.5 : 1.;
                    curSizeUs += otherToAdd;
                    curSizeUk += otherToAdd;
                    curSizeJp += otherToAdd;
                    for (const auto &countryCode : groupUs)
                    {
                        countrycode_size[countryCode] = curSizeUs;
                    }
                    for (const auto &countryCode : groupUk)
                    {
                        countrycode_size[countryCode] = curSizeEu;
                    }
                    for (const auto &countryCode : groupJp)
                    {
                        countrycode_size[countryCode] = curSizeJp;
                    }
                    _list_countryCode_size << countrycode_size;
                }
            }
            else if (targetGender == Gender::Male
                     && age_range_description == Age::Adult)
            {
                QSet<QString> groupEu{"FR","BE","ES","IT","DE","NL","SE","PL","TR"};
                QSet<QString> groupUs{"COM","CA","MX","SA","SG","AE"};
                QSet<QString> groupUk{"UK","IR","AU"};
                QSet<QString> groupJp{"Jp"};

                double firstSizeEu = 38.;
                // US at EU38 = 38−33 = 5
                double curSizeUs = 5.0 - 1.0;
                // UK at EU38 = 38−34 = 4
                double curSizeUk = 4.0 - 1.0;
                // JP at EU38 = 0.5*38+5 = 24
                double curSizeJp = 24.0 - 0.5;

                for (double curSizeEu = firstSizeEu; curSizeEu < 55; ++curSizeEu)
                {
                    QHash<QString, double> countrycode_size;
                    // EU sizes step by 1
                    for (const auto &c : groupEu)
                        countrycode_size[c] = curSizeEu;
                    // increment each map
                    curSizeUs += 1.0;   // US & friends step by 1
                    curSizeUk += 1.0;   // UK/AU/IR step by 1
                    curSizeJp += 0.5;   // JP steps by 0.5 (cm)
                    // US‑group
                    for (const auto &c : groupUs)
                        countrycode_size[c] = curSizeUs;
                    // UK‑group
                    for (const auto &c : groupUk)
                        countrycode_size[c] = curSizeUk;
                    // JP
                    for (const auto &c : groupJp)
                        countrycode_size[c] = curSizeJp;
                    _list_countryCode_size << countrycode_size;
                }
            }
            else
            {
                Q_ASSERT(false);
            }
            return _list_countryCode_size;
        }();
        for (const auto &countryCode_size : list_countryCode_size)
        {
            Q_ASSERT(countryCode_size.contains(countryTo)
                     && countryCode_size.contains(countryFrom));
            if (qAbs(countryCode_size[countryFrom] - num) < 0.0001)
            {
                return countryCode_size[countryTo];
            }
        }
    }
    return origValue;
};

const QHash<QString, TemplateMergerFiller::FuncFiller>
    TemplateMergerFiller::FIELD_IDS_FILLER_NO_SOURCES
{
    {"parent_child", FUNC_FILLER_COPY}
    , {"parent_sku", FUNC_FILLER_COPY}
    , {"relationship_type", FUNC_FILLER_COPY}
    , {"variation_theme", FUNC_FILLER_COPY}
};

const QHash<QString, TemplateMergerFiller::FuncFiller> TemplateMergerFiller::FIELD_IDS_FILLER
{
    {"apparel_size", FUNC_FILLER_CONVERT_CLOTHE_SIZE}
    , {"apparel_size_to", FUNC_FILLER_CONVERT_CLOTHE_SIZE}
    , {"footwear_size", FUNC_FILLER_CONVERT_SHOE_SIZE}
    , {"footwear_to_size", FUNC_FILLER_CONVERT_SHOE_SIZE}
    , {"shapewear_size", FUNC_FILLER_CONVERT_CLOTHE_SIZE}
    , {"shapewear_size_to", FUNC_FILLER_CONVERT_CLOTHE_SIZE}
    , {"package_length", FUNC_FILLER_COPY}
    , {"package_width", FUNC_FILLER_COPY}
    , {"package_height", FUNC_FILLER_COPY}
    , {"package_weight", FUNC_FILLER_COPY}
};

const QHash<QString, QSet<QString>> TemplateMergerFiller::FIELD_IDS_COPY_FROM_OTHER
    {
        {"size_map", {"apparel_size", "footwear_size", "shapewear_size"}}
};


const QSet<QString> TemplateMergerFiller::VALUES_MANDATORY
    = []() -> QSet<QString>
{
    QSet<QString> values;
    for (auto it = SHEETS_MANDATORY.begin();
         it != SHEETS_MANDATORY.end(); ++it)
    {
        values.insert(it.value());
    }
    return values;
}();

const QString TemplateMergerFiller::ID_SKU{"item_sku"};

TemplateMergerFiller::TemplateMergerFiller(const QString &filePathFrom)
{
    m_filePathFrom = filePathFrom;
    m_age = UndefinedAge;
    m_gender = UndefinedGender;
}

void TemplateMergerFiller::fillExcelFiles(
    const QStringList &sourceFilePaths, const QStringList &toFillFilePaths)
{
    setFilePathsToFill(toFillFilePaths);
    if (!sourceFilePaths.isEmpty())
    {
        readInfoSources(sourceFilePaths);
    }
    fillDataAutomatically();
    fillDataLeftChatGpt();
    createToFillXlsx();
}

void TemplateMergerFiller::readSkus(
    const QString &countryCode,
    QStringList &skus,
    QHash<QString, QHash<QString, QHash<QString, QVariant> > > &sku_countryCode_fieldId_origValue)
{
    QXlsx::Document document(m_filePathFrom);
    _selectTemplateSheet(document);
    const auto &fieldId_index = _get_fieldId_index(document);
    Q_ASSERT(fieldId_index.contains(ID_SKU));
    int indColSku = fieldId_index[ID_SKU];
    const auto &dim = document.dimension();
    QHash<QString, qsizetype> fieldId_count;
    int nParents = 0;;
    for (int i=4; i<dim.lastRow(); ++i)
    {
        auto cellSku = document.cellAt(i+1, indColSku + 1);
        if (!cellSku)
        {
            break;
        }
        QString sku{cellSku->value().toString()};
        if (sku.isEmpty())
        {
            break;
        }
        skus << sku;
        if (sku.startsWith("P-"))
        {
            ++nParents;
        }
        for (auto it = fieldId_index.cbegin();
             it != fieldId_index.cend(); ++it)
        {
            const auto &fieldId = it.key();
            int indCol = it.value();
            if (indCol != indColSku)
            {
                auto cell = document.cellAt(i+1, it.value() + 1);
                if (cell)
                {
                    const auto &valueVariant = cell->value();
                    QString valueString{valueVariant.toString()};
                    if (fieldId == "target_gender")
                    {
                        static QSet<QString> women{"Féminin", "Female"};
                        static QSet<QString> men{"Masculin", "Male"};
                        static QSet<QString> unisex{"Unisexe"};
                        if (women.contains(valueString))
                        {
                            m_gender = Gender::Female;
                        }
                        else if (men.contains(valueString))
                        {
                            m_gender = Gender::Male;
                        }
                        else if (unisex.contains(valueString))
                        {
                            m_gender = Gender::Unisex;
                        }
                        else if (!valueString.isEmpty())
                        {
                            Q_ASSERT(false);
                        }
                    }
                    else if (fieldId == "age_range_description")
                    {
                        static QSet<QString> adult{"Adult", "Adulte"};
                        static QSet<QString> kid{"Enfant", "Big Kid"};
                        static QSet<QString> baby{"Infant", "Toddler", "Little Kid", "Nourisson", "Tout-petit"};
                        if (adult.contains(valueString))
                        {
                            m_age = Age::Adult;
                        }
                        else if (kid.contains(valueString))
                        {
                            m_age = Age::kid;
                        }
                        else if (baby.contains(valueString))
                        {
                            m_age = Age::baby;
                        }
                        else if (!valueString.isEmpty())
                        {
                            Q_ASSERT(false);
                        }
                    }
                    if (!valueString.isEmpty())
                    {
                        ++fieldId_count[fieldId];
                        sku_countryCode_fieldId_origValue[sku][countryCode][fieldId] = valueVariant;
                    }
                }
            }
        }
    }
    fieldId_count[ID_SKU] = m_skus.size();
    auto valid1 = m_skus.size();
    auto valid2 = m_skus.size()-nParents;
    const QSet<qsizetype> validCounts{valid1, valid2};
    for (const auto &fieldId : FIELD_IDS_NOT_AI)
    {
        if (fieldId_index.contains(fieldId))
        {
            qsizetype curCount = fieldId_count.value(fieldId, 0);
            if (!validCounts.contains(curCount))
            {
                ExceptionTemplateError exception;
                exception.setInfos(QObject::tr("Informations missing"),
                                   QObject::tr("The field %1 has only %2 values. The correct number should be either %3 or %4.")
                                       .arg(
                                           fieldId,
                                           QString::number(curCount),
                                           QString::number(valid1),
                                           QString::number(valid2))
                                   );
            }
        }
    }
}

void TemplateMergerFiller::setFilePathsToFill(
    const QStringList &toFillFilePaths)
{
    m_toFillFilePaths = toFillFilePaths;
    m_countryCode_fieldIdMandatory.clear();
    m_countryCode_fieldName_fieldId.clear();
    m_sku_countryCode_fieldId_origValue.clear();
    m_skus.clear();
    for (const auto &toFillFilePath : toFillFilePaths)
    {
        const auto &countryCode = _getCountryCode(toFillFilePath);
        QXlsx::Document document(toFillFilePath);
        readSkus(countryCode, m_skus, m_sku_countryCode_fieldId_origValue);
        _readFields(document, countryCode);
        _readMandatory(document, countryCode);
        _readValidValues(document, countryCode);
        _preFillChildOny();
    }
}

void TemplateMergerFiller::readInfoSources(const QStringList &sourceFilePaths)
{
    QHash<QString, QHash<QString, QHash<QString, QVariant>>> sku_countryCode_fieldId_origValue;
    for (const auto &sourceFilePath : sourceFilePaths)
    {
        const auto &countryCode = _getCountryCode(sourceFilePath);
        QXlsx::Document document(sourceFilePath);
        QStringList skus;
        readSkus(countryCode, skus, sku_countryCode_fieldId_origValue);
    }
    for (auto itSku = sku_countryCode_fieldId_origValue.begin();
         itSku != sku_countryCode_fieldId_origValue.end(); ++itSku)
    {
        const auto &sku = itSku.key();
        for (auto itCountryCode = itSku.value().begin();
             itCountryCode != itSku.value().end(); ++itCountryCode)
        {
            const auto &countryCode = itCountryCode.key();
            for (auto itFieldId_value = itCountryCode.value().begin();
                 itFieldId_value != itCountryCode.value().end(); ++itFieldId_value)
            {
                const auto &fieldId = itFieldId_value.key();
                if (!FIELD_IDS_FILLER_NO_SOURCES.contains(fieldId)
                    && !FIELD_IDS_PUT_FIRST_VALUE.contains(fieldId)
                         && m_countryCode_fieldIdMandatory[countryCode].contains(fieldId))
                {
                    m_sku_countryCode_fieldId_value[sku][countryCode][fieldId] = itFieldId_value.value();
                }
            }
        }
    }
}

void TemplateMergerFiller::fillDataAutomatically()
{
    //m_sku_countryCode_fieldId_value;
    int row = 3;
    const auto &countryCodeFrom = _getCountryCode(m_filePathFrom);
    for (const auto &toFillFilePath : m_toFillFilePaths)
    {
        QXlsx::Document document(toFillFilePath);
        _selectTemplateSheet(document);
        const auto &countryCodeTo = _getCountryCode(toFillFilePath);
        const auto &fieldId_index = _get_fieldId_index(document);
        const auto &fieldIdsMandatory = m_countryCode_fieldIdMandatory[countryCodeTo];
        for (const auto &sku : m_skus)
        {
            for (const auto &fieldId : fieldIdsMandatory)
            {
                if (fieldId_index.contains(fieldId))
                {
                    if (!(m_sku_countryCode_fieldId_origValue[sku].contains(countryCodeTo)
                          && m_sku_countryCode_fieldId_origValue[sku][countryCodeTo].contains(fieldId)))
                    {
                        QVariant origValue;
                        if (m_sku_countryCode_fieldId_origValue[sku][countryCodeFrom].contains(fieldId))
                        {
                            origValue = m_sku_countryCode_fieldId_origValue[sku][countryCodeFrom][fieldId];
                        }
                        if (FIELD_IDS_FILLER_NO_SOURCES.contains(fieldId))
                        {
                            const auto &filler = FIELD_IDS_FILLER_NO_SOURCES[fieldId];
                            m_sku_countryCode_fieldId_value[sku][countryCodeTo][fieldId]
                                = filler(countryCodeFrom, countryCodeTo, m_gender, m_age, origValue);
                        }
                        else if (FIELD_IDS_PUT_FIRST_VALUE.contains(fieldId))
                        {
                            Q_ASSERT(m_countryCode_fieldId_possibleValues[countryCodeTo].contains(fieldId));
                            const auto &validValues = m_countryCode_fieldId_possibleValues[countryCodeTo][fieldId];
                            m_sku_countryCode_fieldId_value[sku][countryCodeTo][fieldId] = validValues[0];
                        }
                        else if (!m_sku_countryCode_fieldId_value[sku][countryCodeTo].contains(fieldId))
                        {
                            if (FIELD_IDS_FILLER.contains(fieldId))
                            {
                                const auto &filler = FIELD_IDS_FILLER[fieldId];
                                m_sku_countryCode_fieldId_value[sku][countryCodeTo][fieldId]
                                    = filler(countryCodeFrom, countryCodeTo, m_gender, m_age, origValue);
                            }
                        }
                    }
                }
            }
            for (const auto &fieldId : fieldIdsMandatory)
            {
                if (fieldId_index.contains(fieldId)
                    && FIELD_IDS_COPY_FROM_OTHER.contains(fieldId))
                {
                    const auto &otherFieldIds = FIELD_IDS_COPY_FROM_OTHER[fieldId];
                    bool otherFound = false;
                    for (const auto &otherFieldId : otherFieldIds)
                    {
                        if (m_sku_countryCode_fieldId_value[sku][countryCodeTo].contains(fieldId))
                        {
                            m_sku_countryCode_fieldId_value[sku][countryCodeTo][fieldId]
                                = m_sku_countryCode_fieldId_value[sku][countryCodeTo][otherFieldId];
                            otherFound = true;
                            break;
                        }
                    }
                    if (!otherFound) // Fallback on converting from orig data
                    {
                        for (const auto &otherFieldId : otherFieldIds)
                        {
                            if (m_sku_countryCode_fieldId_origValue[sku][countryCodeFrom].contains(fieldId))
                            {
                                if (FIELD_IDS_FILLER.contains(otherFieldId))
                                {
                                    if (m_sku_countryCode_fieldId_origValue[sku][countryCodeFrom].contains(fieldId))
                                    {
                                        QVariant origValue = m_sku_countryCode_fieldId_origValue[sku][countryCodeFrom][fieldId];
                                        const auto &filler = FIELD_IDS_FILLER_NO_SOURCES[otherFieldId];
                                        m_sku_countryCode_fieldId_value[sku][countryCodeTo][fieldId]
                                            = filler(countryCodeFrom, countryCodeTo, m_gender, m_age, origValue);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

void TemplateMergerFiller::fillDataLeftChatGpt()
{
    // Product is new, without any batterie, as shown on the image, from china and with french metric system
}

void TemplateMergerFiller::createToFillXlsx()
{
    for (const auto &toFillFilePath : m_toFillFilePaths)
    {
        const auto &countryCode = _getCountryCode(toFillFilePath);
        QXlsx::Document document(toFillFilePath);
        _selectTemplateSheet(document);
        const auto &fieldId_index = _get_fieldId_index(document);
        int row = 3;
        for (const auto &sku : m_skus)
        {
            const auto &fieldId_value = m_sku_countryCode_fieldId_value[sku][countryCode];
            for (auto it = fieldId_value.begin();
                 it != fieldId_value.end(); ++it)
            {
                const auto &fieldId = it.key();
                if (fieldId_index.contains(fieldId))
                {
                    int colInd = fieldId_index[fieldId];
                    document.write(row + 1, colInd + 1, it.value());
                }
            }
            ++row;
        }
        QString toFillFilePathNew{toFillFilePath};
        toFillFilePathNew.replace("TOFILL", "FILLED.xlsx");
        Q_ASSERT(toFillFilePathNew != toFillFilePath);
        if (toFillFilePathNew != toFillFilePath)
        {
            document.saveAs(toFillFilePathNew);
        }
    }
}

void TemplateMergerFiller::_readFields(
    QXlsx::Document &document, const QString &countryCode)
{
    _selectTemplateSheet(document);
    auto dimTemplate = document.dimension();
    for (int i = 0; i < dimTemplate.lastColumn(); ++i)
    {
        auto cellFieldName = document.cellAt(2, i + 1);
        auto cellFieldId = document.cellAt(3, i + 1);
        if (cellFieldId && cellFieldName)
        {
            QString fieldId{cellFieldId->value().toString()};
            QString fieldName{cellFieldName->value().toString()};
            m_countryCode_fieldName_fieldId[countryCode][fieldName] = fieldId;
        }
    }
}

void TemplateMergerFiller::_readMandatory(
    QXlsx::Document &document, const QString &countryCode)
{

    _selectMandatorySheet(document);
    auto dimMandatory = document.dimension();
    const int colIndFieldName = 2;
    int colIndMandatory = 0;
    for (int j=0; j<10; ++j)
    {
        auto cell = document.cellAt(2, j + 1);
        if (cell)
        {
            colIndMandatory = j;
        }
        else
        {
            break;
        }
    }
    for (int i = 3; i<dimMandatory.lastRow(); ++i)
    {
        auto cellFieldName = document.cellAt(i+1, colIndFieldName + 1);
        auto cellMandatory = document.cellAt(i+1, colIndMandatory + 1);
        if (cellFieldName && cellMandatory)
        {
            QString fieldName{cellFieldName->value().toString()};
            QString mandatory{cellMandatory->value().toString()};
            if (cellFieldName && cellMandatory && fieldName != mandatory)
            {
                if (VALUES_MANDATORY.contains(mandatory))
                {
                    Q_ASSERT(m_countryCode_fieldName_fieldId[countryCode].contains(fieldName));
                    const auto &fieldId = m_countryCode_fieldName_fieldId[countryCode][fieldName];
                    m_countryCode_fieldIdMandatory[countryCode].insert(fieldId);
                }
            }
        }
    }
}

void TemplateMergerFiller::_readValidValues(
    QXlsx::Document &document, const QString &countryCode)
{
    _selectValidValuesSheet(document);
    const auto &dimValidValues = document.dimension();
    for (int i=1; i<dimValidValues.lastRow(); ++i)
    {
        auto cellFieldName = document.cellAt(i+1, 2);
        if (cellFieldName)
        {
            QString fieldName{cellFieldName->value().toString()};
            if (fieldName.contains(" - "))
            {
                fieldName = fieldName.split(" - ")[0];
            }
            if (m_countryCode_fieldName_fieldId[countryCode].contains(fieldName))
            {
                const auto &fieldId = m_countryCode_fieldName_fieldId[countryCode][fieldName];
                for (int j=2; i<dimValidValues.lastColumn(); ++j)
                {
                    auto cellValue = document.cellAt(i+1, j+1);
                    QString value;
                    if (cellValue)
                    {
                        value = cellValue->value().toString();
                        if (!value.isEmpty())
                        {
                            m_countryCode_fieldId_possibleValues[countryCode][fieldId] << value;
                        }
                    }
                    if (value.isEmpty())
                    {
                        break;
                    }
                }
            }
        }
    }
}

void TemplateMergerFiller::_preFillChildOny()
{
    static QSet<QString> fieldIdsChildOnly{"apparel_size_class"
                                           , "apparel_size"
                                           , "apparel_size_to"
                                           , "apparel_body_type"
                                           , "apparel_height_type"
                                           , "size_map"
                                           , "fulfillment_center_id"
                                           , "package_length"
                                           , "package_width"
                                           , "package_height"
                                           , "package_length_unit_of_measure"
                                           , "package_weight"
                                           , "package_weight_unit_of_measure"
                                           , "package_height_unit_of_measure"
                                           , "package_width_unit_of_measure"
                                           , "batteries_required"
                                           , "supplier_declared_dg_hz_regulation1"
                                           , "condition_type"
                                           , "currency"
                                           , "list_price_with_tax"
                                           , "heel_height"
                                           , "footwear_size_system"
                                           , "footwear_age_group"
                                           , "footwear_gender"
                                           , "footwear_size_class"
                                           , "footwear_width"
                                           , "footwear_size"
                                           , "footwear_to_size"
                                           , "target_gender"
                                           , "age_range_description"
                                           , "shapewear_size_system"
                                           , "shapewear_size_class"
                                           , "shapewear_size"
                                           , "shapewear_size_to"
                                           , "shapewear_body_type"
                                           , "shapewear_height_type"
    };
    for (auto itCountry = m_countryCode_fieldIdMandatory.begin();
         itCountry != m_countryCode_fieldIdMandatory.end(); ++itCountry)
    {
        const auto &fieldIds = itCountry.value();
        for (const auto &fieldId : fieldIds)
        {
            if (fieldIdsChildOnly.contains(fieldId))
            {
                m_countryCode_fieldIdChildOnly[itCountry.key()].insert(fieldId);
            }
        }
    }
}

QString TemplateMergerFiller::_getCountryCode(
    const QString &templateFilePath) const
{
    QStringList elements = QFileInfo{templateFilePath}.baseName().split("-");
    bool isNum = false;
    while (true)
    {
        elements.last().toInt(&isNum);
        if (isNum || elements.last().toUpper() != elements.last())
        {
            elements.takeLast();
        }
        else
        {
            break;
        }
    }
    return elements.last();
}

void TemplateMergerFiller::_selectTemplateSheet(QXlsx::Document &doc)
{
    bool sheetSelected = false;
    for (const QString &sheetName : SHEETS_TEMPLATE)
    {
        if (doc.selectSheet(sheetName))
        {
            sheetSelected = true;
            break;
        }
    }
    Q_ASSERT(sheetSelected);

    if (!sheetSelected)
    {
        QStringList sheets = doc.sheetNames();
        if (sheets.size() >= 5)
        {
            doc.selectSheet(sheets.at(4)); // 5th sheet
        }
        else
        {
            Q_ASSERT(false);
            doc.selectSheet(sheets.first());
        }
    }
}

void TemplateMergerFiller::_selectMandatorySheet(QXlsx::Document &doc)
{
    bool sheetSelected = false;
    for (auto it = SHEETS_MANDATORY.begin();
         it != SHEETS_MANDATORY.end(); ++it)
    {
        if (doc.selectSheet(it.key()))
        {
            sheetSelected = true;
            break;
        }
    }
    Q_ASSERT(sheetSelected);

    if (!sheetSelected)
    {
        QStringList sheets = doc.sheetNames();
        if (sheets.size() >= 7)
        {
            doc.selectSheet(sheets.at(6)); // 7th sheet
        }
        else
        {
            Q_ASSERT(false);
            // Fallback: select the first sheet available.
        }
    }
}

void TemplateMergerFiller::_selectValidValuesSheet(QXlsx::Document &doc)
{
    bool sheetSelected = false;
    for (const QString &sheetName : SHEETS_VALID_VALUES)
    {
        if (doc.selectSheet(sheetName))
        {
            sheetSelected = true;
            break;
        }
    }
    Q_ASSERT(sheetSelected);

    if (!sheetSelected)
    {
        QStringList sheets = doc.sheetNames();
        if (sheets.size() >= 5)
        {
            doc.selectSheet(sheets.at(3)); // 4th sheet
        }
        else
        {
            Q_ASSERT(false);
        }
    }
}

QHash<QString, int> TemplateMergerFiller::_get_fieldId_index(
    QXlsx::Document &doc) const
{
    const auto &dim = doc.dimension();
    QHash<QString, int> colId_index;
    for (int i=0; i<dim.lastColumn(); ++i)
    {
        auto cell = doc.cellAt(3, i+1);
        if (cell)
        {
            QString fieldId{cell->value().toString()};
            if (!fieldId.isEmpty())
            {
                colId_index[cell->value().toString()] = i;
            }
        }
    }
    return colId_index;
}

/*
void TemplateMergerFiller::exportTo(const QString &filePath)
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

