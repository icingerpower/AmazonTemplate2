#include <QHash>
#include <QRegularExpression>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QObject>

#include <xlsxcell.h>
#include <xlsxcellrange.h>

#include "ExceptionTemplateError.h"
#include "GptFiller.h"

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
    = [](const QString &,
         const QString &countryFrom,
         const QString &countryTo,
         const QString &,
         const QHash<QString, QHash<QString, QString>> &,
         const QHash<QString, QHash<QString, QHash<QString, QString>>> &,
         Gender,
         Age,
         const QString &,
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
= [](const QString &sku,
        const QString &,
        const QString &countryTo,
        const QString &langTo,
        const QHash<QString, QHash<QString, QString>> &countryCode_langCode_keywords,
        const QHash<QString, QHash<QString, QHash<QString, QString>>> &skuPattern_countryCode_langCode_keywords,
        Gender,
        Age,
         const QString &,
        const QVariant &origValue) -> QVariant{
    bool found = false;
    for (auto itSkuPattern = skuPattern_countryCode_langCode_keywords.begin();
         itSkuPattern != skuPattern_countryCode_langCode_keywords.end(); ++itSkuPattern)
    {
        const auto &skuPattern = itSkuPattern.key();
        if (sku.contains(skuPattern))
        {
            found = true;
            if (!itSkuPattern.value().contains(countryTo)
                    || !itSkuPattern.value()[countryTo].contains(langTo))
            {
                ExceptionTemplateError exception;
                exception.setInfos(QObject::tr("Uncomplete keywords file"),
                                   QObject::tr("The keywords.txt file need to be complete for all countries inluding country: %1 lang: %2")
                                   .arg(countryTo, langTo));
                exception.raise();
            }
        }
    }

    if (!found)
    {
        if (!countryCode_langCode_keywords.contains(countryTo)
                && !countryCode_langCode_keywords[countryTo].contains(langTo))
        {
            ExceptionTemplateError exception;
            exception.setInfos(QObject::tr("Uncomplete keywords file"),
                               QObject::tr("The keywords.txt file need to be complete for all countries inluding country: %1 lang: %2")
                               .arg(countryTo, langTo));
            exception.raise();
        }
    }
    auto addEnglishKeywords = [countryTo](
            const QHash<QString, QHash<QString, QString>> &countryCode_langCode_keywords
            , const QString &keywords) -> QString {
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

    QString keywords;
    for (auto it = skuPattern_countryCode_langCode_keywords.begin();
         it != skuPattern_countryCode_langCode_keywords.end(); ++it)
    {
        if (sku.contains(it.key()))
        {
            keywords = addEnglishKeywords(it.value(), it.value()[countryTo][langTo]);
            break;
        }
    }
    if (keywords.isEmpty())
    {
        keywords = countryCode_langCode_keywords[countryTo][langTo];
        keywords = addEnglishKeywords(countryCode_langCode_keywords, keywords);
    }
    return keywords;
};

TemplateMergerFiller::FuncFiller TemplateMergerFiller::FUNC_FILLER_COPY
    = [](const QString &,
         const QString &,
         const QString &,
         const QString &,
         const QHash<QString, QHash<QString, QString>> &,
         const QHash<QString, QHash<QString, QHash<QString, QString>>> &,
         Gender,
         Age,
         const QString &,
         const QVariant &origValue) -> QVariant{
    return origValue;
};

TemplateMergerFiller::FuncFiller TemplateMergerFiller::FUNC_FILLER_CONVERT_CLOTHE_SIZE
    = [](const QString &,
        const QString &countryFrom,
         const QString &countryTo,
         const QString &langTo,
         const QHash<QString, QHash<QString, QString>> &,
         const QHash<QString, QHash<QString, QHash<QString, QString>>> &,
         Gender targetGender,
         Age age_range_description,
        const QString &productType,
        const QVariant &origValue) -> QVariant{
    if (productType.toUpper() == "RUG")
    {
        return origValue;
    }
    bool isNum = false;
    int num = origValue.toInt(&isNum);
    static const QList<QHash<QString, int>> list_countryCode_size
    = [targetGender, age_range_description, productType]() -> QList<QHash<QString, int>>
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
            int eu = 48; // set the EU men size (valid: 44,46,48,50,52,54,56,58,60)

            _list_countryCode_size = QList<QHash<QString, int>> {
                {{
                    {"FR", eu},
                    {"BE", eu},
                    {"ES", eu},
                    {"TR", eu},
                    {"DE", eu},
                    {"NL", eu},
                    {"SE", eu},
                    {"PL", eu},
                    {"IT", eu},

                    {"IE", eu - 10}, // Ireland (UK/US inch system)
                    {"UK", eu - 10},
                    {"AU", eu - 10},
                    {"COM", eu - 10}, // US
                    {"CA", eu - 10},
                    {"AE", eu - 10},
                    {"MX", eu - 10},
                    {"SA", eu - 10},
                    {"SG", eu - 10}
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
    Q_ASSERT(countryTo != "JP"); //Should map cm
    if (countryTo == "BE" && langTo == "FR")
    {
        if (origValue == "XL")
        {
            return "TG";
        }
        else if (origValue == "XXL")
        {
            return "TTG";
        }
    }
    if (countryTo == "IE" || countryTo == "COM" || countryTo == "CA")
    {
        static const QHash<QString, QString> equivalences
        {
            {"S", "Small"}
            , {"M", "Medium"}
            , {"L", "Large"}
            , {"XL", "X-Large"}
            , {"XXL", "XX-Large"}
            , {"3XL", "3X-Large"}
            , {"4XL", "4X-Large"}
            , {"5XL", "5X-Large"}
            , {"6XL", "6X-Large"}
        };
        const auto &origValueString = origValue.toString();
        return equivalences.value(origValueString, origValueString);
    }
    return origValue;
};

TemplateMergerFiller::FuncFiller TemplateMergerFiller::FUNC_FILLER_CONVERT_SHOE_SIZE
    = [](const QString &,
        const QString &countryFrom,
         const QString &countryTo,
         const QString &,
         const QHash<QString, QHash<QString, QString>> &,
         const QHash<QString, QHash<QString, QHash<QString, QString>>> &,
         Gender targetGender,
         Age age_range_description,
         const QString &productType,
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
                QSet<QString> groupUs{"COM", "CA", "SA", "SG", "AE"};
                QSet<QString> groupUk{"UK", "IE", "AU"};
                QSet<QString> groupJp{"JP", "MX"};
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
                QSet<QString> groupUs{"COM","CA","SA","SG","AE"};
                QSet<QString> groupUk{"UK","IE","AU"};
                QSet<QString> groupJp{"JP", "MX"};

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
                if (countryTo == "MX")
                {
                    QString sizeCm{QString::number(countryCode_size[countryTo])};
                    if (sizeCm.contains("."))
                    {
                        return sizeCm += " cm";
                    }
                    return sizeCm += ".0 cm";
                }
                return countryCode_size[countryTo];
            }
        }
    }
    return origValue;
};

const QSet<QString> TemplateMergerFiller::FIELD_IDS_NOT_AI{
    "item_same"
    , "item_name#1.value"
    , "product_type#1.value"
    , "feed_product_type"
    , "product_type#1.value"
    , "external_product_id"
    , "amzn1.volt.ca.product_id_value"
    , "external_product_type"
    , "external_product_id_type"
    , "amzn1.volt.ca.product_id_type"
    , "item_sku"
    , "contribution_sku#1.value"
    , "brand_name"
    , "brand#1.value"
    , "manufacturer"
    , "manufacturer#1.value"
    , "parent_sku"
    , "child_parent_sku_relationship#1.parent_sku"

    , "item_display_length"
    , "item_display_dimensions#1.length.value"
    , "item_display_length_unit_of_measure"
    , "item_display_dimensions#1.length.unit"
    , "item_display_width"
    , "item_display_dimensions#1.width.value"
    , "item_display_width_unit_of_measure"
    , "item_display_dimensions#1.width.unit"
    , "item_display_height"
    , "item_display_dimensions#1.height.value"
    , "item_display_height_unit_of_measure"
    , "item_display_dimensions#1.height.unit"
    //, "item_length"
    , "item_length_width#1.length.value"
    //, "item_length_unit_of_measure"
    , "item_length_width#1.length.unit"
    //, "item_width"
    , "item_length_width#1.width.value"
    //, "item_width_unit_of_measure"
    , "item_length_width#1.width.unit"
    , "item_thickness_derived"
    , "item_thickness#1.decimal_value"
    , "item_thickness_unit_of_measure"
    , "item_thickness#1.unit"
    , "number_of_boxes"
    , "number_of_boxes#1.value"
    //, "item_shape"
    //, "rug_form_type#1.value"

    , "item_display_length_unit_of_measure"
    , "item_length_width#1.length.unit"
    , "package_length"
    , "item_package_dimensions#1.length.value"
    , "package_width"
    , "item_package_dimensions#1.width.value"
    , "package_height"
    , "item_package_dimensions#1.height.value"
    , "package_weight"
    , "item_package_weight#1.value"
    , "package_weight"
    , "model"
    , "model_name#1.value"
    , "model_name"
    , "model_number#1.value"
    , "parent_child", "parentage_level#1.value"
    , "shirt_size_to"
    , "shirt_size#1.size_to"
    , "apparel_size_to"
    , "apparel_size#1.size_to"
    , "shirt_size_to"
    , "shirt_size#1.size_to"
    , "footwear_size"
    , "footwear_size#1.size"
    , "footwear_to_size"
    , "footwear_size#1.to_size"
    , "generic_keyword"
    , "generic_keyword#1.value"
    /*
    , "product_description", "product_description#1.value"
    , "bullet_point1", "bullet_point#1.value"
    , "bullet_point2", "bullet_point#2.value"
    , "bullet_point3", "bullet_point#3.value"
    , "bullet_point4", "bullet_point#4.value"
    , "bullet_point5", "bullet_point#5.value"
    //*/
    //, "quantity", "fulfillment_availability#1.quantity"
    //, "list_price_with_tax"
    //, "list_price#1.value_with_tax"
};

const QSet<QString> TemplateMergerFiller::FIELD_IDS_AI_BUT_REQUIRED_IN_FIRST{
    "batteries_required" , "batteries_required#1.value"
    , "variation_theme", "variation_theme#1.name"
    , "supplier_declared_dg_hz_regulation1", "supplier_declared_dg_hz_regulation#1.value"
};

const QSet<QString> TemplateMergerFiller::FIELD_IDS_PUT_FIRST_VALUE{
    "feed_product_type"
    , "product_type#1.value"
    //, "fulfillment_center_id"
    //, "fulfillment_availability#1.fulfillment_channel_code"
    //, "recommended_browse_nodes"
    //, "recommended_browse_nodes#1.value"
};

const QSet<QString> TemplateMergerFiller::FIELD_IDS_EXTRA_MANDATORY{
    "color_name", "color#1.value"
    , "country_of_origin", "country_of_origin#1.value"
    , "batteries_required" , "batteries_required#1.value"
    , "supplier_declared_dg_hz_regulation1", "supplier_declared_dg_hz_regulation#1.value"
    , "supplier_declared_material_regulation1", "supplier_declared_material_regulation1#1.value"
    , "product_description", "product_description#1.value"
    , "fulfillment_center_id"
    , "fulfillment_availability#1.fulfillment_channel_code"
    , "bullet_point1", "bullet_point#1.value"
    , "bullet_point2", "bullet_point#2.value"
    , "bullet_point3", "bullet_point#3.value"
    , "bullet_point4", "bullet_point#4.value"
    , "bullet_point5", "bullet_point#5.value"
    , "package_weight_unit_of_measure", "item_package_weight#1.unit"
    , "package_height_unit_of_measure", "item_package_dimensions#1.height.unit"
    , "package_width_unit_of_measure", "item_package_dimensions#1.width.unit"
    , "package_length_unit_of_measure", "item_package_dimensions#1.length.unit"
    , "department_name", "department#1.value"
    , "target_gender" , "target_gender#1.value"
    , "age_range_description", "age_range_description#1.value"
    , "apparel_body_type", "apparel_size#1.body_type"
    , "apparel_height_type", "apparel_size#1.height_type"
    , "apparel_size_class", "apparel_size#1.size_class"
    , "apparel_size_system", "apparel_size#1.size_system"
    , "shirt_body_type", "shirt_size#1.body_type"
    , "shirt_height_type", "shirt_size#1.height_type"
    , "shirt_size_class", "shirt_size#1.size_class"
    , "shirt_size_system", "shirt_size#1.size_system"
    , "shapewear_body_type", "footwear_size#1.body_type"
    , "shapewear_height_type", "footwear_size#1.height_type"
    , "model_name", "model_name#1.value"
    , "model", "model_number#1.value"
    , "condition_type", "condition_type#1.value"
    , "item_type_name", "item_type_name#1.value"
    , "weave_type", "weave_type#1.value"
    , "care_instructions", "care_instructions#1.value"
    , "relationship_type", "child_parent_sku_relationship#1.child_relationship_type"
    , "variation_theme", "variation_theme#1.name"
    , "main_image_url", "main_product_image_locator#1.media_location"
    , "parent_child", "parentage_level#1.value"
    , "update_delete", "::record_action"
    , "recommended_browse_nodes", "recommended_browse_nodes#1.value"
    , "style_name", "style#1.value"
    , "color_map", "color#1.standardized_values#1"
    , "lifecycle_supply_type"
    , "generic_keywords"
    , "generic_keyword#1.value"
};

const QHash<QString, QSet<QString>> TemplateMergerFiller::PRODUCT_TYPE_FIELD_IDS_EXTRA_MANDATORY
= []() -> QHash<QString, QSet<QString>> {
    QHash<QString, QSet<QString>> productType_extraFieldIds;

    QSet<QString> rugFieldIds{
        "item_length_width#1.length.unit"
        , "item_length_width#1.width.unit"
        , "rug_form_type#1.value"
    };
    productType_extraFieldIds["rug"] = rugFieldIds;
    QSet<QString> clotheFieldIds{
        "special_size_type", "special_size_type#1.value"
        , "outer_material_type1", "outer_material_type", "outer#1.material#1.value"
        , "item_length_description", "item_length_description#1.value"
        , "fabric_type", "fabric_type#1.value"
        , "water_resistance_level", "water_resistance_level#1.value"
        , "material_type", "material#1.value"
        , "occasion_type", "occasion_type#1.value"
        , "inner"
        , "inner_material_type", "inner#1.material#1.value"
        , "closure_type", "closure#1.type#1.value"
        , "sleeve_type", "sleeve#1.type#1.value"
        , "neck_style", "neck#1.neck_style#1.value"
        , "collar_style", "collar_style#1.value"
        , "fit_type", "fit_type#1.value"
        //, "occasion_type", "occasion_type#1.value"
    };
    productType_extraFieldIds["robe"] = clotheFieldIds;
    productType_extraFieldIds["dress"] = clotheFieldIds;
    productType_extraFieldIds["shirt"] = clotheFieldIds;
    QSet<QString> shoeFieldIds{
        "outer_material_type"
        , "outer#1.material#1.value"
        , "closure_type"
        , "closure#1.type#1.value"
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
        , "inner_material_type"
        , "inner#1.material#1.value"
        , "sole_material"
        , "sole_material#1.value"
        , "style_name", "style#1.value"
        , "target_gender", "target_gender#1.value"
        , "age_range_description", "age_range_description#1.value"
        , "water_resistance_level", "water_resistance_level#1.value"
        , "toe_style", "toe_style#1.value"
        , "toe"
        , "shaft_circumference", "shaft#1.circumference#1.value"
        , "shaft_circumference_unit_of_measure", "shaft#1.circumference#1.unit"
        , "shaft_height", "shaft#1.height#1.value"
        , "shaft_height_unit_of_measure", "shaft#1.height#1.unit"
        , "leather_type"
        , "occasion_type#1.value"
        , "occasion_type"
        , "closure#1.type#1.value"
        , "sandal_type#1.value"
        , "heel#1.type#1.value"
        , "height_map#1.value"
    };
    productType_extraFieldIds["boot"] = shoeFieldIds;
    productType_extraFieldIds["shoes"] = shoeFieldIds;
    productType_extraFieldIds["sandal"] = shoeFieldIds;
    for (auto it = productType_extraFieldIds.begin();
         it != productType_extraFieldIds.end(); ++it)
    {
        productType_extraFieldIds[it.key().toUpper()] = it.value();
    }
    return productType_extraFieldIds;
}();

const QSet<QString> TemplateMergerFiller::FIELD_IDS_PATTERN_REMOVE_AS_MANDATORY{
    "lithium"
    , "number_of_batteries"
    , "battery_type"
    , "battery_weight"
    , "ghs_classification"
    , "united_nations_regulatory"
    , "url_"
    , "sheet_url"
    //, "material_regulation"
    , "battery_cell"
    , "dsa_"
    , "flavor"
    , "release_date"
    , "item_volume"
    , "item_weight"
    , "team_name"
    , "quantity", "fulfillment_availability#1.quantity"
    , "merchant_shipping_group#1.value"
    , "hazmat#1.aspect"
    , "footwear_size_unisex"
    , "footwear_to_size_unisex"
    , "footwear_width_unisex"
    , "footwear_gender_unisex"
    //, "height_map"
};

const QSet<QString> TemplateMergerFiller::FIELD_IDS_ALWAY_SAME_VALUE{
    "department_name", "department#1.value"
    , "target_gender", "target_gender#1.value"
    , "age_range_description", "age_range_description#1.value"
    , "apparel_size_class", "apparel_size#1.size_class"
    , "apparel_size_system", "apparel_size#1.size_system"
    , "update_delete", "::record_action"
    , "apparel_body_type", "apparel_size#1.body_type"
    , "apparel_height_type", "apparel_size#1.height_type"
    , "shirt_size_class", "shirt_size#1.size_class"
    , "shirt_size_system", "shirt_size#1.size_system"
    , "shirt_body_type", "shirt_size#1.body_type"
    , "shirt_height_type", "shirt_size#1.height_type"
    , "package_weight_unit_of_measure", "item_package_weight#1.unit"
    , "package_height_unit_of_measure", "item_package_dimensions#1.height.unit"
    , "package_width_unit_of_measure", "item_package_dimensions#1.width.unit"
    , "package_length_unit_of_measure", "item_package_dimensions#1.length.unit"
    , "special_size_type" , "special_size_type#1.value"
    , "outer_material_type", "outer#1.material#1.value"
    , "batteries_required", "batteries_required#1.value"
    , "item_display_length_unit_of_measure"
    , "item_display_dimensions#1.length.unit"
    , "item_display_width_unit_of_measure"
    , "item_display_dimensions#1.width.unit"
    , "item_display_height_unit_of_measure"
    , "item_display_dimensions#1.height.unit"
    //, "item_length_unit_of_measure"
    , "item_length_width#1.length.unit"
    //, "item_width_unit_of_measure"
    , "item_length_width#1.width.unit"
    , "item_thickness_unit_of_measure"
    , "item_thickness#1.unit"
    , "number_of_boxes"
    , "number_of_boxes#1.value"
    //, "item_shape"

};

const QSet<QString> TemplateMergerFiller::FIELD_IDS_ALWAY_SAME_VALUE_CHILD{
    "item_length_description", "item_length_description#1.value"
    , "fit_type", "fit_type#1.value"
    , "collar_style", "collar_style#1.value"
    , "weave_type", "weave_type#1.value"
    , "item_type_name", "item_type_name#1.value"
    , "special_size_type", "special_size_type#1.value"
    , "occasion_type", "occasion_type#1.value"
};

const QHash<QString, TemplateMergerFiller::FuncFiller>
    TemplateMergerFiller::FIELD_IDS_FILLER_NO_SOURCES
{
    //{"parent_child", FUNC_FILLER_COPY}
    //, {"parentage_level#1.value", FUNC_FILLER_COPY}
    {"parent_sku", FUNC_FILLER_COPY}
    , {"child_parent_sku_relationship#1.parent_sku", FUNC_FILLER_COPY}
    //, {"relationship_type", FUNC_FILLER_COPY}
    //, {"child_parent_sku_relationship#1.child_relationship_type", FUNC_FILLER_COPY}
    //, {"variation_theme", FUNC_FILLER_COPY}
    //, {"variation_theme#1.name", FUNC_FILLER_COPY}
    , {"manufacturer", FUNC_FILLER_COPY}
    , {"model", FUNC_FILLER_COPY}
    , {"model_name#1.value", FUNC_FILLER_COPY}
    , {"model_name", FUNC_FILLER_COPY}
    , {"model_number#1.value", FUNC_FILLER_COPY}
    , {"manufacturer#1.value", FUNC_FILLER_COPY}
    , {"number_of_items", FUNC_FILLER_COPY}
    , {"number_of_items#1.value", FUNC_FILLER_COPY}
    , {"item_package_quantity", FUNC_FILLER_COPY}
    , {"item_package_quantity#1.value", FUNC_FILLER_COPY}
    , {"part_number", FUNC_FILLER_COPY}
    , {"part_number#1.value", FUNC_FILLER_COPY}
    , {"main_image_url", FUNC_FILLER_COPY}
    , {"main_product_image_locator#1.media_location", FUNC_FILLER_COPY}
};

const QSet<QString> TemplateMergerFiller::FIELD_IDS_NO_SOURCES{
    "standard_price"
    , "purchasable_offer#1.our_price#1.schedule#1.value_with_tax"
    , "list_price"
    , "list_price_with_tax"
    , "list_price#1.value_with_tax"
};

const QHash<QString, TemplateMergerFiller::FuncFiller> TemplateMergerFiller::FIELD_IDS_FILLER
{
    {"apparel_size", FUNC_FILLER_CONVERT_CLOTHE_SIZE}
    , {"apparel_size#1.size", FUNC_FILLER_CONVERT_CLOTHE_SIZE}
    , {"apparel_size_to", FUNC_FILLER_CONVERT_CLOTHE_SIZE}
    , {"apparel_size#1.size_to", FUNC_FILLER_CONVERT_CLOTHE_SIZE}
    , {"shirt_size", FUNC_FILLER_CONVERT_CLOTHE_SIZE}
    , {"shirt_size#1.size", FUNC_FILLER_CONVERT_CLOTHE_SIZE}
    , {"shirt_size_to", FUNC_FILLER_CONVERT_CLOTHE_SIZE}
    , {"shirt_size#1.size_to", FUNC_FILLER_CONVERT_CLOTHE_SIZE}
    , {"footwear_size", FUNC_FILLER_CONVERT_SHOE_SIZE}
    , {"footwear_size#1.size", FUNC_FILLER_CONVERT_SHOE_SIZE}
    , {"footwear_to_size", FUNC_FILLER_CONVERT_SHOE_SIZE}
    , {"footwear_size#1.to_size", FUNC_FILLER_CONVERT_SHOE_SIZE}
    , {"shapewear_size", FUNC_FILLER_CONVERT_CLOTHE_SIZE}
    , {"shapewear_size#1.size", FUNC_FILLER_CONVERT_CLOTHE_SIZE}
    , {"shapewear_size_to", FUNC_FILLER_CONVERT_CLOTHE_SIZE}
    , {"shapewear_size#1.size_to", FUNC_FILLER_CONVERT_CLOTHE_SIZE}
    , {"size_map", FUNC_FILLER_CONVERT_CLOTHE_SIZE}
    , {"brand_name", FUNC_FILLER_COPY}
    , {"brand#1.value", FUNC_FILLER_COPY}
    , {"item_sku", FUNC_FILLER_COPY}
    , {"contribution_sku#1.value", FUNC_FILLER_COPY}
    , {"external_product_id", FUNC_FILLER_COPY}
    , {"amzn1.volt.ca.product_id_value", FUNC_FILLER_COPY}
    , {"external_product_id_type", FUNC_FILLER_COPY}
    , {"external_product_type", FUNC_FILLER_COPY}
    , {"external_product_id_type", FUNC_FILLER_COPY}
    , {"amzn1.volt.ca.product_id_type", FUNC_FILLER_COPY}
    , {"package_length", FUNC_FILLER_COPY}
    , {"item_package_dimensions#1.length.value", FUNC_FILLER_COPY}
    , {"package_width", FUNC_FILLER_COPY}
    , {"item_package_dimensions#1.width.value", FUNC_FILLER_COPY}
    , {"package_height", FUNC_FILLER_COPY}
    , {"item_package_dimensions#1.height.value", FUNC_FILLER_COPY}
    , {"package_weight", FUNC_FILLER_COPY}

    , {"item_display_length", FUNC_FILLER_COPY}
    , {"item_display_dimensions#1.length.value", FUNC_FILLER_COPY}
    , {"item_display_width", FUNC_FILLER_COPY}
    , {"item_display_dimensions#1.width.value", FUNC_FILLER_COPY}
    , {"item_display_height", FUNC_FILLER_COPY}
    , {"item_display_dimensions#1.height.value", FUNC_FILLER_COPY}
    , {"item_length_width#1.length.value", FUNC_FILLER_COPY}
    , {"item_length_width#1.width.value", FUNC_FILLER_COPY}
    , {"item_thickness_derived", FUNC_FILLER_COPY}
    , {"item_thickness#1.decimal_value", FUNC_FILLER_COPY}
    , {"number_of_boxes", FUNC_FILLER_COPY}
    , {"number_of_boxes#1.value", FUNC_FILLER_COPY}

    , {"item_package_weight#1.value", FUNC_FILLER_COPY}
    , {"shaft_circumference", FUNC_FILLER_COPY}
    , {"shaft#1.circumference#1.value", FUNC_FILLER_COPY}
    , {"shaft_height", FUNC_FILLER_COPY}
    , {"shaft#1.height#1.value", FUNC_FILLER_COPY}
    , {"size_name", FUNC_FILLER_COPY}
    , {"size#1.value", FUNC_FILLER_COPY}
    , {"generic_keywords", FUNC_FILLER_PUT_KEYWORDS}
    , {"generic_keyword#1.value", FUNC_FILLER_PUT_KEYWORDS}
    , {"standard_price", FUNC_FILLER_PRICE}
    , {"purchasable_offer#1.our_price#1.schedule#1.value_with_tax", FUNC_FILLER_PRICE}
    , {"list_price", FUNC_FILLER_PRICE}
    , {"list_price_with_tax", FUNC_FILLER_PRICE}
    , {"list_price#1.value_with_tax", FUNC_FILLER_PRICE}
};

const QStringList TemplateMergerFiller::FIELD_IDS_COLOR_NAME{
    "color_name"
    , "color#1.value"
};

const QStringList TemplateMergerFiller::FIELD_IDS_SIZE{
    "apparel_size"
    , "apparel_size#1.size"
    , "shirt_size"
    , "shirt_size#1.size"
    , "footwear_size"
    , "footwear_size#1.size"
    , "shapewear_size"
    , "shapewear_size#1.size"
    , "size_map"
    , "size_name"
    , "size#1.value"
};

const QSet<QString> TemplateMergerFiller::FIELD_IDS_CHILD_ONLY{
    "apparel_size_class"
    , "apparel_size#1.size_class"
    , "apparel_size_system"
    , "apparel_size#1.size_system"
    , "apparel_size"
    , "apparel_size#1.size"
    , "apparel_size_to"
    , "apparel_size#1.size_to"
    , "shirt_size"
    , "shirt_size#1.size"
    , "shirt_size_to"
    , "shirt_size#1.size_to"
    , "apparel_body_type"
    , "apparel_size#1.body_type"
    , "apparel_height_type"
    , "apparel_size#1.height_type"
    , "closure_type"
    , "closure#1.type#1.value"
    , "sleeve_type"
    , "sleeve#1.type#1.value"
    , "leather_type"
    , "size_map"
    , "size_name"
    , "size#1.value"
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

    , "item_display_length"
    , "item_display_dimensions#1.length.value"
    , "item_display_length_unit_of_measure"
    , "item_display_dimensions#1.length.unit"
    , "item_display_width"
    , "item_display_dimensions#1.width.value"
    , "item_display_width_unit_of_measure"
    , "item_display_dimensions#1.width.unit"
    , "item_display_height"
    , "item_display_dimensions#1.height.value"
    , "item_display_height_unit_of_measure"
    , "item_display_dimensions#1.height.unit"
    //, "item_length"
    , "item_length_width#1.length.value"
    //, "item_length_unit_of_measure"
    , "item_length_width#1.length.unit"
    //, "item_width"
    , "item_length_width#1.width.value"
    //, "item_width_unit_of_measure"
    , "item_length_width#1.width.unit"
    , "item_thickness_derived"
    , "item_thickness#1.decimal_value"
    , "item_thickness_unit_of_measure"
    , "item_thickness#1.unit"
    , "number_of_boxes"
    , "number_of_boxes#1.value"
    //, "item_shape"
    , "rug_form_type#1.value"

    , "material_type", "material#1.value"
    , "inner_material_type", "inner#1.material#1.value"
    , "occasion_type", "occasion_type#1.value"
    //, "batteries_required", "batteries_required#1.value"
    // , "supplier_declared_dg_hz_regulation1", "supplier_declared_dg_hz_regulation#1.value"
    , "supplier_declared_material_regulation1"
    , "supplier_declared_material_regulation1#1.value"
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
    //, "product_description", "product_description#1.value"
    //, "bullet_point1", "bullet_point#1.value"
    //, "bullet_point2", "bullet_point#2.value"
    //, "bullet_point3", "bullet_point#3.value"
    //, "bullet_point4", "bullet_point#4.value"
    //, "bullet_point5", "bullet_point#5.value"
    , "color_map", "color#1.standardized_values#1"
    , "fit_type", "fit_type#1.value"
    , "collar_style", "collar_style#1.value"
    , "color_name", "color#1.value"
    , "weave_type", "weave_type#1.value"
    , "pattern_name"
    , "department_name", "department#1.value"
    , "lifecycle_supply_type"
    , "item_length_description", "item_length_description#1.value"
    //, "fabric_type", "fabric_type#1.value"
    , "standard_price", "purchasable_offer#1.our_price#1.schedule#1.value_with_tax"
    , "are_batteries_included"
    , "external_product_id"
    , "external_product_id_type"
    , "amzn1.volt.ca.product_id_value"
    , "external_product_type"
    , "amzn1.volt.ca.product_id_type"
    , "outer_material_type", "outer#1.material#1.value"
    , "outer_material_type1"
    , "relationship_type", "child_parent_sku_relationship#1.child_relationship_type"
    , "manufacturer", "manufacturer#1.value"
    , "parent_sku", "child_parent_sku_relationship#1.parent_sku"
    , "relationship_type", "child_parent_sku_relationship#1.child_relationship_type"
    , "merchant_shipping_group#1.value"
    , "item_type_name", "item_type_name#1.value"
    , "special_size_type", "special_size_type#1.value"
    //, "item_type", "item_type"
    , "pattern_name", "pattern_name"
    , "model_name", "model_name#1.value"
    , "model", "model_number#1.value"
    , "part_number", "part_number#1.value"
    , "style_name", "style#1.value"
    , "care_instructions", "care_instructions#1.value"
    , "main_image_url", "main_product_image_locator#1.media_location"
    , "shaft_circumference", "shaft#1.circumference#1.value"
    , "shaft_circumference_unit_of_measure", "shaft#1.circumference#1.unit"
    , "shaft_height", "shaft#1.height#1.value"
    , "shaft_height_unit_of_measure", "shaft#1.height#1.unit"
    , "toe_style", "toe_style#1.value"
    , "leather_type"
    , "target_audience", "target_audience#1.value"
    , "supplier_package_type", "supplier_package_type#1.value"
    , "occasion_type#1.value"
    , "occasion_type"
    , "closure#1.type#1.value"
    , "sandal_type#1.value"
    , "sole_material", "sole_material#1.value"
    , "heel#1.type#1.value"
    , "height_map#1.value"
};

const QMultiHash<QString, QSet<QString>> TemplateMergerFiller::AUTO_SELECT_PATTERN_POSSIBLE_VALUES
= []() -> QMultiHash<QString, QSet<QString>>
{
    QMultiHash<QString, QSet<QString>> pattern_possibleValues;

    for (const auto &key : {"age_range_description", "age_group"})
    {
        pattern_possibleValues.insert(
                    key,
                    QSet<QString>{
                        "Adulte"
                        , "Adult"
                        , "Adult"
                        , "Yetişkin"
                        , "Adulto"
                        , "Erwachsener"
                        , "Volwassene"
                        , "Adulte"
                        , "Adult"
                        , "Adulte"
                        , "Volwassene"
                        , "Vuxen"
                        , "Adulto"
                        , "Dla dorosłych"  // PL-pl
                        , "Erwachsene" // DE
                    }
                    );
    }
    pattern_possibleValues.insert(
                "batteries_required",
                QSet<QString>{
                    "No"
                    , "Non"
                    , "Nee"
                    , "Nej"
                    , "Yok hayır"
                    , "Nein"   // DE-de
                    , "Nie"    // PL-pl
                });


    pattern_possibleValues.insert(
                "body_type",
                QSet<QString>{
                    "Taille normale" // CA-fr, BE-fr; FR-fr: suggested (≈10% wrong)
                    , "Regular" // CA-en, IE-en, COM-en, NL-nl, DE-de, ES-es, BE-nl
                    , "Regelbunden" // SE-se
                    , "Standart" // TR-tr: suggested (≈30% wrong)
                    , "Taglia normale" // IT-it: suggested (≈15% wrong)
                    , "Regularny" // PL-pl
                }
                );

    pattern_possibleValues.insert(
                "closure_type",
                QSet<QString>{
                    "a enfiler"
                    , "Pull-On" // IE-en, COM-en, FR-fr
                    , "Pull-on" // SE-se, NL-nl, ES-es, IT-it, DE-de, UK-en
                    , "Pull On" // PL-pl
                    , "pull_on" // BE-fr
                    , "Slip-On"
                });
    pattern_possibleValues.insert(
                "closure_type",
                QSet<QString>{
                    "Zip"                 // IE-en, SE-se, COM-en, IT-it, UK-en
                    , "Rits"              // NL-nl
                    , "Cremallera"        // ES-es
                    , "Zamek błyskawiczny"// PL-pl
                    , "zip"               // BE-fr
                    , "Éclair"            // FR-fr
                    , "Reißverschluss"    // DE-de
                });
    pattern_possibleValues.insert(
                "closure_type",
                QSet<QString>{
                    "Lace Up" // IE-en
                    , "Lace-Up" // SE-se, COM-en, UK-en
                    , "Veters" // NL-nl
                    , "Cordones" // ES-es
                    , "Zatrzask" // PL-pl → closest match to lace up (80% chance wrong)
                    , "lace_up" // BE-fr
                    , "Stringata" // IT-it
                    , "Lacets" // FR-fr
                    , "Schnürsenkel" // DE-de
                });


    pattern_possibleValues.insert(
                "country_of_origin",
                QSet<QString>{
                    "Chine",   // FR - French
                    "Cina",    // IT - Italian
                    "China",   // ES - Spanish
                    "China",   // EN - English (UK/US/IE/etc.)
                    "china",   // NL - Dutch
                    "Kina",    // SE - Swedish
                    "Chiny",   // PL - Polish
                    "Çin",     // TR - Turkish
                    "China",   // DE - German
                    "中国"      // JP - Japanese
                });
    for (const auto &key : {"footwear_width", "footwear_size#1.width"})
    {
        pattern_possibleValues.insert(
                    key,
                    QSet<QString>{
                        "Medium"    // UK-en, SE-se, IE-en, COM-en; NL-nl, DE-de → closest guess (40% chance wrong)
                        , "Moyen"   // FR-fr; BE-fr → closest guess (35% chance wrong)
                        , "Normale" // IT-it
                        , "Normal" // DE
                        , "Estándar"// ES-es
                        , "Średni"  // PL-pl
                        , "medium"  // Nl
                        , "mediano" // MX-es
                        , "Medium" // COM-en
                        , "M"
                        , "Normaal" // NL
                        , "Orta"
                        , "Media"
                    });
        pattern_possibleValues.insert(
                    key,
                    QSet<QString>{
                        "smal"      // NL-nl
                        , "Narrow"  // IE-en, COM-en, UK-en
                        , "Estrecho"// ES-es
                        , "Smala"   // SE-se
                        , "Wąskie"  // PL-pl
                        , "Étroit"  // FR-fr, BE-fr
                        , "Stretta" // IT-it
                        , "Schmal"  // DE-de
                        , "Estrecho" // MX-es
                        , "Mince" // CA-fr — closest to “étroit”; risk of mismatch ≈20%
                        , "Smal"
                        , "Dar"
                        , "Stretta"
                    });
        for (const auto &key : {"fulfillment_availability#1.fulfillment_channel_code", "fulfillment_center_id"})
        {
            pattern_possibleValues.insert(
                        key,
                        QSet<QString>{
                            "AMAZON_NA"
                            , "AMAZON_EU"
                            , "AMAZON_JP"
                            , "AMAZON_AU"
                        });
        }
        pattern_possibleValues.insert(
                    key,
                    QSet<QString>{
                        "breed"    // NL-nl
                        , "Wide"     // IE-en, COM-en, UK-en
                        , "Ancho"    // ES-es
                        , "Breda"    // SE-se
                        , "Szerokie" // PL-pl
                        , "Large"    // FR-fr, BE-fr
                        , "Larga"    // IT-it
                        , "Weit"     // DE-de
                    });
    }
    pattern_possibleValues.insert(
                "gender",
                QSet<QString>{
                    "Mujer" // MX-es
                    , "Women" // CA-en
                    , "Femme" // CA-fr
                    , "Female" // CA-fr
                    , "Women" // COM-en
                    , "Femenino" // MX
                    , "Féminin"
                    , "Femmina" // MX
                    , "Vrouwelijk"
                    , "Kvinna"
                    , "Weiblich"
                    , "Damen"
                    , "Donna"
                    , "Dames"
                    , "Kobiety"
                    , "Żeńskie"
                    , "Kvinnor"
                    , "Kadın"
                });
    pattern_possibleValues.insert(
                "gender",
                QSet<QString>{
                    "Hombre" // MX-es
                    , "Men" // CA-en
                    , "Male" // CA-en
                    , "Homme" // CA-fr
                    , "Masculino" // MX
                    , "Männlich"
                    , "Masculin"
                    , "Maschio"
                    , "Mannelijk"
                    , "Man"
                    , "Herren"
                    , "Mannen"
                    , "Dla meżczyzn"
                    , "Męskie"
                    , "Män"
                    , "Erkek"
                });

    pattern_possibleValues.insert(
                "height_type",
                QSet<QString>{
                    "Regular" // IE-en
                    , "Standart" // TR-tr: suggested (≈30% wrong)
                    , "Regular" // COM-en
                    , "Regular" // CA-en
                    , "Taille normale" // CA-fr
                    , "Regular" // DE-de
                    , "Regular" // BE-nl
                    , "Taille normale" // BE-fr
                    , "Regolare" // IT-it: suggested (≈20% wrong)
                    , "Regelbunden" // SE-se
                    , "Regular" // NL-nl
                    , "Taille normale" // FR-fr: suggested (≈10% wrong)
                    , "Regular" // ES-es
                    , "Regularny" // PL-pl
                }
                );
    for (const auto &key : {"inner_material_type", "inner#1.material#1.value"})
    {
        pattern_possibleValues.insert(
                    key,
                    QSet<QString>{
                        "Syntetisk"      // SE-se
                        , "Synthetic"    // IE-en, UK-en; COM-en → closest guess (50% chance wrong)
                        , "Synthetisch"  // NL-nl
                        , "Sintético"    // ES-es
                        , "Syntetyczny"  // PL-pl
                        , "Synthétique"  // BE-fr, FR-fr
                        , "Sintetico"    // IT-it
                        , "Synthetik"    // DE-de
                        , "Sentetik"    // TR
                    });
    }
    pattern_possibleValues.insert(
                "leather_type",
                QSet<QString>{
                    "Kunstmatig"       // NL-nl
                    , "Sintetica"        // IT-it
                    , "Artificial"       // UK-en, ES-es; COM-en, IE-en → closest guess (60% chance wrong)
                    , "faux_suede_leather"   // BE-fr → closest guess to “Artificial” (70% chance wrong)
                    , "Artificiel"       // FR-fr
                    , "Artificiell"      // SE-se
                    , "Kunstleder"       // DE-de
                    , "Sztuczna skóra"   // PL-pl → closest guess (70% chance wrong)
                });

    pattern_possibleValues.insert(
                "lifecycle_supply_type",
                QSet<QString>{
                    "Year Round Replenishable" // SE-se, NL-nl, PL-pl, ES-es, IT-it, FR-fr, DE-de, UK-en
                    , "Rechargeable toute l'année" // BE-fr
                });
    for (const auto &key : {"outer_material_type", "outer#1.material#1.value"})
    {
        pattern_possibleValues.insert(
                    key,
                    QSet<QString>{
                        "Synthetic"               // IE-en, COM-en, UK-en; SE-se, NL-nl → closest guess (40% chance wrong)
                        , "synthetic"             // BE-fr
                        , "Sintetico"             // IT-it
                        , "Synthétique"           // FR-fr
                        , "Synthetik"             // DE-de
                        , "Sintético"             // ES-es
                        , "Tworzywo syntetyczne"  // PL-pl
                        , "Synthetisch"  // NL
                        , "Syntet"  // SE
                        , "Synthetic Resin"
                    });
        pattern_possibleValues.insert(
                    key,
                    QSet<QString>{
                        "Polyurethane (PU)" // CA-en, COM-en
                        , "Polyuréthane (PU)" // CA-fr
                        , "Poliuretano (PU)" // MX-es
                        , "Polyurethan (PU)" // DE
                        , "Polyuréthane" // FR
                        , "Polyurethaan (PU)" // NL
                        , "Poliuretan (PU)"
                        , "Polyuretan (PU)"
                        , "Poliüretan (PU)"
                        , "polyurethane_pu"
                    });
        pattern_possibleValues.insert(
                    key,
                    QSet<QString>{
                        "Ante" // ES-es
                        , "Mocka" // SE-se
                        , "Süet" // TR-tr
                        , "Zamsz" // PL-pl
                        , "Suede" // IE-en
                        , "Suede" // UK-en
                        , "Wildleder" // DE-de
                        , "Suède" // NL-nl
                        , "Pelle scamosciata" // IT-it
                        , "Suède" // BE-fr
                        , "Suède" // FR-fr
                    });
        pattern_possibleValues.insert(
                    key,
                    QSet<QString>{
                        "Faux Suede" // UK-en
                        , "Ante sintético" // ES-es
                        , "Pelle scamosciata sintetica" // IT-it
                        , "Konstmocka" // SE-se
                        , "Faux Suede" // IE-en
                        , "Faux suède" // CA-fr
                        , "Faux Suede" // CA-en
                        , "Suni Süet" // TR-tr
                        , "Kunstsuède" // NL-nl
                        , "Faux Suede" // COM-en
                        , "Wildlederimitat" // DE-de
                        , "Faux suède" // FR-fr
                        , "Sztuczny zamsz" // PL-pl
                        , "Ante sintético" // MX-es
                        , "faux_suede" // BE-fr
                    });
    }

    for (const auto &key : {
         "package_height_unit_of_measure"
         , "item_package_dimensions#1.height.unit"
         , "package_width_unit_of_measure"
         , "item_package_dimensions#1.width.unit"
         , "package_length_unit_of_measure"
         , "item_display_length_unit_of_measure"
         , "item_display_dimensions#1.length.unit"
         , "item_display_width_unit_of_measure"
         , "item_display_dimensions#1.width.unit"
         , "item_display_height_unit_of_measure"
         , "item_display_dimensions#1.height.unit"
         , "item_thickness_unit_of_measure"
         , "item_thickness#1.unit"
})
    {
        pattern_possibleValues.insert(
                    key,
                    QSet<QString>{
                        "CM"           // DE-de, UK-en, IT-it, FR-fr, ES-es
                        , "Centimeters"  // IE-en, COM-en
                        , "centimeter"   // NL-nl, SE-se
                        , "centimeters"  // BE-fr
                        , "centymetry"   // PL-pl
                        , "Centymetry"   // PL-pl
                        , "Centimètres"   // PL-pl
                        , "Centimetri" // IT-it
                        , "Zentimeter" // DE-de
                        , "Centímetros" // ES-es
                        , "Centimeter" // NL-nl
                        , "Santimetre"
                        , "Centimetres"
                    });
    }
    for (const auto &key : {"package_weight", "item_package_weight#1.value"})
    {
        pattern_possibleValues.insert(
                    key,
                    QSet<QString>{
                        "GR"     // DE-de, UK-en, IT-it, FR-fr, ES-es
                        , "Gramy"  // PL-pl
                        , "Grams"  // IE-en, COM-en
                        , "Gramos"
                        , "gram"   // NL-nl
                        , "Gram"   // SE-se
                        , "grams"  // BE-fr
                        , "Grammes"
                        , "Gramm"
                        , "Grammi"
                    });
    }

    for (const auto &key : {"parentage_level#1.value", "parent_child"})
    {
        pattern_possibleValues.insert(
                    key,
                    QSet<QString>{
                        "Child"
                        , "Enfant"
                        , "Barn"
                        , "Çocuk"
                        , "Alt"
                        , "Kind"
                        , "Enfant"
                        , "Hijo"
                        , "Onderliggend"
                        , "Dziecko"   // PL-pl
                        , "Bambino"   // IT-it
                        , "Niños"     // MX-es
                        , "child"     // BE-nl / BE-fr
                    });
        pattern_possibleValues.insert(
                    key,
                    QSet<QString>{
                        "Parent"
                        , "Superior"
                        , "Üst Ürün"
                        , "Principal"
                        , "Ouder"
                        , "Ana"
                        , "Bovenliggend"
                        , "Förälder"
                        , "Lverordnad"
                        , "Nadrzędny"
                        , "Eltern"           // DE-de
                        , "Rodzic"           // PL-pl
                        , "Articolo parent"  // IT-it
                        , "Principal."       // MX-es (note the dot)
                        , "parent"           // BE-nl / BE-fr (lowercase)
                    });
    }
    for (const auto &key : {"shaft_height_unit_of_measure", "shaft#1.height#1.unit"})
    {
        pattern_possibleValues.insert(
                    key,
                    QSet<QString>{
                        "centimeter" // SE-se, NL-nl
                        , "CM" // UK-en, FR-fr, IT-it, DE-de, ES-es; COM-en, IE-en, PL-pl → closest guess (30% chance wrong)
                        , "centimeters" // BE-fr
                    });
    }
    for (const auto &key : {"shaft#1.circumference#1.unit", "shaft_circumference_unit_of_measure"})
    {
        pattern_possibleValues.insert(
                    key,
                    QSet<QString>{
                        "Centimeter" // SE-se
                        , "Centimetres" // UK-en
                        , "Centimeters" // COM-en, IE-en, NL-nl; PL-pl → closest guess (40% chance wrong)
                        , "centymetry"   // PL-pl
                        , "Centimètres" // BE-fr, FR-fr
                        , "Centimetri" // IT-it
                        , "Zentimeter" // DE-de
                        , "Centímetros" // ES-es
                    });
    }
    for (const auto &key : {"shaft#1.height#1.value", "shaft_height"})
    {
        pattern_possibleValues.insert(
                    key,
                    QSet<QString>{
                        "Vrist" // SE-se
                        , "Ankle" // UK-en, COM-en, IE-en; DE-de, PL-pl → closest guess (40% chance wrong)
                        , "Cheville" // BE-fr, FR-fr
                        , "cheville" // BE-fr, FR-fr
                        , "Alla caviglia"// IT-it
                        , "Enkel" // NL-nl
                        , "Tobillo" // ES-es
                    });
        pattern_possibleValues.insert(
                    key,
                    QSet<QString>{
                        "Over-The-Knee" // SE-se, UK-en; DE-de, PL-pl → closest guess (45% chance wrong)
                        , "Above the Knee" // COM-en, IE-en
                        , "Au-dessus du genou"// BE-fr
                        , "cuissarde" // FR-fr
                        , "Sopra il ginocchio"// IT-it
                        , "Over de knie" // NL-nl
                        , "Sobre la rodilla" // ES-es
                    });
        pattern_possibleValues.insert(
                    key,
                    QSet<QString>{
                        "Knähög" // SE-se
                        , "Knee-High" // UK-en; DE-de, PL-pl → closest guess (45% chance wrong)
                        , "Knee High" // COM-en, IE-en
                        , "Genou haut" // BE-fr
                        , "genoux" // FR-fr
                        , "Al ginocchio" // IT-it
                        , "Knie-Hoog" // NL-nl
                        , "Altura de rodilla" // ES-es
                    });
    }

    pattern_possibleValues.insert(
                "size_class",
                QSet<QString>{
                    "Alpha" // IE-en
                    , "Alfa" // SE
                    , "Alfabetik" // TR-tr: suggested (≈35% wrong)
                    , "Alpha" // COM-en
                    , "Alpha" // CA-en
                    , "Alpha/lettres" // CA-fr
                    , "Alphanumerisch" // DE-de
                    , "Alfabetisch" // BE-nl
                    , "Alpha/lettres" // BE-fr
                    , "Alfabetico" // IT-it: suggested (≈35% wrong)
                    , "Alfabetisk" // SE-se: suggested (≈35% wrong)
                    , "Alfabetisch" // NL-nl
                    , "Alpha/lettres" // FR-fr: suggested (≈20% wrong)
                    , "Letras" // ES-es
                    , "Testo"  // IT-it
                }
                );
    pattern_possibleValues.insert(
                "size_class",
                QSet<QString>{
                    "Numérique" // FR-fr, CA-fr, BE-fr
                    , "Numeric" // COM-en, CA-en, IE-en
                    , "Numeriek" // NL-nl, BE-nl
                    , "Numerisch" // DE-de
                    , "Números" // ES-es
                    , "Numerisk" // SE-se
                    , "Numerico" // IT-it: suggested (≈40% wrong)
                    , "Sayısal" // TR-tr: suggested (≈40% wrong)
                    , "Numeryczny" // PL-pl
                    , "Numero"     // IT-it
                    , "Numérica"   // MX-es
                    , "Zakres liczbowy" // PL
                    , "Numeriskt"   // SE
                }
                );
    pattern_possibleValues.insert(
                "sole_material",
                QSet<QString>{
                    "Synthetic"            // SE-se, UK-en
                    , "Synthetic Rubber"   // IE-en, COM-en — closest to generic “synthétique” (25% chance wrong)
                    , "Synthetisch"        // NL-nl
                    , "Caucho sintético"   // ES-es — closest to generic “synthétique” (20% chance wrong)
                    , "Kauczuk"            // PL-pl — closest to generic “synthétique” (40% chance wrong)
                    , "synthetic_rubber"   // BE-fr — closest to generic “synthétique” (25% chance wrong)
                    , "Sintetico"          // IT-it
                    , "Synthétique"        // FR-fr
                    , "Synthetik"          // DE-de
                });
    pattern_possibleValues.insert(
                "sole_material",
                QSet<QString>{
                    "Synthetic Rubber" // CA-en, COM-en
                    , "Caoutchouc synthétique" // CA-fr
                    , "Corcho sintético" // MX-es
                });
    /*
    pattern_possibleValues.insert(
                "sole_material",
                QSet<QString>{
                    "Rubber"            // SE-se, UK-en
                    , "Synthetic Rubber"   // IE-en, COM-en — closest to generic “synthétique” (25% chance wrong)
                    , "Synthetisch"        // NL-nl
                    , "Caucho sintético"   // ES-es — closest to generic “synthétique” (20% chance wrong)
                    , "Kauczuk"            // PL-pl — closest to generic “synthétique” (40% chance wrong)
                    , "synthetic_rubber"   // BE-fr — closest to generic “synthétique” (25% chance wrong)
                    , "Sintetico"          // IT-it
                    , "Synthétique"        // FR-fr
                    , "Synthetik"          // DE-de
                });
                //*/


    pattern_possibleValues.insert(
                "supplier_declared_dg_hz_regulation1",
                QSet<QString>{
                    //"",
                    "N/A",
                    "Inte tillämplig",
                    "Not Applicable",       // IT-it, FR-fr, CA-en, IE-en, COM-en, ES-es, DE-de
                    "Ej tillämpbar",        // SE-se
                    "Non applicable",       // CA-fr, BE-fr
                    "Uygun Değil",          // TR-tr
                    "Niet van toepassing"   // NL-nl, BE-nl
                }
                );

    pattern_possibleValues.insert(
                "supplier_declared_material_regulation1",
                QSet<QString>{
                    "Non applicabile",     // IT-it
                    "Ej tillämpbar",       // SE-se — suggested (no list); ~60% confidence
                    "Non applicable",      // FR-fr, CA-fr (+ BE-fr — suggested; ~80%)
                    "Not Applicable",      // CA-en, COM-en (+ IE-en — suggested; ~90%)
                    "Uygulanamaz",         // TR-tr — suggested (no list); ~70%
                    "Nicht zutreffend",    // DE-de, NL-nl
                    "Niet van toepassing", // BE-nl — suggested (no list); ~70%
                    "No aplicable"         // ES-es
                }
                );
    pattern_possibleValues.insert(
                "toe_style",
                QSet<QString>{
                    "Round Toe" // IE-en, COM-en, UK-en
                    , "Rund tå" // SE-se
                    , "Ronde Teen" // NL-nl
                    , "Punta redonda"// ES-es
                    , "Nosek okrągły"// PL-pl
                    , "round_toe" // BE-fr
                    , "Punta rotonda"// IT-it
                    , "Bout Rond" // FR-fr
                    , "Rundzehe" // DE-de
                });
    pattern_possibleValues.insert(
                "toe_style",
                QSet<QString>{
                    "Open Toe"       // IE-en, COM-en, UK-en
                    , "Öppen tå"     // SE-se
                    , "Open teen"    // NL-nl
                    , "Punta abierta"// ES-es
                    , "Nosek odkryty"// PL-pl
                    , "open_toe"     // BE-fr
                    , "Punta aperta" // IT-it
                    , "Bout Ouvert"  // FR-fr
                    , "Offen"        // DE-de
                });
    pattern_possibleValues.insert(
                "toe_style",
                QSet<QString>{
                    "Pointed Toe"  // IE-en, COM-en, UK-en
                    , "Spetsad tå" // SE-se
                    , "Puntige teen" // NL-nl
                    , "De punta"   // ES-es
                    , "W szpic"    // PL-pl
                    , "pointed_toe"// BE-fr
                    , "a punta"    // IT-it
                    , "Bout Pointu"// FR-fr
                    , "Spitzzehe"  // DE-de
                });


    for (const auto &key : {"update_delete"
         , "::record_action"})
    {
        pattern_possibleValues.insert(
                    key,
                    QSet<QString>{
                        "Aanmaken of vervangen (volledige update)"
                        , "Skapa eller ersätt (fullständig uppdatering)"
                        , "Create or Replace (Full Update)"
                        , "Créer ou remplacer (actualisation complète)"
                        , "Erstellen oder Ersetzen (Vollständige Aktualisierung)"
                        , "Actualisation"
                        , "Update"
                        , "Mise à jour complète"
                        , "Aktualisierung"
                        , "Aggiorna"
                        , "Actualización"
                        , "Uppdatera"
                        , "Bijwerken"
                        , "update"
                        , "Aktualizuj"
                        , "Oluştur veya Değiştir (Tam Güncelleme)"
                        , "Créez ou remplacez (mise à jour complète)"
                        , "Utwórz lub zastąp (pełna aktualizacja)"       // PL-pl
                        , "Crea o sostituisci (aggiornamento completo)"  // IT-it
                        , "Créer ou remplacer (mise à jour complète)"    // CA-fr
                        , "Crear o reemplazar (actualización completa)"  // MX-es
                    });
    }

    pattern_possibleValues.insert(
                "variation_theme",
                QSet<QString>{
                    "SizeColor"
                    //, "Size"
                    , "color-size"
                    , "COULEUR/TAILLE"
                    , "TAILLE/COULEUR"
                    , "MAAT/KLEUR"
                    , "KLEUR/MAAT"
                    , "SIZE/COLOR"
                    , "COLOR/SIZE"
                    , "sizecolor"
                    , "ÖLÇÜ/RENK"
                    , "RENK/ÖLÇÜ"
                    , "RENK/BOYUT"
                    , "TAILLE/COULEUR"
                    , "SIZE/COLOUR"      // UK-en
                    , "COLOUR/SIZE"      // UK-en
                    , "GRÖSSE/FARBE"     // DE-de
                    , "FARBE/GRÖSSE"
                    , "ROZMIAR/KOLOR"    // PL-pl
                    , "FORMATO/COLORE"   // IT-it
                    , "COLORE/FORMATO"   // IT-it
                    , "COLORE/DIMENSIONI"
                    , "TAMAÑO/COLOR"     // MX-es
                    , "COLOR/TAMAÑO"
                    , "Nazwa rozmiaru-Nazwa koloru"
                    , "KolorRozmiaru"
                    , "STORLEK/FÄRG"
                    , "FÄRG/STORLEK"
                });



    //supplier_declared_material_regulation1
    //supplier_declared_dg_hz_regulation1
    return pattern_possibleValues;
}();

const QHash<QString, QString> TemplateMergerFiller::MAPPING_FIELD_ID
= []() -> QHash<QString, QString>{
    QHash<QString, QString> _mappingFieldIdTemp{
        {"feed_product_type", "product_type#1.value"}
        , {"item_sku", "contribution_sku#1.value"}
        , {"brand_name", "brand#1.value"}
        , {"::title", "::title"} // Include also the brand in the begin
        , {"::listing_status", "::listing_status"}
        , {"manufacturer", "manufacturer#1.value"}
        , {"toe_style", "toe_style#1.value"}
        , {"toe", "toe"}
        , {"shaft_circumference", "shaft#1.circumference#1.value"}
        , {"shaft_circumference_unit_of_measure", "shaft#1.circumference#1.unit"}
        , {"shaft_height", "shaft#1.height#1.value"}
        , {"shaft_height_unit_of_measure", "shaft#1.height#1.unit"}
        , {"leather_type", "leather_type"}
        , {"sole_material", "sole_material#1.value"}
        , {"collection_name", "collection#1.value"}
        , {"gpsr_safety_attestation", "gpsr_safety_attestation#1.value"}
        , {"offering_start_date", "purchasable_offer#1.start_at.value"}
        , {"offering_end_date", "purchasable_offer#1.end_at.value"}
        , {"purchasable_offer#1.minimum_seller_allowed_price#1.schedule#1.value_with_tax", "purchasable_offer#1.minimum_seller_allowed_price#1.schedule#1.value_with_tax"}
        , {"purchasable_offer#1.maximum_seller_allowed_price#1.schedule#1.value_with_tax", "purchasable_offer#1.maximum_seller_allowed_price#1.schedule#1.value_with_tax"}
        , {"sale_price", "purchasable_offer#1.our_price#1.schedule#1.value_with_tax"}
        , {"product_tax_code", "product_tax_code#1.value"}
        , {"is_expiration_dated_product", "is_expiration_dated_product#1.value"}
        , {"unit_count", "unit_count#1.value"}
        , {"unit_count_type", "unit_count#1.type.value"}
        , {"parent_child", "parentage_level#1.value"}
        , {"quantity", "fulfillment_availability#1.quantity"}
        , {"parent_sku", "child_parent_sku_relationship#1.parent_sku"}
        , {"package_length", "item_package_dimensions#1.length.value"}
        , {"package_width", "item_package_dimensions#1.width.value"}
        , {"package_height", "item_package_dimensions#1.height.value"}

        , {"item_display_length", "item_display_dimensions#1.length.value"}
        , {"item_display_length_unit_of_measure", "item_display_dimensions#1.length.unit"}
        , {"item_display_width", "item_display_dimensions#1.width.value"}
        , {"item_display_width_unit_of_measure","item_display_dimensions#1.width.unit"}
        , {"item_display_height", "item_display_dimensions#1.height.value"}
        , {"item_display_height_unit_of_measure", "item_display_dimensions#1.height.unit"}
        , {"item_length", "item_length_width#1.length.value"}
        , {"item_length_unit_of_measure", "item_length_width#1.length.unit"}
        , {"item_width", "item_length_width#1.width.value"}
        , {"item_width_unit_of_measure", "item_length_width#1.width.unit"}
        , {"item_thickness_derived", "item_thickness#1.decimal_value"}
        , {"item_thickness_unit_of_measure", "item_thickness#1.unit"}
        , {"number_of_boxes", "number_of_boxes#1.value"}
        , {"rug_form_type#1.value", "rug_form_type#1.value"}
        , {"item_length_width#1.length.value", "item_length_width#1.length.value"}
        , {"item_length_width#1.length.unit", "item_length_width#1.length.unit"}
        , {"item_length_width#1.width.value", "item_length_width#1.width.value"}
        , {"item_length_width#1.width.unit", "item_length_width#1.width.unit"}

        , {"package_weight", "item_package_weight#1.value"}
        , {"standard_price", "purchasable_offer#1.our_price#1.schedule#1.value_with_tax"}
        , {"list_price_with_tax", "list_price#1.value_with_tax"}
        , {"list_price", "list_price#1.value"}
        , {"uvp_list_price", "uvp_list_price#1.value"}
        , {"generic_keywords", "generic_keyword#1.value"}
        , {"external_product_id", "amzn1.volt.ca.product_id_value"}
        , {"external_product_id_type", "amzn1.volt.ca.product_id_type"}
        , {"shirt_size", "shirt_size#1.size"}
        , {"shirt_size_to", "shirt_size#1.size_to"}
        , {"apparel_size", "apparel_size#1.size"}
        , {"apparel_size_to", "apparel_size#1.size_to"}
        , {"apparel_size_class", "apparel_size#1.size_class"}
        , {"apparel_size_system", "apparel_size#1.size_system"}
        , {"apparel_body_type", "apparel_size#1.body_type"}
        , {"apparel_height_type", "apparel_size#1.height_type"}
        , {"footwear_size", "footwear_size#1.size"}
        , {"footwear_to_size", "footwear_size#1.to_size"}
        , {"footwear_size_class", "footwear_size#1.size_class"}
        , {"footwear_size_system", "footwear_size#1.size_system"}
        , {"footwear_width", "footwear_size#1.width"}
        , {"shapewear_size", "shapewear_size#1.size"}
        , {"shapewear_size_to", "shapewear_size#1.size_to"}
        , {"shapewear_size_class", "shapewear_size#1.size_class"}
        , {"shapewear_size_system", "shapewear_size#1.size_system"}
        , {"shapewear_body_type", "shapewear_size#1.body_type"}
        , {"shapewear_height_type", "shapewear_size#1.height_type"}
        , {"shirt_size_class", "shirt_size#1.size_class"}
        , {"shirt_size_system", "shirt_size#1.size_system"}
        , {"shirt_body_type", "shirt_size#1.body_type"}
        , {"shirt_height_type", "shirt_size#1.height_type"}
        , {"relationship_type", "child_parent_sku_relationship#1.child_relationship_type"}
        , {"variation_theme", "variation_theme#1.name"}
        , {"package_weight", "item_package_weight#1.value"}
        , {"package_length", "item_package_dimensions#1.length.value"}
        , {"package_width", "item_package_dimensions#1.width.value"}
        , {"package_height", "item_package_dimensions#1.height.value"}
        , {"package_weight_unit_of_measure", "item_package_weight#1.unit"}
        , {"package_height_unit_of_measure", "item_package_dimensions#1.height.unit"}
        , {"package_width_unit_of_measure", "item_package_dimensions#1.width.unit"}
        , {"package_length_unit_of_measure", "item_package_dimensions#1.length.unit"}
        , {"target_gender", "target_gender#1.value"}
        , {"age_range_description", "age_range_description#1.value"}
        , {"color_name", "color#1.value"}
        , {"color_map", "color#1.standardized_values#1"}
        , {"size_map", "size_map"}
        , {"size_name", "size_name"}
        , {"size#1.value", "size#1.value"}
        , {"update_delete", "::record_action"}
        , {"recommended_browse_nodes", "recommended_browse_nodes#1.value"}
        , {"style_name", "style#1.value"}
        , {"condition_type", "condition_type#1.value"}
        , {"batteries_required", "batteries_required#1.value"}
        , {"are_batteries_included", "batteries_included#1.value"}
        , {"care_instructions", "care_instructions#1.value"}
        , {"supplier_declared_dg_hz_regulation1", "supplier_declared_dg_hz_regulation#1.value"}
        , {"supplier_declared_dg_hz_regulation2", "supplier_declared_dg_hz_regulation#2.value"}
        , {"supplier_declared_dg_hz_regulation3", "supplier_declared_dg_hz_regulation#3.value"}
        , {"supplier_declared_dg_hz_regulation4", "supplier_declared_dg_hz_regulation#4.value"}
        , {"supplier_declared_dg_hz_regulation5", "supplier_declared_dg_hz_regulation#5.value"}
        , {"supplier_declared_material_regulation1", "supplier_declared_material_regulation#1.value"}
        , {"eu_toys_safety_directive_age_warning", "eu_toys_safety_directive_age_warning#1.value"}
        , {"item_name", "item_name#1.value"}
        , {"product_description", "product_description#1.value"}
        , {"bullet_point1", "bullet_point#1.value"}
        , {"bullet_point2", "bullet_point#2.value"}
        , {"bullet_point3", "bullet_point#3.value"}
        , {"bullet_point4", "bullet_point#4.value"}
        , {"bullet_point5", "bullet_point#5.value"}
        , {"fit_type", "fit_type#1.value"}
        , {"collar_style", "collar_style#1.value"}
        , {"lifecycle_supply_type", "lifecycle_supply_type"}
        , {"item_length_description", "item_length_description#1.value"}
        , {"lifestyle", "lifestyle#1.value"}
        , {"fabric_type", "fabric_type#1.value"}
        , {"department_name", "department#1.value"}
        , {"currency", "currency"}
        , {"fulfillment_center_id", "fulfillment_availability#1.fulfillment_channel_code"}
        , {"weave_type", "weave_type#1.value"}
        , {"outer_material_type", "outer#1.material#1.value"}
        , {"outer_material_type1", "outer_material_type1"}
        , {"country_of_origin", "country_of_origin#1.value"}
        //, {"item_type", "item_type#1.value"}
        , {"item_type_name", "item_type_name#1.value"}
        , {"special_size_type", "special_size_type#1.value"}
        , {"item_type", "item_type_keyword#1.value"}
        , {"pattern_name", "pattern_name"}
        , {"closure_type", "closure#1.type#1.value"}
        , {"model_name", "model_name#1.value"}
        , {"target_audience", "target_audience#1.value"}
        , {"supplier_package_type", "supplier_package_type#1.value"}
        , {"model", "model_number#1.value"}
        , {"part_number", "part_number#1.value"}
        , {"main_image_url", "main_product_image_locator#1.media_location"}
        , {"other_image_url1", "other_product_image_locator_1#1.media_location"}
        , {"other_image_url2", "other_product_image_locator_2#1.media_location"}
        , {"other_image_url3", "other_product_image_locator_3#1.media_location"}
        , {"other_image_url4", "other_product_image_locator_4#1.media_location"}
        , {"other_image_url5", "other_product_image_locator_5#1.media_location"}
        , {"other_image_url6", "other_product_image_locator_6#1.media_location"}
        , {"other_image_url7", "other_product_image_locator_7#1.media_location"}
        , {"other_image_url8", "other_product_image_locator_8#1.media_location"}
        , {"swatch_image_url1", "swatch_product_image_locator#1.media_location"}
        , {"gpsr_manufacturer_reference_email_address", "gpsr_manufacturer_reference#1.gpsr_manufacturer_email_address"}
        , {"occasion_type", "occasion_type#1.value"}
        , {"merchant_shipping_group_name", "merchant_shipping_group#1.value"}

        , {"fabric_type", "fabric_type#1.value"}
        , {"size_name", "footwear_size#1.size"}
        , {"generic_keywords", "generic_keyword#1.value"}
        //, {"generic_keywords", "target_audience_keyword#1.value"}
        , {"style_name", "style#1.value"}
        , {"target_gender", "target_gender#1.value"}
        , {"target_audience_keyword", "target_audience_keyword#1.value"}

        , {"footwear_age_group", "footwear_size#1.age_group"}
        , {"footwear_size", "footwear_size#1.width"}
        , {"footwear_gender", "footwear_size#1.gender"}
        , {"platform_height_unit_of_measure", "footwear_platform_height#1.unit"} // I guessed platform_height_unit_of_measure
        , {"platform_height", "footwear_platform_height#1.value"}
        , {"lining_description", "lining_description#1.value"}


        , {"offering_can_be_gift_messaged", "gift_options#1.can_be_messaged"}
        , {"offering_can_be_giftwrapped", "gift_options#1.can_be_wrapped"}

        , {"headwear_size", "headwear_size#1.size"}
        , {"headwear_size_class", "headwear_size#1.size_class"}
        , {"headwear_size_system", "headwear_size#1.size_system"}

        , {"heel_height", "heel#1.height#1.decimal_value"}
        , {"heel_height_string", "heel#1.height#1.unit"}
        , {"heel_type", "heel#1.type#1.value"}

        , {"ingredients", "ingredients#1.value"}
        , {"material_composition", "material_composition#1.value"}
        , {"is_heat_sensitive", "is_heat_sensitive#1.value"}

        , {"item_height_unit_of_measure", "item_dimensions#1.height.unit"}
        , {"item_height", "item_dimensions#1.height.value"}
        , {"item_length_unit_of_measure", "item_dimensions#1.length.unit"}
        , {"item_length", "item_dimensions#1.length.value"}
        , {"item_width_unit_of_measure", "item_dimensions#1.width.unit"}
        , {"item_width", "item_dimensions#1.width.value"}

        , {"website_shipping_weight_unit_of_measure", "item_display_weight#1.unit"}
        , {"website_shipping_weight", "item_display_weight#1.value"}

        , {"item_package_quantity", "item_package_quantity#1.value"}
        , {"item_shape", "item_shape#1.value"}

        , {"item_weight_unit_of_measure", "item_weight#1.unit"}
        , {"item_weight", "item_weight#1.value"}

        , {"language_type1", "language#1.type"}
        , {"language_value1", "language#1.value"}
        , {"language_type2", "language#2.type"}
        , {"language_value2", "language#2.value"}
        , {"language_type3", "language#3.type"}
        , {"language_value3", "language#3.value"}
        , {"language_type4", "language#4.type"}
        , {"language_value4", "language#4.value"}
        , {"language_type5", "language#5.type"}
        , {"language_value5", "language#5.value"}

        , {"material_type", "material#1.value"}
        , {"material#2.value", "material#2.value"}
        , {"neck_style", "neck#1.neck_style#1.value"}
        , {"number_of_items", "number_of_items#1.value"}
        , {"product_site_launch_date", "product_site_launch_date#1.value"}

        , {"sale_end_date", "purchasable_offer#1.discounted_price#1.schedule#1.end_at"}
        , {"sale_from_date", "purchasable_offer#1.discounted_price#1.schedule#1.start_at"}
        , {"sale_price", "purchasable_offer#1.discounted_price#1.schedule#1.value_with_tax"}

        , {"sleeve_type", "sleeve#1.type#1.value"}

        , {"water_resistance_level", "water_resistance_level#1.value"}
        , {"binding", "binding#1.value"}
        , {"condition_note", "condition_note#1.value"}
        , {"handbag_silhouette", "handbag_silhouette#1.value"}
        , {"number_of_pieces", "number_of_pieces#1.value"}
        , {"number_of_items", "number_of_items#1.value"}
        , {"pattern_type", "pattern#1.value"}
        , {"cylinder_correction", "cylinder_correction#1.value"}
        , {"lens_addition_power", "lens_addition_power#1.value"}
        , {"product_benefit", "product_benefit#1.value"}
        , {"progressive_discount_type", "purchasable_offer#1.quantity_discount_plan#1.schedule#1.discount_type"}
        , {"progressive_discount_lower_bound1", "purchasable_offer#1.quantity_discount_plan#1.schedule#1.levels#1.lower_bound"}
        , {"progressive_discount_value1", "purchasable_offer#1.quantity_discount_plan#1.schedule#1.levels#1.value"}
        , {"skip_offer", "skip_offer#1.value"}
        , {"special_feature", "special_feature#1.value"}
        , {"strap_type", "strap_type#1.value"}
        , {"warranty_description", "warranty_description#1.value"}

        , {"inner_material_type", "inner#1.material#1.value"}
        , {"sandal_type", "sandal_type#1.value"}
        , {"height_map#1.value", "height_map#1.value"}
    };
    QHash<QString, QString> _mappingFieldId = _mappingFieldIdTemp;
    for (auto it = _mappingFieldIdTemp.begin();
         it != _mappingFieldIdTemp.end(); ++it)
    {
        _mappingFieldId[it.value()] = it.key();
    }
    return _mappingFieldId;
}();

const QHash<QString, QHash<QString, QSet<QString>>> TemplateMergerFiller::COUNTRY_LANG_FIELD_ID_TO_REMOVE
= []() -> QHash<QString, QHash<QString, QSet<QString>>>{
    QHash<QString, QHash<QString, QSet<QString>>> countryCode_langCode_fieldId;
    countryCode_langCode_fieldId["BE"]["NL"].insert("apparel_size#1.body_type");
    for (auto itCountryCode = countryCode_langCode_fieldId.begin();
         itCountryCode != countryCode_langCode_fieldId.end(); ++itCountryCode)
    {
        for (auto itLangCode = itCountryCode.value().begin();
             itLangCode != itCountryCode.value().end(); ++itLangCode)
        {
            auto &fieldIds = itLangCode.value();
            QSet<QString> fieldIdsToAdd;
            for (const auto &fieldId : std::as_const(fieldIds))
            {
                Q_ASSERT(TemplateMergerFiller::MAPPING_FIELD_ID.contains(fieldId));
                const auto &mappedFieldId = TemplateMergerFiller::MAPPING_FIELD_ID[fieldId];
                if (!fieldIds.contains(mappedFieldId))
                {
                    fieldIdsToAdd.insert(mappedFieldId);
                }
            }
            fieldIds.unite(fieldIdsToAdd);
        }
    }
    return countryCode_langCode_fieldId;
}();

const QHash<QString, QStringList> TemplateMergerFiller::FIELD_IDS_COPY_FROM_OTHER
{
        {"size_map", {"apparel_size", "footwear_size", "shapewear_size", "size_name", "size#1.value"}}
        , {"size_name", {"apparel_size", "footwear_size", "shapewear_size", "size_map", "size#1.value"}}
        , {"size#1.value", {"apparel_size", "footwear_size", "shapewear_size", "size_map", "size_name"}}
};

const QHash<QString, QHash<QString, QHash<QString, QHash<QString, QSet<QString>>>>>
TemplateMergerFiller::TYPE_COUNTRY_LANG_FIELD_ID_POSSIBLE_VALUES
= []() -> QHash<QString, QHash<QString, QHash<QString, QHash<QString, QSet<QString>>>>> {
    QHash<QString, QHash<QString, QHash<QString, QHash<QString, QSet<QString>>>>>
            productType_countryCode_langCode_fieldId_possibleValues;
    productType_countryCode_langCode_fieldId_possibleValues["robe"]["SE"]["SE"]["apparel_size#1.size_class"]
            = QSet<QString>{
            "Alfa"
            , "Numerisk"
};
    productType_countryCode_langCode_fieldId_possibleValues["shirt"]["FR"]["FR"]["neck#1.neck_style#1.value"]
            = QSet<QString>{
            "À capuche",
            "À col haut",
            "À une manche",
            "Avec col",
            "Cache coeur",
            "Col asymétrique",
            "Col bateau",
            "Col bénitier",
            "Col bijou",
            "Col carmen",
            "Col carré",
            "Col châle",
            "Col choker",
            "Col cranté",
            "Col fendu",
            "Col keyhole",
            "Col mao",
            "Col marin",
            "Col mock",
            "Col noué",
            "Col rond",
            "Col roulé",
            "Col V",
            "Décolleté en coeur",
            "Dos nu",
            "Encolure dégagée",
            "Henley"
        };
    productType_countryCode_langCode_fieldId_possibleValues["shirt"]["BE"]["FR"]["collar_style"]
            = QSet<QString>{
            "pajama_collar",
                "hidden_button_down_collar",
                "cutaway_collar",
                "tab_collar",
                "club_collar",
                "point_collar",
                "spear_collar",
                "wingtip_collar",
                "mandarin_collar",
                "button_down_collar",
                "extreme_cutaway_collar",
                "spread_collar"
            };

    productType_countryCode_langCode_fieldId_possibleValues["shoes"]["BE"]["FR"]["shaft_height_unit_of_measure"]
            = QSet<QString>{
            "Centimètres"
};
    productType_countryCode_langCode_fieldId_possibleValues["dress"]["SE"]["SE"]["apparel_size#1.size_class"]
            = QSet<QString>{
            "Alpha"
            , "Numerisk"
};
    productType_countryCode_langCode_fieldId_possibleValues["shirt"]["SE"]["SE"]["shirt_size#1.size_class"]
            = QSet<QString>{
            "Alfa"
            , "Numerisk"
};
    productType_countryCode_langCode_fieldId_possibleValues["boot"]["BE"]["FR"]["shaft_height_unit_of_measure"]
            = QSet<QString>{
            "centimeters"
};
    productType_countryCode_langCode_fieldId_possibleValues["boot"]["BE"]["FR"]["shaft_height"]
            = QSet<QString>{
            "Cheville"
            , "Au-dessus du genou"
            , "Mollet haut"
            , "haut"
};
    productType_countryCode_langCode_fieldId_possibleValues["boot"]["DE"]["DE"]["shaft_height"]
            = QSet<QString>{
            "Halbschaft", "Kurzschaft", "Langschaft", "Over-Knee", "Wadenhoch", "Kniehoch", "Über dem Knie", "Knöchel"
};
    productType_countryCode_langCode_fieldId_possibleValues["boot"]["NL"]["NL"]["footwear_width"]
            = QSet<QString>{
            "medium", "smal", "breed"
};
    productType_countryCode_langCode_fieldId_possibleValues["boot"]["DE"]["DE"]["footwear_width"]
            = QSet<QString>{
            "Normal", "Schmal", "Weit"
};
    productType_countryCode_langCode_fieldId_possibleValues["boot"]["BE"]["FR"]["footwear_width"]
            = QSet<QString>{
            "Moyen", "Étroit", "Large"
};
    productType_countryCode_langCode_fieldId_possibleValues["shoes"]["MX"]["ES"]["footwear_size#1.size_class"]
            = QSet<QString>{
            "Medición", "Letras"
};
    productType_countryCode_langCode_fieldId_possibleValues["shoes"]["MX"]["ES"]["footwear_size#1.width"]
            = QSet<QString>{
            "A", "B", "C", "D", "E"
};
    productType_countryCode_langCode_fieldId_possibleValues["shoes"]["CA"]["FR"]["footwear_size#1.width"]
            = productType_countryCode_langCode_fieldId_possibleValues["boot"]["BE"]["FR"]["footwear_width"];
    for (const auto &key : {"boot", "sandal"})
    {
        productType_countryCode_langCode_fieldId_possibleValues[key]["MX"]["ES"]["footwear_size#1.width"]
                = productType_countryCode_langCode_fieldId_possibleValues["shoes"]["MX"]["ES"]["footwear_size#1.width"];
        productType_countryCode_langCode_fieldId_possibleValues[key]["MX"]["ES"]["footwear_size#1.size_class"]
                = productType_countryCode_langCode_fieldId_possibleValues["shoes"]["MX"]["ES"]["footwear_size#1.size_class"];
        productType_countryCode_langCode_fieldId_possibleValues[key]["CA"]["FR"]["footwear_size#1.width"]
                = productType_countryCode_langCode_fieldId_possibleValues["shoes"]["CA"]["FR"]["footwear_size#1.width"];
    }
    for (auto it = productType_countryCode_langCode_fieldId_possibleValues.begin();
         it != productType_countryCode_langCode_fieldId_possibleValues.end(); ++it)
    {
        productType_countryCode_langCode_fieldId_possibleValues[it.key().toUpper()] = it.value();
    }
    return productType_countryCode_langCode_fieldId_possibleValues;
}();

void TemplateMergerFiller::_recordValueAllVersion(
        QHash<QString, QVariant> &fieldId_value, const QString &fieldId, const QVariant &value)
{
    if (fieldId.contains("list_price"))
    {
        for (const auto &id : {"uvp_list_price#1.value", "list_price#1.value", "list_price", "list_price_with_tax", "list_price#1.value_with_tax"})
        {
            fieldId_value[id] = value;
        }
    }
    else if (fieldId != "sole_material#1.value" && fieldId != "sole_material"
             && !fieldId.contains("inner")
             && (fieldId.contains("material#1.value")
             || fieldId.contains("material_type")
             || (fieldId.contains("outer") && fieldId.contains("material"))))
    {
        for (const auto &id : {"material#1.value", "outer_material_type", "outer_material_type1", "outer#1.material#1.value"})
        {
            fieldId_value[id] = value;
        }
    }
    /*
    else if (fieldId.contains("fabric_type"))
    {
        for (const auto &id : {"fabric_type", "outer_material_type1", "fabric_type#1.value"})
        {
            fieldId_value[id] = value;
        }
    }
    //*/
    else if (fieldId.contains("model"))
    {
        for (const auto &id : {"model", "model_number#1.value", "model_name#1.value", "model_name"})
        {
            fieldId_value[id] = value;
        }
    }
    else
    {
        if (!fieldId.contains("image")
                && fieldId.contains("number_of_items")
                && fieldId.contains("part_number")
                && fieldId.contains("item_package_quantity"))
        {
            Q_ASSERT(!value.toString().isEmpty());
        }
        Q_ASSERT(fieldId != "external_product_type");
        Q_ASSERT(!fieldId.isEmpty());
        fieldId_value[fieldId] = value;
        Q_ASSERT(!MAPPING_FIELD_ID[fieldId].isEmpty());
        fieldId_value[MAPPING_FIELD_ID[fieldId]] = value;
    }
    Q_ASSERT(fieldId_value.contains(fieldId));
}

QString TemplateMergerFiller::_getCustomInstructions(const QString &sku) const
{
    QString instructions{m_customInstructions};
    for (auto it = m_skuPattern_customInstructions.begin();
         it != m_skuPattern_customInstructions.end(); ++it)
    {
        const auto &skuPattern = it.key();
        if (sku.contains(skuPattern, Qt::CaseInsensitive))
        {
            instructions += " ";
            instructions += it.value();
        }
    }
    return instructions;
}

GptFiller *TemplateMergerFiller::gptFiller() const
{
    return m_gptFiller;
}

void TemplateMergerFiller::_preFillChildOny()
{
    static QHash<QString, QHash<QString, QSet<QString>>> countryCode_langCode_parent;
    static QSet<QString> fieldIdsAllParents;
    for (auto itCountry = m_countryCode_langCode_fieldIdMandatory.begin();
         itCountry != m_countryCode_langCode_fieldIdMandatory.end(); ++itCountry)
    {
        const auto &countryCode = itCountry.key();
        for (auto itLangCode = itCountry.value().begin();
             itLangCode != itCountry.value().end(); ++itLangCode)
        {
            const auto &langCode = itLangCode.key();
            const auto &fieldIds = itLangCode.value();
            for (const auto &fieldId : fieldIds)
            {
                if (FIELD_IDS_CHILD_ONLY.contains(fieldId))
                {
                    m_countryCode_langCode_fieldIdChildOnly[countryCode][langCode].insert(fieldId);
                }
                else
                {
                    countryCode_langCode_parent[countryCode][langCode].insert(fieldId);
                    fieldIdsAllParents.insert(fieldId);
                }
            }
            qDebug() << "--\nPARENT field ids - The following field id will be filled for the parents:"
                     << countryCode << "-" << langCode << fieldIdsAllParents;
        }
    }
    if (countryCode_langCode_parent.size() > 1)
    {
        qDebug() << "--\nPARENT field ids - The following field id will be filled for the parents:"
                 << fieldIdsAllParents;
    }
}

void TemplateMergerFiller::_preFillTitles()
{
    QHash<QString, QHash<QString, QString>> skuParent_langCode_title;
    for (auto itSku = m_sku_countryCode_langCode_fieldId_value.begin();
         itSku != m_sku_countryCode_langCode_fieldId_value.end(); ++itSku)
    {
        const auto &sku = itSku.key();
        bool isParent = _isSkuParent(sku);
        if (isParent)
        {
            for (auto itCountryCode = itSku.value().begin();
                 itCountryCode != itSku.value().end(); ++itCountryCode)
            {
                const auto &countryCode = itCountryCode.key();
                for (auto itLangCode = itCountryCode.value().begin();
                     itLangCode != itCountryCode.value().end(); ++itLangCode)
                {
                    const auto &langCode = itLangCode.key();
                    auto itFieldId = itLangCode.value().constFind("item_name");
                    if (itFieldId != itLangCode.value().end())
                    {
                        const auto &title = itFieldId.value().toString();
                        skuParent_langCode_title[sku][langCode] = title;
                    }
                }
            }
        }
    }
    for (auto itSku = m_sku_countryCode_langCode_fieldId_value.begin();
         itSku != m_sku_countryCode_langCode_fieldId_value.end(); ++itSku)
    {
        const auto &sku = itSku.key();
        bool isParent = _isSkuParent(sku);
        if (isParent)
        {
            for (auto itCountryCode = itSku.value().begin();
                 itCountryCode != itSku.value().end(); ++itCountryCode)
            {
                const auto &countryCode = itCountryCode.key();
                for (auto itLangCode = itCountryCode.value().begin();
                     itLangCode != itCountryCode.value().end(); ++itLangCode)
                {
                    const auto &langCode = itLangCode.key();
                    auto itFieldId = itLangCode.value().constFind("item_name");
                    if (itFieldId == itLangCode.value().cend()
                            && skuParent_langCode_title.contains(sku)
                            && skuParent_langCode_title[sku].contains(langCode))
                    {
                        const auto &title = skuParent_langCode_title[sku][langCode];
                        _recordValueAllVersion(itLangCode.value(), "item_name", title);
                    }
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

TemplateMergerFiller::TemplateMergerFiller(const QString &filePathFrom,
                                           const QString &customInstructions,
                                           const QString &apiKey,
                                           std::function<void (QString &)> callBackLog)
{
    m_callBackLog = callBackLog;
    m_filePathFrom = filePathFrom;
    if (!customInstructions.trimmed().isEmpty())
    {
        const QStringList &lines = customInstructions.split("\n");
        m_customInstructions = lines[0].trimmed();
        if (!m_customInstructions.endsWith("."))
        {
            m_customInstructions += ".";
        }
        QStringList lastSkus;
        QStringList lastInstructions;
        for (int i=1; i<lines.size(); ++i)
        {
            const auto &line = lines[i];
            if (line.startsWith("["))
            {
                if (!lastSkus.isEmpty() && !lastInstructions.isEmpty())
                {
                    for (const auto &sku : lastSkus)
                    {
                        m_skuPattern_customInstructions[sku] = lastInstructions.join(" ");
                    }
                }
                lastSkus.clear();
                lastInstructions.clear();
                const auto &lineSkus = lines[i].mid(1, line.size()-2);
                const auto &skus = lineSkus.split(",");
                for (const auto &sku : skus)
                {
                    lastSkus << sku.trimmed();
                }
            }
            else if (!lastSkus.isEmpty())
            {
                lastInstructions << line.trimmed();
                if (!lastInstructions.last().endsWith("."))
                {
                    lastInstructions.last() += ".";
                }
            }
        }
        if (!lastSkus.isEmpty() && !lastInstructions.isEmpty())
        {
            for (const auto &sku : lastSkus)
            {
                m_skuPattern_customInstructions[sku] = lastInstructions.join(" ");
            }
        }
    }
    //m_age = UndefinedAge;
    m_age = Age::Adult;
    m_gender = UndefinedGender;
    QDir workingDir{QFileInfo{filePathFrom}.dir()};
    m_workdingDirImages = workingDir.absoluteFilePath("images");
    m_gptFiller = new GptFiller{workingDir.path(), apiKey};
}

TemplateMergerFiller::~TemplateMergerFiller()
{
    m_gptFiller->deleteLater();
}

void TemplateMergerFiller::clearPreviousChatgptReplies()
{
    m_gptFiller->clear();
}


void TemplateMergerFiller::_preFillExcelFiles(
        const QStringList &keywordFilePaths
        , const QStringList &sourceFilePaths
        , const QStringList &toFillFilePaths)
{
    _setFilePathsToFill(keywordFilePaths, toFillFilePaths);
    if (!sourceFilePaths.isEmpty())
    {
        _readInfoSources(sourceFilePaths);
    }
    _checkMinimumValues(m_filePathFrom);
    _checkVarationsNotMissing();
    _fillDataAutomatically();
}

QSet<QString> TemplateMergerFiller::getAllMandatoryFieldIds() const
{
    QSet<QString> mandatoryFieldIds;
    for (auto itCountryCode = m_countryCode_langCode_fieldIdMandatory.begin();
         itCountryCode != m_countryCode_langCode_fieldIdMandatory.end(); ++itCountryCode)
    {
        for (auto itLangCode = itCountryCode.value().begin();
             itLangCode != itCountryCode.value().end(); ++itLangCode)
        {
            mandatoryFieldIds.unite(itLangCode.value());
        }
    }
    mandatoryFieldIds.subtract(FIELD_IDS_PATTERN_REMOVE_AS_MANDATORY);
    return mandatoryFieldIds;
}

const QHash<QString, QHash<QString, QHash<QString, QStringList>>>
&TemplateMergerFiller::countryCode_langCode_fieldId_possibleValues() const
{
    return m_countryCode_langCode_fieldId_possibleValues;
}

void TemplateMergerFiller::preFillExcelFiles(
        const QStringList &keywordFilePaths
        , const QStringList &sourceFilePaths
        , const QStringList &toFillFilePaths
        , std::function<void()> callBackFinishedSuccess
        , std::function<void(const QString &)> callbackFinishedFailure)
{
    _preFillExcelFiles(keywordFilePaths, sourceFilePaths, toFillFilePaths);
    QSet<QString> *mandatoryFieldIds = new QSet<QString>{getAllMandatoryFieldIds()};
    m_gptFiller->askTrueMandatory(m_productType,
                                  *mandatoryFieldIds,
                                  [this, mandatoryFieldIds, callBackFinishedSuccess, callbackFinishedFailure](
                                  const QSet<QString> &notMandatoryFieldIds){
        QString text{QObject::tr("PREFILL - CHATGPT suggested to remove the following field ids as mandatory:\n")};
        QStringList notMandatoryFieldIdsSorted{notMandatoryFieldIds.begin(), notMandatoryFieldIds.end()};
        std::sort(notMandatoryFieldIdsSorted.begin(), notMandatoryFieldIdsSorted.end());
        text += notMandatoryFieldIdsSorted.join("\n");
        text += "\n\nThe following field ids remain:\n";
        auto remainingMandatoryFieldIds = *mandatoryFieldIds;
        remainingMandatoryFieldIds.subtract(notMandatoryFieldIds);
        QStringList remainingMandatoryFieldIdsSorted{remainingMandatoryFieldIds.begin(), remainingMandatoryFieldIds.end()};
        std::sort(remainingMandatoryFieldIdsSorted.begin(), remainingMandatoryFieldIdsSorted.end());
        text += remainingMandatoryFieldIdsSorted.join("\n");
        qDebug() << text;
        m_callBackLog(text);
        auto notMandatoryFieldIdsCopy = notMandatoryFieldIds;
        notMandatoryFieldIdsCopy.subtract(FIELD_IDS_EXTRA_MANDATORY);
        for (auto itCountryCode = m_countryCode_langCode_fieldIdMandatory.begin();
             itCountryCode != m_countryCode_langCode_fieldIdMandatory.end(); ++itCountryCode)
        {
            for (auto itLangCode = itCountryCode.value().begin();
                 itLangCode != itCountryCode.value().end(); ++itLangCode)
            {
                itLangCode.value().subtract(notMandatoryFieldIdsCopy);
            }
        }
        delete mandatoryFieldIds;
        callBackFinishedSuccess();
    }
    , callbackFinishedFailure);
}

void TemplateMergerFiller::fillExcelFiles(
    const QStringList &keywordFilePaths
    , const QStringList &sourceFilePaths
    , const QStringList &toFillFilePaths
    , std::function<void (int, int)> callBackProgress
    , std::function<void ()> callBackFinishedSuccess
    , std::function<void (const QString &)> callbackFinishedFailure)
{
    _preFillExcelFiles(keywordFilePaths, sourceFilePaths, toFillFilePaths);
    QSet<QString> *mandatoryFieldIds = new QSet<QString>{getAllMandatoryFieldIds()};
    m_gptFiller->askTrueMandatory(m_productType,
                                  *mandatoryFieldIds,
                                  [this, mandatoryFieldIds, callBackProgress, callBackFinishedSuccess, callbackFinishedFailure](
                                      const QSet<QString> &notMandatoryFieldIds){
                                      QString text{QObject::tr("CHATGPT suggested to remove the following field ids as mandatory:\n")};
                                      QStringList notMandatoryFieldIdsSorted{notMandatoryFieldIds.begin(), notMandatoryFieldIds.end()};
                                      std::sort(notMandatoryFieldIdsSorted.begin(), notMandatoryFieldIdsSorted.end());
                                      text += notMandatoryFieldIdsSorted.join("\n");
                                      text += "\n\nThe following field ids remain:\n";
                                      auto remainingMandatoryFieldIds = *mandatoryFieldIds;
                                      remainingMandatoryFieldIds.subtract(notMandatoryFieldIds);
                                      QStringList remainingMandatoryFieldIdsSorted{remainingMandatoryFieldIds.begin(), remainingMandatoryFieldIds.end()};
                                      std::sort(remainingMandatoryFieldIdsSorted.begin(), remainingMandatoryFieldIdsSorted.end());
                                      text += remainingMandatoryFieldIdsSorted.join("\n");
                                      qDebug() << text;
                                      m_callBackLog(text);
                                      auto notMandatoryFieldIdsCopy = notMandatoryFieldIds;
                                      notMandatoryFieldIdsCopy.subtract(FIELD_IDS_EXTRA_MANDATORY);
                                      for (auto itCountryCode = m_countryCode_langCode_fieldIdMandatory.begin();
                                           itCountryCode != m_countryCode_langCode_fieldIdMandatory.end(); ++itCountryCode)
                                      {
                                          for (auto itLangCode = itCountryCode.value().begin();
                                               itLangCode != itCountryCode.value().end(); ++itLangCode)
                                          {
                                              //Q_ASSERT(itLangCode.value().contains("main_image_url")
                                              //|| itLangCode.value().contains("main_product_image_locator#1.media_location"));
                                              itLangCode.value().subtract(notMandatoryFieldIdsCopy);
                                              //Q_ASSERT(itLangCode.value().contains("main_image_url")
                                              //|| itLangCode.value().contains("main_product_image_locator#1.media_location"));
                                          }
                                      }
                                      delete mandatoryFieldIds;
                                      _fillDataLeftChatGpt(callBackProgress, callBackFinishedSuccess, callbackFinishedFailure);
                                  }
    , callbackFinishedFailure);
}

void TemplateMergerFiller::fillAiDescOnly(
        const QStringList &keywordFilePaths
        , const QStringList &sourceFilePaths
        , const QStringList &toFillFilePaths
        , std::function<void (int, int)> callBackProgress
        , std::function<void ()> callBackFinishedSuccess
        , std::function<void (const QString &)> callbackFinishedFailure)
{
    _preFillExcelFiles(keywordFilePaths, sourceFilePaths, toFillFilePaths);
    QSet<QString> *mandatoryFieldIds = new QSet<QString>{getAllMandatoryFieldIds()};
    m_gptFiller->askTrueMandatory(m_productType,
                                  *mandatoryFieldIds,
                                  [this, mandatoryFieldIds, callBackProgress, callBackFinishedSuccess, callbackFinishedFailure](
                                      const QSet<QString> &notMandatoryFieldIds){
                                      QString text{QObject::tr("fillAiDescOnly - CHATGPT suggested to remove the following field ids as mandatory:\n")};
                                      QStringList notMandatoryFieldIdsSorted{notMandatoryFieldIds.begin(), notMandatoryFieldIds.end()};
                                      std::sort(notMandatoryFieldIdsSorted.begin(), notMandatoryFieldIdsSorted.end());
                                      text += notMandatoryFieldIdsSorted.join("\n");
                                      text += "\n\nThe following field ids remain:\n";
                                      auto remainingMandatoryFieldIds = *mandatoryFieldIds;
                                      remainingMandatoryFieldIds.subtract(notMandatoryFieldIds);
                                      QStringList remainingMandatoryFieldIdsSorted{remainingMandatoryFieldIds.begin(), remainingMandatoryFieldIds.end()};
                                      std::sort(remainingMandatoryFieldIdsSorted.begin(), remainingMandatoryFieldIdsSorted.end());
                                      text += remainingMandatoryFieldIdsSorted.join("\n");
                                      qDebug() << text;
                                      m_callBackLog(text);
                                      for (auto itCountryCode = m_countryCode_langCode_fieldIdMandatory.begin();
                                           itCountryCode != m_countryCode_langCode_fieldIdMandatory.end(); ++itCountryCode)
                                      {
                                          for (auto itLangCode = itCountryCode.value().begin();
                                               itLangCode != itCountryCode.value().end(); ++itLangCode)
                                          {
                                              itLangCode.value().subtract(notMandatoryFieldIds);
                                          }
                                      }
                                      delete mandatoryFieldIds;
                                      _fillDataLeftChatGptAiDescOnly(callBackProgress, callBackFinishedSuccess, callbackFinishedFailure);
                                  }
    , callbackFinishedFailure);
}

QSet<QString> TemplateMergerFiller::getLangCodesTo() const
{
    QSet<QString> langCodes;
    for (const auto &toFillFilePath : m_toFillFilePaths)
    {
        langCodes << _getLangCode(toFillFilePath);
    }
    return langCodes;
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

void TemplateMergerFiller::_readParentSkus(
        QXlsx::Document &document,
        const QString &countryCode,
        const QString &langCode,
        QMultiHash<QString, QString> &skuParent_skus)
{
    _selectTemplateSheet(document);
    const auto &fieldId_index = _get_fieldId_index(document);
    int indColSku = _getIndColSku(fieldId_index);
    int indColSkuParent = _getIndColSkuParent(fieldId_index);
    const auto &dim = document.dimension();
    int lastRow = dim.lastRow();
    auto version = _getDocumentVersion(document);
    int row = _getRowFieldId(version) + 1;
    for (int i=row; i<lastRow; ++i)
    {
        auto cellSku = document.cellAt(i+1, indColSku + 1);
        if (!cellSku)
        {
            break;
        }
        QString sku{cellSku->value().toString()};
        if (sku.startsWith("ABC"))
        {
            continue;
        }
        auto cellSkuParent = document.cellAt(i+1, indColSkuParent + 1);
        if (cellSkuParent)
        {
            QString skuParent{cellSkuParent->value().toString()};
            if (!skuParent.isEmpty())
            {
                skuParent_skus.insert(skuParent, sku);
            }
        }
    }
}

void TemplateMergerFiller:: _readSkus(QXlsx::Document &document,
                                      const QString &countryCode,
                                      const QString &langCode,
                                      QStringList &skus,
                                      QHash<QString, SkuInfo> &sku_infos,
                                      QHash<QString, QHash<QString, QHash<QString, QHash<QString, QVariant>>>> &sku_countryCode_langCode_fieldId_origValue,
                                      bool isMainFile)
{
    _selectTemplateSheet(document);
    const auto &fieldId_index = _get_fieldId_index(document);
    int indColSku = _getIndColSku(fieldId_index);
    int indColSkuParent = _getIndColSkuParent(fieldId_index);
    int indColColorName = _getIndColColorName(fieldId_index);
    int indColSize = _getIndColSize(fieldId_index);
    int indColTitle = _getIndColTitle(fieldId_index);
    const auto &dim = document.dimension();
    QHash<QString, qsizetype> fieldId_count;
    int nParents = 0;
    auto version = _getDocumentVersion(document);
    int row = _getRowFieldId(version) + 1;
    int lastRow = dim.lastRow();

    QSet<QString> missingFieldIdsSet;
    for (int i=row; i<lastRow; ++i)
    {
        auto cellSku = document.cellAt(i+1, indColSku + 1);
        if (!cellSku)
        {
            break;
        }
        QString sku{cellSku->value().toString()};
        if (sku.startsWith("ABC"))
        {
            Q_ASSERT(!isMainFile);
            break; //it means it is the exemple of an empty file
        }
        if (sku.isEmpty())
        {
            break;
        }
        for (auto it = fieldId_index.cbegin();
             it != fieldId_index.cend(); ++it)
        {
            const auto &fieldId = it.key();
            int indCol = it.value();
            auto cell = document.cellAt(i+1, indCol + 1);
            if (cell)
            {
                if (!MAPPING_FIELD_ID.contains(fieldId) && !missingFieldIdsSet.contains(fieldId))
                {
                    missingFieldIdsSet.insert(fieldId);
                }
            }
        }
    }
    QStringList missingFieldIds{missingFieldIdsSet.begin(), missingFieldIdsSet.end()};
    std::sort(missingFieldIds.begin(), missingFieldIds.end());
    if (missingFieldIdsSet.size() > 0)
    {
        ExceptionTemplateError exception;
        exception.setInfos(QObject::tr("Field ids not supported"),
                           QObject::tr("The following field ids are not supported and need to be added in the mapping:\n %1")
                           .arg(missingFieldIds.join("\n")));
        exception.raise();
    }


    for (int i=row; i<lastRow; ++i)
    {
        auto cellSku = document.cellAt(i+1, indColSku + 1);
        if (!cellSku)
        {
            break;
        }
        QString sku{cellSku->value().toString()};
        if (sku.startsWith("ABC"))
        {
            break; //it means it is the exemple of an empty file
        }
        if (sku.isEmpty())
        {
            break;
        }
        skus << sku;
        if (_isSkuParent(sku))
        {
            ++nParents;
        }
        else
        {
            auto cellSkuParent = document.cellAt(i+1, indColSkuParent + 1);
            auto cellColorName = document.cellAt(i+1, indColColorName + 1);
            auto cellSize = document.cellAt(i+1, indColSize + 1);
            auto cellTitle = document.cellAt(i+1, indColTitle + 1);
            if (cellSkuParent)
            {
                QString skuParent{cellSkuParent->value().toString()};
                sku_infos[sku].skuParent = skuParent;
                sku_infos[sku].customInstructions = _getCustomInstructions(sku);
                if (cellColorName)
                {
                    QString colorName{cellColorName->value().toString()};
                    sku_infos[sku].colorOrig = colorName;
                    QString imageFileName{skuParent};
                    if (!imageFileName.isEmpty())
                    {
                        imageFileName += "-";
                        imageFileName += colorName;
                    }
                    imageFileName += ".jpg";
                    sku_infos[sku].imageFilePath = m_workdingDirImages.absoluteFilePath(imageFileName);
                }
                if (cellSize)
                {
                    QString size{cellSize->value().toString()};
                    sku_infos[sku].sizeOrig = size;
                }
                if (cellTitle && cellSize)
                {
                    QString title{cellTitle->value().toString()};
                    int indPar = title.indexOf("(");
                    if (indPar > 10 && indPar < title.lastIndexOf(")"))
                    {
                        const auto &elements = title.split("(");
                        auto subElements = elements.last().split(")")[0].split(",");
                        sku_infos[sku].sizeTitleOrig = subElements.last().trimmed();
                    }
                }
            }
        }
        Q_ASSERT(fieldId_index.contains("item_name")
                 || fieldId_index.contains("item_name#1.value"));
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
                    if (fieldId == "feed_product_type"
                            || fieldId == "product_type#1.value")
                    {
                        if (!valueString.isEmpty())
                        {
                            m_productType = valueString;
                        }
                    }
                    else if (fieldId == "target_gender"
                             || fieldId == "target_gender#1.value")
                    {
                        static QSet<QString> women{"Féminin", "Female", "Femme"};
                        static QSet<QString> men{"Masculin", "Homme", "Male", "Männlich"};
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
}

void TemplateMergerFiller::_setFilePathsToFill(const QStringList &keywordFilePaths,
                                              const QStringList &toFillFilePaths)
{
    m_toFillFilePaths = toFillFilePaths;
    m_countryCode_langCode_fieldIdMandatory.clear();
    m_countryCode_langCode_fieldName_fieldId.clear();
    m_sku_countryCode_langCode_fieldId_origValue.clear();
    m_skus.clear();
    _readKeywords(keywordFilePaths);

    const auto &countryCodeFrom = _getCountryCode(m_filePathFrom);
    const auto &langCodeFrom = _getLangCode(m_filePathFrom);
    QStringList toFillFilePathsSorted{toFillFilePaths.begin(), toFillFilePaths.end()};
    std::sort(toFillFilePathsSorted.begin(), toFillFilePathsSorted.end());
    toFillFilePathsSorted.insert(0, toFillFilePathsSorted.takeAt(toFillFilePathsSorted.indexOf(m_filePathFrom)));
    for (const auto &toFillFilePath : toFillFilePathsSorted)
    {
        const auto &countryCodeTo = _getCountryCode(toFillFilePath);
        const auto &langCodeTo = _getLangCode(toFillFilePath);
        QXlsx::Document document(toFillFilePath);
        if (countryCodeTo == countryCodeFrom)
        {
            _readParentSkus(document, countryCodeTo, langCodeTo, m_skuParent_skus);
            _readSkus(document, countryCodeTo, langCodeTo, m_skus, m_sku_skuInfos, m_sku_countryCode_langCode_fieldId_origValue, true);
            m_sku_countryCode_langCode_fieldId_value = m_sku_countryCode_langCode_fieldId_origValue;
            Q_ASSERT(m_sku_countryCode_langCode_fieldId_origValue.size() > 0);
        }
        else
        {
            QStringList skus;
            QHash<QString, SkuInfo> sku_skuInfos;
            _readSkus(document, countryCodeTo, langCodeTo, skus, sku_skuInfos, m_sku_countryCode_langCode_fieldId_value);
        }
        _readFields(document, countryCodeTo, langCodeTo);
        _readMandatory(document, countryCodeTo, langCodeTo);
        _preFillChildOny();
        _preFillTitles();
        _readValidValues(document, countryCodeTo, langCodeTo, countryCodeFrom, langCodeFrom);
    }
    /*
    QSet<QString> allSelectFieldIds;
    for (auto itCountryCode = m_countryCode_langCode_fieldId_possibleValues.begin();
         itCountryCode != m_countryCode_langCode_fieldId_possibleValues.end(); ++itCountryCode)
    {
        for (auto itLangCode = itCountryCode.value().cbegin();
             itLangCode != itCountryCode.value().cend(); ++itLangCode)
        {
            for (auto itFieldId = itLangCode.value().cbegin();
                 itFieldId != itLangCode.value().cend(); ++itFieldId)
            {
                allSelectFieldIds.insert(itFieldId.key());
            }
        }
    }
    for (const auto &toFillFilePath : toFillFilePathsSorted)
    {
        const auto &countryCodeTo = _getCountryCode(toFillFilePath);
        const auto &langCodeTo = _getLangCode(toFillFilePath);
        QXlsx::Document document(toFillFilePath);
        const auto &fieldId_index = _get_fieldId_index(document);
        for (auto itFieldId_index = fieldId_index.constBegin();
             itFieldId_index != fieldId_index.constEnd(); ++itFieldId_index)
        {
            const auto &fieldId = itFieldId_index.key();
            if (allSelectFieldIds.contains(fieldId))
            {
                if (!m_countryCode_langCode_fieldId_possibleValues[countryCodeTo][langCodeTo].contains(fieldId))
                {
                    ExceptionTemplateError exception;
                    exception.setInfos("Select field missing",
                                       "In the template of " + countryCodeTo + "/" + langCodeTo + " the following field id doesn't have its possible values: " + fieldId);
                    exception.raise();
                }
            }
        }
    }
    //*/
}

void TemplateMergerFiller::_readInfoSources(const QStringList &sourceFilePaths)
{
    QHash<QString, QHash<QString, QHash<QString, QHash<QString, QVariant>>>> sku_countryCode_langCode_fieldId_origValue;
    const auto &countryCodeFrom = _getCountryCode(m_filePathFrom);
    const auto &langCodeFrom = _getLangCode(m_filePathFrom);
    for (const auto &sourceFilePath : sourceFilePaths)
    {
        const auto &countryCodeTo = _getCountryCode(sourceFilePath);
        const auto &langCodeTo = _getLangCode(sourceFilePath);
        QXlsx::Document document(sourceFilePath);
        QStringList skus;
        QHash<QString, SkuInfo> sku_infos;
        _readSkus(document, countryCodeTo, langCodeTo, skus, sku_infos, sku_countryCode_langCode_fieldId_origValue);
        if (langCodeTo == langCodeFrom && countryCodeFrom == countryCodeTo)
        {
            for (auto it = sku_infos.begin();
                 it != sku_infos.end(); ++it)
            {
                const auto &curSku = it.key();
                if (m_sku_skuInfos.contains(curSku))
                {
                    if (m_sku_skuInfos[curSku].sizeOrig.isEmpty())
                    {
                        m_sku_skuInfos[curSku].sizeOrig = it.value().sizeOrig;
                    }
                    if (m_sku_skuInfos[curSku].colorOrig.isEmpty())
                    {
                        m_sku_skuInfos[curSku].colorOrig = it.value().colorOrig;
                    }
                    if (m_sku_skuInfos[curSku].sizeTitleOrig.isEmpty())
                    {
                        m_sku_skuInfos[curSku].sizeTitleOrig = it.value().sizeTitleOrig;
                    }
                    if (m_sku_skuInfos[curSku].imageFilePath.isEmpty())
                    {
                        m_sku_skuInfos[curSku].imageFilePath = it.value().imageFilePath;
                    }
                }
            }
        }
    }
    QHash<QString, QHash<QString, QHash<QString, QVariant>>> sku_langCode_fieldId_origValue;
    static QSet<QString> fieldIdsLang{
        "product_description", "product_description#1.value"
        , "bullet_point1", "bullet_point#1.value"
        , "bullet_point2", "bullet_point#2.value"
        , "bullet_point3", "bullet_point#3.value"
        , "bullet_point4", "bullet_point#4.value"
        , "bullet_point5", "bullet_point#5.value"
        //, "item_name", "item_name#1.value"
    };
    QHash<QString, QHash<QString, QSet<QString>>> countryCode_langCode_fieldIds;
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
                m_countryCode_sourceSkus[countryCode].insert(sku);
                for (auto itLangCode = itCountryCode.value().begin();
                     itLangCode != itCountryCode.value().end(); ++itLangCode)
                {
                    const auto &langCode = itLangCode.key();
                    for (auto itFieldId_value = itLangCode.value().begin();
                         itFieldId_value != itLangCode.value().end(); ++itFieldId_value)
                    {
                        const auto &fieldId = itFieldId_value.key();
                        if (fieldIdsLang.contains(fieldId))
                        {
                            countryCode_langCode_fieldIds[countryCode][langCode].insert(fieldId);
                            _recordValueAllVersion(sku_langCode_fieldId_origValue[sku][langCode],
                                                   fieldId,
                                                   itFieldId_value.value());
                        }
                        if (!FIELD_IDS_FILLER_NO_SOURCES.contains(fieldId)
                            && !FIELD_IDS_NO_SOURCES.contains(fieldId)
                            && !FIELD_IDS_PUT_FIRST_VALUE.contains(fieldId)
                            && m_countryCode_langCode_fieldIdMandatory.contains(countryCode)
                            && m_countryCode_langCode_fieldIdMandatory[countryCode].contains(langCode)
                            && m_countryCode_langCode_fieldIdMandatory[countryCode][langCode].contains(fieldId))
                        {
                            if (!_isSkuParent(sku) || !m_countryCode_langCode_fieldIdChildOnly[countryCode][langCode].contains(fieldId))
                            {
                                if (!fieldId.contains("parent_child")
                                        && !fieldId.contains("parentage_level")
                                        && !fieldId.contains("variation_theme")
                                        && !fieldId.contains("parent_sku"))
                                {
                                    countryCode_langCode_fieldIds[countryCode][langCode].insert(fieldId);
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
    // Here we also get the data from the orig file that may contains text
    for (auto itSku = m_sku_countryCode_langCode_fieldId_origValue.begin();
         itSku != m_sku_countryCode_langCode_fieldId_origValue.end(); ++itSku)
    {
        const auto &sku = itSku.key();
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
                    if (fieldIdsLang.contains(fieldId))
                    {
                        countryCode_langCode_fieldIds[countryCode][langCode].insert(fieldId);
                        _recordValueAllVersion(sku_langCode_fieldId_origValue[sku][langCode],
                                               fieldId,
                                               itFieldId_value.value());
                    }
                }
            }
        }
    }

    for (auto itSku = sku_countryCode_langCode_fieldId_origValue.begin();
         itSku != sku_countryCode_langCode_fieldId_origValue.end(); ++itSku)
    {
        const auto &sku = itSku.key();
        for (auto itCountryCode = m_countryCode_langCode_fieldIdMandatory.begin(); // Here it won’t read
             itCountryCode != m_countryCode_langCode_fieldIdMandatory.end(); ++itCountryCode)
        {
            const auto &countryCode = itCountryCode.key();
            for (auto itLangCode = itCountryCode.value().begin();
                 itLangCode != itCountryCode.value().end(); ++itLangCode)
            {
                const auto &langCode = itLangCode.key();
                const auto &fieldIds = itLangCode.value();
                for (const auto &fieldId : fieldIds)
                {
                    if (sku_langCode_fieldId_origValue[sku][langCode].contains(fieldId)
                            && !m_sku_countryCode_langCode_fieldId_value[sku][countryCode][langCode].contains(fieldId))
                    {
                        const auto &langValue = sku_langCode_fieldId_origValue[sku][langCode][fieldId];
                        _recordValueAllVersion(m_sku_countryCode_langCode_fieldId_value[sku][countryCode][langCode],
                                               fieldId,
                                               langValue);
                    }
                }
            }
        }
    }
}

void TemplateMergerFiller::_checkMinimumValues(const QString &toFillFilePath)
{
    QXlsx::Document document(toFillFilePath);
    _selectTemplateSheet(document);
    const auto &countryCodeTo = _getCountryCode(toFillFilePath);
    const auto &langCodeTo = _getLangCode(toFillFilePath);
    const auto &fieldId_index = _get_fieldId_index(document);
    int nParents = m_skus.size() - m_skuParent_skus.size();
    auto valid1 = m_skus.size();
    auto valid2 = m_skus.size()-nParents;
    const QSet<qsizetype> validCounts{valid1, valid2};
    auto neededFieldIds = FIELD_IDS_NOT_AI;
    neededFieldIds.unite(FIELD_IDS_AI_BUT_REQUIRED_IN_FIRST);
    QHash<QString, qsizetype> fieldId_count;
    for (auto itSku = m_sku_countryCode_langCode_fieldId_value.begin();
         itSku != m_sku_countryCode_langCode_fieldId_value.end(); ++itSku)
    {
        const auto &sku = itSku.key();
        const auto &fieldId_value = itSku.value()[countryCodeTo][langCodeTo];
        for (const auto &fieldId : neededFieldIds)
        {
            if (fieldId_value.contains(fieldId))
            {
                ++fieldId_count[fieldId];
            }
        }
    }

    for (const auto &fieldId : neededFieldIds)
    {
        static QSet<QString> excludeFieldIds{ //Field ids that can be uncomplete as retrieved from source
            "external_product_id"
            , "amzn1.volt.ca.product_id_value"
            , "external_product_type"
            , "external_product_id_type"
            , "amzn1.volt.ca.product_id_type"
            , "apparel_size#1.size"
            , "apparel_size_to"
            , "apparel_size#1.size_to"
            , "shirt_size#1.size"
            , "shirt_size_to"
            , "shirt_size#1.size_to"
            , "footwear_size"
            , "footwear_to_size"
            , "footwear_size#1.to_size"
            , "generic_keyword#1.value"
        };
        if (excludeFieldIds.contains(fieldId))
        {
            continue;
        }
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
    QStringList skuMissingColors;
    QStringList missingFileNameImages;
    for (auto it = m_sku_skuInfos.begin();
         it != m_sku_skuInfos.end(); ++it)
    {
        const auto &sku = it.key();
        const auto &info = it.value();
        if (!QFile::exists(info.imageFilePath))
        {
            QFileInfo imageFileInfo{info.imageFilePath};
            QString fileName{"images/"};
            fileName += imageFileInfo.fileName();
            if (!missingFileNameImages.contains(fileName))
            {
                missingFileNameImages << fileName;
            }
        }
        if (info.colorOrig.isEmpty())
        {
            if (!skuMissingColors.contains(it.key()))
            {
                skuMissingColors << it.key();
            }
        }
    }
    if (skuMissingColors.size() > 0)
    {
        std::sort(skuMissingColors.begin(), skuMissingColors.end());
        ExceptionTemplateError exception;
        exception.setInfos(QObject::tr("Images missing"),
                           QObject::tr("The following SKUs doesn't have a color name:\n%1")
                           .arg(skuMissingColors.join("\n")));
        exception.raise();
    }
    if (missingFileNameImages.size() > 0)
    {
        std::sort(missingFileNameImages.begin(), missingFileNameImages.end());
        ExceptionTemplateError exception;
        exception.setInfos(QObject::tr("Images missing"),
                           QObject::tr("The following images are missing:\n%1")
                           .arg(missingFileNameImages.join("\n")));
        exception.raise();
    }
    if (m_productType.isEmpty())
    {
        ExceptionTemplateError exception;
        exception.setInfos(QObject::tr("Product type missing"),
                           QObject::tr("The product type is needed"));
        exception.raise();
    }


}

void TemplateMergerFiller::_checkVarationsNotMissing()
{
    const auto &countryCodeFrom = _getCountryCode(m_filePathFrom);
    QHash<QString, int> skuPArent_nColors;
    for (auto itSku = m_sku_countryCode_langCode_fieldId_origValue.begin();
         itSku != m_sku_countryCode_langCode_fieldId_origValue.end(); ++itSku)
    {
        const auto &sku = itSku.key();
        bool isParent = _isSkuParent(sku);
        if (!isParent)
        {
            const auto &infos = m_sku_skuInfos[sku];
            if ((infos.sizeOrig.isEmpty() && !infos.sizeTitleOrig.isEmpty())
                || (!infos.sizeOrig.isEmpty() && infos.sizeTitleOrig.isEmpty()))
            {
                ExceptionTemplateError exception;
                exception.setInfos(QObject::tr("Size error"),
                                   QObject::tr("Sku %1 has missing size informations").arg(sku));
                exception.raise();
            }
            if (!skuPArent_nColors.contains(infos.skuParent))
            {
                skuPArent_nColors[infos.skuParent] = 0;
            }
            if (!infos.colorOrig.isEmpty())
            {
                ++skuPArent_nColors[infos.skuParent];
            }
            const auto &fieldId_origValue = itSku.value().begin().value().begin().value();
            if (!m_countryCode_sourceSkus[countryCodeFrom].contains(sku) && !fieldId_origValue.contains("item_name"))
            {
                ExceptionTemplateError exception;
                exception.setInfos(QObject::tr("Title missing"),
                                   QObject::tr("Sku %1 has missing title").arg(sku));
                exception.raise();
            }
        }
    }
    for (auto it = skuPArent_nColors.begin();
         it != skuPArent_nColors.end(); ++it)
    {
        if (it.value() > 0)
        {
            int nSkusWithColor = it.value();
            const auto &skuParent = it.key();
            int nSkusOfParent = m_skuParent_skus.values(skuParent).size();
            if (nSkusOfParent != nSkusWithColor)
            {
                ExceptionTemplateError exception;
                exception.setInfos(QObject::tr("Color error"),
                                   QObject::tr("Sku parent %1 doesn't have always color information").arg(skuParent));
                exception.raise();
            }
        }
    }
}

void TemplateMergerFiller::_fillDataAutomatically()
{
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
            bool isParent = _isSkuParent(sku);
            for (const auto &fieldId : fieldIdsMandatory)
            {
                if (!isParent || !m_countryCode_langCode_fieldIdChildOnly[countryCodeTo][langCodeTo].contains(fieldId))
                {
                    if (fieldId_index.contains(fieldId))
                    {
                        if (!(m_sku_countryCode_langCode_fieldId_origValue[sku].contains(countryCodeTo)
                              && m_sku_countryCode_langCode_fieldId_origValue[sku][countryCodeTo].contains(langCodeTo)
                              && m_sku_countryCode_langCode_fieldId_origValue[sku][countryCodeTo][langCodeTo].contains(fieldId)))
                        {
                            QVariant origValue;
                            Q_ASSERT(MAPPING_FIELD_ID.contains(fieldId));
                            const auto &mappedFieldId = MAPPING_FIELD_ID[fieldId];
                            if (m_sku_countryCode_langCode_fieldId_origValue[sku][countryCodeFrom][langCodeFrom].contains(fieldId))
                            {
                                origValue = m_sku_countryCode_langCode_fieldId_origValue[sku][countryCodeFrom][langCodeFrom][fieldId];
                            }
                            else if (m_sku_countryCode_langCode_fieldId_origValue[sku][countryCodeFrom][langCodeFrom].contains(mappedFieldId))
                            {
                                origValue = m_sku_countryCode_langCode_fieldId_origValue[sku][countryCodeFrom][langCodeFrom][mappedFieldId];
                            }
                            else if (FIELD_IDS_SIZE.contains(fieldId)) //If it is a  size, we get the orig size value
                            {
                                for (const auto &sizeFieldId : FIELD_IDS_SIZE)
                                {
                                    if (m_sku_countryCode_langCode_fieldId_origValue[sku][countryCodeFrom][langCodeFrom].contains(sizeFieldId))
                                    {
                                        origValue = m_sku_countryCode_langCode_fieldId_origValue[sku][countryCodeFrom][langCodeFrom][sizeFieldId];
                                    }
                                }
                            }
                            if (FIELD_IDS_FILLER_NO_SOURCES.contains(fieldId))
                            {
                                const auto &filler = FIELD_IDS_FILLER_NO_SOURCES[fieldId];
                                const auto &fillerValue = filler(sku,
                                                                 countryCodeFrom,
                                                                 countryCodeTo,
                                                                 langCodeTo,
                                                                 m_countryCode_langCode_keywords,
                                                                 m_skuPattern_countryCode_langCode_keywords,
                                                                 m_gender,
                                                                 m_age,
                                                                 m_productType,
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
                                    if (origValue.isValid()
                                            || fieldId.contains("keyword"))
                                    {
                                        const auto &fillerValue = filler(sku,
                                                                         countryCodeFrom,
                                                                         countryCodeTo,
                                                                         langCodeTo,
                                                                         m_countryCode_langCode_keywords,
                                                                         m_skuPattern_countryCode_langCode_keywords,
                                                                         m_gender,
                                                                         m_age,
                                                                         m_productType,
                                                                         origValue);
                                        _recordValueAllVersion(m_sku_countryCode_langCode_fieldId_value[sku][countryCodeTo][langCodeTo],
                                                               fieldId,
                                                               fillerValue);
                                    }
                                }
                                else
                                { // We field the text if a same lang is in orig value and it is not a title
                                    if (langCodeTo == langCodeFrom
                                            && !fieldId.contains("item_name")
                                            && !m_countryCode_langCode_fieldId_possibleValues[countryCodeTo][langCodeTo].contains(fieldId)
                                            && m_sku_countryCode_langCode_fieldId_origValue[sku][countryCodeFrom][langCodeFrom].contains(fieldId))
                                    {
                                        _recordValueAllVersion(m_sku_countryCode_langCode_fieldId_value[sku][countryCodeTo][langCodeTo],
                                                               fieldId,
                                                               m_sku_countryCode_langCode_fieldId_origValue[sku][countryCodeFrom][langCodeFrom][fieldId]);
                                    }
                                }
                            }
                        }
                    }
                }
            }
            // We do a second round to fill size from other columns
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
                            if (m_sku_countryCode_langCode_fieldId_value[sku][countryCodeTo][langCodeTo].contains(otherFieldId))
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
                                            TemplateMergerFiller::FuncFiller filler;
                                            QVariant origValue = m_sku_countryCode_langCode_fieldId_origValue[sku][countryCodeFrom][langCodeFrom][fieldId];
                                            if (FIELD_IDS_FILLER_NO_SOURCES.contains(otherFieldId))
                                            {
                                                filler = FIELD_IDS_FILLER_NO_SOURCES[otherFieldId];
                                            }
                                            else if (FIELD_IDS_FILLER.contains(otherFieldId))
                                            {
                                                filler = FIELD_IDS_FILLER[otherFieldId];
                                            }
                                            else
                                            {
                                                continue;
                                            }
                                            //const auto &filler = FIELD_IDS_FILLER_NO_SOURCES[otherFieldId];
                                            const auto &fillerValue = filler(sku,
                                                                             countryCodeFrom,
                                                                             countryCodeTo,
                                                                             langCodeTo,
                                                                             m_countryCode_langCode_keywords,
                                                                             m_skuPattern_countryCode_langCode_keywords,
                                                                             m_gender,
                                                                             m_age,
                                                                             m_productType,
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

void TemplateMergerFiller::initGptFiller()
{
    const auto &langCodes  = getLangCodesTo();
    const auto &countryCodeFrom = _getCountryCode(m_filePathFrom);
    const auto &langCodeFrom = _getLangCode(m_filePathFrom);
    m_gptFiller->init(
                countryCodeFrom
                , langCodeFrom
                , m_productType
                , FIELD_IDS_NOT_AI
                , m_sku_skuInfos
                , m_sku_countryCode_langCode_fieldId_origValue
                , m_countryCode_langCode_fieldId_possibleValues
                , m_countryCode_langCode_fieldIdMandatory
                , m_countryCode_langCode_fieldIdChildOnly
                , m_countryCode_langCode_fieldId_hint
                , m_sku_countryCode_langCode_fieldId_value
                );

}

void TemplateMergerFiller::_fillDataLeftChatGpt(
        std::function<void (int, int)> callBackProgress,
        std::function<void ()> callBackFinishedSuccess,
        std::function<void (const QString &)> callBackFinishedError)
{
    initGptFiller();
    m_gptFiller->askFillingTransBullets(
                [this, callBackFinishedSuccess, callBackFinishedError]() {
        m_gptFiller->askProductAiDescriptions(
                    [this, callBackFinishedSuccess, callBackFinishedError]() {
            m_gptFiller->askFillingDescBullets(
                        [this, callBackFinishedSuccess, callBackFinishedError]() {
                m_gptFiller->askFillingSelectsAndTexts(
                            [this, callBackFinishedSuccess, callBackFinishedError]() {
                    m_gptFiller->askFillingTitles(
                                [this, callBackFinishedSuccess]() {
                        _createToFillXlsx();
                        callBackFinishedSuccess();
                    }
                    , [this, callBackFinishedError](const QString &error) {
                        _createToFillXlsx();
                        callBackFinishedError(error);
                    }
                    );
                }
                , [this, callBackFinishedError](const QString &error) {
                    _createToFillXlsx();
                    callBackFinishedError(error);
                }
                );
            }
            , callBackFinishedError
            );
        }
        , callBackFinishedError
        );
    }
    , callBackFinishedError
    );
}

void TemplateMergerFiller::_fillDataLeftChatGptAiDescOnly(
        std::function<void (int, int)> callBackProgress
        , std::function<void ()> callBackFinishedSuccess
        , std::function<void (const QString &)> callBackFinishedError)
{
    initGptFiller();
    m_gptFiller->askProductAiDescriptions(
                callBackFinishedSuccess
                , callBackFinishedError
                );
}

void TemplateMergerFiller::_fixAlwaysSameValue()
{
    //m_sku_countryCode_langCode_fieldId_value;
    //QHash<QString, QHash<QString, QHash<QString, QHash<QString, int>>>>
            //countryCode_langCode_fieldId_parentSku_value_count;
    QHash<QString, QHash<QString, QHash<QString, QHash<QString, int>>>>
            countryCode_langCode_fieldId_value_count;
    for (auto itSku = m_sku_countryCode_langCode_fieldId_value.begin();
         itSku != m_sku_countryCode_langCode_fieldId_value.end(); ++itSku)
    {
        const auto &sku = itSku.key();
        //const auto &skuParent = m_skuParent_skus
        for (auto itCountryCode = itSku.value().begin();
             itCountryCode != itSku.value().end(); ++itCountryCode)
        {
            const auto &countryCode = itCountryCode.key();
            for (auto itLangCode = itCountryCode.value().begin();
                 itLangCode != itCountryCode.value().end(); ++itLangCode)
            {
                const auto &langCode = itLangCode.key();
                for (const auto &fieldId : FIELD_IDS_ALWAY_SAME_VALUE)
                {
                    auto itFieldId = itLangCode.value().constFind(fieldId);
                    if (itFieldId != itLangCode.value().end())
                    {
                        const auto &value = itFieldId.value().toString();
                        if (!value.isEmpty())
                        {
                            if (!countryCode_langCode_fieldId_value_count[countryCode][langCode][fieldId].contains(value))
                            {
                                countryCode_langCode_fieldId_value_count[countryCode][langCode][fieldId][value] = 0;
                            }
                            ++countryCode_langCode_fieldId_value_count[countryCode][langCode][fieldId][value];
                        }
                    }
                }
            }
        }
    }
    //QHash<QString, QHash<QString, QHash<QString, QMap<int, QString>>>> countryCode_langCode_fieldId_count_value;
    QHash<QString, QHash<QString, QHash<QString, QString>>> countryCode_langCode_fieldId_valueUniqueToUse;
    for (auto itCountryCode = countryCode_langCode_fieldId_value_count.begin();
         itCountryCode != countryCode_langCode_fieldId_value_count.end(); ++itCountryCode)
    {
        const auto &countryCode = itCountryCode.key();
        for (auto itLangCode = itCountryCode.value().begin();
             itLangCode != itCountryCode.value().end(); ++itLangCode)
        {
            const auto &langCode = itLangCode.key();
            for (auto itFieldId = itLangCode.value().begin();
                 itFieldId != itLangCode.value().end(); ++itFieldId)
            {
                const auto &fieldId = itFieldId.key();
                if (itFieldId.value().size() > 1)
                {
                    QMap<int, QString> count_value;
                    for (auto itValue_count = itFieldId.value().begin();
                         itValue_count != itFieldId.value().end(); ++itValue_count)
                    {
                        const auto &value = itValue_count.key();
                        const auto &count = itValue_count.value();
                        //countryCode_langCode_fieldId_count_value[countryCode][langCode][fieldId][count] = value
                        count_value[count] = value;
                    }
                    const auto &valueMax = count_value.last();
                    countryCode_langCode_fieldId_valueUniqueToUse[countryCode][langCode][fieldId] = valueMax;
                }
            }
        }
    }
    if (countryCode_langCode_fieldId_valueUniqueToUse.size() > 0)
    {
        for (auto itSku = m_sku_countryCode_langCode_fieldId_value.begin();
             itSku != m_sku_countryCode_langCode_fieldId_value.end(); ++itSku)
        {
            const auto &sku = itSku.key();
            for (auto itCountryCode = itSku.value().begin();
                 itCountryCode != itSku.value().end(); ++itCountryCode)
            {
                const auto &countryCode = itCountryCode.key();
                for (auto itLangCode = itCountryCode.value().begin();
                     itLangCode != itCountryCode.value().end(); ++itLangCode)
                {
                    const auto &langCode = itLangCode.key();
                    if (countryCode_langCode_fieldId_valueUniqueToUse[countryCode][langCode].size() > 1)
                    {
                        for (auto itFieldId_value = countryCode_langCode_fieldId_valueUniqueToUse[countryCode][langCode].begin();
                             itFieldId_value != countryCode_langCode_fieldId_valueUniqueToUse[countryCode][langCode].end(); ++itFieldId_value)
                        {
                            const auto &fieldId = itFieldId_value.key();
                            auto itFieldId = itLangCode.value().constFind(fieldId);
                            if (itFieldId != itLangCode.value().cend())
                            {
                                const auto &valueUniqueToUse = itFieldId_value.value();
                                itLangCode.value()[fieldId] = valueUniqueToUse;
                            }
                        }
                    }
                }
            }
        }
    }
}

void TemplateMergerFiller::_createToFillXlsx()
{
    _fixAlwaysSameValue();
    for (const auto &toFillFilePath : m_toFillFilePaths)
    {
        const auto &countryCodeTo = _getCountryCode(toFillFilePath);
        const auto &langCodeTo = _getLangCode(toFillFilePath);
        QXlsx::Document document(toFillFilePath);
        auto version = _getDocumentVersion(document);
        _selectTemplateSheet(document);
        const auto &fieldId_index = _get_fieldId_index(document);
        int row = _getRowFieldId(version) + 1;
        document.setRowHidden(row, false);
        if (version == V02 && toFillFilePath != m_filePathFrom)
        {
            ++row;
            auto dim = document.dimension();
            for (int j=0; j<dim.columnCount(); ++j)
            {
                auto cell = document.cellAt(row+1, j+1);
                if (cell)
                {
                    auto format = cell->format();
                    document.write(row+1, j+1, QVariant{}, format);
                }
                auto cellNext = document.cellAt(row+2, j+1);
                if (cellNext)
                {
                    auto format = cell->format();
                    document.write(row+2, j+1, QVariant{}, format);
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
        for (auto colInd : std::as_const(allColIndexes))
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

void TemplateMergerFiller::_readKeywords(const QStringList &filePaths)
{
    for (const auto &filePath : filePaths)
    {
        auto patterns = QFileInfo{filePath}.baseName().split("__");
        patterns.takeFirst();
        QFile file{filePath};
        if (file.open(QFile::ReadOnly))
        {
            QTextStream stream{&file};
            auto lines = stream.readAll().split("\n");
            file.close();
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
                    if (patterns.isEmpty())
                    {
                        m_countryCode_langCode_keywords[countryCode][langCode] = lines[i+1];
                    }
                    else
                    {
                        for (const auto &pattern : patterns)
                        {
                            m_skuPattern_countryCode_langCode_keywords[pattern][countryCode][langCode] = lines[i+1];
                        }
                    }
                }
            }
        }
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
    const int colIndHint = 3;
    const int colIndHintSecond = 4;
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
                    || FIELD_IDS_EXTRA_MANDATORY.contains(fieldId)
                    || (PRODUCT_TYPE_FIELD_IDS_EXTRA_MANDATORY.contains(m_productType)
                        && PRODUCT_TYPE_FIELD_IDS_EXTRA_MANDATORY[m_productType].contains(fieldId))
                    )
                {
                    bool toExclude = false;
                    for (const auto &patternToExclude : FIELD_IDS_PATTERN_REMOVE_AS_MANDATORY)
                    {
                        if (fieldId.contains(patternToExclude, Qt::CaseInsensitive))
                        {
                            toExclude = true;
                            break;
                        }
                    }
                    if (!toExclude)
                    {
                        m_countryCode_langCode_fieldIdMandatory[countryCode][langCode].insert(fieldId);
                        auto cellHint = document.cellAt(i+1, colIndHint + 1);
                        auto cellHintSecond = document.cellAt(i+1, colIndHintSecond + 1);
                        QString hint;
                        if (cellHint)
                        {
                            hint += cellHint->value().toString();
                        }
                        if (cellHintSecond)
                        {
                            hint += " Exemple: ";
                            hint += cellHintSecond->value().toString();
                        }
                        if (!hint.isEmpty())
                        {
                            m_countryCode_langCode_fieldId_hint[countryCode][langCode][fieldId] = hint;
                        }
                        if (fieldId.startsWith("bullet") && fieldId.contains("1"))
                        { // We add next bullet point because in the documentation, only the first is described
                            for (int i=2; i<=5; ++i)
                            {
                                QString fieldIdBullet = fieldId;
                                fieldIdBullet.replace("1", QString::number(i));
                                m_countryCode_langCode_fieldIdMandatory[countryCode][langCode].insert(fieldIdBullet);
                            }
                        }
                    }
                }
            }
        }
    }
}

void TemplateMergerFiller::_readValidValues(
        QXlsx::Document &document
        , const QString &countryCodeTo
        , const QString &langCodeTo
        , const QString &countryCodeFrom
        , const QString &langCodeFrom
        )
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
                Q_ASSERT(m_countryCode_langCode_fieldName_fieldId[countryCodeTo][langCodeTo].contains(fieldName));
                if (m_countryCode_langCode_fieldName_fieldId[countryCodeTo][langCodeTo].contains(fieldName))
                {
                    const auto &fieldId = m_countryCode_langCode_fieldName_fieldId[countryCodeTo][langCodeTo][fieldName];
                    for (int j=2; i<dimValidValues.lastColumn(); ++j)
                    {
                        auto cellValue = document.cellAt(i+1, j+1);
                        QString value;
                        if (cellValue)
                        {
                            value = cellValue->value().toString();
                            if (!value.isEmpty())
                            {
                                m_countryCode_langCode_fieldId_possibleValues[countryCodeTo][langCodeTo][fieldId] << value;
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
    if (TYPE_COUNTRY_LANG_FIELD_ID_POSSIBLE_VALUES.contains(m_productType)
            && TYPE_COUNTRY_LANG_FIELD_ID_POSSIBLE_VALUES[m_productType].contains(countryCodeTo)
            && TYPE_COUNTRY_LANG_FIELD_ID_POSSIBLE_VALUES[m_productType][countryCodeTo].contains(langCodeTo))
    {
        for (auto it = TYPE_COUNTRY_LANG_FIELD_ID_POSSIBLE_VALUES[m_productType][countryCodeTo][langCodeTo].begin();
             it != TYPE_COUNTRY_LANG_FIELD_ID_POSSIBLE_VALUES[m_productType][countryCodeTo][langCodeTo].end(); ++it)
        {
            const auto &fieldId = it.key();
            if (!m_countryCode_langCode_fieldId_possibleValues[countryCodeTo][langCodeTo].contains(fieldId))
            {
                m_countryCode_langCode_fieldId_possibleValues[countryCodeTo][langCodeTo][fieldId]
                        = it->values();
            }
        }
    }

    // Fill automatically unique value of possibleValues
    QHash<QString, QHash<QString, QHash<QString, QStringList>>> countryCode_langCode_fieldId_possibleValues;
    //* // COMMENT when filling AUTO_SELECT_PATTERN_POSSIBLE_VALUES from the UE with DialogPossibleValues
    for (auto itFieldId = m_countryCode_langCode_fieldId_possibleValues[countryCodeTo][langCodeTo].begin();
         itFieldId != m_countryCode_langCode_fieldId_possibleValues[countryCodeTo][langCodeTo].end(); ++itFieldId)
    { // We alread fill the values we can deduct, like choice when one choice only, or country from china, to reduce the number of query that will be done by AI
        const auto &fieldId = itFieldId.key();
        if (m_countryCode_langCode_fieldIdMandatory[countryCodeTo][langCodeTo].contains(fieldId))
        {
            const auto &possibleValues = itFieldId.value();
            if (possibleValues.size() == 1
                    || (possibleValues.size() == 2 && possibleValues[0] == possibleValues[1]))
            {
                const auto &uniquePossibleValue = *possibleValues.begin();
                for (auto itSku = m_sku_countryCode_langCode_fieldId_origValue.begin();
                     itSku != m_sku_countryCode_langCode_fieldId_origValue.end(); ++itSku)
                {
                    const auto &sku = itSku.key();
                    bool isParent = _isSkuParent(sku);
                    if (!isParent || !m_countryCode_langCode_fieldIdChildOnly[countryCodeTo][langCodeTo].contains(fieldId))
                    {
                        _recordValueAllVersion(m_sku_countryCode_langCode_fieldId_value[sku][countryCodeTo][langCodeTo], fieldId, uniquePossibleValue);

                    }
                }
            }
            else
            {
                const auto &patternFieldIds = AUTO_SELECT_PATTERN_POSSIBLE_VALUES.uniqueKeys();
                for (const auto &patternFieldId : patternFieldIds)
                {
                    bool isParent = false;
                    QString sku;
                    if (fieldId.contains(patternFieldId))
                    {
                        const auto & listPossibleValues = AUTO_SELECT_PATTERN_POSSIBLE_VALUES.values(patternFieldId);
                        bool found = true;
                        if (countryCodeTo == "CA" && langCodeTo == "EN" && (patternFieldId.contains("outer") || patternFieldId.contains("sole") || patternFieldId.contains("footwear_size#1.width")))
                        {
                            int TEMP=10;++TEMP;
                        }
                        QString lastFromValue;
                        QString lastFieldId;
                        QString lastSku;
                        for (auto itSku = m_sku_countryCode_langCode_fieldId_origValue.begin();
                             itSku != m_sku_countryCode_langCode_fieldId_origValue.end(); ++itSku)
                        {
                            sku = itSku.key();
                            isParent = _isSkuParent(sku);
                            if (!isParent || !m_countryCode_langCode_fieldIdChildOnly[countryCodeTo][langCodeTo].contains(fieldId))
                            {
                                if (m_sku_countryCode_langCode_fieldId_origValue[sku][countryCodeFrom][langCodeFrom].contains(fieldId))
                                {
                                    if (!m_sku_countryCode_langCode_fieldId_value[sku][countryCodeTo][langCodeTo].contains(fieldId))
                                    {
                                        found = false;
                                        const QString &fromValue = m_sku_countryCode_langCode_fieldId_origValue[sku][countryCodeFrom][langCodeFrom][fieldId].toString();
                                        lastFromValue = fromValue;
                                        lastFieldId = fieldId;
                                        lastSku = sku;
                                        for (const auto &valuesEquivalent : listPossibleValues)
                                        {
                                            if (valuesEquivalent.contains(fromValue))
                                            {
                                                for (const auto &possibleValue : possibleValues)
                                                {
                                                    if (valuesEquivalent.contains(possibleValue))
                                                    {
                                                        _recordValueAllVersion(m_sku_countryCode_langCode_fieldId_value[sku][countryCodeTo][langCodeTo], fieldId, possibleValue);
                                                        //m_sku_countryCode_langCode_fieldId_value[sku][countryCodeTo][langCodeTo][fieldId] = possibleValue;
                                                        found = true;
                                                        break;
                                                    }
                                                }
                                            }
                                            if (found)
                                            {
                                                break;
                                            }
                                        }
                                    }
                                }
                            }
                        }
                        if (!found)
                        {
                            qDebug() << "NOT FOUND";
                            qDebug() << listPossibleValues;
                            qDebug() << "lastFromValue:" << lastFromValue;
                            qDebug() << "sku:" << lastSku;
                            qDebug() << "patternFieldId:" << patternFieldId;
                            qDebug() << "fieldId:" << fieldId;
                            qDebug() << "lastFieldId:" << lastFieldId;
                            qDebug() << "Add one of those:" << possibleValues;
                            countryCode_langCode_fieldId_possibleValues[countryCodeTo][langCodeTo][fieldId] = possibleValues;
                        }
                        //Q_ASSERT(found); // Check fromValue / possibleValues and fieldId
                    }
                }
            }
        }
    }
    if (countryCode_langCode_fieldId_possibleValues.size() > 0)
    {
        QStringList errors;
        for (auto itCountry = countryCode_langCode_fieldId_possibleValues.begin();
             itCountry != countryCode_langCode_fieldId_possibleValues.end(); ++itCountry)
        {
            const auto &countryCode = itCountry.key();
            for (auto itLangCode = itCountry.value().begin();
                 itLangCode != itCountry.value().end(); ++itLangCode)
            {
                const auto &langcode = itLangCode.key();
                for (auto itFieldId = itLangCode.value().begin();
                     itFieldId != itLangCode.value().end(); ++itFieldId)
                {
                    const auto &fieldId = itFieldId.key();
                    const auto &possibleValues = itFieldId.value();
                    //std::sort(possibleValues.begin(), possibleValues.end());
                    errors << countryCode + " - " + langcode + " - " + fieldId + " : " + possibleValues.join(", ");
                }
            }
        }
        ExceptionTemplateError exception;
        exception.setInfos("Missing auto attributes",
                           errors.join("\n"));
        exception.raise();
    }
    //*/
}

bool TemplateMergerFiller::_isSkuParent(const QString &sku) const
{
    return m_skuParent_skus.contains(sku);
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
    if (fieldId.contains(" "))
    {
        fieldId = fieldId.split(" ")[0];
    }
    Q_ASSERT(!fieldId.contains("[") && !fieldId.contains("]"));
}

int TemplateMergerFiller::_getIndColSku(
    const QHash<QString, int> &fieldId_index) const
{
    static QStringList possibleValues{"item_sku", "contribution_sku#1.value"};
    return _getIndCol(fieldId_index, possibleValues);
}

int TemplateMergerFiller::_getIndColSkuParent(
    const QHash<QString, int> &fieldId_index) const
{
    static QStringList possibleValues{"parent_sku", "child_parent_sku_relationship#1.parent_sku"};
    return _getIndCol(fieldId_index, possibleValues);
}

int TemplateMergerFiller::_getIndColColorName(
    const QHash<QString, int> &fieldId_index) const
{
    return _getIndCol(fieldId_index, FIELD_IDS_COLOR_NAME);
}

int TemplateMergerFiller::_getIndColSize(
    const QHash<QString, int> &fieldId_index) const
{
    return _getIndCol(fieldId_index, FIELD_IDS_SIZE);
}

int TemplateMergerFiller::_getIndColTitle(
    const QHash<QString, int> &fieldId_index) const
{
    static QStringList possibleValues{"item_name", "item_name#1.value"};
    return _getIndCol(fieldId_index, possibleValues);
}

int TemplateMergerFiller::_getIndCol(
        const QHash<QString, int> &fieldId_index, const QStringList &possibleValues) const
{
    for (const auto &possibleSkuId : possibleValues)
    {
        auto it = fieldId_index.find(possibleSkuId);
        if (it != fieldId_index.end())
        {
            return it.value();
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

