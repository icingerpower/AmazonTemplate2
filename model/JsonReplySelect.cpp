#include <QJsonDocument>

#include "JsonReplySelect.h"

const QString JsonReplySelect::PROMPT
= QString::fromUtf8(
            R"(I need to create a product page for amazon (FBA) with a page template to fill.
We always use the metric unit system when there is a choice. This is the description done from another prompt that analysed the product images:

%1
%2

Return your answer only as a single JSON object matching the following pattern (no extra keys, no comments, no prose):
%3
You will have to replace each "values" by "value" and select only one value that match the previous details.
Now produce the JSON.)"
);

JsonReplySelect::JsonReplySelect(
        const QString &workingDir
        , const QMultiHash<QString, QString> &skuParent_skus
        , const QHash<QString, SkuInfo> *sku_info
        , const QHash<QString, QHash<QString, QSet<QString>>> *countryCode_langCode_fieldIdChildOnly
        , QHash<QString, QHash<QString, QHash<QString, QHash<QString, QVariant>>>> *sku_countryCode_langCode_fieldId_value
        )
    : JsonReplyAbstract(workingDir, skuParent_skus, sku_info, countryCode_langCode_fieldIdChildOnly, sku_countryCode_langCode_fieldId_value)
{

}

QString JsonReplySelect::getName() const
{
    return "selects";
}

bool JsonReplySelect::isJsonReplyCorrect(
        const QString &skuParent
        , const QString &color
        , const QStringList &countryCodes
        , const QStringList &langCodes
        , const QString &fieldId
        , const QJsonObject &jsonReply) const
{
    bool correctReply = true;
    for (int i=0; i<countryCodes.size(); ++i)
    {
        const auto &countryCode = countryCodes[i];
        const auto &langCode = langCodes[i];
        if (jsonReply.contains(countryCode) && jsonReply[countryCode].isObject())
        {
            const auto &subReply = jsonReply[countryCode].toObject();
            if (subReply.contains(langCode) && subReply[langCode].isObject())
            {
                const auto &subSubReply = subReply[langCode].toObject();
                if (!subSubReply.contains("value"))
                {
                    correctReply = false;
                }
                else if (subSubReply["value"].toString().contains("TODO"))
                {
                    correctReply = false;
                }
            }
            else
            {
                correctReply = false;
            }
        }
        else
        {
            correctReply = false;
        }
    }
    return correctReply;
}

bool JsonReplySelect::isJsonReplyCorrect(
        const QString &skuParent
        , const QString &color
        , const QStringList &countryCodes
        , const QStringList &langCodes
        , const QList<QSet<QString>> &listPossibleValues
        , const QString &fieldId
        , const QString &jsonReply) const
{
    const QJsonDocument &jsonDoc = QJsonDocument::fromJson(_tryToFixJson(jsonReply).toUtf8());
    bool correctReply = true;
    if (jsonDoc.isObject())
    {
        const auto &jsonReply = _getReplyObject(jsonDoc);
        if (isJsonReplyCorrect(skuParent,
                               color,
                               countryCodes,
                               langCodes,
                               fieldId,
                               jsonReply))
        {
            for (int i=0; i<countryCodes.size(); ++i)
            {
                const auto &countryCode = countryCodes[i];
                const auto &langCode = langCodes[i];
                const auto &possibleValues = listPossibleValues[i];
                const auto &subReply = jsonReply[countryCode].toObject();
                const auto &subSubReply = subReply[langCode].toObject();
                const auto &value = subSubReply["value"].toString();
                if (!possibleValues.contains(value))
                {
                    return false;
                }
            }
        }
    }
    else
    {
        return false;
    }
    return true;
}

void JsonReplySelect::record_fieldId_values(
        const QJsonObject &jsonReplyOfOneCountryLang
        , const QString &countryCode
        , const QString &langCode
        , const QString &skuParent
        , const QString &colorOrig
        , const QString &fieldId
        , const QHash<QString, QHash<QString, QHash<QString, QString>>> &) const
{
    const auto &value = jsonReplyOfOneCountryLang["value"].toVariant();
    const auto &fieldIdsChildOnly = (*m_countryCode_langCode_fieldIdChildOnly)[countryCode][langCode];
    for (auto itSku = m_sku_countryCode_langCode_fieldId_value->begin();
         itSku != m_sku_countryCode_langCode_fieldId_value->end(); ++itSku)
    {
        const auto &sku = itSku.key();
        bool isParent = m_skuParent_skus.contains(sku);
        const auto &curSkuParent = isParent ? sku : (*m_sku_infos)[sku].skuParent;
        if (curSkuParent == skuParent)
        {
            auto &countryCode_langCode_fieldId_value = itSku.value();
            if (countryCode_langCode_fieldId_value.contains(countryCode)
                    && countryCode_langCode_fieldId_value[countryCode].contains(langCode))
            {
                if (!isParent || !fieldIdsChildOnly.contains(fieldId))
                {
                    if (!countryCode_langCode_fieldId_value[countryCode][langCode].contains(fieldId))
                    {
                        countryCode_langCode_fieldId_value[countryCode][langCode][fieldId]
                                = value;
                    }
                }
            }
        }
    }
}
