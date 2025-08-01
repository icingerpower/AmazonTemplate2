#include <QHash>
#include <QRegularExpression>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QObject>

#include <xlsxcell.h>
#include <xlsxcellrange.h>

#include "ExceptionTemplateError.h"

#include "TemplateMergerFiller.h"

const QStringList TemplateMergerFiller::SHEETS_TEMPLATE{
        "Template"
        , "Vorlage"
        , "Modèle"
        , "Sjabloon"
        , "Mall"
        , "Szablon"
        , "Plantilla"
        , "Modello"
        , "Şablon"
        , "Gabarit"
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
    , "Geçerli Değerler"
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
    , {"Definiciones de datos", "Obligatorio"}
};

TemplateMergerFiller::FuncFiller TemplateMergerFiller::FUNC_FILLER_PRICE
    = [](const QString &countryFrom,
         const QString &countryTo,
         const QString &,
         const QHash<QString, QHash<QString, QString>> &,
         Gender,
         Age,
         const QVariant &origValue) -> QVariant{
    static QHash<QString, double> country_rate{
        {"FR", 1.}
        , {"DE", 1.}
        , {"IT", 1.}
        , {"ES", 1.}
        , {"BE", 1.}
        , {"NL", 1.}
        , {"IE", 1.}
        , {"UK", 0.86}
        , {"SE", 11.1435}
        , {"PL", 4.2675}
        , {"TR", 46.7776}
        , {"COM", 1.1527/1.15}   // US (USD)
        , {"CA", 1.5900/1.15}
        , {"MX", 21.6193}
        , {"JP", 170.96}
        , {"AU", 1.7775}
        , {"AE", 4.2333}     // via USD peg: 1.1527 * 3.6725
    };
    bool isNum = false;
    double price = origValue.toDouble(&isNum);
    if (!isNum)
    {
        return origValue;
    }
    double priceEur = price / country_rate[countryFrom];
    double newPrice = priceEur * country_rate[countryTo];
    if (qAbs(priceEur - newPrice) > 0.0001)
    {
        return std::round(newPrice - 0.99) + 0.99;
    }
    return newPrice;
};

TemplateMergerFiller::FuncFiller TemplateMergerFiller::FUNC_FILLER_PUT_KEYWORDS
    = [](const QString &,
         const QString &countryTo,
         const QString &langTo,
         const QHash<QString, QHash<QString, QString>> &countryCode_langCode_keywords,
         Gender,
         Age,
         const QVariant &origValue) -> QVariant{
    if (!countryCode_langCode_keywords.contains(countryTo)
        || !countryCode_langCode_keywords[countryTo].contains(langTo))
    {
        ExceptionTemplateError exception;
        exception.setInfos(QObject::tr("Uncomplete keywords file"),
                           QObject::tr("The keywords.txt file need to be complete for all countries inluding country: %1 lang: %2")
                               .arg(countryTo, langTo));
        exception.raise();
    }
    const auto &keywords = countryCode_langCode_keywords[countryTo][langTo];
    static const QSet<QString> enCountries{"COM", "AU", "UK", "CA"};
    if (!enCountries.contains(countryTo) && keywords.size() < 200
        && countryCode_langCode_keywords.contains("UK")
        && countryCode_langCode_keywords["UK"].contains("EN"))
    {
        const auto &keywordsSplited = keywords.split(" ");
        QSet<QString> mergedKeywords{keywordsSplited.begin(), keywordsSplited.end()};
        int curSize = keywords.size();
        const auto &keywordsSplitedUk = countryCode_langCode_keywords["UK"]["EN"].split(" ");
        for (const auto &enKeyword : keywordsSplitedUk)
        {
            if (curSize + enKeyword.size() > 235)
            {
                break;
            }
            mergedKeywords.insert(enKeyword);
            curSize += enKeyword.size();
        }
        QStringList mergedKeywordsSorted{mergedKeywords.begin(), mergedKeywords.end()};
        std::sort(mergedKeywordsSorted.begin(), mergedKeywordsSorted.end());
        return mergedKeywordsSorted.join(" ");
    }
    return keywords;
};

TemplateMergerFiller::FuncFiller TemplateMergerFiller::FUNC_FILLER_COPY
    = [](const QString &,
         const QString &,
         const QString &,
         const QHash<QString, QHash<QString, QString>> &,
         Gender,
         Age,
         const QVariant &origValue) -> QVariant{
    return origValue;
};

