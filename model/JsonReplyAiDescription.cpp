#include <QJsonArray>

#include "JsonReplyAiDescription.h"

const QString JsonReplyAiDescription::PROMPT_DESC_PRODUCT
= QString::fromUtf8(
            R"(I am selling the product of the attached image in amazon (FBA).
Return your answer only as a single JSON object matching exactly this schema (no extra keys, no comments, no prose):
{"COM":{"EN": {
{"description_ai":"TODO"}
,{"title_amz_and_seo":"TODO"}
,{"description_amz_imperial_and_metric":"TODO"}
,{"description_amz_metric_only":"TODO"}
,{"max_description_amz_length":TODO}
,{"bullets_imperial_and_metric":["TODO","TODO","TODO","TODO","TODO"]}
,{"bullets_metric_only":["TODO","TODO","TODO","TODO","TODO"]}
,{"max_bullet_point_length":TODO}
}}}

%1

How to fill each field (strict rules)
Language: Always American English "COM" / "EN".

No hallucinations: Use only what is clearly visible in the image or explicitly stated earlier in this prompt.
If a fact is uncertain, choose the safest best guess and add the tag (assumed) inside description_ai next to that attribute. Only add (assumed) in description_ai and never in any other field.

Materials & claims: Do not claim genuine leather/silk/cotton/wool, certifications, brand names, warranties, or set/pack counts unless unambiguously visible. Avoid medical/therapeutic claims.

Units:
• “_imperial_and_metric” fields must show both units side-by-side (e.g., “2 in / 5 cm”).
• “_metric_only” fields must use metric only (cm, mm, g, kg, °C).
Use decimals with a dot in EN.

Title (title_amz_and_seo): ≤ 100 characters, no color names, natural/classy tone, include 2–3 high-intent keywords relevant to the image (e.g., “cocktail dress”, “evening dress”, “lace bodycon”). No keyword stuffing.

Descriptions (description_amz_*):
HTML allowed only in these two fields. Use <p>, <b>, <ul>, <li>, <br> and stay between 700 and  1500 characters.
Structure:
Hook paragraph for use-cases/occasions.
5 benefit-led bullets (each starts with a bold label via <b>).
Close with care/fit/return-safe facts (no guarantees or pricing/shipping promises).
No mention about return policy.
Include 3–5 relevant SEO keywords naturally once each.

Bullets arrays (bullets_*): exactly 5 strings each, no HTML, > 100 characters and < 300 characters per bullet (please triple check this point).
Bullet 1 = hook/occasion. Bullets 2–5 start with a short benefit label like Fit:, Comfort:, Style:, Care: then an outcome or clothing sizing information when the product has sizes. One emoji at the start of each bullet, and no other emoji in that bullet.

Compliance: No competitor mentions, no “#1/best/100%” claims, no price/shipping/time promises, no medical benefits, no adult content.

What to pack into description_ai (very detailed, machine-readable prose)
Write one compact paragraph that lists every visible attribute so future prompts can map to Amazon fields. Include, when visible or reasonably inferable. You will be asked later to select or fill out the following attributes: %2

Output checks (must-pass)
Return only the JSON object.
Keys and order exactly as in the schema.
Title ≤ 100 chars.
5 bullets in each array; each bullet < 250 chars; first has hook; others use Label: format; exactly one emoji at the start.
HTML used only inside the two description fields and only with allowed tags.
No color words anywhere in the title.
Now analyze the single attached image and produce the JSON.)");

JsonReplyAiDescription::JsonReplyAiDescription(
        const QString &workingDir
        , const QMultiHash<QString, QString> &skuParent_skus
        , const QHash<QString, SkuInfo> *sku_infos
        , const QHash<QString, QHash<QString, QSet<QString>>> *countryCode_langCode_fieldIdChildOnly
        , QHash<QString, QHash<QString, QHash<QString, QHash<QString, QVariant>>>> *sku_countryCode_langCode_fieldId_value)
    : JsonReplyAbstract(workingDir, skuParent_skus, sku_infos, countryCode_langCode_fieldIdChildOnly, sku_countryCode_langCode_fieldId_value)
{
}

