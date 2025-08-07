#include <QSaveFile>
#include <QJsonArray>
#include <QJsonDocument>

#include "GptFiller.h"

#include "../common/openai/OpenAi.h"

const int GptFiller::N_RETRY{5};

const QString GptFiller::PROMPT_FIRST{
    "I am selling the product of the attached image in amazon (FBA). "
    "You need to help me fill the product page template. "
    "The product is new, from China, without hazmat materials. "
    "We use international metric system. "
    "Inventory is Year round replenishable. "
    "For each field, I will send you json informations. "
    "You need to reply as json. Only return as reply the content of the "
    "json object \"return\" replacing TODO by the field values. "
    "If You are not sure guess the best choice as you can only"
    " reply in json without adding any details or explanation."
};

const QString GptFiller::PROMPT_INTRODUCE_JSON{
    "Here is the first question that I am asking in json format:"
};

const QString GptFiller::PROMPT_ASK_NOT_MANDATORY{
    "I am create product page on amazon with a page template."
    " The product category is %1. Can you tell me,"
    " among those attributes marked as mandatory,"
    " which ones are in fact not mandatory (Usually "
    "attributes related to batterie, regulation and liquid) for %1:\n%2\n"
    "You need to reply in with a json list, in the following format: "
    "[\"field1\", \"field2\"]"
};

const QString GptFiller::PROMPT_TEXT_START{
    "I am selling the product of the attached image in amazon (FBA). "
};

const QString GptFiller::PROMPT_TEXT_END_DESCRIPTION{
    "Write an Amazon product description of ≤1500 characters including emoji and HTML using only <p>, <b>, <ul>, <li>, <br>."
            "\nStructure:"
                        "\n<p> Hook: connect with the buyer and mention occasions/uses."
        "\n<ul> 5 benefit-led bullets (each starts with a bold benefit label)."
                               "\n<p> Close with reassurance/compatibility/care (facts only)."
                                                 "\nTone: confident, benefit-oriented, no hype, no unverifiable superlatives."
                                         "\nCompliance: no medical/therapeutic claims, no competitor mentions, no shipping/price promises, no guarantees unless explicitly provided."
                                                              "\nSEO: Find SEO keywords and include them naturally once (no stuffing)."
                                                                    "\nIf any required input is missing, omit it—do not invent."
        "\nBefore returning, self-check: character count ≤1500; only allowed tags; bullets are benefit-led and policy-safe. Return only the HTML."
                                           "\nPlease reply in the following language: %1"
                        "\nReply with a json object in the following json format (one key per lang):"
    "{\"%1\":{\"description\":\"TODO\", \"charCount\": 1255, \"policy_ok\": true, \"used_keywords\":\"TODO\"}}"
    //",\"DE\":{\"description\":\"TODO\", \"langCode\": \"de\", \"charCount\": 1255, \"policy_ok\": true, \"used_keywords\":\"TODO\"}}"
};

const QString GptFiller::PROMPT_TEXT_END_BULLET_POINTS{
    "\nWrite exactly FIVE Amazon \"Key Product Features\" bullet points."
    "\nConstraints:"
    "\n- Each bullet ≤ 250 characters (including spaces)."
    "\n- Each bullet starts with one emoji and can't have more than one."
    "\n- Hook: The first bullet connect with the buyer and mention occasions."
    "\n- Start each other bullet with a short <Benefit label>: then a clear outcome (e.g., \"Easy care: Machine-wash cold\")."
    "\n- Use concise, skimmable phrasing; sentence fragments are fine."
    "\nTone: confident, benefit-oriented, factual; no hype or unverifiable superlatives."
    "\nCompliance (strict): no medical/therapeutic claims; no competitor mentions; no shipping/price promises; no guarantees/warranties/certifications unless explicitly provided in inputs; avoid #1/best/100% claims."
    "\nUse local decimal separators and units (e.g., metric for France, imperial for US) according to locale. "
    "\nSEO: Find SEO keywords and include them naturally ONCE across the five bullets (no stuffing)."
    "\nImage vs text: Use the image only for high-level cues. If a fact isn’t in the inputs, omit it—do not invent."
    "\nBefore returning, SELF-CHECK for each language: exactly 5 bullets; each ≤ 200 chars; no HTML; policy-safe; keywords used at most once."
    "\nPlease reply in the following language: %1"
    "\nReturn ONLY the following JSON object with one key per lang (no extra text):"
    "{\"%1\":{\"bullets\":[\"...\",\"...\",\"...\",\"...\",\"...\"], \"charCounts\":[0,0,0,0,0], \"policy_ok\": true, \"used_keywords\":[\"...\"]}}"
    //"\"EN\":{\"bullets\":[\"...\",\"...\",\"...\",\"...\",\"...\"],, \"charCounts\":[0,0,0,0,0], \"policy_ok\": true, \"langCode\": \"en\", \"used_keywords\":[\"...\"]}}"
};
//, \"boolean_that_confirms_if_you_were_able_to_access_to_previous_image_from_previous_prompt\":TODO


const QString GptFiller::PROMPT_TEXT_END_TITLE{
    "\nPlease translate the title %1 from %2 to the following languages: %3."
    "\nConstraints:"
    "\nCompliance (strict): no medical/therapeutic claims; no competitor mentions; no shipping/price promises; no guarantees/warranties/certifications unless explicitly provided in inputs; avoid #1/best/100% claims."
    "\nSEO: Find SEO keywords and include them naturally ONCE across the title."
    "\nNo emoji and no html."
    "\nTitles can't more than 150 characters."
    "\nDon't include color information."
    "\nReturn ONLY the following JSON object with one key per lang (no extra text):"
    "{\"ES\":{\"title\":\"TODO\", \"charCount\": 144, \"policy_ok\": true, \"used_keywords\":\"TODO\"}"
    ", {\"DE\":{\"title\":\"TODO\", \"charCount\": 144, \"policy_ok\": true, \"used_keywords\":\"TODO\"}}"
};

GptFiller::GptFiller(const QString &workingDir,
                     const QString &apiKey,
                     QObject *parent)
    : QObject{parent}
{
    QDir dir = workingDir;
    m_jsonFilePath = dir.absoluteFilePath("chatgpt.json");
    m_nQueries = 0;
    OpenAi::instance()->init(apiKey);
    m_stopAsked = false;
    _loadReplies();
}