TemplateMergerFiller::FuncFiller TemplateMergerFiller::FUNC_FILLER_CONVERT_CLOTHE_SIZE
    = [](const QString &countryFrom,
         const QString &countryTo,
         const QString &,
         const QHash<QString, QHash<QString, QString>> &,
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
                    , {"IE", 4} // Irland
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
                    , {"IE", 26} // Irland
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
         const QString &,
         const QHash<QString, QHash<QString, QString>> &,
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
                QSet<QString> groupUk{"UK", "IE", "AU"};
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
                QSet<QString> groupUk{"UK","IE","AU"};
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

const QSet<QString> TemplateMergerFiller::FIELD_IDS_NOT_AI{
    "feed_product_type"
    , "product_type#1.value"
    , "item_sku"
    , "contribution_sku#1.value"
    , "brand_name"
    , "brand#1.value"
    , "manufacturer"
    , "manufacturer#1.value"
    , "parent_child"
    , "parentage_level#1.value"
    , "parent_sku"
    , "child_parent_sku_relationship#1.parent_sku"
    , "package_length"
    , "item_package_dimensions#1.length.value"
    , "package_width"
    , "item_package_dimensions#1.width.value"
    , "package_height"
    , "item_package_dimensions#1.height.value"
    , "package_weight"
    , "item_package_weight#1.value"
};

const QSet<QString> TemplateMergerFiller::FIELD_IDS_PUT_FIRST_VALUE{
    "feed_product_type"
    , "product_type#1.value"
    , "fulfillment_center_id"
    , "fulfillment_availability#1.fulfillment_channel_code"
    , "recommended_browse_nodes"
    , "recommended_browse_nodes#1.value"
};

const QHash<QString, TemplateMergerFiller::FuncFiller>
    TemplateMergerFiller::FIELD_IDS_FILLER_NO_SOURCES
{
    {"parent_child", FUNC_FILLER_COPY}
    , {"parentage_level#1.value", FUNC_FILLER_COPY}
    , {"parent_sku", FUNC_FILLER_COPY}
    , {"child_parent_sku_relationship#1.parent_sku", FUNC_FILLER_COPY}
    , {"relationship_type", FUNC_FILLER_COPY}
    , {"child_parent_sku_relationship#1.child_relationship_type", FUNC_FILLER_COPY}
    , {"variation_theme", FUNC_FILLER_COPY}
    , {"variation_theme#1.name", FUNC_FILLER_COPY}
};

const QSet<QString> TemplateMergerFiller::FIELD_IDS_NO_SOURCES{
    "standard_price"
    , "purchasable_offer#1.our_price#1.schedule#1.value_with_tax"
    , "list_price_with_tax"
    , "list_price#1.value_with_tax"
};

const QHash<QString, TemplateMergerFiller::FuncFiller> TemplateMergerFiller::FIELD_IDS_FILLER
{
    {"apparel_size", FUNC_FILLER_CONVERT_CLOTHE_SIZE}
    , {"apparel_size#1.size", FUNC_FILLER_CONVERT_CLOTHE_SIZE}
    , {"apparel_size_to", FUNC_FILLER_CONVERT_CLOTHE_SIZE}
    , {"apparel_size#1.size_to", FUNC_FILLER_CONVERT_CLOTHE_SIZE}
    , {"footwear_size", FUNC_FILLER_CONVERT_SHOE_SIZE}
    , {"footwear_size#1.size", FUNC_FILLER_CONVERT_CLOTHE_SIZE}
    , {"footwear_to_size", FUNC_FILLER_CONVERT_SHOE_SIZE}
    , {"footwear_size#1.to_size", FUNC_FILLER_CONVERT_CLOTHE_SIZE}
    , {"shapewear_size", FUNC_FILLER_CONVERT_CLOTHE_SIZE}
    , {"shapewear_size#1.size", FUNC_FILLER_CONVERT_CLOTHE_SIZE}
    , {"shapewear_size_to", FUNC_FILLER_CONVERT_CLOTHE_SIZE}
    , {"shapewear_size#1.size_to", FUNC_FILLER_CONVERT_CLOTHE_SIZE}
    , {"item_sku", FUNC_FILLER_COPY}
    , {"contribution_sku#1.value", FUNC_FILLER_COPY}
    , {"external_product_id", FUNC_FILLER_COPY}
    , {"amzn1.volt.ca.product_id_value", FUNC_FILLER_COPY}
    , {"external_product_type", FUNC_FILLER_COPY}
    , {"amzn1.volt.ca.product_id_type", FUNC_FILLER_COPY}
    , {"package_length", FUNC_FILLER_COPY}
    , {"item_package_dimensions#1.length.value", FUNC_FILLER_COPY}
    , {"package_width", FUNC_FILLER_COPY}
    , {"item_package_dimensions#1.width.value", FUNC_FILLER_COPY}
    , {"package_height", FUNC_FILLER_COPY}
    , {"item_package_dimensions#1.height.value", FUNC_FILLER_COPY}
    , {"package_weight", FUNC_FILLER_COPY}
    , {"item_package_weight#1.value", FUNC_FILLER_COPY}
    , {"size_name", FUNC_FILLER_COPY}
    , {"generic_keywords", FUNC_FILLER_PUT_KEYWORDS}
    , {"generic_keyword#1.value", FUNC_FILLER_PUT_KEYWORDS}
    , {"standard_price", FUNC_FILLER_PRICE}
    , {"purchasable_offer#1.our_price#1.schedule#1.value_with_tax", FUNC_FILLER_PRICE}
    , {"list_price_with_tax", FUNC_FILLER_PRICE}
    , {"list_price#1.value_with_tax", FUNC_FILLER_PRICE}
};

const QHash<QString, QSet<QString>> TemplateMergerFiller::FIELD_IDS_COPY_FROM_OTHER
    {
        {"size_map", {"apparel_size", "footwear_size", "shapewear_size"}}
};

void TemplateMergerFiller::_recordValueAllVersion(
    QHash<QString, QVariant> &fieldId_value, const QString fieldId, const QVariant &value)
{
    static QHash<QString, QString> mappingFieldId
        = []() -> QHash<QString, QString>{
        QHash<QString, QString> _mappingFieldIdTemp{
                                                {"feed_product_type", "product_type#1.value"}
                                                , {"item_sku", "contribution_sku#1.value"}
                                                , {"brand_name", "brand#1.value"}
                                                , {"manufacturer", "manufacturer#1.value"}
                                                , {"parent_child", "parentage_level#1.value"}
                                                , {"parent_sku", "child_parent_sku_relationship#1.parent_sku"}
                                                , {"package_length", "item_package_dimensions#1.length.value"}
                                                , {"package_width", "item_package_dimensions#1.width.value"}
                                                , {"package_height", "item_package_dimensions#1.height.value"}
                                                , {"package_weight", "item_package_weight#1.value"}
                                                , {"standard_price", "purchasable_offer#1.our_price#1.schedule#1.value_with_tax"}
                                                , {"list_price_with_tax", "list_price#1.value_with_tax"}
                                                , {"generic_keywords", "generic_keyword#1.value"}
                                                , {"external_product_id", "amzn1.volt.ca.product_id_value"}
                                                , {"external_product_type", "amzn1.volt.ca.product_id_type"}
                                                , {"apparel_size", "apparel_size#1.size"}
                                                , {"apparel_size_to", "apparel_size#1.size_to"}
                                                , {"footwear_size", "footwear_size#1.size"}
                                                , {"footwear_to_size", "footwear_size#1.to_size"}
                                                , {"shapewear_size", "shapewear_size#1.size"}
                                                , {"shapewear_size_to", "shapewear_size#1.size_to"}
                                                , {"relationship_type", "child_parent_sku_relationship#1.child_relationship_type"}
                                                , {"variation_theme", "variation_theme#1.name"}
        };
        QHash<QString, QString> _mappingFieldId = _mappingFieldIdTemp;
        for (auto it = _mappingFieldIdTemp.begin();
             it != _mappingFieldIdTemp.end(); ++it)
        {
            _mappingFieldId[it.value()] = it.key();
        }
        return _mappingFieldId;
    }();
    fieldId_value[fieldId] = value;
    fieldId_value[mappingFieldId[fieldId]] = value;
}

void TemplateMergerFiller::_preFillChildOny()
{
    static QSet<QString> fieldIdsChildOnly{"apparel_size_class"
                                           , "apparel_size#1.size_class"
                                           , "apparel_size_system"
                                           , "apparel_size#1.size_system"
                                           , "apparel_size"
                                           , "apparel_size#1.size"
                                           , "apparel_size_to"
                                           , "apparel_size#1.size_to"
                                           , "apparel_body_type"
                                           , "apparel_size#1.body_type"
                                           , "apparel_height_type"
                                           , "apparel_size#1.height_type"
                                           , "size_map"
                                           , "fulfillment_center_id"
                                           , "fulfillment_availability#1.fulfillment_channel_code"
                                           , "package_length"
                                           , "item_package_dimensions#1.length.value"
                                           , "package_width"
                                           , "item_package_dimensions#1.width.value"
                                           , "package_height"
                                           , "item_package_dimensions#1.height.value"
                                           , "package_length_unit_of_measure"
                                           , "item_package_dimensions#1.length.unit"
                                           , "package_weight"
                                           , "item_package_weight#1.value"
                                           , "package_weight_unit_of_measure"
                                           , "item_package_weight#1.unit"
                                           , "package_height_unit_of_measure"
                                           , "item_package_dimensions#1.height.unit"
                                           , "package_width_unit_of_measure"
                                           , "item_package_dimensions#1.width.unit"
                                           , "batteries_required"
                                           , "supplier_declared_dg_hz_regulation1"
                                           , "condition_type"
                                           , "condition_type#1.value"
                                           , "currency"
                                           , "list_price_with_tax"
                                           , "list_price#1.value_with_tax"
                                           , "heel_height"
                                           , "heel#1.height#1.decimal_value"
                                           , "footwear_size_system"
                                           , "footwear_size#1.size_system"
                                           , "footwear_age_group"
                                           , "footwear_size#1.age_group"
                                           , "footwear_gender"
                                           , "footwear_size#1.gender"
                                           , "footwear_size_class"
                                           , "footwear_size#1.size_class"
                                           , "footwear_width"
                                           , "footwear_size#1.width"
                                           , "footwear_size"
                                           , "footwear_size#1.size"
                                           , "footwear_to_size"
                                           , "footwear_size#1.to_size"
                                           , "target_gender"
                                           , "target_gender#1.value"
                                           , "age_range_description"
                                           , "age_range_description#1.value"
                                           , "shapewear_size_system"
                                           , "shapewear_size#1.size_system"
                                           , "shapewear_size_class"
                                           , "shapewear_size#1.size_class"
                                           , "shapewear_size"
                                           , "shapewear_size#1.size"
                                           , "shapewear_size_to"
                                           , "shapewear_size#1.size_to"
                                           , "shapewear_body_type"
                                           , "shapewear_size#1.body_type"
                                           , "shapewear_height_type"
                                           , "shapewear_size#1.height_type"
    };
    for (auto itCountry = m_countryCode_langCode_fieldIdMandatory.begin();
         itCountry != m_countryCode_langCode_fieldIdMandatory.end(); ++itCountry)
    {
        for (auto itLangCode = itCountry.value().begin();
             itLangCode != itCountry.value().end(); ++itLangCode)
        {
            const auto &fieldIds = itLangCode.value();
            for (const auto &fieldId : fieldIds)
            {
                if (fieldIdsChildOnly.contains(fieldId))
                {
                    m_countryCode_langCode_fieldIdChildOnly[itCountry.key()][itLangCode.key()].insert(fieldId);
                }
            }
        }
    }
}



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

TemplateMergerFiller::TemplateMergerFiller(const QString &filePathFrom)
{
    m_filePathFrom = filePathFrom;
    m_age = UndefinedAge;
    m_gender = UndefinedGender;
}

void TemplateMergerFiller::fillExcelFiles(
    const QString &keywordFilePath,
    const QStringList &sourceFilePaths,
    const QStringList &toFillFilePaths)
{
    setFilePathsToFill(keywordFilePath, toFillFilePaths);
    if (!sourceFilePaths.isEmpty())
    {
        readInfoSources(sourceFilePaths);
    }
    fillDataAutomatically();
    fillDataLeftChatGpt();
    createToFillXlsx();
}

TemplateMergerFiller::Version TemplateMergerFiller::_getDocumentVersion(
    QXlsx::Document &document) const
{
    _selectTemplateSheet(document);
    auto firstCell = document.cellAt(1, 1);
    if (firstCell)
    {
        const QString version{firstCell->value().toString()};
        if (version.startsWith("settings", Qt::CaseInsensitive))
        {
            return V02;
        }
        else
        {
            return V01;
        }
    }
    Q_ASSERT(false);
    return V01;
}

void TemplateMergerFiller::readSkus(QXlsx::Document &document,
                                    const QString &countryCode,
                                    const QString &langCode,
                                    QStringList &skus,
                                    QHash<QString, QString> &sku_skuParent,
                                    QHash<QString, QHash<QString, QHash<QString, QHash<QString, QVariant>>>> &sku_countryCode_langCode_fieldId_origValue,
                                    bool isMainFile)
{
    _selectTemplateSheet(document);
    const auto &fieldId_index = _get_fieldId_index(document);
    int indColSku = _getIdSku(fieldId_index);
    int indColSkuParent = _getIdSkuParent(fieldId_index);
    const auto &dim = document.dimension();
    QHash<QString, qsizetype> fieldId_count;
    int nParents = 0;
    for (int i=3; i<dim.lastRow(); ++i)
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
        else
        {
        auto cellSkuParent = document.cellAt(i+1, indColSkuParent + 1);
        if (cellSkuParent)
        {
            QString skuParent{cellSku->value().toString()};
            sku_skuParent[sku] = skuParent;
        }

        }
        for (auto it = fieldId_index.cbegin();
             it != fieldId_index.cend(); ++it)
        {
            const auto &fieldId = it.key();
            int indCol = it.value();
            auto cell = document.cellAt(i+1, indCol + 1);
            if (cell)
            {
                const auto &valueVariant = cell->value();
                QString valueString{valueVariant.toString()};
                if (isMainFile)
                {
                    if (fieldId == "target_gender"
                        || fieldId == "target_gender#1.value")
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
                    else if (fieldId == "age_range_description"
                               || fieldId == "age_range_description#1.value")
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
                }
                if (!valueString.isEmpty())
                {
                    ++fieldId_count[fieldId];
                    _recordValueAllVersion(sku_countryCode_langCode_fieldId_origValue[sku][countryCode][langCode],
                                           fieldId,
                                           valueVariant);
                }
            }
        }
    }
    if (isMainFile)
    {
        auto valid1 = skus.size();
        auto valid2 = skus.size()-nParents;
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
                    exception.raise();
                }
            }
        }
    }
}

