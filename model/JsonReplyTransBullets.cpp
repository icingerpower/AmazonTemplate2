#include "JsonReplyTransBullets.h"

const QString JsonReplyTransBullets::PROMPT
= QString::fromUtf8(
            R"(I need to create a product page for amazon (FBA) with a page template to fill.
You will need to translate the given bullet point in the local language, adding SEO keywords (no stuffing to keep the bullet points natural).
Compliance (strict): no medical/therapeutic claims; no competitor mentions; no shipping/price promises; no guarantees/warranties/certifications unless explicitly provided in inputs; avoid #1/best/100% claims.
One emoji at the start of each bullet and no html in bullet points.
For translation to english, add both metric and imperial units without removing words
For translation from english to another language, remove imperial units
This is the bullet points (from language %1) to translate
%2
This is the input json to complete. It contains the languages and countries to do.
Return your answer only as a single JSON object matching this pattern (no extra keys, no comments, no prose).
%3
Now produce the JSON.)"
);

JsonReplyTransBullets::JsonReplyTransBullets(
        const QString &workingDir
        , const QMultiHash<QString, QString> &skuParent_skus
        , const QHash<QString, SkuInfo> *sku_info
        , const QHash<QString, QHash<QString, QSet<QString>>> *countryCode_langCode_fieldIdChildOnly
        , QHash<QString, QHash<QString, QHash<QString, QHash<QString, QVariant>>>> *sku_countryCode_langCode_fieldId_value)
    : JsonReplyAbstract(workingDir, skuParent_skus, sku_info, countryCode_langCode_fieldIdChildOnly, sku_countryCode_langCode_fieldId_value)
{

}

QString JsonReplyTransBullets::getName() const
{
    return "trans_bullets";
}

bool JsonReplyTransBullets::isJsonReplyCorrect(
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
                if (subSubReply.contains(fieldId) && subSubReply[fieldId].isObject())
                {
                    const auto &subSubSubReply = subSubReply[fieldId].toObject();
                    if (subSubSubReply.contains("value"))
                    {
                        const auto &bullet = subSubSubReply["value"].toString();
                        int lengthBullet = bullet.trimmed().size();
                        if (lengthBullet < 8 || lengthBullet > 400)
                        {
                            qDebug() << "JsonReplyTransBullets::isJsonReplyCorrect failed because length is:" << lengthBullet << "-"  << countryCode << "-"  << langCode << "-" << bullet;
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

void JsonReplyTransBullets::record_fieldId_values(
        const QJsonObject &jsonReplyOfOneCountryLang
        , const QString &countryCode
        , const QString &langCode
        , const QString &skuParent
        , const QString &colorOrig
        , const QString &fieldId
        , const QHash<QString, QHash<QString, QHash<QString, QString>>> &sku_countryCode_langCode_varTitleInfos) const
{
    static const QHash<QString, QString> fieldIdsMapping
            = []() -> QHash<QString, QString>{
        QHash<QString, QString> _fieldIdsMapping;
        for (int i=1; i<=5; ++i)
        {
            QString fieldId_V1 = QString{"bullet_point%1"}.arg(i);
            QString fieldId_V2 = QString{"bullet_point#%1.value"}.arg(i);
            _fieldIdsMapping[fieldId_V1] = fieldId_V2;
            _fieldIdsMapping[fieldId_V2] = fieldId_V1;
        }
        return _fieldIdsMapping;
    }();
    for (auto itSku = m_sku_countryCode_langCode_fieldId_value->begin();
         itSku != m_sku_countryCode_langCode_fieldId_value->end(); ++itSku)
    {
        const auto &sku = itSku.key();
        bool isParent = m_skuParent_skus.contains(sku);
        const auto &curSkuParent = isParent ? sku : (*m_sku_infos)[sku].skuParent;
        const auto &curColorOrig = isParent ? QString{} : (*m_sku_infos)[sku].colorOrig;
        if (curSkuParent == skuParent && (colorOrig == curColorOrig || colorOrig.isEmpty()))
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
                            if (!isParent || !fieldIdsChildOnly.contains(fieldId))
                            {
                                if (!countryCode_langCode_fieldId_value[countryCode][langCode].contains(fieldId))
                                {
                                    const auto &object = jsonReplyOfOneCountryLang[fieldId].toObject();
                                    const auto &value = object["value"].toString();
                                    Q_ASSERT(!value.isEmpty());
                                    countryCode_langCode_fieldId_value[countryCode][langCode][fieldId]
                                            = value;
                                    Q_ASSERT(fieldIdsMapping.contains(fieldId));
                                    const auto &fieldIdMapped = fieldIdsMapping[fieldId];
                                    countryCode_langCode_fieldId_value[countryCode][langCode][fieldIdMapped]
                                            = value;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