void GptFiller::askFillingTitles(const QString &countryCodeFrom
                                 , const QString &langCodeFrom
                                 , const QHash<QString, QSet<QString>> &countryCode_sourceSkus
                                 , const QHash<QString, SkuInfo> &sku_infos
                                 , const QHash<QString, QHash<QString, QString>> &sku_langCode_varTitleInfos
                                 , QHash<QString, QHash<QString, QHash<QString, QHash<QString, QVariant>>>> &sku_countryCode_langCode_fieldId_value
                                 , std::function<void ()> callbackFinishedSuccess
                                 , std::function<void (const QString &)> callbackFinishedFailure)
{
    m_sku_langCode_varTitleInfos = sku_langCode_varTitleInfos;
    m_sku_countryCode_langCode_fieldId_value = &sku_countryCode_langCode_fieldId_value;
    QHash<QString, QString> skuParent_titleOrig;
    QHash<QString, QStringList> sku_langCodesTo;
    for (auto itSku = sku_countryCode_langCode_fieldId_value.begin();
         itSku != sku_countryCode_langCode_fieldId_value.end(); ++itSku)
    {
        const auto &sku = itSku.key();
        for (auto itCountry = itSku.value().begin();
             itCountry != itSku.value().end(); ++itCountry)
        {
            for (auto itLang = itCountry.value().begin();
                 itLang != itCountry.value().end(); ++itLang)
            {
                const auto &langCode = itLang.key();
                if (langCode != langCodeFrom && !sku_langCodesTo[sku].contains(langCode))
                {
                    sku_langCodesTo[sku] << langCode;
                }
            }
        }
    }
    QHash<QString, QString> sku_langCodesToJoined;
    for (auto it = sku_langCodesTo.begin();
         it != sku_langCodesTo.end(); ++it)
    {
        QString langCodesToJoined{"\""};
        langCodesToJoined += it.value().join("\",\"");
        langCodesToJoined += "\"";
        sku_langCodesToJoined[it.key()] = langCodesToJoined;
    }

    m_nDone = 0;
    m_nQueries = 0;
    for (auto it = sku_infos.begin();
         it != sku_infos.end(); ++it)
    {
        if (m_stopAsked)
        {
            return;
        }
        const auto &sku = it.key();
        const auto &info = it.value();
        const auto &skuParent = info.skuParent;
        const QString titleFieldId{"item_name"};
        if (!skuParent_titleOrig.contains(skuParent))
        {
            if (!(*m_sku_countryCode_langCode_fieldId_origValue)[sku][countryCodeFrom][langCodeFrom].contains(titleFieldId)
                && !countryCode_sourceSkus[countryCodeFrom].contains(sku))
            {
                const QString &message = QObject::tr("The title is missing in the source template for the SKU: %1").arg(sku);
                askStop();
                callbackFinishedFailure(message);
            }
            const auto &langCodesTo = sku_langCodesTo[sku];
            const auto &langCodesToJoined = sku_langCodesToJoined[sku];
            if (!_reloadJsonTitles(skuParent, langCodesTo, langCodesToJoined))
            {
                const QString &title
                    = (*m_sku_countryCode_langCode_fieldId_origValue)[sku][countryCodeFrom][langCodeFrom][titleFieldId].toString();
                skuParent_titleOrig[skuParent] = title.split(" (")[0];
                QString question{PROMPT_TEXT_START};
                question += PROMPT_TEXT_END_TITLE.arg(skuParent_titleOrig[skuParent], langCodeFrom, langCodesToJoined);
                auto image = QSharedPointer<QImage>{new QImage{info.imageFilePath}};
                ++m_nQueries;
                const auto &langCodesTo = sku_langCodesTo[sku];
                OpenAi::instance()->askQuestion(
                    question
                    , *image
                    , skuParent
                    , [this, skuParent, langCodesTo, langCodesToJoined](const QString &jsonReply) -> bool{
                        qDebug() << "REPLY ChatGpt TITLES:" << jsonReply;
                        return _recordJsonTitles(skuParent, langCodesTo, jsonReply);
                    }
                    , [this, image, info, skuParent, langCodesToJoined, callbackFinishedSuccess](const QString &jsonReply){
                        auto reply = _getReplyObject(jsonReply);
                        reply["skuParent"] = skuParent;
                        reply["langCodeToJoined"] = langCodesToJoined;
                        m_skuParent_langCodesToJoined_jsonReplyTitles[skuParent][langCodesToJoined] = reply;
                        _saveReplies();
                        ++m_nDone;
                        if (m_nDone == m_nQueries)
                        {
                            callbackFinishedSuccess();
                        }
                    }
                    , [this, image, info, skuParent, callbackFinishedFailure](const QString &jsonReply){
                        ++m_nDone;
                        askStop();
                        if (m_nDone == m_nQueries)
                        {
                            callbackFinishedFailure(jsonReply);
                        }
                    }
                    , N_RETRY
                    , "gpt-4.1"
                    );

            }
        }
    }
}

void GptFiller::askFillingDescBullets(
    const QHash<QString, SkuInfo> &sku_infos
    , const QHash<QString, QHash<QString, QSet<QString>>> &countryCode_langCode_fieldIdMandatory
    , QHash<QString, QHash<QString, QHash<QString, QHash<QString, QVariant>>>> &sku_countryCode_langCode_fieldId_value
    , const QSet<QString> &langCodes
    , std::function<void ()> callbackFinishedSuccess
    , std::function<void (const QString &)> callbackFinishedFailure)
{
    m_nDone = 0;
    m_nQueries = 0;
    m_sku_infos = &sku_infos;
    m_sku_countryCode_langCode_fieldId_value = &sku_countryCode_langCode_fieldId_value;
    m_countryCode_langCode_fieldIdMandatory = &countryCode_langCode_fieldIdMandatory;
    QHash<QString, QSet<QString>> skuParents_colorsDone;
    for (auto it = m_sku_infos->begin();
         it != m_sku_infos->end(); ++it)
    {
        const auto &sku = it.key();
        const auto &info = it.value();
        const auto &skuParent = info.skuParent;
        const auto &colorOrig = info.colorOrig;
        const auto &skuParentColor = skuParent + colorOrig;
        if (!skuParents_colorsDone[skuParent].contains(colorOrig))
            //&& skuParents_colorsDone.size() < 2) //TEMP
        {
            for (const auto &langCode : langCodes)
            {
                if (!_reloadJsonDesc(skuParent, colorOrig, langCode)
                    || !_reloadJsonBullets(skuParentColor, langCode))
                {
                    Q_ASSERT(QFile::exists(info.imageFilePath));
                    auto image = QSharedPointer<QImage>{new QImage{info.imageFilePath}};
                    QString question{PROMPT_TEXT_START};
                    question += info.customInstructions;
                    question += PROMPT_TEXT_END_DESCRIPTION.arg(langCode);
                    ++m_nQueries;
                    if (m_stopAsked)
                    {
                        return;
                    }
                    OpenAi::instance()->askQuestion(
                        question
                        , *image
                        , skuParentColor
                        , [this, skuParent, colorOrig, langCode, callbackFinishedSuccess, skuParentColor](const QString &jsonReply) -> bool{
                            return _recordJsonDescription(skuParent, colorOrig, langCode, jsonReply);
                        }
                        , [this, image, info, skuParent, colorOrig, langCode, callbackFinishedSuccess, callbackFinishedFailure, skuParentColor](const QString &jsonReply){
                            if (m_stopAsked)
                            {
                                return;
                            }
                            auto replyObject = _getReplyObject(jsonReply);
                            replyObject["skuParentColor"] = skuParent + colorOrig;
                            replyObject["langCode"] = langCode;
                            m_skuParentColor_langCode_jsonReplyDesc[skuParentColor][langCode] = replyObject;
                            _saveReplies();
                            qDebug() << "REPLY ChatGpt description first field id:" << jsonReply;
                            QString questionBullets{PROMPT_TEXT_START};
                            questionBullets += info.customInstructions;
                            questionBullets += PROMPT_TEXT_END_BULLET_POINTS.arg(langCode);
                            OpenAi::instance()->askQuestion(
                                questionBullets
                                , *image
                                , skuParentColor
                                , [this, skuParent, colorOrig, langCode, callbackFinishedSuccess, skuParentColor](const QString &jsonReply) -> bool {
                                    qDebug() << "REPLY ChatGpt bullet points first field id:" << jsonReply;
                                    return _recordJsonBulletPoints(skuParent, colorOrig, langCode, jsonReply);
                                }
                                , [this, image, skuParent, colorOrig, callbackFinishedSuccess, langCode, skuParentColor](const QString &jsonReply) {
                                    auto replyObject = _getReplyObject(jsonReply);
                                    replyObject["skuParentColor"] = skuParent + colorOrig;
                                    replyObject["langCode"] = langCode;
                                    m_skuParentColor_langCode_jsonReplyBullets[skuParentColor][langCode] = replyObject;
                                    _saveReplies();
                                    ++m_nDone;
                                    if (m_nDone == m_nQueries)
                                    {
                                        callbackFinishedSuccess();
                                    }
                                }
                                , [this, image, skuParent, colorOrig, callbackFinishedFailure, skuParentColor](const QString &jsonReply) {
                                    ++m_nDone;
                                    if (m_nDone == m_nQueries)
                                    {
                                        askStop();
                                        callbackFinishedFailure(jsonReply);
                                    }
                                }
                                , N_RETRY
                                , "gpt-4.1"
                                );
                        }
                        , [this, image, skuParent, colorOrig, callbackFinishedFailure, skuParentColor](const QString &jsonReply) {
                            qCritical () << "GptFiller::askFillingDescBullets failed on descripition:" << jsonReply;
                            askStop();
                            ++m_nDone;
                            if (m_nDone == m_nQueries)
                            {
                                callbackFinishedFailure(jsonReply);
                            }
                        }
                        , N_RETRY
                        , "gpt-4.1"
                        );
                }
            }
        }
    }
}

