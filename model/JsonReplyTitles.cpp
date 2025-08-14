#include "JsonReplyTitles.h"

const QString JsonReplyTitles::PROMPT
= QString::fromUtf8(
            R"(I need to create a product page for amazon (FBA) with a page template to fill.
This is the description done from another prompt that analysed the product images:
%1
You will suggest natural titles with around 4 local Google SEO keywords / title (no stuffing to keep each title naturals)
Compliance (strict): no medical/therapeutic claims; no competitor mentions; no shipping/price promises; no guarantees/warranties/certifications unless explicitly provided in inputs; avoid #1/best/100% claims.
Titles can't more than 100 characters.
No emoji and no html.
Return your answer only as a single JSON object matching this pattern (no extra keys, no comments, no prose):. Only replace the TODO with the correct values.
This is the input json that contains the languages and countries to do.
%2
Now produce the JSON.)"
);

JsonReplyTitles::JsonReplyTitles(
        const QString &workingDir
        , const QMultiHash<QString, QString> &skuParent_skus
        , const QHash<QString, SkuInfo> *sku_info
        , const QHash<QString, QHash<QString, QSet<QString>>> *countryCode_langCode_fieldIdChildOnly
        , QHash<QString, QHash<QString, QHash<QString, QHash<QString, QVariant>>>> *sku_countryCode_langCode_fieldId_value)
    : JsonReplyAbstract(workingDir, skuParent_skus, sku_info, countryCode_langCode_fieldIdChildOnly, sku_countryCode_langCode_fieldId_value)
{
}

QString JsonReplyTitles::getName() const
{
    return "titles";
}

bool JsonReplyTitles::isJsonReplyCorrect(
        const QString &skuParent
        , const QString &color
        , const QStringList &countryCodes
        , const QStringList &langCodes
        , const QString &
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
                if (!subSubReply.contains("title"))
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

void JsonReplyTitles::record_fieldId_values(
        const QJsonObject &jsonReplyOfOneCountryLang,
        const QString &countryCode,
        const QString &langCode,
        const QString &skuParent,
        const QString &colorOrig,
        const QString &,
        const QHash<QString, QHash<QString, QHash<QString, QString>>> &sku_countryCode_langCode_varTitleInfos) const
{
    for (auto itSku = m_sku_countryCode_langCode_fieldId_value->begin();
         itSku != m_sku_countryCode_langCode_fieldId_value->end(); ++itSku)
    {
        const auto &sku = itSku.key();
        bool isParent = m_skuParent_skus.contains(sku);
        const auto &curSkuParent = isParent ? sku : (*m_sku_infos)[sku].skuParent;
        if (curSkuParent == skuParent)
        {
            auto &countryCode_langCode_fieldId_value = itSku.value();
            for (auto itCountryCode = m_countryCode_langCode_fieldIdChildOnly->begin();
                 itCountryCode != m_countryCode_langCode_fieldIdChildOnly->end(); ++itCountryCode)
            {
                const auto &curCountryCode = itCountryCode.key();
                if (curCountryCode == countryCode)
                {
                    for (auto itLangCode = itCountryCode.value().begin();
                         itLangCode != itCountryCode.value().end(); ++itLangCode)
                    {
                        const auto &curLangCode = itLangCode.key();
                        if (curLangCode == langCode)
                        {
                            const auto &fieldIdsChildOnly = itLangCode.value();
                            static const QSet<QString> titleFieldId{
                                "item_name"
                                , "item_name#1.value"
                            };
                            for (const auto &fieldId : titleFieldId)
                            {
                                QString title;
                                if ((*m_sku_countryCode_langCode_fieldId_value)[skuParent][countryCode][langCode].contains(fieldId))
                                {
                                    title = (*m_sku_countryCode_langCode_fieldId_value)[skuParent][countryCode][langCode][fieldId].toString();
                                }
                                else
                                {
                                    title = jsonReplyOfOneCountryLang["title"].toString();
                                }
                                if (!isParent)
                                {
                                    title += " " + sku_countryCode_langCode_varTitleInfos[sku][countryCode][langCode];
                                }
                                if (!countryCode_langCode_fieldId_value[countryCode][langCode].contains(fieldId))
                                {
                                    countryCode_langCode_fieldId_value[countryCode][langCode][fieldId]
                                            = title;
                                    Q_ASSERT(!(*m_sku_countryCode_langCode_fieldId_value)[sku][countryCode][langCode][fieldId].toString().trimmed().isEmpty());

                                }
                                Q_ASSERT(!(*m_sku_countryCode_langCode_fieldId_value)[sku][countryCode][langCode][fieldId].toString().trimmed().isEmpty());
                            }
                        }
                    }
                }
            }
        }
    }
}