void TemplateMergerFiller::setFilePathsToFill(const QString &keywordFilePath,
                                              const QStringList &toFillFilePaths)
{
    m_toFillFilePaths = toFillFilePaths;
    m_countryCode_langCode_fieldIdMandatory.clear();
    m_countryCode_langCode_fieldName_fieldId.clear();
    m_sku_countryCode_langCode_fieldId_origValue.clear();
    m_skus.clear();
    _readKeywords(keywordFilePath);

    const auto &countryCodeFrom = _getCountryCode(m_filePathFrom);
    QStringList toFillFilePathsSorted{toFillFilePaths.begin(), toFillFilePaths.end()};
    std::sort(toFillFilePathsSorted.begin(), toFillFilePathsSorted.end());
    toFillFilePathsSorted.insert(0, toFillFilePathsSorted.takeAt(toFillFilePathsSorted.indexOf(m_filePathFrom)));
    for (const auto &toFillFilePath : toFillFilePaths)
    {
        const auto &countryCodeTo = _getCountryCode(toFillFilePath);
        const auto &langCodeTo = _getLangCode(toFillFilePath);
        QXlsx::Document document(toFillFilePath);
        if (countryCodeTo == countryCodeFrom)
        {
            readSkus(document, countryCodeFrom, langCodeTo, m_skus, m_sku_skuParent, m_sku_countryCode_langCode_fieldId_origValue, true);
        }
        _readFields(document, countryCodeTo, langCodeTo);
        _readMandatory(document, countryCodeTo, langCodeTo);
        _readValidValues(document, countryCodeTo, langCodeTo);
        _preFillChildOny();
    }
}