void GptFiller::askFilling(const QString &countryCodeFrom
                           , const QString &langCodeFrom
                           , const QSet<QString> &fieldIdsToIgnore
                           , const QHash<QString, SkuInfo> &sku_infos
                           , const QHash<QString, QHash<QString, QHash<QString, QHash<QString, QVariant>>>> &sku_countryCode_langCode_fieldId_origValue
                           , const QHash<QString, QHash<QString, QHash<QString, QStringList>>> &countryCode_langCode_fieldId_possibleValues
                           , const QHash<QString, QHash<QString, QSet<QString>>> &countryCode_langCode_fieldIdMandatory
                           , const QHash<QString, QHash<QString, QSet<QString>>> &countryCode_langCode_fieldIdChildOnly
                           , QHash<QString, QHash<QString, QHash<QString, QHash<QString, QVariant>>>> &sku_countryCode_langCode_fieldId_value
                           , std::function<void (int, int)> callBackProgress
                           , std::function<void ()> callbackFinishedSuccess
                           , std::function<void (const QString &)> callbackFinishedFailure
                           )
{
    m_callBackProgress = callBackProgress;
    m_callbackFinishedFailure = callbackFinishedFailure;
    m_callbackFinishedSuccess = callbackFinishedSuccess;
    m_fieldIdsToIgnore = &fieldIdsToIgnore;
    m_countryCodeFrom = countryCodeFrom;
    m_langCodeFrom = langCodeFrom;
    m_sku_infos = &sku_infos;
    for (auto it = m_sku_infos->begin();
         it != m_sku_infos->end(); ++it)
    {
        m_skuParent_skus.insert(it.value().skuParent, it.key());
    }
    m_countryCode_langCode_fieldIdChildOnly = &countryCode_langCode_fieldIdChildOnly;
    m_sku_countryCode_langCode_fieldId_origValue = &sku_countryCode_langCode_fieldId_origValue;
    m_countryCode_langCode_fieldId_possibleValues = &countryCode_langCode_fieldId_possibleValues;
    m_countryCode_langCode_fieldIdMandatory = &countryCode_langCode_fieldIdMandatory;
    m_sku_countryCode_langCode_fieldId_value = &sku_countryCode_langCode_fieldId_value;
    for (auto itCountryCode = countryCode_langCode_fieldIdMandatory.begin();
         itCountryCode != countryCode_langCode_fieldIdMandatory.end(); ++itCountryCode)
    {
        for (auto itLangCode = itCountryCode.value().begin();
             itLangCode != itCountryCode.value().end(); ++itLangCode)
        {
            m_listCountryCode_langCode << QPair<QString, QString>{itCountryCode.key(), itLangCode.key()};
        }
    }
    _prepareQueries();
    _processQueries();
}

void GptFiller::askTrueMandatory(const QString &productType,
                                 const QSet<QString> &mandatordyFieldIds,
                                 std::function<void (const QSet<QString> &)> callbackFinishedSuccess,
                                 std::function<void (const QString &)> callbackFinishedFailure)
{
    QStringList mandatordyFieldIdsList{mandatordyFieldIds.begin(), mandatordyFieldIds.end()};
    std::sort(mandatordyFieldIdsList.begin(), mandatordyFieldIdsList.end());
    const QString &question = PROMPT_ASK_NOT_MANDATORY.arg(
        productType,
        mandatordyFieldIdsList.join("\n"));
    OpenAi::instance()->askQuestion(
        question
        , productType
        , [this, callbackFinishedSuccess](const QString &jsonReply) -> bool{
            qDebug() << "REPLY ChatGpt askTrueMandatory:" << jsonReply;
            return jsonReply.count("[") == 1 && jsonReply.count("]") == 1
                   && jsonReply.indexOf("[") < jsonReply.lastIndexOf("]");
        }
        , [this, callbackFinishedSuccess](const QString &jsonReply){
            QString replyNoBrackets = jsonReply.split("[")[1].split("]")[0];
            replyNoBrackets.replace(", ", ",");
            replyNoBrackets.replace(" ,", ",");
            replyNoBrackets.replace("\"", "");
            const auto &fieldIds = replyNoBrackets.split(",");
            QSet<QString> notMandatoryFieldIds{fieldIds.begin(), fieldIds.end()};
            callbackFinishedSuccess(notMandatoryFieldIds);
        }
        , [this, callbackFinishedFailure](const QString &jsonReply){
            askStop();
            callbackFinishedFailure(jsonReply);
        }
        , N_RETRY
        , "gpt-4.1"); // We take a better model as this question is important
}

void GptFiller::clear()
{
    QFile::remove(m_jsonFilePath);
    m_skuParent_fieldId_jsonReplySelect.clear();
    m_skuParent_colorOrig_fieldId_jsonReplyText.clear();
}

void GptFiller::askStop()
{
    //m_stopAsked = true;
}

