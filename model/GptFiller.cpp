#include <QSaveFile>
#include <QJsonArray>
#include <QJsonDocument>

#include "GptFiller.h"

#include "../common/openai/OpenAi.h"

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

GptFiller::GptFiller(const QString &workingDir,
                     const QString &apiKey,
                     QObject *parent)
    : QObject{parent}
{
    QDir dir = workingDir;
    m_jsonFilePath = dir.absoluteFilePath("chatgpt.ini");
    m_nQueries = 0;
    OpenAi::instance()->init(apiKey);
    m_stopAsked = false;
    _loadReplies();
}

void GptFiller::askFilling(const QString &countryCodeFrom
                           , const QString &langCodeFrom
                           , const QSet<QString> &fieldIdsToIgnore
                           , const QHash<QString, SkuInfo> &sku_infos
                           , const QHash<QString, QHash<QString, QHash<QString, QHash<QString, QVariant>>>> &sku_countryCode_langCode_fieldId_origValue
                           , const QHash<QString, QHash<QString, QHash<QString, QStringList>>> &countryCode_langCode_fieldId_possibleValues
                           , const QHash<QString, QHash<QString, QSet<QString>>> &countryCode_langCode_fieldIdMandatory
                           , QHash<QString, QHash<QString, QHash<QString, QHash<QString, QVariant>>>> &skuParent_colorOrig_langCode_fieldId_valueText
                           , QHash<QString, QHash<QString, QHash<QString, QHash<QString, QVariant>>>> &sku_countryCode_langCode_fieldId_value
                           , std::function<void ()> callbackFinished
                           , std::function<void (int, int)> callBackProgress)
{
    m_callBackProgress = callBackProgress;
    m_callbackFinished = callbackFinished;
    m_fieldIdsToIgnore = &fieldIdsToIgnore;
    m_countryCodeFrom = countryCodeFrom;
    m_langCodeFrom = langCodeFrom;
    m_sku_infos = sku_infos;
    for (auto it = m_sku_infos.begin();
         it != m_sku_infos.end(); ++it)
    {
        m_skuParent_skus.insert(it.value().skuParent, it.key());
    }
    m_sku_countryCode_langCode_fieldId_origValue = &sku_countryCode_langCode_fieldId_origValue;
    m_countryCode_langCode_fieldId_possibleValues = &countryCode_langCode_fieldId_possibleValues;
    m_countryCode_langCode_fieldIdMandatory = &countryCode_langCode_fieldIdMandatory;
    m_skuParent_colorOrig_langCode_fieldId_valueText = &skuParent_colorOrig_langCode_fieldId_valueText;
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

void GptFiller::askTrueMandatory(
    const QString &productType,
    const QSet<QString> &mandatordyFieldIds,
    std::function<void (const QSet<QString> &)> callbackFinished)
{
    QStringList mandatordyFieldIdsList{mandatordyFieldIds.begin(), mandatordyFieldIds.end()};
    std::sort(mandatordyFieldIdsList.begin(), mandatordyFieldIdsList.end());
    const QString &question = PROMPT_ASK_NOT_MANDATORY.arg(
        productType,
        mandatordyFieldIdsList.join("\n"));
    OpenAi::instance()->askQuestion(
        question
        , productType
        , [this, callbackFinished](const QString &jsonReply){
            qDebug() << "REPLY ChatGpt askTrueMandatory:" << jsonReply;
            if (jsonReply.count("[") == 1 && jsonReply.count("]") == 1
                && jsonReply.indexOf("[") < jsonReply.lastIndexOf("]"))
            {
                QString replyNoBrackets = jsonReply.split("[")[1].split("]")[0];
                replyNoBrackets.replace(", ", ",");
                replyNoBrackets.replace(" ,", ",");
                replyNoBrackets.replace("\"", "");
                const auto &fieldIds = replyNoBrackets.split(",");
                QSet<QString> notMandatoryFieldIds{fieldIds.begin(), fieldIds.end()};
                callbackFinished(notMandatoryFieldIds);
            }
        }
        , "gpt-4.1"); // We take a better model as this question is important
}

void GptFiller::clear()
{
    QFile::remove(m_jsonFilePath);
    m_skuParent_fieldId_jsonSelectReply.clear();
    m_skuParent_colorOrig_fieldId_jsonTextReply.clear();
}

void GptFiller::askStop()
{
    m_stopAsked = true;
}

void GptFiller::_prepareQueries()
{
    QSet<QString> skuParentsDone;
    QHash<QString, QSet<QString>> skuParents_colorsDone;
    m_nQueries = 0;
    static QSet<QString> fieldIds1;
    static QSet<QString> fieldIds2;
    for (auto itSku = m_sku_countryCode_langCode_fieldId_value->begin();
         itSku != m_sku_countryCode_langCode_fieldId_value->end(); ++itSku)
    {
        const auto &sku = itSku.key();
        const auto &skuParent = m_sku_infos[sku].skuParent;
        const auto &colorOrig = m_sku_infos[sku].colorOrig;
        //const auto &customInstructions = m_sku_infos[sku].customInstructions;
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
                    if (!m_fieldIdsToIgnore->contains(fieldId)
                        && !(*m_sku_countryCode_langCode_fieldId_value)[sku][countryCodeTo][langCodeTo].contains(fieldId))
                    {
                        if ((*m_countryCode_langCode_fieldId_possibleValues)[countryCodeTo][langCodeTo].contains(fieldId))
                        { // Value to select
                            if (fieldIds1.size() < 3 || fieldIds1.contains(sku+fieldId))
                            {
                                fieldIds1.insert(sku+fieldId);
                            if (!skuParentsDone.contains(skuParent)
                                && !_isJsonSelectDone(skuParent, fieldId))
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
                        }
                        else
                        { // Creative text
                            if (fieldIds2.size() < 3 || fieldIds2.contains(sku+fieldId))
                            {
                                fieldIds2.insert(sku+fieldId);
                            if (!skuParents_colorsDone[skuParent].contains(colorOrig)
                                && !_isJsonTextDone(skuParent, colorOrig, fieldId))
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
    for (auto itSkuParent = m_skuParent_fieldId_jsonSelect.begin();
         itSkuParent != m_skuParent_fieldId_jsonSelect.end(); ++itSkuParent)
    {
        const QString &skuParent = itSkuParent.key();
        bool first = true;
        for (auto itFieldId_jsonSelect = itSkuParent.value().begin();
             itFieldId_jsonSelect != itSkuParent.value().end(); ++itFieldId_jsonSelect)
        {
            if (m_stopAsked)
            {
                return;
            }
            QString question;
            const QString &firstSku = m_skuParent_skus.value(skuParent);
            const auto &firstInfo = m_sku_infos[firstSku];
            if (first)
            {
                question += PROMPT_FIRST;
                question += "\n";
                question += firstInfo.customInstructions;
                question += "\n";
                question += PROMPT_INTRODUCE_JSON;
                question += "\n";
            }
            const auto &jsonObject = itFieldId_jsonSelect.value();
            const QString &json = QString::fromUtf8(QJsonDocument(jsonObject).toJson(QJsonDocument::Compact));
            question += json;
            qDebug() << "Asking ChatGpt field id:" << itFieldId_jsonSelect.key() << " - " << question;
            if (first)
            {
                QImage *image = new QImage{firstInfo.imageFilePath};
                OpenAi::instance()->askQuestion(
                    question,
                    skuParent,
                    [this, image, skuParent](const QString &jsonReply){
                        delete image;
                        qDebug() << "REPLY ChatGpt select first field id:" << jsonReply;
                        _recordJsonSelect(skuParent, jsonReply);
                    }); //TODO I stop here + needs to ask ChatGpt about frequency
            }
            else
            {
                OpenAi::instance()->askQuestion(
                    question,
                    skuParent,
                    [this, skuParent](const QString &jsonReply){
                        qDebug() << "REPLY ChatGpt select second field id:" << jsonReply;
                        _recordJsonSelect(skuParent, jsonReply);
                    });
            }
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
            bool first = true;
            for (auto itFieldId_jsonText = itColorOrig.value().begin();
                 itFieldId_jsonText != itColorOrig.value().end(); ++itFieldId_jsonText)
            {
                if (m_stopAsked)
                {
                    return;
                }
                const QString &firstSku = m_skuParent_skus.value(skuParent);
                const auto &firstInfo = m_sku_infos[firstSku];
                QString question;
                if (first)
                {
                    question += PROMPT_FIRST;
                    question += "\n";
                    question += firstInfo.customInstructions;
                    question += "\n";
                    question += PROMPT_INTRODUCE_JSON;
                    question += "\n";
                }
                const auto &jsonObject = itFieldId_jsonText.value();
                const QString &json = QString::fromUtf8(QJsonDocument(jsonObject).toJson(QJsonDocument::Compact));
                question += json;
                qDebug() << "Asking ChatGpt field id:" << itFieldId_jsonText.key() << " - " << question;
                if (first)
                {
                    QImage *image = new QImage{firstInfo.imageFilePath};
                    OpenAi::instance()->askQuestion(
                        question,
                        skuParentColor,
                        [this, image, colorOrig, skuParent](const QString &jsonReply){
                            delete image;
                            qDebug() << "REPLY text first ChatGpt field id:" << jsonReply;
                            _recordJsonText(skuParent, colorOrig, jsonReply);
                        }); //TODO I stop here + needs to ask ChatGpt about frequency
                }
                else
                {
                    OpenAi::instance()->askQuestion(
                        question,
                        skuParentColor,
                        [this, skuParent, colorOrig](const QString &jsonReply){
                            qDebug() << "REPLY text second ChatGpt field id:" << jsonReply;
                            _recordJsonText(skuParent, colorOrig, jsonReply);
                        });
                }
            }
        }
    }
}

bool GptFiller::_recordJsonSelect(const QString &skuParent, const QString &jsonReply)
{
    ++m_nDone;
    m_callBackProgress(m_nDone, m_nQueries);
    const QJsonDocument &jsonDoc = QJsonDocument::fromJson(jsonReply.toUtf8());
    bool correctReply = true;
    if (jsonDoc.isObject())
    {
        const QJsonObject &reply = jsonDoc.object();
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
    }
    else
    {
        correctReply = false;
    }
    if (m_nDone == m_nQueries)
    {
        m_callbackFinished();
    }
    return correctReply;
}

bool GptFiller::_recordJsonText(const QString &skuParent,
                                const QString &colorOrig,
                                const QString &jsonReply)
{
    ++m_nDone;
    m_callBackProgress(m_nDone, m_nQueries);
    const QJsonDocument &jsonDoc = QJsonDocument::fromJson(jsonReply.toUtf8());
    bool correctReply = true;
    if (jsonDoc.isObject())
    {
        const QJsonObject &reply = jsonDoc.object();
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
    }
    else
    {
        correctReply = false;
    }
    if (m_nDone == m_nQueries)
    {
        m_callbackFinished();
    }
    return correctReply;
}

void GptFiller::_saveReplies()
{

    QJsonArray jsonSelects;
    for (auto itSkuParent = m_skuParent_fieldId_jsonSelectReply.begin();
         itSkuParent != m_skuParent_fieldId_jsonSelectReply.end(); ++itSkuParent)
    {
        for (auto itFieldId_jsonSelect = itSkuParent.value().begin();
             itFieldId_jsonSelect != itSkuParent.value().end(); ++itFieldId_jsonSelect)
        {
            jsonSelects << itFieldId_jsonSelect.value();
        }
    }
    QJsonArray jsonTexts;
    for (auto itSkuParent = m_skuParent_colorOrig_fieldId_jsonTextReply.begin();
         itSkuParent != m_skuParent_colorOrig_fieldId_jsonTextReply.end(); ++itSkuParent)
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
    QJsonObject jsonFinal;
    jsonFinal["selects"] = jsonSelects;
    jsonFinal["texts"] = jsonTexts;
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
        m_skuParent_fieldId_jsonSelectReply.clear();
        m_skuParent_colorOrig_fieldId_jsonTextReply.clear();

        // --- SELECTS
        QJsonArray jsonSelects = obj.value("selects").toArray();
        for (const QJsonValue& v : jsonSelects)
        {
            QJsonObject sel = v.toObject();
            QString skuParent = sel.value("skuParent").toString();
            QString fieldId   = sel.value("fieldId").toString(); // assumed

            if (!skuParent.isEmpty() && !fieldId.isEmpty())
            {
                m_skuParent_fieldId_jsonSelectReply[skuParent][fieldId] = sel;
            }
        }

        // --- TEXTS
        QJsonArray jsonTexts = obj.value("texts").toArray();
        for (const QJsonValue& v : jsonTexts)
        {
            QJsonObject txt = v.toObject();
            QString skuParent = txt.value("skuParent").toString();
            QString colorOrig = txt.value("colorOrig").toString();
            QString fieldId   = txt.value("fieldId").toString(); // assumed

            if (!skuParent.isEmpty() && !colorOrig.isEmpty() && !fieldId.isEmpty())
            {
                m_skuParent_colorOrig_fieldId_jsonTextReply
                    [skuParent][colorOrig][fieldId] = txt;
            }
        }
    }
}

bool GptFiller::_isJsonTextDone(const QString &skuParent, const QString &colorOrig, const QString &fieldId) const
{
    return m_skuParent_colorOrig_fieldId_jsonTextReply.contains(skuParent)
    && m_skuParent_colorOrig_fieldId_jsonTextReply[skuParent].contains(colorOrig)
    && m_skuParent_colorOrig_fieldId_jsonTextReply[skuParent][colorOrig].contains(fieldId);
}

bool GptFiller::_isJsonSelectDone(const QString &skuParent, const QString &fieldId) const
{
    return m_skuParent_fieldId_jsonSelectReply.contains(skuParent)
    && m_skuParent_fieldId_jsonSelectReply[skuParent].contains(fieldId);
}