void TemplateMergerFiller::readInfoSources(const QStringList &sourceFilePaths)
{
    QHash<QString, QHash<QString, QHash<QString, QHash<QString, QVariant>>>> sku_countryCode_langCode_fieldId_origValue;
    for (const auto &sourceFilePath : sourceFilePaths)
    {
        const auto &countryCode = _getCountryCode(sourceFilePath);
        const auto &langCodeTo = _getLangCode(sourceFilePath);
        QXlsx::Document document(sourceFilePath);
        QStringList skus;
        QHash<QString, QString> sku_skuParent;
        readSkus(document, countryCode, langCodeTo, skus, sku_skuParent, sku_countryCode_langCode_fieldId_origValue);
    }
    QSet<QString> skusSet{m_skus.begin(), m_skus.end()};
    for (auto itSku = sku_countryCode_langCode_fieldId_origValue.begin();
         itSku != sku_countryCode_langCode_fieldId_origValue.end(); ++itSku)
    {
        const auto &sku = itSku.key();
        if (skusSet.contains(sku))
        {
            for (auto itCountryCode = itSku.value().begin();
                 itCountryCode != itSku.value().end(); ++itCountryCode)
            {
                const auto &countryCode = itCountryCode.key();
                for (auto itLangCode = itCountryCode.value().begin();
                     itLangCode != itCountryCode.value().end(); ++itLangCode)
                {
                    const auto &langCode = itLangCode.key();
                    for (auto itFieldId_value = itLangCode.value().begin();
                         itFieldId_value != itLangCode.value().end(); ++itFieldId_value)
                    {
                        const auto &fieldId = itFieldId_value.key();
                        if (!FIELD_IDS_FILLER_NO_SOURCES.contains(fieldId)
                            && !FIELD_IDS_NO_SOURCES.contains(fieldId)
                            && !FIELD_IDS_PUT_FIRST_VALUE.contains(fieldId)
                            && m_countryCode_langCode_fieldIdMandatory[countryCode][langCode].contains(fieldId))
                        {
                            if (!_isSkuParent(sku) || !m_countryCode_langCode_fieldIdChildOnly[countryCode][langCode].contains(fieldId))
                            {
                                _recordValueAllVersion(m_sku_countryCode_langCode_fieldId_value[sku][countryCode][langCode],
                                                       fieldId,
                                                       itFieldId_value.value());
                            }
                        }
                    }
                }
            }
        }
    }
}

