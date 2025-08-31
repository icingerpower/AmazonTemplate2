#include "JsonReplyText.h"

const QString JsonReplyText::PROMPT
= QString::fromUtf8(
            R"(I need to create a product page for amazon (FBA) with a page template to fill.
This is the description done from another prompt that analysed the product images:
%1
%2
This is the input json that contains the languages and countries to do with also some hints. The field "documentation_with_exemple" is the documentation from amazon with exemple). If "fromValue" is available, you can just translate the "value" in "fromValue" into the field "value" of the other languages.
Return your answer only as a single JSON object matching the following pattern (no extra keys, no comments, no prose) and replacing the TODO with the correct values.
%3
Now produce the JSON.)"
);

const QString JsonReplyText::PROMPT_TRANSLATE
= QString::fromUtf8(
            R"(I need to create a product page for amazon (FBA) with a page template to fill.
I will give you an input json that contains the languages and countries.
The field "valueFrom" contains "value" which is the value to translate.
Return your answer only as a single JSON object matching the following pattern (no extra keys, no comments, no prose) and replacing the TODO with the correct values.
%1
Now produce the JSON.)"
);

JsonReplyText::JsonReplyText(
        const QString &workingDir
        , const QMultiHash<QString, QString> &skuParent_skus
        , const QHash<QString, SkuInfo> *sku_info
        , const QHash<QString, QHash<QString, QSet<QString>>> *countryCode_langCode_fieldIdChildOnly
        , QHash<QString, QHash<QString, QHash<QString, QHash<QString, QVariant>>>> *sku_countryCode_langCode_fieldId_value)
    : JsonReplySelect(workingDir, skuParent_skus, sku_info, countryCode_langCode_fieldIdChildOnly, sku_countryCode_langCode_fieldId_value)
{

}

QString JsonReplyText::getName() const
{
    return "texts";
}

bool JsonReplyText::isJsonReplyCorrect(
        const QString &skuParent
        , const QString &color
        , const QStringList &countryCodes
        , const QStringList &langCodes
        , const QString &fieldId
        , const QJsonObject &jsonReply) const
{
    bool correctReply = true;
    if (JsonReplySelect::isJsonReplyCorrect(skuParent,
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
            const auto &subReply = jsonReply[countryCode].toObject();
            const auto &subSubReply = subReply[langCode].toObject();
            const auto &value = subSubReply["value"].toString();
            if (value.trimmed().isEmpty())
            {
                return false;
            }
            if (fieldId.contains("color") || fieldId.contains("type") || fieldId.contains("pattern"))
            {
                if (value.size() > 30)
                {
                    return false;
                }
            }
            if (fieldId.contains("material"))
            {
                if (value.size() > 50)
                {
                    return false;
                }
            }
            if (fieldId.contains("style"))
            {
                if (value.size() > 100)
                {
                    return false;
                }
            }
            else if (value.size() > 60)
            {
                int TEMP=10;++TEMP;
            }
            else if (value.size() > 100)
            {
                return false;
            }
        }
    }
    else
    {
        correctReply = false;
    }
    return correctReply;
}