void JsonReplyAiDescription::remove(const QString &skuParent, const QString &color)
{
    if (m_skuParent_color_countryCode_langCode_fieldId_jsonObject[skuParent].size() == 1)
    {
        m_skuParent_color_countryCode_langCode_fieldId_jsonObject.remove(skuParent);
    }
    else
    {
        m_skuParent_color_countryCode_langCode_fieldId_jsonObject[skuParent].remove(color);
    }
    save();
}

QHash<QString, QHash<QString, QJsonObject> > JsonReplyAiDescription::get_skuParent_color_jsonReply() const
{
    QHash<QString, QHash<QString, QJsonObject>> skuParent_color_jsonReply;
    for (auto itSkuParent = m_skuParent_color_countryCode_langCode_fieldId_jsonObject.begin();
         itSkuParent != m_skuParent_color_countryCode_langCode_fieldId_jsonObject.end(); ++itSkuParent)
    {
        const auto &skuParent = itSkuParent.key();
        for (auto itColor = itSkuParent.value().begin();
             itColor != itSkuParent.value().end(); ++itColor)
        {
            const auto &color = itColor.key();
            const auto &countryFirst = itColor.value().begin().value();
            const auto &langFirst = countryFirst.begin().value();
            const auto &objectFirst = langFirst.begin().value();
            skuParent_color_jsonReply[skuParent][color] = objectFirst;
        }
    }
    return skuParent_color_jsonReply;
}

QString JsonReplyAiDescription::getName() const
{
    return "ai_descriptions";
}

bool JsonReplyAiDescription::isJsonReplyCorrect(
        const QString &skuParent,
        const QString &color,
        const QStringList &countryCodes,
        const QStringList &langCodes,
        const QString &,
        const QJsonObject &jsonReply) const
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
                bool cond1 = subSubReply.contains("description_ai");
                bool cond2 = subSubReply.contains("title_amz_and_seo");
                bool cond3 = subSubReply.contains("description_amz_imperial_and_metric");
                bool cond4 = subSubReply.contains("description_amz_metric_only");
                bool cond5 = subSubReply.contains("bullets_imperial_and_metric");
                bool cond6 = subSubReply.contains("bullets_metric_only");
                bool cond7 = subSubReply["description_ai"].isString();
                bool cond8 = subSubReply["title_amz_and_seo"].isString();
                bool cond9 = subSubReply["description_amz_imperial_and_metric"].isString();
                bool cond10 = subSubReply["description_amz_metric_only"].isString();
                bool cond11 = subSubReply["bullets_imperial_and_metric"].isArray();
                bool cond12 = subSubReply["bullets_metric_only"].isArray();
                if (cond1 && cond2 && cond3 && cond4 && cond5 && cond6
                        && cond7 && cond8 && cond9 && cond10 && cond11 && cond12)
                {
                    int length_title_amz_and_seo = subSubReply["title_amz_and_seo"].toString().size();
                    int length_description_amz_imperial_and_metric = subSubReply["description_amz_imperial_and_metric"].toString().size();
                    int length_description_amz_metric_only = subSubReply["description_amz_metric_only"].toString().size();
                    const auto &bullets_imperial_and_metric = subSubReply["bullets_imperial_and_metric"].toArray();
                    const auto &bullets_metric_only = subSubReply["bullets_metric_only"].toArray();
                    QList<int> length_bullets_imperial_and_metric;
                    for (const auto &bullet : bullets_imperial_and_metric)
                    {
                        length_bullets_imperial_and_metric << bullet.toString().size();
                    }
                    int max_length_bullets_imperial_and_metric = *std::max_element(
                                length_bullets_imperial_and_metric.begin(), length_bullets_imperial_and_metric.end());
                    QList<int> length_bullets_metric_only;
                    for (const auto &bullet : bullets_metric_only)
                    {
                        length_bullets_metric_only << bullet.toString().size();
                    }
                    int max_length_bullets_metric_only = *std::max_element(
                                length_bullets_metric_only.begin(), length_bullets_metric_only.end());
                    if (length_title_amz_and_seo > 110
                            || length_description_amz_imperial_and_metric > 1500
                            || length_description_amz_metric_only > 1500
                            //|| max_length_bullets_imperial_and_metric > 400 // Always fail so better to ask to shorten while doing the translation
                            //|| max_length_bullets_metric_only > 400
                            || bullets_imperial_and_metric.size() != 5
                            || bullets_metric_only.size() != 5
                            )
                    {
                        correctReply = false;
                    }
                }
                else
                {
                    correctReply = false;
                }
            }
        }
        else
        {
            correctReply = false;
        }
    }
    return correctReply;
}