void GptFiller::_prepareQueries()
{
    QSet<QString> skuParentsDone;
    QHash<QString, QSet<QString>> skuParents_colorsDone;
    m_nQueries = 0;
    for (auto itSku = m_sku_countryCode_langCode_fieldId_value->begin();
         itSku != m_sku_countryCode_langCode_fieldId_value->end(); ++itSku)
    {
        const auto &sku = itSku.key();
        bool isParent = sku.startsWith("P-");
        const auto &skuParent = isParent ? sku : (*m_sku_infos)[sku].skuParent;
        const auto &colorOrig = isParent ? QString{} :(*m_sku_infos)[sku].colorOrig;
        //const auto &customInstructions = (*m_sku_infos)[sku].customInstructions;
        for (auto itCountryCode = m_countryCode_langCode_fieldIdMandatory->begin();
             itCountryCode != m_countryCode_langCode_fieldIdMandatory->end(); ++itCountryCode)
        {
            const auto &countryCodeTo = itCountryCode.key();
            for (auto itLangCode = itCountryCode.value().begin();
                 itLangCode != itCountryCode.value().end(); ++itLangCode)
            {
                const auto &langCodeTo = itLangCode.key();
                for (const auto &fieldId : itLangCode.value())
                {
                    if (!isParent || (*m_countryCode_langCode_fieldIdChildOnly)[countryCodeTo][langCodeTo].contains(fieldId))
                    {
                        if (!m_fieldIdsToIgnore->contains(fieldId)
                            && !(*m_sku_countryCode_langCode_fieldId_value)[sku][countryCodeTo][langCodeTo].contains(fieldId))
                        {
                            if ((*m_countryCode_langCode_fieldId_possibleValues)[countryCodeTo][langCodeTo].contains(fieldId))
                            { // Value to select
                                if (!skuParentsDone.contains(skuParent)
                                    && !_reloadJsonSelect(skuParent, fieldId))
                                {
                                    QJsonObject &jsonObject = m_skuParent_fieldId_jsonSelect[skuParent][fieldId];
                                    if (!jsonObject.contains("skuParent"))
                                    {
                                        jsonObject["values"] = QJsonArray{};
                                        jsonObject["task"] = "fill_select";
                                        jsonObject["skuParent"] = skuParent;
                                        jsonObject["fieldId"] = fieldId;
                                        ++m_nQueries;
                                        //if (!customInstructions.isEmpty())
                                        //{
                                        //jsonObject["customInstructions"] = customInstructions;
                                        //}
                                        if ((*m_sku_countryCode_langCode_fieldId_origValue)[sku][m_countryCodeFrom][m_langCodeFrom].contains(fieldId))
                                        {
                                            QJsonObject origValue;
                                            origValue["countryCodeFrom"] = m_countryCodeFrom;
                                            origValue["langCodeFrom"] = m_langCodeFrom;
                                            origValue["value"] = (*m_sku_countryCode_langCode_fieldId_origValue)[sku][m_countryCodeFrom][m_langCodeFrom][fieldId].toString();
                                            jsonObject["fromValue"] = origValue;
                                        }
                                    }
                                    QJsonArray jsonPossibleValuesByLang = jsonObject["values"].toArray();
                                    QJsonArray jsonPossibleValues;
                                    for (const auto &possibleValue : (*m_countryCode_langCode_fieldId_possibleValues)[countryCodeTo][langCodeTo][fieldId])
                                    {
                                        jsonPossibleValues << possibleValue;
                                    }
                                    QJsonObject jsonPossibleValuesObject;
                                    jsonPossibleValuesObject["countryCodeTo"] = countryCodeTo;
                                    jsonPossibleValuesObject["langCodeTo"] = langCodeTo;
                                    jsonPossibleValuesObject["possibleValues"] = jsonPossibleValues;
                                    jsonPossibleValuesByLang.append(jsonPossibleValuesObject);
                                    jsonObject["values"] = jsonPossibleValuesByLang;
                                    auto itCountryCodeFrom = (*m_sku_countryCode_langCode_fieldId_origValue)[sku].begin();
                                    auto itLangCodeFrom = itCountryCodeFrom->begin();
                                    if (itLangCodeFrom.value().contains(fieldId))
                                    {
                                        jsonObject["langCodeFrom"] = itCountryCodeFrom.key();
                                        jsonObject["valueFrom"] = itLangCodeFrom.value().value(fieldId).toString();
                                    }
                                    m_skuParent_fieldId_jsonSelect[skuParent][fieldId] = jsonObject;
                                }
                            }
                            else
                            { // Creative text
                                if (!skuParents_colorsDone[skuParent].contains(colorOrig)
                                    && !_reloadJsonText(skuParent, colorOrig, fieldId))
                                {
                                    QJsonObject &jsonObject = m_skuParent_colorOrig_fieldId_jsonText[skuParent][colorOrig][fieldId];
                                    if (!jsonObject.contains("skuParent"))
                                    {
                                        jsonObject["skuParent"] = skuParent;
                                        jsonObject["task"] = "fill_text";
                                        jsonObject["colorOrig"] = colorOrig;
                                        jsonObject["fieldId"] = fieldId;
                                        jsonObject["values"] = QJsonObject{};
                                        ++m_nQueries;
                                        //if (!customInstructions.isEmpty())
                                        //{
                                        //jsonObject["customInstructions"] = customInstructions;
                                        //}
                                        if ((*m_sku_countryCode_langCode_fieldId_origValue)[sku][m_countryCodeFrom][m_langCodeFrom].contains(fieldId))
                                        {
                                            QJsonObject origValue;
                                            origValue["countryCodeFrom"] = m_countryCodeFrom;
                                            origValue["langCodeFrom"] = m_langCodeFrom;
                                            origValue["value"] = (*m_sku_countryCode_langCode_fieldId_origValue)[sku][m_countryCodeFrom][m_langCodeFrom][fieldId].toString();
                                            jsonObject["fromValue"] = origValue;
                                        }
                                    }
                                    auto jsonObjectLangs = jsonObject["values"].toObject();
                                    jsonObjectLangs[langCodeTo] = "";
                                    jsonObject["values"] = jsonObjectLangs;
                                    m_skuParent_colorOrig_fieldId_jsonText[skuParent][colorOrig][fieldId] = jsonObject;
                                }
                            }
                        }
                    }
                }
            }
        }
        skuParentsDone.insert(skuParent);
        skuParents_colorsDone[skuParent].insert(colorOrig);
    }
    for (auto itSkuParent = m_skuParent_fieldId_jsonSelect.begin();
         itSkuParent != m_skuParent_fieldId_jsonSelect.end(); ++itSkuParent)
    {
        for (auto itFieldId_jsonSelect = itSkuParent.value().begin();
             itFieldId_jsonSelect != itSkuParent.value().end(); ++itFieldId_jsonSelect)
        {
            auto &jsonObject = itFieldId_jsonSelect.value();
            QJsonObject reply;
            reply["skuParent"] = itSkuParent.key();
            reply["fieldId"] = jsonObject["fieldId"];
            const QJsonArray &jsonPossibleValuesByLang = jsonObject["values"].toArray();
            QJsonArray arrayReply;
            for (const auto &jsonPossibleValuesObject : jsonPossibleValuesByLang)
            {
                auto countryLangObject = jsonPossibleValuesObject.toObject();
                countryLangObject.remove("possibleValues");
                countryLangObject["value"] = "TODO";
                arrayReply << countryLangObject;
            }
            reply["values"] = arrayReply;
            jsonObject["return"] = reply;
        }
    }
    for (auto itSkuParent = m_skuParent_colorOrig_fieldId_jsonText.begin();
         itSkuParent != m_skuParent_colorOrig_fieldId_jsonText.end(); ++itSkuParent)
    {
        for (auto itColorOrig = itSkuParent.value().begin();
             itColorOrig != itSkuParent.value().end(); ++itColorOrig)
        {
            for (auto itFieldId_jsonText = itColorOrig.value().begin();
                 itFieldId_jsonText != itColorOrig.value().end(); ++itFieldId_jsonText)
            {
                auto &jsonObject = itFieldId_jsonText.value();
                QJsonObject reply;
                reply["skuParent"] = itSkuParent.key();
                reply["colorOrig"] = jsonObject["colorOrig"];
                reply["fieldId"] = jsonObject["fieldId"];
                auto objectValues = jsonObject["values"].toObject();
                for (auto it = objectValues.begin();
                     it != objectValues.end(); ++it)
                {
                    it.value() = "TODO";
                }
                reply["values"] = objectValues;
                jsonObject["return"] = reply;
            }
        }
    }
}