void TemplateMergerFiller::fillDataAutomatically()
{
    //m_sku_countryCode_fieldId_value;
    const auto &countryCodeFrom = _getCountryCode(m_filePathFrom);
    const auto &langCodeFrom = _getLangCode(m_filePathFrom);
    for (const auto &toFillFilePath : m_toFillFilePaths)
    {
        QXlsx::Document document(toFillFilePath);
        _selectTemplateSheet(document);
        const auto &countryCodeTo = _getCountryCode(toFillFilePath);
        const auto &langCodeTo = _getLangCode(toFillFilePath);
        const auto &fieldId_index = _get_fieldId_index(document);
        const auto &fieldIdsMandatory = m_countryCode_langCode_fieldIdMandatory[countryCodeTo][langCodeTo];
        for (const auto &sku : m_skus)
        {
            for (const auto &fieldId : fieldIdsMandatory)
            {
                if (fieldId_index.contains(fieldId))
                {
                    if (!(m_sku_countryCode_langCode_fieldId_origValue[sku].contains(countryCodeTo)
                          && m_sku_countryCode_langCode_fieldId_origValue[sku][countryCodeTo].contains(langCodeTo)
                          && m_sku_countryCode_langCode_fieldId_origValue[sku][countryCodeTo][langCodeTo].contains(fieldId)))
                    {
                        QVariant origValue;
                        if (m_sku_countryCode_langCode_fieldId_origValue[sku][countryCodeFrom][langCodeFrom].contains(fieldId))
                        {
                            origValue = m_sku_countryCode_langCode_fieldId_origValue[sku][countryCodeFrom][langCodeFrom][fieldId];
                        }
                        if (!_isSkuParent(sku) || !m_countryCode_langCode_fieldIdChildOnly[countryCodeTo][langCodeTo].contains(fieldId))
                        {
                            if (FIELD_IDS_FILLER_NO_SOURCES.contains(fieldId))
                            {
                                const auto &filler = FIELD_IDS_FILLER_NO_SOURCES[fieldId];
                                const auto &fillerValue = filler(countryCodeFrom,
                                                                 countryCodeTo,
                                                                 langCodeTo,
                                                                 m_countryCode_langCode_keywords,
                                                                 m_gender,
                                                                 m_age,
                                                                 origValue);
                                _recordValueAllVersion(m_sku_countryCode_langCode_fieldId_value[sku][countryCodeTo][langCodeTo],
                                                       fieldId,
                                                       fillerValue);
                            }
                            else if (FIELD_IDS_PUT_FIRST_VALUE.contains(fieldId))
                            {
                                Q_ASSERT(m_countryCode_langCode_fieldId_possibleValues[countryCodeTo][langCodeTo].contains(fieldId));
                                const auto &validValues = m_countryCode_langCode_fieldId_possibleValues[countryCodeTo][langCodeTo][fieldId];
                                _recordValueAllVersion(m_sku_countryCode_langCode_fieldId_value[sku][countryCodeTo][langCodeTo],
                                                       fieldId,
                                                       validValues[0]);
                            }
                            else if (!m_sku_countryCode_langCode_fieldId_value[sku][countryCodeTo][langCodeTo].contains(fieldId))
                            {
                                if (FIELD_IDS_FILLER.contains(fieldId))
                                {
                                    const auto &filler = FIELD_IDS_FILLER[fieldId];
                                    const auto &fillerValue = filler(countryCodeFrom,
                                                                     countryCodeTo,
                                                                     langCodeTo,
                                                                     m_countryCode_langCode_keywords,
                                                                     m_gender,
                                                                     m_age,
                                                                     origValue);
                                    _recordValueAllVersion(m_sku_countryCode_langCode_fieldId_value[sku][countryCodeTo][langCodeTo],
                                                           fieldId,
                                                           fillerValue);
                                }
                            }
                        }
                    }
                }
            }
            for (const auto &fieldId : fieldIdsMandatory)
            {
                if (!_isSkuParent(sku) || !m_countryCode_langCode_fieldIdChildOnly[countryCodeTo][langCodeTo].contains(fieldId))
                {
                    if (fieldId_index.contains(fieldId)
                        && FIELD_IDS_COPY_FROM_OTHER.contains(fieldId))
                    {
                        const auto &otherFieldIds = FIELD_IDS_COPY_FROM_OTHER[fieldId];
                        bool otherFound = false;
                        for (const auto &otherFieldId : otherFieldIds)
                        {
                            if (m_sku_countryCode_langCode_fieldId_value[sku][countryCodeTo][langCodeTo].contains(fieldId))
                            {
                                _recordValueAllVersion(m_sku_countryCode_langCode_fieldId_value[sku][countryCodeTo][langCodeTo],
                                                       fieldId,
                                                       m_sku_countryCode_langCode_fieldId_value[sku][countryCodeTo][langCodeTo][otherFieldId]);
                                otherFound = true;
                                break;
                            }
                        }
                        if (!otherFound) // Fallback on converting from orig data
                        {
                            for (const auto &otherFieldId : otherFieldIds)
                            {
                                if (m_sku_countryCode_langCode_fieldId_origValue[sku][countryCodeFrom][langCodeFrom].contains(fieldId))
                                {
                                    if (FIELD_IDS_FILLER.contains(otherFieldId))
                                    {
                                        if (m_sku_countryCode_langCode_fieldId_origValue[sku][countryCodeFrom][langCodeFrom].contains(fieldId))
                                        {
                                            QVariant origValue = m_sku_countryCode_langCode_fieldId_origValue[sku][countryCodeFrom][langCodeFrom][fieldId];
                                            const auto &filler = FIELD_IDS_FILLER_NO_SOURCES[otherFieldId];
                                            const auto &fillerValue = filler(countryCodeFrom,
                                                                             countryCodeTo,
                                                                             langCodeTo,
                                                                             m_countryCode_langCode_keywords,
                                                                             m_gender,
                                                                             m_age,
                                                                             origValue);
                                            _recordValueAllVersion(m_sku_countryCode_langCode_fieldId_origValue[sku][countryCodeTo][langCodeTo],
                                                                   fieldId,
                                                                   fillerValue);
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
}

void TemplateMergerFiller::fillDataLeftChatGpt()
{
    // Product is new, without any batterie, as shown on the image, from china. We use international metric system. Inventory is Year round replenishable
}

void TemplateMergerFiller::createToFillXlsx()
{
    for (const auto &toFillFilePath : m_toFillFilePaths)
    {
        const auto &countryCodeTo = _getCountryCode(toFillFilePath);
        const auto &langCodeTo = _getLangCode(toFillFilePath);
        QXlsx::Document document(toFillFilePath);
        auto version = _getDocumentVersion(document);
        _selectTemplateSheet(document);
        const auto &fieldId_index = _get_fieldId_index(document);
        int row = _getRowFieldId(version) + 1;
        if (version == V02)
        {
            auto dim = document.dimension();
            for (int j=0; j<dim.columnCount(); ++j)
            {
                auto cell = document.cellAt(row+1, j+1);
                if (cell)
                {
                    auto format = cell->format();
                    document.write(row+1, j+1, QVariant{}, format);
                }
            }
        }
        QSet<int> allColIndexes;
        for (const auto &sku : m_skus)
        {
            const auto &fieldId_value = m_sku_countryCode_langCode_fieldId_value[sku][countryCodeTo][langCodeTo];
            for (auto it = fieldId_value.begin();
                 it != fieldId_value.end(); ++it)
            {
                const auto &fieldId = it.key();
                if (fieldId_index.contains(fieldId))
                {
                    int colInd = fieldId_index[fieldId];
                    allColIndexes << colInd;
                    document.write(row + 1, colInd + 1, it.value());
                }
            }
            ++row;
        }
        for (auto colInd : qAsConst(allColIndexes))
        {
            document.setColumnHidden(colInd + 1, false);
        }
        QString toFillFilePathNew{toFillFilePath};
        toFillFilePathNew.replace("TOFILL", "FILLED");
        Q_ASSERT(toFillFilePathNew != toFillFilePath);
        if (toFillFilePathNew != toFillFilePath)
        {
            document.saveAs(toFillFilePathNew);
        }
    }
}

void TemplateMergerFiller::_readKeywords(const QString &filePath)
{
    QFile file{filePath};
    if (file.open(QFile::ReadOnly))
    {
        QTextStream stream{&file};
        auto lines = stream.readAll().split("\n");
        for (int i=0; i<lines.size(); ++i)
        {
            if (lines[i].startsWith("[") && lines[i].endsWith("]") && i+1<lines.size())
            {
                const auto &countryLangCode = lines[i].mid(1, lines[i].size()-2);
                QString langCode = _getLangCodeFromText(countryLangCode);
                QString countryCode;
                if (countryLangCode.contains("_"))
                {
                    const auto &elements = countryLangCode.split("_");
                    countryCode = elements[1].toUpper();
                }
                else
                {
                    countryCode = countryLangCode;
                }
                m_countryCode_langCode_keywords[countryCode][langCode] = lines[i+1];
            }
        }
        file.close();
    }
}

void TemplateMergerFiller::_readFields(
    QXlsx::Document &document,
    const QString &countryCode,
    const QString &langCode)
{
    auto version = _getDocumentVersion(document);
    _selectTemplateSheet(document);
    int rowFieldId = _getRowFieldId(version);
    int rowFieldName = rowFieldId - 1;
    auto dimTemplate = document.dimension();
    for (int i = 0; i < dimTemplate.lastColumn(); ++i)
    {
        auto cellFieldName = document.cellAt(rowFieldName + 1, i + 1);
        auto cellFieldId = document.cellAt(rowFieldId + 1, i + 1);
        if (cellFieldId && cellFieldName)
        {
            QString fieldId{cellFieldId->value().toString()};
            _formatFieldId(fieldId);
            QString fieldName{cellFieldName->value().toString()};
            if (!m_countryCode_langCode_fieldName_fieldId[countryCode][langCode].contains(fieldName))
            {
                m_countryCode_langCode_fieldName_fieldId[countryCode][langCode][fieldName] = fieldId;
            }
        }
    }
}

void TemplateMergerFiller::_readMandatory(
    QXlsx::Document &document,
    const QString &countryCode,
    const QString &langCode)
{

    _selectMandatorySheet(document);
    auto dimMandatory = document.dimension();
    const int colIndFieldId = 1;
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
        auto cellFieldId = document.cellAt(i+1, colIndFieldId + 1);
        auto cellFieldName = document.cellAt(i+1, colIndFieldName + 1);
        auto cellMandatory = document.cellAt(i+1, colIndMandatory + 1);
        if (cellFieldId && cellFieldName && cellMandatory)
        {
            QString fieldId{cellFieldId->value().toString()};
            _formatFieldId(fieldId);
            QString mandatory{cellMandatory->value().toString()};
            if (cellFieldName && cellMandatory && fieldId != mandatory)
            {
                if (VALUES_MANDATORY.contains(mandatory)
                    || FIELD_IDS_FILLER.contains(fieldId)
                    || FIELD_IDS_COPY_FROM_OTHER.contains(fieldId)
                    || FIELD_IDS_NOT_AI.contains(fieldId)
                    || FIELD_IDS_PUT_FIRST_VALUE.contains(fieldId)
                    || FIELD_IDS_FILLER_NO_SOURCES.contains(fieldId)
                    )
                {
                    m_countryCode_langCode_fieldIdMandatory[countryCode][langCode].insert(fieldId);
                }
            }
        }
    }
}

void TemplateMergerFiller::_readValidValues(
    QXlsx::Document &document, const QString &countryCode, const QString &langCode)
{
    _selectValidValuesSheet(document);
    const auto &dimValidValues = document.dimension();
    for (int i=1; i<dimValidValues.lastRow(); ++i)
    {
        auto cellFieldName = document.cellAt(i+1, 2);
        if (cellFieldName)
        {
            QString fieldName{cellFieldName->value().toString()};
            if (fieldName.contains(" - ["))
            {
                fieldName = fieldName.split(" - [")[0];
            }
            if (!fieldName.isEmpty())
            {
                Q_ASSERT(m_countryCode_langCode_fieldName_fieldId[countryCode][langCode].contains(fieldName));
                if (m_countryCode_langCode_fieldName_fieldId[countryCode][langCode].contains(fieldName))
                {
                    const auto &fieldId = m_countryCode_langCode_fieldName_fieldId[countryCode][langCode][fieldName];
                    for (int j=2; i<dimValidValues.lastColumn(); ++j)
                    {
                        auto cellValue = document.cellAt(i+1, j+1);
                        QString value;
                        if (cellValue)
                        {
                            value = cellValue->value().toString();
                            if (!value.isEmpty())
                            {
                                m_countryCode_langCode_fieldId_possibleValues[countryCode][langCode][fieldId] << value;
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
}

bool TemplateMergerFiller::_isSkuParent(const QString &sku) const
{
    return sku.startsWith("P-");
}

QString TemplateMergerFiller::_getCountryCode(
    const QString &templateFilePath) const
{
    QStringList elements = QFileInfo{templateFilePath}.baseName().split("-");
    bool isNum = false;
    while (true)
    {
        elements.last().toInt(&isNum);
        if (isNum)
        {
            elements.takeLast();
        }
        else
        {
            break;
        }
    }
    if (elements.last().contains("_"))
    {
        return elements.last().split("_").last();

    }
    return elements.last();
}

QString TemplateMergerFiller::_getLangCode(const QString &templateFilePath) const
{
    QStringList elements = QFileInfo{templateFilePath}.baseName().split("-");
    bool isNum = false;
    while (true)
    {
        elements.last().toInt(&isNum);
        if (isNum)
        {
            elements.takeLast();
        }
        else
        {
            break;
        }
    }
    const auto &langInfos = elements.last();
    return _getLangCodeFromText(langInfos);
}

QString TemplateMergerFiller::_getLangCodeFromText(const QString &langInfos) const
{
    if (langInfos.contains("_"))
    {
        return langInfos.split("_")[0].toUpper();
    }
    if (langInfos.contains("UK", Qt::CaseInsensitive)
        || langInfos.contains("COM", Qt::CaseInsensitive)
        || langInfos.contains("AU", Qt::CaseInsensitive)
        || langInfos.contains("CA", Qt::CaseInsensitive)
        || langInfos.contains("SG", Qt::CaseInsensitive)
        || langInfos.contains("SA", Qt::CaseInsensitive)
        || langInfos.contains("IE", Qt::CaseInsensitive)
        || langInfos.contains("AE", Qt::CaseInsensitive)
        )
    {
        return "EN";
    }
    if (langInfos.contains("BE", Qt::CaseInsensitive))
    {
        return "FR";
    }
    if (langInfos.contains("MX", Qt::CaseInsensitive))
    {
        return "ES";
    }
    return langInfos;

}

void TemplateMergerFiller::_selectTemplateSheet(QXlsx::Document &doc) const
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

void TemplateMergerFiller::_selectMandatorySheet(QXlsx::Document &doc) const
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

void TemplateMergerFiller::_selectValidValuesSheet(QXlsx::Document &doc) const
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
    auto version = _getDocumentVersion(doc);
    int rowCell = _getRowFieldId(version);
    const auto &dim = doc.dimension();
    QHash<QString, int> colId_index;
    int lastColumn = dim.lastColumn();
    for (int i=0; i<lastColumn; ++i)
    {
        auto cell = doc.cellAt(rowCell + 1, i + 1);
        if (cell)
        {
            QString fieldId{cell->value().toString()};
            if (!fieldId.isEmpty())
            {
                _formatFieldId(fieldId);
                if (!colId_index.contains(fieldId))
                {
                    colId_index[fieldId] = i;
                }
            }
        }
    }
    return colId_index;
}

void TemplateMergerFiller::_formatFieldId(QString &fieldId) const
{
    static const QRegularExpression brackets(R"(\[[^\]]*\])");
    fieldId.remove(brackets);
    Q_ASSERT(!fieldId.contains("[") && !fieldId.contains("]"));
}

int TemplateMergerFiller::_getIdSku(
    const QHash<QString, int> &fieldId_index) const
{
    static QSet<QString> possibleValues{"item_sku", "contribution_sku#1.value"};
    for (const auto &possibleSkuId : possibleValues)
    {
        auto itSku = fieldId_index.find(possibleSkuId);
        if (itSku != fieldId_index.end())
        {
            return itSku.value();
        }
    }
    Q_ASSERT(false);
    return -1;
}

int TemplateMergerFiller::_getIdSkuParent(
    const QHash<QString, int> &fieldId_index) const
{
    static QSet<QString> possibleValues{"parent_sku", "child_parent_sku_relationship#1.parent_sku"};
    for (const auto &possibleSkuId : possibleValues)
    {
        auto itSkuParent = fieldId_index.find(possibleSkuId);
        if (itSkuParent != fieldId_index.end())
        {
            return itSkuParent.value();
        }
    }
    Q_ASSERT(false);
    return -1;
}

int TemplateMergerFiller::_getRowFieldId(Version version) const
{
    if (version == V01)
    {
        return 2;
    }
    else if (version == V02)
    {
        return 4;
    }
    else
    {
        Q_ASSERT(false);
        return -1;
    }
}