void JsonReplyAiDescription::record_fieldId_values(
        const QJsonObject &jsonReplyOfOneCountryLang
        , const QString &countryCode
        , const QString &langCode
        , const QString &skuParent
        , const QString &colorOrig
        , const QString &
        , const QHash<QString, QHash<QString, QHash<QString, QString>>> &sku_countryCode_langCode_varTitleInfos) const
{
    const auto &description = jsonReplyOfOneCountryLang["description_amz_imperial_and_metric"].toString();
    const auto &jsonArrayBullets = jsonReplyOfOneCountryLang["bullets_imperial_and_metric"].toArray();
    if (description.size() > 1500)
    {
        return;
    }
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
        if (bullet.size() > 400)
        {
            return;
        }
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
        if (curSkuParent == skuParent && colorOrig == curColorOrig)
        {
            auto &countryCode_langCode_fieldId_value = itSku.value();
            for (auto itCountryCode = m_countryCode_langCode_fieldIdChildOnly->begin();
                 itCountryCode != m_countryCode_langCode_fieldIdChildOnly->end(); ++itCountryCode)
            {
                const auto &curCountryCode = itCountryCode.key();
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
                                if (!countryCode_langCode_fieldId_value[curCountryCode][langCode].contains(fieldId))
                                {
                                    countryCode_langCode_fieldId_value[curCountryCode][langCode][fieldId]
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

QString JsonReplyAiDescription::get_description_ai(
        const QString &skuParent, const QString &color) const
{
    return m_skuParent_color_countryCode_langCode_fieldId_jsonObject[skuParent][color]["COM"]["EN"][QString{}]["description_ai"].toString();
}

QString JsonReplyAiDescription::get_description_amz_imperial_and_metric(
        const QString &skuParent, const QString &color) const
{
    return m_skuParent_color_countryCode_langCode_fieldId_jsonObject[skuParent][color]["COM"]["EN"][QString{}]["description_amz_imperial_and_metric"].toString();
}

QString JsonReplyAiDescription::get_description_amz_metric_only(
        const QString &skuParent, const QString &color) const
{
    return m_skuParent_color_countryCode_langCode_fieldId_jsonObject[skuParent][color]["COM"]["EN"][QString{}]["description_amz_metric_only"].toString();
}

QStringList JsonReplyAiDescription::get_bullets_imperial_and_metric(
        const QString &skuParent, const QString &color) const
{
    QStringList bullets_imperial_and_metric;
    const auto &arrayBullets = m_skuParent_color_countryCode_langCode_fieldId_jsonObject[skuParent][color]["COM"]["EN"][QString{}]["bullets_imperial_and_metric"].toArray();
    for (const auto &bullet : arrayBullets)
    {
        bullets_imperial_and_metric << bullet.toString();
    }
    return bullets_imperial_and_metric;
}

QStringList JsonReplyAiDescription::get_bullets_metric_only(
        const QString &skuParent, const QString &color) const
{
    QStringList bullets_metric_only;
    const auto &arrayBullets = m_skuParent_color_countryCode_langCode_fieldId_jsonObject[skuParent][color]["COM"]["EN"][QString{}]["bullets_metric_only"].toArray();
    for (const auto &bullet : arrayBullets)
    {
        bullets_metric_only << bullet.toString();
    }
    return bullets_metric_only;
}