void GptFiller::_processQueries()
{
    m_nDone = 0;
    auto updateAfterQueryProcessed = [this](){
        ++m_nDone;
        m_callBackProgress(m_nDone, m_nQueries);
        if (m_nDone == m_nQueries)
        {
            m_callbackFinishedSuccess();
        }
    };
    auto updateAfterQueryProcessedFailed = [this](const QString &error){
        ++m_nDone;
        m_callBackProgress(m_nDone, m_nQueries);
        if (m_nDone == m_nQueries)
        {
            m_callbackFinishedFailure(error);
        }
    };
    for (auto itSkuParent = m_skuParent_fieldId_jsonSelect.begin();
         itSkuParent != m_skuParent_fieldId_jsonSelect.end(); ++itSkuParent)
    {
        const QString &skuParent = itSkuParent.key();
        for (auto itFieldId_jsonSelect = itSkuParent.value().begin();
             itFieldId_jsonSelect != itSkuParent.value().end(); ++itFieldId_jsonSelect)
        {
            if (m_stopAsked)
            {
                return;
            }
            const auto &fieldId = itFieldId_jsonSelect.key();
            QString question{PROMPT_FIRST};
            const QString &firstSku = m_skuParent_skus.value(skuParent);
            const auto &firstInfo = (*m_sku_infos)[firstSku];
            question += "\n";
            question += firstInfo.customInstructions;
            question += "\n";
            question += PROMPT_INTRODUCE_JSON;
            question += "\n";
            const auto &jsonObject = itFieldId_jsonSelect.value();
            const QString &json = QString::fromUtf8(QJsonDocument(jsonObject).toJson(QJsonDocument::Compact));
            question += json;
            qDebug() << "Asking ChatGpt field id:" << itFieldId_jsonSelect.key() << " - " << question;
            QImage *image = new QImage{firstInfo.imageFilePath};
            OpenAi::instance()->askQuestion(
                question
                , *image
                , skuParent
                , [this, skuParent](const QString &jsonReply) -> bool{
                    qDebug() << "REPLY ChatGpt SELECT field id:" << jsonReply;
                    return _recordJsonSelect(skuParent, jsonReply);
                }
                , [this, image, skuParent, fieldId, updateAfterQueryProcessed](const QString &jsonReply){
                    m_skuParent_fieldId_jsonReplySelect[skuParent][fieldId] = _getReplyObject(jsonReply);
                    _saveReplies();
                    delete image;
                    updateAfterQueryProcessed();
                }
                , [this, image, skuParent, updateAfterQueryProcessedFailed](const QString &jsonReply){
                    askStop();
                    qDebug() << "REPLY FAILURE ChatGpt SELECT field id:"
                             << skuParent << "-" << jsonReply;
                    delete image;
                    updateAfterQueryProcessedFailed(jsonReply);
                }
                , N_RETRY
                );
        }
    }
    for (auto itSkuParent = m_skuParent_colorOrig_fieldId_jsonText.begin();
         itSkuParent != m_skuParent_colorOrig_fieldId_jsonText.end(); ++itSkuParent)
    {
        const auto &skuParent = itSkuParent.key();
        for (auto itColorOrig = itSkuParent.value().begin();
             itColorOrig != itSkuParent.value().end(); ++itColorOrig)
        {
            const auto &colorOrig = itColorOrig.key();
            const auto &skuParentColor = skuParent + colorOrig;
            for (auto itFieldId_jsonText = itColorOrig.value().begin();
                 itFieldId_jsonText != itColorOrig.value().end(); ++itFieldId_jsonText)
            {
                if (m_stopAsked)
                {
                    return;
                }
                const QString &firstSku = m_skuParent_skus.value(skuParent);
                const auto &firstInfo = (*m_sku_infos)[firstSku];
                const auto &fieldId = itFieldId_jsonText.key();
                QString question{PROMPT_FIRST};
                question += PROMPT_FIRST;
                question += "\n";
                question += firstInfo.customInstructions;
                question += "\n";
                question += PROMPT_INTRODUCE_JSON;
                question += "\n";
                const auto &jsonObject = itFieldId_jsonText.value();
                const QString &json = QString::fromUtf8(QJsonDocument(jsonObject).toJson(QJsonDocument::Compact));
                question += json;
                qDebug() << "Asking ChatGpt field id:" << itFieldId_jsonText.key() << " - " << question;
                QImage *image = new QImage{firstInfo.imageFilePath};
                OpenAi::instance()->askQuestion(
                    question
                    , *image
                    , skuParentColor
                    , [this, colorOrig, skuParent](
                        const QString &jsonReply) -> bool{
                        qDebug() << "REPLY ChatGpt TEXT field id:" << jsonReply;
                        return _recordJsonText(skuParent, colorOrig, jsonReply);
                    }
                    , [this, colorOrig, skuParent, fieldId, image, updateAfterQueryProcessed](const QString &jsonReply){
                        m_skuParent_colorOrig_fieldId_jsonReplyText[skuParent][colorOrig][fieldId] = _getReplyObject(jsonReply);
                        _saveReplies();
                        delete image;
                        updateAfterQueryProcessed();
                    }
                    , [this, image, skuParent, updateAfterQueryProcessedFailed](const QString &jsonReply){
                        askStop();
                        qDebug() << "REPLY FAILURE ChatGpt TEXT first field id:"
                                 << skuParent << "-" << jsonReply;
                        delete image;
                        updateAfterQueryProcessedFailed(jsonReply);
                    }
                    , N_RETRY
                    );
            }
        }
    }
}

bool GptFiller::_recordJsonSelect(const QString &skuParent, const QString &jsonReply)
{
    const QJsonDocument &jsonDoc = QJsonDocument::fromJson(
        _tryToFixJson(jsonReply).toUtf8());
    bool correctReply = true;
    if (jsonDoc.isObject())
    {
        const auto &reply = _getReplyObject(jsonDoc);
        return _recordJsonSelect(skuParent, reply);
    }
    else
    {
        correctReply = false;
    }
    return correctReply;
}

