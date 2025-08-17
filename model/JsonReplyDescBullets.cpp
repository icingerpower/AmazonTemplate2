#include <QJsonArray>

#include "JsonReplyDescBullets.h"

const QString JsonReplyDescBullets::PROMPT
= QString::fromUtf8(
            R"(I need to create a product page for amazon (FBA) with a page template to fill.

You will need to translate and/or fix/shorten the description and 5 given bullet points in the local language, adding SEO keywords (no stuffing to keep the bullet points natural).
Compliance (strict): no medical/therapeutic claims; no competitor mentions; no shipping/price promises; no guarantees/warranties/certifications unless explicitly provided in inputs; avoid #1/best/100% claims. No mention about return policy.
Each description > 500 characters and < 1500 characters (including spaces).
Each bullet > 100 and < 400 characters (including spaces).
One emoji at the start of each bullet and no html in bullet points.
including in the description emoji and HTML using only <p>, <b>, <ul>, <li>, <br>.
This is the description done in english from another prompt that analysed the product images:
%1
Those are the bullets points done in english from the other prompt:
%2
Check those bullets points and correct them if size is not > 100 and < 400. Also fix if the following is not respected or not included:
%3
Return your answer only as a single JSON object matching this pattern (no extra keys, no comments, no prose):.
This is the input json to complete. It contains the languages and countries to do.
%4
Now produce the JSON.)"
);

JsonReplyDescBullets::JsonReplyDescBullets(
        const QString &workingDir
        , const QMultiHash<QString, QString> &skuParent_skus
        , const QHash<QString, SkuInfo> *sku_info
        , const QHash<QString, QHash<QString, QSet<QString>>> *countryCode_langCode_fieldIdChildOnly
        , QHash<QString, QHash<QString, QHash<QString, QHash<QString, QVariant>>>> *sku_countryCode_langCode_fieldId_value)
    : JsonReplyAbstract(workingDir, skuParent_skus, sku_info, countryCode_langCode_fieldIdChildOnly, sku_countryCode_langCode_fieldId_value)
{

}

QString JsonReplyDescBullets::getName() const
{
    return "descBullets";
}

bool JsonReplyDescBullets::isJsonReplyCorrect(
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
                bool containsDescription = subSubReply.contains("description");
                bool containsBullets = subSubReply.contains("bullets");
                if (containsDescription && containsBullets)
                {
                    if (subSubReply["bullets"].isArray())
                    {
                        const auto &array = subSubReply["bullets"].toArray();
                        if (array.size() == 5)
                        {
                            const auto &description = subSubReply["description"].toString();
                            int lengthDesc = description.trimmed().length();
                            if (lengthDesc < 8 || lengthDesc > 1500)
                            {
                                correctReply = false;
                            }
                            else if (description.count("<p>") != description.count("</p>"))
                            {
                                correctReply = false;
                            }
                            else if (description.count("<b>") != description.count("</b>"))
                            {
                                correctReply = false;
                            }
                            else
                            {
                                for (const auto &bullet : array)
                                {
                                    const auto &bulletString = bullet.toString();
                                    int lengthBullet = bulletString.trimmed().size();
                                    if (lengthBullet < 8 || lengthBullet > 400)
                                    {
                                        correctReply = false;
                                        break;
                                    }
                                    else if (bulletString.contains("/>")) // No HTML allowed
                                    {
                                        correctReply = false;
                                        break;
                                    }
                                }
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
        else
        {
            correctReply = false;
        }
    }
    return correctReply;
}

void JsonReplyDescBullets::record_fieldId_values(
        const QJsonObject &jsonReplyOfOneCountryLang
        , const QString &countryCode
        , const QString &langCode
        , const QString &skuParent
        , const QString &colorOrig
        , const QString &
        , const QHash<QString, QHash<QString, QHash<QString, QString>>> &sku_countryCode_langCode_varTitleInfos) const
{
    const auto &description = jsonReplyOfOneCountryLang["description"].toString();
    const auto &jsonArrayBullets = jsonReplyOfOneCountryLang["bullets"].toArray();
    QHash<QString, QString> fieldId_value{
        {"product_description", description}
        , {"product_description#1.value", description}
    };
    static const QSet<QString> bulletPatternFieldIds{
        "bullet_point#%1.value"
        , "bullet_point%1"
    };
    for (int i=0; i<jsonArrayBullets.size(); ++i)
    {
        const auto &bullet = jsonArrayBullets[i].toString();
        for (const auto &bulletPatternFieldId : bulletPatternFieldIds)
        {
            const auto &fieldId = bulletPatternFieldId.arg(i+1);
            fieldId_value[fieldId] = bullet;
        }
    }
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
                            for (auto itFieldId = fieldId_value.begin();
                                 itFieldId != fieldId_value.end(); ++itFieldId)
                            {
                                const auto &fieldId = itFieldId.key();
                                if (!isParent || !fieldIdsChildOnly.contains(fieldId))
                                {
                                    const auto &value = itFieldId.value();
                                    Q_ASSERT(!value.isEmpty());
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
            }
        }
    }
}