bool GptFiller::_recordJsonSelect(
    const QString &skuParent, const QJsonObject &reply)
{
    bool correctReply = true;
    if (reply.contains("values")
        && reply.contains("fieldId"))
    {
        const auto &fieldId = reply["fieldId"].toString();
        const auto &values = reply["values"].toArray();
        for (auto &value : values)
        {
            const auto &countryLangObject = value.toObject();
            if (countryLangObject.contains("langCodeTo")
                && countryLangObject.contains("countryCodeTo")
                && countryLangObject.contains("value"))
            {
                const auto &langCodeTo = countryLangObject["langCodeTo"].toString();
                const auto &countryCodeTo = countryLangObject["countryCodeTo"].toString();
                const auto &value = countryLangObject["value"].toString();
                const auto &skus = m_skuParent_skus.values(skuParent);
                for (const auto &sku : skus)
                {
                    (*m_sku_countryCode_langCode_fieldId_value)[sku][countryCodeTo][langCodeTo][fieldId] = value;
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
    return correctReply;

}

bool GptFiller::_recordJsonText(const QString &skuParent,
                                const QString &colorOrig,
                                const QString &jsonReply)
{
    const QJsonDocument &jsonDoc = QJsonDocument::fromJson(_tryToFixJson(jsonReply).toUtf8());
    bool correctReply = true;
    if (jsonDoc.isObject())
    {
        const QJsonObject &reply = _getReplyObject(jsonDoc);
        return _recordJsonText(skuParent, colorOrig, reply);
    }
    else
    {
        correctReply = false;
    }
    return correctReply;
}

bool GptFiller::_recordJsonText(
    const QString &skuParent, const QString &colorOrig, const QJsonObject &reply)
{
    bool correctReply = true;
    if (reply.contains("values")
        && reply.contains("fieldId")
        && reply.contains("colorOrig"))
    {
        const auto &fieldId = reply["fieldId"].toString();
        const auto &colorOrig = reply["colorOrig"].toString();
        if (reply["values"].isObject())
        {
            const auto &objectValues = reply["values"].toObject();
            for (auto it = objectValues.begin();
                 it != objectValues.end(); ++it)
            {
                const auto &langCodeTo = it.key();
                const auto &value = it.value().toString();
                const auto &skus = m_skuParent_skus.values(skuParent);
                for (const auto &sku : skus)
                {
                    auto &countryCode_langCode_fieldId_value = (*m_sku_countryCode_langCode_fieldId_value)[sku];
                    for (auto itCountryCode = countryCode_langCode_fieldId_value.begin();
                         itCountryCode != countryCode_langCode_fieldId_value.end(); ++itCountryCode)
                    {
                        if (itCountryCode.value().contains(langCodeTo))
                        {
                            itCountryCode.value()[langCodeTo][fieldId] = value;
                        }
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
    return correctReply;
}

bool GptFiller::_recordJsonTitles(
    const QString &skuParent, const QStringList &langCodesTo, const QString &jsonReply)
{
    const QJsonDocument &jsonDoc = QJsonDocument::fromJson(_tryToFixJson(jsonReply).toUtf8());
    bool correctReply = true;
    if (jsonDoc.isObject())
    {
        QHash<QString, QString> langCode_title;
        const QJsonObject &reply = _getReplyObject(jsonDoc);
        return _recordJsonTitles(skuParent, langCodesTo, reply);
    }
    else
    {
        correctReply = false;
    }
    return correctReply;
}

bool GptFiller::_recordJsonTitles(
    const QString &skuParent, const QStringList &langCodesTo, const QJsonObject &reply)
{
    bool correctReply = true;
    QHash<QString, QString> langCode_title;
    for (auto it = reply.begin();
         it != reply.end(); ++it)
    {
        const auto &langCode = it.key().toUpper();
        const auto &descObject = it.value().toObject();
        if (descObject.contains("title"))
        {
            langCode_title[langCode] = descObject["title"].toString();
        }
        else
        {
            correctReply = false;
        }
    }
    if (correctReply)
    {
        QSet<QString> langCodesDone;
        for (auto itSku = m_sku_countryCode_langCode_fieldId_value->begin();
             itSku != m_sku_countryCode_langCode_fieldId_value->end(); ++itSku)
        {
            const auto &sku = itSku.key();
            bool isParent = sku.startsWith("P-");
            const auto &curSkuParent = isParent ? sku : (*m_sku_infos)[sku].skuParent;
            if (curSkuParent == skuParent)
            {
                auto &countryCode_langCode_fieldId_value = itSku.value();
                correctReply = false;
                for (auto itCountryCode = m_countryCode_langCode_fieldIdMandatory->begin();
                     itCountryCode != m_countryCode_langCode_fieldIdMandatory->end(); ++itCountryCode)
                {
                    const auto &countryCode = itCountryCode.key();
                    for (auto itLangCode = itCountryCode.value().begin();
                         itLangCode != itCountryCode.value().end(); ++itLangCode)
                    {
                        const auto &langCode = itLangCode.key();
                        if (langCode_title.contains(langCode))
                        {
                            langCodesDone.insert(langCode);
                            correctReply = true;
                            static const QSet<QString> titleFieldId{
                                "item_name"
                                , "item_name#1.value"
                            };
                            for (const auto &fieldId : titleFieldId)
                            {
                                auto title = langCode_title[langCode];
                                if (isParent)
                                {
                                    title += " " + m_sku_langCode_varTitleInfos[sku][langCode];
                                }
                                if (!countryCode_langCode_fieldId_value[countryCode][langCode].contains(fieldId))
                                {
                                    countryCode_langCode_fieldId_value[countryCode][langCode][fieldId]
                                        = title;
                                    Q_ASSERT(!(*m_sku_countryCode_langCode_fieldId_value)[sku][countryCode][langCode][fieldId].toString().isEmpty());
                                }
                            }
                        }
                    }
                }
            }
        }
        if (langCodesDone.size() < langCodesTo.size())
        {
            correctReply = false;
        }
    }
    return correctReply;
}

bool GptFiller::_recordJsonDescription(
    const QString &skuParent,
    const QString &colorOrig,
    const QString &langCodeDone,
    const QString &jsonReply)
{
    const QJsonDocument &jsonDoc = QJsonDocument::fromJson(_tryToFixJson(jsonReply).toUtf8());
    bool correctReply = true;
    if (jsonDoc.isObject())
    {
        const QJsonObject &reply = _getReplyObject(jsonDoc);
        return _recordJsonDescription(skuParent, colorOrig, langCodeDone, reply);
    }
    else
    {
        correctReply = false;
    }
    return correctReply;
}

bool GptFiller::_recordJsonDescription(
    const QString &skuParent,
    const QString &colorOrig,
    const QString &langCodeDone,
    const QJsonObject &reply)
{
    bool correctReply = true;
    QHash<QString, QString> langCode_description;
    for (auto it = reply.begin();
         it != reply.end(); ++it)
    {
        const auto &key = it.key();
        if (key != "langCode" && key != "skuParentColor")
        {
            const auto &langCode = key.toUpper();
            const auto &descObject = it.value().toObject();
            if (descObject.contains("description"))
            {
                langCode_description[langCode] = descObject["description"].toString();
            }
            else if (langCode == "LANGOCDE")
            {
                correctReply = false;
            }
        }
    }
    if (correctReply)
    {
        for (auto itSku = m_sku_countryCode_langCode_fieldId_value->begin();
             itSku != m_sku_countryCode_langCode_fieldId_value->end(); ++itSku)
        {
            const auto &sku = itSku.key();
            bool isParent = sku.startsWith("P-");
            const auto &curSkuParent = isParent ? sku : (*m_sku_infos)[sku].skuParent;
            if (curSkuParent == skuParent && (isParent || (*m_sku_infos)[sku].colorOrig == colorOrig))
            {
                auto &countryCode_langCode_fieldId_value = itSku.value();
                correctReply = false;
                for (auto itCountryCode = m_countryCode_langCode_fieldIdMandatory->begin();
                     itCountryCode != m_countryCode_langCode_fieldIdMandatory->end(); ++itCountryCode)
                {
                    const auto &countryCode = itCountryCode.key();
                    for (auto itLangCode = itCountryCode.value().begin();
                         itLangCode != itCountryCode.value().end(); ++itLangCode)
                    {
                        const auto &curLangCode = itLangCode.key();
                        if (curLangCode == langCodeDone && langCode_description.contains(langCodeDone))
                        {
                            correctReply = true;
                            static const QSet<QString> descrFieldId{
                                "product_description"
                                , "product_description#1.value"
                            };
                            for (const auto &fieldId : descrFieldId)
                            {
                                countryCode_langCode_fieldId_value[countryCode][langCodeDone][fieldId]
                                    = langCode_description[langCodeDone];
                            }
                        }
                    }
                }
            }
        }
    }
    return correctReply;
}

bool GptFiller::_recordJsonBulletPoints(
    const QString &skuParent,
    const QString &colorOrig,
    const QString &langCodeDone,
    const QString &jsonReply)
{
    // TODO title translation + langCodes in question
    const QJsonDocument &jsonDoc = QJsonDocument::fromJson(_tryToFixJson(jsonReply).toUtf8());
    bool correctReply = true;
    if (jsonDoc.isObject())
    {
        const QJsonObject &reply = _getReplyObject(jsonDoc);
        return _recordJsonBulletPoints(skuParent, colorOrig, langCodeDone, reply);
    }
    else
    {
        correctReply = false;
    }
    return correctReply;
}

bool GptFiller::_recordJsonBulletPoints(
    const QString &skuParent,
    const QString &colorOrig,
    const QString &langCodeDone,
    const QJsonObject &reply)
{
    bool correctReply = true;
    QHash<QString, QStringList> langCode_bulletPoints;
    for (auto it = reply.begin();
         it != reply.end(); ++it)
    {
        const auto &key = it.key();
        if (key != "langCode" && key != "skuParentColor")
        {
            const auto &langCode = key.toUpper();
            const auto &descObject = it.value().toObject();
            if (descObject.contains("bullets") && descObject["bullets"].isArray())
            {
                const QJsonArray &arrayBullets = descObject["bullets"].toArray();
                if (arrayBullets.size() > 4)
                {
                    for (auto &jsonValue : arrayBullets)
                    {
                        langCode_bulletPoints[langCode] << jsonValue.toString();
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
    }
    if (correctReply)
    {
        for (auto itSku = m_sku_countryCode_langCode_fieldId_value->begin();
             itSku != m_sku_countryCode_langCode_fieldId_value->end(); ++itSku)
        {
            const auto &sku = itSku.key();
            bool isParent = sku.startsWith("P-");
            const auto &curSkuParent = isParent ? sku : (*m_sku_infos)[sku].skuParent;
            if (curSkuParent == skuParent && (isParent || (*m_sku_infos)[sku].colorOrig == colorOrig))
            {
                correctReply = false;
                auto &countryCode_langCode_fieldId_value = itSku.value();
                for (auto itCountryCode = m_countryCode_langCode_fieldIdMandatory->begin();
                     itCountryCode != m_countryCode_langCode_fieldIdMandatory->end(); ++itCountryCode)
                {
                    const auto &countryCode = itCountryCode.key();
                    for (auto itLangCode = itCountryCode.value().begin();
                         itLangCode != itCountryCode.value().end(); ++itLangCode)
                    {
                        const auto &curLangCode = itLangCode.key();
                        if (curLangCode == langCodeDone
                            && langCode_bulletPoints.contains(langCodeDone))
                        {
                            correctReply = true;
                            auto itLangCode = itCountryCode.value().find(langCodeDone);
                            auto &fieldId_value = itLangCode.value();
                            static const QSet<QString> bulletPatternFieldIds{
                                "bullet_point#%1.value"
                                , "bullet_point%1"
                            };
                            for (int i=1; i<=5; ++i)
                            {
                                for (const auto &bulletPatternFieldId : bulletPatternFieldIds)
                                {
                                    const auto &bulletFieldId = bulletPatternFieldId.arg(i);
                                    countryCode_langCode_fieldId_value[countryCode][langCodeDone][bulletFieldId]
                                        = langCode_bulletPoints[langCodeDone][i-1];
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    return correctReply;
}

void GptFiller::_saveReplies()
{

    QJsonArray jsonSelects;
    for (auto itSkuParent = m_skuParent_fieldId_jsonReplySelect.begin();
         itSkuParent != m_skuParent_fieldId_jsonReplySelect.end(); ++itSkuParent)
    {
        for (auto itFieldId_jsonSelect = itSkuParent.value().begin();
             itFieldId_jsonSelect != itSkuParent.value().end(); ++itFieldId_jsonSelect)
        {
            jsonSelects << itFieldId_jsonSelect.value();
        }
    }
    QJsonArray jsonTexts;
    for (auto itSkuParent = m_skuParent_colorOrig_fieldId_jsonReplyText.begin();
         itSkuParent != m_skuParent_colorOrig_fieldId_jsonReplyText.end(); ++itSkuParent)
    {
        for (auto itColorOrig = itSkuParent.value().begin();
             itColorOrig != itSkuParent.value().end(); ++itColorOrig)
        {
            for (auto itFieldId_jsonText = itColorOrig.value().begin();
                 itFieldId_jsonText != itColorOrig.value().end(); ++itFieldId_jsonText)
            {
                jsonTexts << itFieldId_jsonText.value();
            }
        }
    }

    QJsonArray jsonDescriptions;
    for (auto itSkuParentColor = m_skuParentColor_langCode_jsonReplyDesc.begin();
         itSkuParentColor != m_skuParentColor_langCode_jsonReplyDesc.end(); ++itSkuParentColor)
    {
        for (auto itLangCode = itSkuParentColor.value().begin();
             itLangCode != itSkuParentColor.value().end(); ++itLangCode)
        {
            jsonDescriptions << itLangCode.value();
        }
    }

    QJsonArray jsonBulletPoints;
    for (auto itSkuParentColor = m_skuParentColor_langCode_jsonReplyBullets.begin();
         itSkuParentColor != m_skuParentColor_langCode_jsonReplyBullets.end(); ++itSkuParentColor)
    {
        for (auto itLangCode = itSkuParentColor.value().begin();
             itLangCode != itSkuParentColor.value().end(); ++itLangCode)
        {
            jsonBulletPoints << itLangCode.value();
        }
    }

    QJsonArray jsonTitles;
    for (auto itSkuParent = m_skuParent_langCodesToJoined_jsonReplyTitles.begin();
         itSkuParent != m_skuParent_langCodesToJoined_jsonReplyTitles.end(); ++itSkuParent)
    {
        for (auto itLangCodeJoined = itSkuParent.value().begin();
             itLangCodeJoined != itSkuParent.value().end(); ++itLangCodeJoined)
        {
            jsonTitles << itLangCodeJoined.value();
        }
    }

    QJsonObject jsonFinal;
    jsonFinal["texts"] = jsonTexts;
    jsonFinal["selects"] = jsonSelects;
    jsonFinal["descriptions"] = jsonDescriptions;
    jsonFinal["bullets"] = jsonBulletPoints;
    jsonFinal["titles"] = jsonTitles;

    QSaveFile file(m_jsonFilePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        return;
    }

    QJsonDocument doc(jsonFinal);
    const QByteArray bytes = doc.toJson(QJsonDocument::Indented);
    if (file.write(bytes) == -1)
    {
        return;
    }
    file.commit();
}

void GptFiller::_loadReplies()
{
    if (QFile::exists(m_jsonFilePath))
    {
        QFile file(m_jsonFilePath);
        if (!file.open(QIODevice::ReadOnly))
        {
            return;
        }
        QByteArray bytes = file.readAll();
        file.close();

        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(bytes, &err);
        if (doc.isNull() || !doc.isObject())
        {
            return;
        }
        QJsonObject obj = doc.object();

        // Clear current maps
        m_skuParent_fieldId_jsonReplySelect.clear();
        m_skuParent_colorOrig_fieldId_jsonReplyText.clear();

        // --- SELECTS
        QJsonArray jsonSelects = obj.value("selects").toArray();
        for (const QJsonValue& v : jsonSelects)
        {
            const QJsonObject &sel = v.toObject();
            const QString &skuParent = sel.value("skuParent").toString();
            const QString &fieldId   = sel.value("fieldId").toString(); // assumed

            if (!skuParent.isEmpty() && !fieldId.isEmpty())
            {
                m_skuParent_fieldId_jsonReplySelect[skuParent][fieldId] = sel;
            }
        }

        // --- TEXTS
        QJsonArray jsonTexts = obj.value("texts").toArray();
        for (const QJsonValue& v : jsonTexts)
        {
            const QJsonObject &txt = v.toObject();
            const QString &skuParent = txt.value("skuParent").toString();
            const QString &colorOrig = txt.value("colorOrig").toString();
            const QString &fieldId   = txt.value("fieldId").toString(); // assumed

            if (!skuParent.isEmpty() && !colorOrig.isEmpty() && !fieldId.isEmpty())
            {
                m_skuParent_colorOrig_fieldId_jsonReplyText
                    [skuParent][colorOrig][fieldId] = txt;
            }
        }

        // --- DESCRIPTIONS
        const QJsonArray &descriptions = obj.value("descriptions").toArray();
        for (const QJsonValue &v : descriptions)
        {
            if (v.isObject())
            {
                const QJsonObject &d = v.toObject();
                const QString &skuParentColor = d.value("skuParentColor").toString();
                const QString &langCode       = d.value("langCode").toString();
                if (skuParentColor.isEmpty() || langCode.isEmpty())
                {
                    continue;
                }
                m_skuParentColor_langCode_jsonReplyDesc[skuParentColor][langCode] = d;
            }
        }

        // --- BULLETS
        const QJsonArray bullets = obj.value("bullets").toArray();
        for (const QJsonValue &v : bullets)
        {
            if (v.isObject())
            {
                const QJsonObject &b = v.toObject();
                const QString &skuParentColor = b.value("skuParentColor").toString();
                const QString &langCode       = b.value("langCode").toString();
                if (skuParentColor.isEmpty() || langCode.isEmpty())
                {
                    continue;
                }
                m_skuParentColor_langCode_jsonReplyBullets[skuParentColor][langCode] = b;
            }
        }

        // --- TITLES
        const QJsonArray &titles = obj.value("titles").toArray();
        for (const QJsonValue &v : titles)
        {
            if (v.isObject())
            {
                const QJsonObject &t = v.toObject();
                const QString &skuParent      = t.value("skuParent").toString();
                const QString &langCodeToJoined = t.value("langCodeToJoined").toString();
                if (skuParent.isEmpty() || langCodeToJoined.isEmpty())
                {
                    continue;
                }
                m_skuParent_langCodesToJoined_jsonReplyTitles[skuParent][langCodeToJoined] = t;
            }
        }
    }
}

bool GptFiller::_reloadJsonText(const QString &skuParent, const QString &colorOrig, const QString &fieldId)
{
    if (m_skuParent_colorOrig_fieldId_jsonReplyText.contains(skuParent)
    && m_skuParent_colorOrig_fieldId_jsonReplyText[skuParent].contains(colorOrig)
    && m_skuParent_colorOrig_fieldId_jsonReplyText[skuParent][colorOrig].contains(fieldId))
    {
        return _recordJsonText(skuParent, colorOrig, m_skuParent_colorOrig_fieldId_jsonReplyText[skuParent][colorOrig][fieldId]);
    }
    return false;
}

bool GptFiller::_reloadJsonSelect(const QString &skuParent, const QString &fieldId)
{
    if (m_skuParent_fieldId_jsonReplySelect.contains(skuParent)
        && m_skuParent_fieldId_jsonReplySelect[skuParent].contains(fieldId))
    {
        return _recordJsonSelect(
            skuParent, m_skuParent_fieldId_jsonReplySelect[skuParent][fieldId]);
    }
    return false;
}

bool GptFiller::_reloadJsonTitles(
    const QString &skuParent, const QStringList &langCodesTo, const QString &langCodesToJoined)
{
    if (m_skuParent_langCodesToJoined_jsonReplyTitles.contains(skuParent)
        && m_skuParent_langCodesToJoined_jsonReplyTitles[skuParent].contains(langCodesToJoined))
    {
        return _recordJsonTitles(skuParent, langCodesTo, m_skuParent_langCodesToJoined_jsonReplyTitles[skuParent][langCodesToJoined]);
    }
    return false;
}

bool GptFiller::_reloadJsonDesc(
    const QString &skuParent, const QString &skuColor, const QString &langCode)
{
    const QString &skuParentColor = skuParent + skuColor;
    if (m_skuParentColor_langCode_jsonReplyDesc.contains(skuParentColor)
        && m_skuParentColor_langCode_jsonReplyDesc[skuParentColor].contains(langCode))
    {
        return _recordJsonDescription(
            skuParent, skuColor, langCode, m_skuParentColor_langCode_jsonReplyDesc[skuParentColor][langCode]);
    }
    return false;
}

bool GptFiller::_reloadJsonBullets(
    const QString &skuParentColor, const QString &langCode)
{
    return m_skuParentColor_langCode_jsonReplyBullets.contains(skuParentColor)
           && m_skuParentColor_langCode_jsonReplyBullets[skuParentColor].contains(langCode);
}

QString GptFiller::_tryToFixJson(const QString &jsonReply) const
{
    if (jsonReply.startsWith("```json\n"))
    {
        int indexFirst = jsonReply.indexOf("\n");
        int indexlast = jsonReply.indexOf("\n```");
        int length = indexlast - indexFirst - 1;
        return jsonReply.mid(indexFirst+1, length);
    }
    else if (jsonReply.contains("\",\"charCounts"))
    {
        return QString{jsonReply}.replace("\",\"charCounts", "\"],\"charCounts");
    }
    return jsonReply;
}

QJsonObject GptFiller::_getReplyObject(const QJsonDocument &jsonDoc) const
{
    QJsonObject reply = jsonDoc.object();
    if (reply.contains("reply") && reply["reply"].isObject())
    {
        return reply["reply"].toObject();
    }
    else if (reply.contains("return") && reply["return"].isObject())
    {
        return reply["return"].toObject();
    }
    return reply;
}

QJsonObject GptFiller::_getReplyObject(const QString &jsonReply) const
{
    const QJsonDocument &jsonDoc = QJsonDocument::fromJson(
        _tryToFixJson(jsonReply).toUtf8());
    return _getReplyObject(jsonDoc);
}



