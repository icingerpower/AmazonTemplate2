#include <QSaveFile>
#include <QJsonArray>
#include <QJsonDocument>

#include "JsonReplyAiDescription.h"
#include "JsonReplySelect.h"
#include "JsonReplyText.h"
#include "JsonReplyDescBullets.h"
#include "JsonReplyTransBullets.h"
#include "JsonReplyTitles.h"
#include "TemplateMergerFiller.h"

#include "GptFiller.h"

#include "../common/openai/OpenAi.h"

const QString GptFiller::SETTINGS_KEY_MANDATORY{"mandatoryFieldIds"};
const int GptFiller::N_RETRY{6};

const QString GptFiller::PROMPT_ASK_NOT_MANDATORY{
    "I am creating product pages on amazon with a page template."
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
    m_jsonFilePath = dir.absoluteFilePath("chatgpt.json");
    m_workingDir = workingDir;
    m_nQueries = 0;
    m_jsonReplyAiDescription = nullptr,
    OpenAi::instance()->init(apiKey);
    OpenAi::instance()->setMaxInFlight(1);
    OpenAi::instance()->setMinSpacingMs(30000);
    OpenAi::instance()->setMaxRetries(N_RETRY);
}

void GptFiller::init(
        const QString &countryCodeFrom
        , const QString &langCodeFrom
        , const QString &productType
        , const QSet<QString> &fieldIdsToIgnore
        , const QHash<QString, SkuInfo> &sku_infos
        , const QHash<QString, QHash<QString, QHash<QString, QHash<QString, QVariant>>>> &sku_countryCode_langCode_fieldId_origValue
        , const QHash<QString, QHash<QString, QHash<QString, QStringList>>> &countryCode_langCode_fieldId_possibleValues
        , const QHash<QString, QHash<QString, QSet<QString>>> &countryCode_langCode_fieldIdMandatory
        , const QHash<QString, QHash<QString, QSet<QString>>> &countryCode_langCode_fieldIdChildOnly
        , const QHash<QString, QHash<QString, QHash<QString, QString> > > &countryCode_langCode_fieldId_hint
        , QHash<QString, QHash<QString, QHash<QString, QHash<QString, QVariant>>>> &sku_countryCode_langCode_fieldId_value)
{
    m_productType = productType;
    m_fieldIdsToIgnore = &fieldIdsToIgnore;
    m_countryCodeFrom = countryCodeFrom;
    m_langCodeFrom = langCodeFrom;
    m_sku_infos = &sku_infos;
    m_skuParent_skus.clear();
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
    m_countryCode_langCode_fieldId_hint = &countryCode_langCode_fieldId_hint;
    m_jsonReplyAiDescription = new JsonReplyAiDescription{
            m_workingDir
            , m_skuParent_skus
            , m_sku_infos
            , m_countryCode_langCode_fieldIdChildOnly
            , m_sku_countryCode_langCode_fieldId_value
    };
    m_jsonReplyAiDescription->load();
    m_jsonReplySelect = new JsonReplySelect{
            m_workingDir
            , m_skuParent_skus
            , m_sku_infos
            , m_countryCode_langCode_fieldIdChildOnly
            , m_sku_countryCode_langCode_fieldId_value};
    m_jsonReplySelect->load();
    m_jsonReplyText = new JsonReplyText{
            m_workingDir
            , m_skuParent_skus
            , m_sku_infos
            , m_countryCode_langCode_fieldIdChildOnly
            , m_sku_countryCode_langCode_fieldId_value};
    m_jsonReplyText->load();
    m_jsonReplyTransBullets = new JsonReplyTransBullets{
            m_workingDir
            , m_skuParent_skus
            , m_sku_infos
            , m_countryCode_langCode_fieldIdChildOnly
            , m_sku_countryCode_langCode_fieldId_value};
    m_jsonReplyTransBullets->load();

    m_jsonReplyDescBullets = new JsonReplyDescBullets{
            m_workingDir
            , m_skuParent_skus
            , m_sku_infos
            , m_countryCode_langCode_fieldIdChildOnly
            , m_sku_countryCode_langCode_fieldId_value};
    m_jsonReplyDescBullets->load();
    m_jsonReplyTitles = new JsonReplyTitles{
            m_workingDir
            , m_skuParent_skus
            , m_sku_infos
            , m_countryCode_langCode_fieldIdChildOnly
            , m_sku_countryCode_langCode_fieldId_value};
    m_jsonReplyTitles->load();
}

QList<PromptInfo> GptFiller::getProductAiDescriptionsPrompts() const
{
    QList<PromptInfo> promptInfos;
    QHash<QString, QSet<QString>> parent_colorDone;
    const QSet<QString> &allFieldIds = _getAllFieldIds();

    QStringList allFieldIdsSorted{allFieldIds.begin(), allFieldIds.end()};
    std::sort(allFieldIdsSorted.begin(), allFieldIdsSorted.end());
    const QString countryCodeTo{"COM"};
    const QString langCodeTo{"EN"};

    for (auto it = m_sku_infos->begin();
         it != m_sku_infos->end(); ++it)
    {
        const auto &sku = it.key();
        bool isParent = m_skuParent_skus.contains(sku);
        if (!isParent)
        {
            const auto &info = it.value();
            const auto &skuParent = info.skuParent;
            const auto &colorOrig = info.colorOrig;
            const auto &skuParentColor = skuParent + colorOrig;
            if (!parent_colorDone[skuParent].contains(colorOrig))
            {
                if (!m_jsonReplyAiDescription->reloadJson(
                            countryCodeTo,
                            langCodeTo,
                            skuParent,
                            colorOrig,
                            QString{},
                            m_sku_countryCode_langCode_varTitleInfos))
                {

                    PromptInfo promptInfo;
                    promptInfo.skuParent = skuParent;
                    promptInfo.colorOrig = colorOrig;
                    promptInfo.imageFilePath = info.imageFilePath;
                    promptInfo.question =
                            JsonReplyAiDescription::PROMPT_DESC_PRODUCT.arg
                            (info.customInstructions, allFieldIdsSorted.join(", "));
                    promptInfos << promptInfo;
                }
            }
            parent_colorDone[skuParent].insert(colorOrig);
        }
    }
    std::sort(promptInfos.begin(), promptInfos.end(), [](
              const PromptInfo &promptInfo1, const PromptInfo &promptInfo2) -> bool{
        return promptInfo1.skuParent + promptInfo1.colorOrig < promptInfo2.skuParent + promptInfo2.colorOrig;
    });
    return promptInfos;
}

int GptFiller::recordProductAiDescriptionsPrompts(const QList<PromptInfo> &promptInfos)
{
    int nFailed = 0;
    for (const auto &promptInfo : promptInfos)
    {
        if (!m_jsonReplyAiDescription->tryToRecordReply(
                    promptInfo.skuParent
                    , promptInfo.colorOrig
                    , QStringList{"COM"}
                    , QStringList{"EN"}
                    , QString{}
                    , m_sku_countryCode_langCode_varTitleInfos
                    , promptInfo.reply))
        {
            ++nFailed;
        }
    }
    return nFailed;
}

bool GptFiller::recordProductAiDescriptionsPrompt(const PromptInfo &promptInfo)
{
    if (recordProductAiDescriptionsPrompts(
                QList<PromptInfo>{promptInfo}) == 0)
    {
        return true;
    }
    return false;
}

QSet<QString> GptFiller::_getAllFieldIds() const
{
    QSet<QString> allFieldIds;
    for (auto itCountryCode = m_countryCode_langCode_fieldIdMandatory->begin();
         itCountryCode != m_countryCode_langCode_fieldIdMandatory->end(); ++itCountryCode)
    {
        for (auto itLangcode = itCountryCode.value().begin();
             itLangcode != itCountryCode.value().end(); ++itLangcode)
        {
            allFieldIds.unite(itLangcode.value());
        }
    }
    return allFieldIds;
}

void GptFiller::askProductAiDescriptions(
        std::function<void ()> callbackFinishedSuccess,
        std::function<void (const QString &)> callbackFinishedFailure)
{
    m_nDone = 0;
    m_nDoneFailed = 0;
    m_nQueries = 0;

    QHash<QString, QSet<QString>> parent_colorDone;
    const QSet<QString> &allFieldIds = _getAllFieldIds();

    QStringList allFieldIdsSorted{allFieldIds.begin(), allFieldIds.end()};
    std::sort(allFieldIdsSorted.begin(), allFieldIdsSorted.end());
    const QString countryCodeTo{"COM"};
    const QString langCodeTo{"EN"};

    for (auto it = m_sku_infos->begin();
         it != m_sku_infos->end(); ++it)
    {
        const auto &sku = it.key();
        bool isParent = m_skuParent_skus.contains(sku);
        if (!isParent)
        {
            const auto &info = it.value();
            const auto &skuParent = info.skuParent;
            const auto &colorOrig = info.colorOrig;
            const auto &skuParentColor = skuParent + colorOrig;
            if (!parent_colorDone[skuParent].contains(colorOrig))
            {
                if (!m_jsonReplyAiDescription->reloadJson(
                            countryCodeTo,
                            langCodeTo,
                            skuParent,
                            colorOrig,
                            QString{},
                            m_sku_countryCode_langCode_varTitleInfos))
                {
                    QString question{
                        JsonReplyAiDescription::PROMPT_DESC_PRODUCT.arg
                                (info.customInstructions, allFieldIdsSorted.join(", "))};
                    auto image = QSharedPointer<QImage>{new QImage{info.imageFilePath}};
                    qDebug() << "\nWill ask QUESTIONS: GptFiller::askProductAiDescriptions:" << question;
                    ++m_nQueries;
                    OpenAi::instance()->askQuestion(
                                question
                                , *image
                                , skuParent + colorOrig
                                , N_RETRY
                                , "gpt-4.1" //"gpt-5"
                                , [this, skuParent, colorOrig, countryCodeTo, langCodeTo](const QString &jsonReply) -> bool{
                        qDebug() << "REPLY GptFiller::askProductAiDescriptions:" << jsonReply;
                        return m_jsonReplyAiDescription->tryToRecordReply(
                                    skuParent
                                    , colorOrig
                                    , QStringList{countryCodeTo}
                                    , QStringList{langCodeTo}
                                    , QString{}
                                    , m_sku_countryCode_langCode_varTitleInfos
                                    , jsonReply
                                    );
                        // TODO if fails, I need to find a non-async way to ask to fix prompt
                    }
                    , [this, image, info, skuParent, callbackFinishedSuccess, callbackFinishedFailure](const QString &jsonReply){
                        ++m_nDone;
                        qDebug() << "GptFiller::askProductAiDescriptions OK" << m_nDone << "/" << m_nQueries;
                        if (m_nDone + m_nDoneFailed == m_nQueries)
                        {
                            OpenAi::instance()->setMaxInFlight(2);
                            OpenAi::instance()->setMinSpacingMs(1000);
                            if (m_nDoneFailed == 0)
                            {
                                callbackFinishedSuccess();
                            }
                            else
                            {
                                callbackFinishedFailure(m_lastError);
                            }
                        }
                    }
                    , [this, callbackFinishedFailure](const QString &jsonReply){
                        ++m_nDoneFailed;
                        qDebug() << "GptFiller::askProductAiDescriptions KO" << m_nDone << "/" << m_nQueries;
                        m_lastError = jsonReply;
                        if (m_nDone + m_nDoneFailed == m_nQueries)
                        {
                            OpenAi::instance()->setMaxInFlight(2);
                            OpenAi::instance()->setMinSpacingMs(1000);
                            callbackFinishedFailure("AI description: " + jsonReply);
                        }
                    }
                    );

                }
            }
            parent_colorDone[skuParent].insert(colorOrig);
        }
    }
    if (m_nQueries == 0)
    {
        OpenAi::instance()->setMaxInFlight(2);
        OpenAi::instance()->setMinSpacingMs(1000);
        callbackFinishedSuccess();
    }
    else
    {
        qDebug() << "Starting GptFiller::askProductAiDescriptions number of queries:" << m_nQueries;
    }
}

void GptFiller::askFillingTransBullets(
        std::function<void ()> callbackFinishedSuccess
        , std::function<void (const QString &)> callbackFinishedFailure)
{
    m_nDone = 0;
    m_nQueries = 0;
    m_nDoneFailed = 0;
    QHash<QString, QSet<QString>> skuParents_colorsDone;
    QHash<QString, QHash<QString, QHash<QString, JsonSourceInfos>>> skuParent_color_fieldId_jsonSourceTransBullets;

    for (auto itSku = m_sku_countryCode_langCode_fieldId_value->begin();
         itSku != m_sku_countryCode_langCode_fieldId_value->end(); ++itSku)
    {
        const auto &sku = itSku.key();
        bool isParent = m_skuParent_skus.contains(sku);
        const auto &skuParent = isParent ? sku : (*m_sku_infos)[sku].skuParent;
        auto colorOrig = isParent ? QString{} :(*m_sku_infos)[sku].colorOrig;
        const auto &fieldId_origValue = (*m_sku_countryCode_langCode_fieldId_origValue)[sku][m_countryCodeFrom][m_langCodeFrom];
        QStringList fieldIds;
        for (int i=1; i<=5; ++i)
        {
            QString fieldIdBulletPoint = "bullet_point" + QString::number(i);
            if (fieldId_origValue.contains(fieldIdBulletPoint))
            {
                fieldIds << fieldIdBulletPoint;
            }
        }
        if (!skuParents_colorsDone[skuParent].contains(colorOrig))
        {
            for (auto itCountryCode = m_countryCode_langCode_fieldIdMandatory->begin();
                 itCountryCode != m_countryCode_langCode_fieldIdMandatory->end(); ++itCountryCode)
            {
                const auto &countryCodeTo = itCountryCode.key();
                for (auto itLangCode = itCountryCode.value().begin();
                     itLangCode != itCountryCode.value().end(); ++itLangCode)
                {
                    const auto &langCodeTo = itLangCode.key();

                    int sizeBefore = (*m_sku_countryCode_langCode_fieldId_value)[sku].size();
                    const auto &fieldId_value = (*m_sku_countryCode_langCode_fieldId_value)[sku][countryCodeTo][langCodeTo];
                    int sizeAfter = (*m_sku_countryCode_langCode_fieldId_value)[sku].size();
                    Q_ASSERT(sizeBefore == sizeAfter);
                    for (const auto &fieldId : fieldIds)
                    {
                        if (!isParent || !(*m_countryCode_langCode_fieldIdChildOnly)[countryCodeTo][langCodeTo].contains(fieldId))
                        {
                            if (!fieldId_value.contains(fieldId))
                            {
                                if (!m_jsonReplyTransBullets->reloadJson(
                                            countryCodeTo,
                                            langCodeTo,
                                            skuParent,
                                            colorOrig,
                                            fieldId,
                                            m_sku_countryCode_langCode_varTitleInfos))
                                {
                                    JsonSourceInfos &jsonSourceInfos = skuParent_color_fieldId_jsonSourceTransBullets[skuParent][colorOrig][fieldId];
                                    jsonSourceInfos.countryCodesTo << countryCodeTo;
                                    jsonSourceInfos.langCodesTo << langCodeTo;
                                    if (!jsonSourceInfos.object.contains(countryCodeTo))
                                    {
                                        jsonSourceInfos.object[countryCodeTo] = QJsonObject{};
                                    }
                                    auto countryObject = jsonSourceInfos.object[countryCodeTo].toObject();
                                    QJsonObject valueObject;
                                    valueObject["value"] = "TODO";
                                    QJsonObject fieldObject;
                                    fieldObject[fieldId] = valueObject;
                                    countryObject[langCodeTo] = fieldObject;
                                    jsonSourceInfos.object[countryCodeTo] = countryObject;
                                }
                            }
                        }
                    }
                    int sizeAfter2 = (*m_sku_countryCode_langCode_fieldId_value)[sku].size();
                    Q_ASSERT(sizeBefore == sizeAfter2);
                }
            }
        }
        skuParents_colorsDone[skuParent].insert(colorOrig);
    }
    _askFillingTransBullets(skuParent_color_fieldId_jsonSourceTransBullets
                           , callbackFinishedSuccess
                           , callbackFinishedFailure);
    if (m_nQueries == 0)
    {
        callbackFinishedSuccess();
    }
    else
    {
        qDebug() << "Starting GptFiller::askFillingTransBullets number of queries:" << m_nQueries;
    }
}

void GptFiller::_askFillingTransBullets(
        const QHash<QString, QHash<QString, QHash<QString, JsonSourceInfos>>> &skuParent_color_fieldId_jsonSourceTransBullets
        , std::function<void ()> callbackFinishedSuccess
        , std::function<void (const QString &)> callbackFinishedFailure)
{
    for (auto itSkuParent = skuParent_color_fieldId_jsonSourceTransBullets.begin();
         itSkuParent != skuParent_color_fieldId_jsonSourceTransBullets.end(); ++itSkuParent)
    {
        const auto &skuParent = itSkuParent.key();
        for (auto itColor = itSkuParent.value().begin();
             itColor != itSkuParent.value().end(); ++itColor)
        {
            const auto &colorOrig = itColor.key();
            for (auto itFieldId = itColor.value().begin();
                 itFieldId != itColor.value().end(); ++itFieldId)
            {
                const auto &fieldId = itFieldId.key();
                const auto &jsonObjectSource = itFieldId.value();
                const auto &jsonSource = _getStringFromJson(jsonObjectSource.object);
                const auto &customInstructions = (*m_sku_infos)[skuParent].customInstructions;
                const auto &aiDescription = m_jsonReplyAiDescription->get_description_ai(skuParent, colorOrig);
                const auto &countryCodesTo = jsonObjectSource.countryCodesTo;
                const auto &langCodesTo = jsonObjectSource.langCodesTo;
                auto skuFirst = m_skuParent_skus.values(skuParent)[0];
                const auto &bulletFrom = (*m_sku_countryCode_langCode_fieldId_origValue)[skuFirst][m_countryCodeFrom][m_langCodeFrom][fieldId].toString();
                QString question{
                    JsonReplyTransBullets::PROMPT.arg
                            (m_langCodeFrom, bulletFrom, jsonSource)};
                qDebug().noquote() << "\nWill ask QUESTIONS: GptFiller::askFillingTransBullets:" << question;
                ++m_nQueries;
                OpenAi::instance()->askQuestion(
                            question
                            , skuParent
                            , N_RETRY * 2
                            , "gpt-4.1"
                            , [this, skuParent, colorOrig, fieldId, countryCodesTo, langCodesTo](const QString &jsonReply) -> bool{
                    qDebug().noquote() << "REPLY GptFiller::_askFillingTransBullets TRANS:" << jsonReply;
                    return m_jsonReplyTransBullets->tryToRecordReply(
                                skuParent
                                , colorOrig
                                , countryCodesTo
                                , langCodesTo
                                , fieldId
                                , m_sku_countryCode_langCode_varTitleInfos
                                , jsonReply
                                );
                }
                , [this, callbackFinishedSuccess, callbackFinishedFailure](const QString &jsonReply){
                    ++m_nDone;
                    qDebug() << "GptFiller::_askFillingTransBullets OK" << m_nDone << "/" << m_nQueries;
                    if (m_nDone + m_nDoneFailed == m_nQueries)
                    {
                        if (m_nDoneFailed > 0)
                        {
                            callbackFinishedFailure("Select: " + m_lastError);
                        }
                        else
                        {
                            callbackFinishedSuccess();

                        }
                    }
                }
                , [this, callbackFinishedFailure](const QString &jsonReply){
                    ++m_nDoneFailed;
                    m_lastError = jsonReply;
                    qDebug() << "GptFiller::_askFillingTransBullets KO" << m_nDone << "/" << m_nQueries;
                    if (m_nDone + m_nDoneFailed == m_nQueries)
                    {
                        callbackFinishedFailure("Select: " + jsonReply);
                    }
                }
                );
            }
        }
     }
}

void GptFiller::askFillingDescBullets(
        std::function<void ()> callbackFinishedSuccess
        , std::function<void (const QString &)> callbackFinishedFailure)
{
    m_nDone = 0;
    m_nQueries = 0;
    m_nDoneFailed = 0;
    QHash<QString, QSet<QString>> skuParents_colorsDone;
    QHash<QString, QHash<QString, JsonSourceInfos>> skuParent_color_jsonSourceDescBullets;

    for (auto itSku = m_sku_countryCode_langCode_fieldId_value->begin();
         itSku != m_sku_countryCode_langCode_fieldId_value->end(); ++itSku)
    {
        const auto &sku = itSku.key();
        bool isParent = m_skuParent_skus.contains(sku);
        const auto &skuParent = isParent ? sku : (*m_sku_infos)[sku].skuParent;
        auto colorOrig = isParent ? QString{} :(*m_sku_infos)[sku].colorOrig;
        if (!skuParents_colorsDone[skuParent].contains(colorOrig))
        {
            for (auto itCountryCode = m_countryCode_langCode_fieldIdMandatory->begin();
                 itCountryCode != m_countryCode_langCode_fieldIdMandatory->end(); ++itCountryCode)
            {
                const auto &countryCodeTo = itCountryCode.key();
                for (auto itLangCode = itCountryCode.value().begin();
                     itLangCode != itCountryCode.value().end(); ++itLangCode)
                {
                    const auto &langCodeTo = itLangCode.key();

                    const auto &fieldId_value = (*m_sku_countryCode_langCode_fieldId_value)[sku][countryCodeTo][langCodeTo];
                    if (!_containsDescription(fieldId_value)
                            || !_containsBullets(fieldId_value))
                    {
                        if (!m_jsonReplyDescBullets->reloadJson(
                                    countryCodeTo,
                                    langCodeTo,
                                    skuParent,
                                    colorOrig,
                                    QString{},
                                    m_sku_countryCode_langCode_varTitleInfos))
                        {
                            JsonSourceInfos &jsonSourceInfos = skuParent_color_jsonSourceDescBullets[skuParent][colorOrig];
                            jsonSourceInfos.countryCodesTo << countryCodeTo;
                            jsonSourceInfos.langCodesTo << langCodeTo;
                            if (!jsonSourceInfos.object.contains(countryCodeTo))
                            {
                                jsonSourceInfos.object[countryCodeTo] = QJsonObject{};
                            }
                            auto countryObject = jsonSourceInfos.object[countryCodeTo].toObject();
                            QJsonObject valuesObject;
                            valuesObject["description"] = "TODO";
                            QJsonArray array;
                            for (int i=0; i<5; ++i)
                            {
                                array.append("TODO");
                            }
                            valuesObject["bullets"] = array;
                            countryObject[langCodeTo] = valuesObject;
                            jsonSourceInfos.object[countryCodeTo] = countryObject;
                        }
                    }
                }
            }
            skuParents_colorsDone[skuParent].insert(colorOrig);
        }
    }
    _askFillingDescBullets(skuParent_color_jsonSourceDescBullets
                           , callbackFinishedSuccess
                           , callbackFinishedFailure);
    if (m_nQueries == 0)
    {
        callbackFinishedSuccess();
    }
    else
    {
        qDebug() << "Starting GptFiller::askFillingDescBullets number of queries:" << m_nQueries;
    }
}

void GptFiller::_askFillingDescBullets(
        const QHash<QString, QHash<QString, JsonSourceInfos>> &skuParent_color_jsonSourceDescBullets
        , std::function<void ()> callbackFinishedSuccess
        , std::function<void (const QString &)> callbackFinishedFailure)
{
    for (auto itSkuParent = skuParent_color_jsonSourceDescBullets.begin();
         itSkuParent != skuParent_color_jsonSourceDescBullets.end(); ++itSkuParent)
    {
        const auto &skuParent = itSkuParent.key();
        for (auto itColor = itSkuParent.value().begin();
             itColor != itSkuParent.value().end(); ++itColor)
        {
            const auto &colorOrig = itColor.key();
            const auto &jsonObjectSource = itColor.value();
            const auto &jsonSource = _getStringFromJson(jsonObjectSource.object);
            const auto &description_en
                    = m_jsonReplyAiDescription->get_description_amz_metric_only(skuParent, colorOrig);
            const auto &bullets_en
                    = m_jsonReplyAiDescription->get_bullets_metric_only(skuParent, colorOrig);
            QString bullets_en_formated = "[\"" + bullets_en.join("\",\"") + "\"]";
            const auto &countryCodesTo = jsonObjectSource.countryCodesTo;
            const auto &langCodesTo = jsonObjectSource.langCodesTo;
            auto skuFirst = m_skuParent_skus.values(skuParent)[0];
            const auto &info = (*m_sku_infos)[skuFirst];
            const auto &customInstructions = info.customInstructions;
            QString question{
                JsonReplyDescBullets::PROMPT.arg
                        (description_en, bullets_en_formated, customInstructions, jsonSource)};
            ++m_nQueries;
            qDebug().noquote() << "\nGptFiller::askFillingDescBullets question:" << question;
            OpenAi::instance()->askQuestion(
                        question
                        , skuParent
                        , N_RETRY * 2
                        , "gpt-4.1"
                        , [this, skuParent, colorOrig, countryCodesTo, langCodesTo](const QString &jsonReply) -> bool{
                qDebug().noquote() << "\nREPLY GptFiller::askFillingDescBullets TEXT:" << jsonReply;
                return m_jsonReplyDescBullets->tryToRecordReply(
                            skuParent
                            , colorOrig
                            , countryCodesTo
                            , langCodesTo
                            , QString{}
                            , m_sku_countryCode_langCode_varTitleInfos
                            , jsonReply
                            );
            }
            , [this, callbackFinishedSuccess, callbackFinishedFailure](const QString &jsonReply){
                ++m_nDone;
                qDebug() << "GptFiller::_askFillingDescBullets OK" << m_nDone <<  "/" << m_nQueries;
                if (m_nDone + m_nDoneFailed == m_nQueries)
                {
                    if (m_nDoneFailed > 0)
                    {
                        callbackFinishedFailure("Text: " + m_lastError);
                    }
                    else
                    {
                        callbackFinishedSuccess();
                    }
                }
            }
            , [this, callbackFinishedFailure](const QString &jsonReply){
                ++m_nDoneFailed;
                m_lastError = jsonReply;
                qDebug() << "GptFiller::_askFillingDescBullets " << m_nDone <<  "/" << m_nQueries;
                if (m_nDone + m_nDoneFailed == m_nQueries)
                {
                    callbackFinishedFailure("Text: " + jsonReply);
                }
            }
            );
        }
    }
}



void GptFiller::askFillingSelectsAndTexts(
        std::function<void ()> callbackFinishedSuccess
        , std::function<void (const QString &)> callbackFinishedFailure)
{
    m_nDone = 0;
    m_nQueries = 0;
    m_nDoneFailed = 0;
    QHash<QString, QSet<QString>> skuParents_colorsDone;

    QHash<QString, QHash<QString, QHash<QString, JsonSourceInfos>>> skuParent_color_fieldId_jsonSourceText;
    QHash<QString, QHash<QString, QHash<QString, QHash<QString, QHash<QString, JsonSourceInfos>>>>>
            skuParent_color_countryCode_langCode_fieldId_jsonSourceSelect;
    for (auto itSku = m_sku_countryCode_langCode_fieldId_value->begin();
         itSku != m_sku_countryCode_langCode_fieldId_value->end(); ++itSku)
    {
        const auto &sku = itSku.key();
        bool isParent = m_skuParent_skus.contains(sku);
        const auto &skuParent = isParent ? sku : (*m_sku_infos)[sku].skuParent;
        auto colorOrig = isParent ? QString{} :(*m_sku_infos)[sku].colorOrig;
        if (!skuParents_colorsDone[skuParent].contains(colorOrig))
        {
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
                        static const QSet<QString> extraPatternToIgnore{
                            "generic_keywords", "list_price", "bullet_point", "product_description", "item_name"};
                        bool toIgnore = false;
                        for (const auto &pattern : extraPatternToIgnore)
                        {
                            if (fieldId.contains(pattern))
                            {
                                toIgnore = true;
                                break;
                            }
                        }
                        if (!toIgnore)
                        {
                            QString colorFinal = colorOrig;
                            if (!(*m_countryCode_langCode_fieldIdChildOnly)[countryCodeTo][langCodeTo].contains(fieldId)
                                    || TemplateMergerFiller::FIELD_IDS_ALWAY_SAME_VALUE_CHILD.contains(fieldId))
                            {
                                colorFinal.clear();
                            }
                            if (!isParent || !(*m_countryCode_langCode_fieldIdChildOnly)[countryCodeTo][langCodeTo].contains(fieldId))
                            {
                                if (!m_fieldIdsToIgnore->contains(fieldId)
                                        && !(*m_sku_countryCode_langCode_fieldId_value)[sku][countryCodeTo][langCodeTo].contains(fieldId))
                                {
                                    Q_ASSERT(!fieldId.contains("color") || isParent || !colorFinal.isEmpty());;
                                    if ((*m_countryCode_langCode_fieldId_possibleValues)[countryCodeTo][langCodeTo].contains(fieldId))
                                    { // Value to select
                                        if (!m_jsonReplySelect->reloadJson(
                                                    countryCodeTo,
                                                    langCodeTo,
                                                    skuParent,
                                                    colorFinal,
                                                    fieldId,
                                                    m_sku_countryCode_langCode_varTitleInfos))
                                        {
                                            QJsonArray jsonPossibleValues;
                                            const auto &possibleValues = (*m_countryCode_langCode_fieldId_possibleValues)[countryCodeTo][langCodeTo][fieldId];
                                            for (const auto &possibleValue : possibleValues)
                                            {
                                                jsonPossibleValues << possibleValue;
                                            }
                                            Q_ASSERT(jsonPossibleValues.size() > 1);
                                            Q_ASSERT(jsonPossibleValues.size() < 55); // Implement a rule with TemplateMergerFiller::AUTO_SELECT_PATTERN_POSSIBLE_VALUES
                                            JsonSourceInfos &jsonSourceInfos = skuParent_color_countryCode_langCode_fieldId_jsonSourceSelect[skuParent][colorFinal][countryCodeTo][langCodeTo][fieldId];
                                            jsonSourceInfos.countryCodesTo << countryCodeTo;
                                            jsonSourceInfos.langCodesTo << langCodeTo;
                                            if (jsonSourceInfos.object.size() == 0)
                                            {
                                                if ((*m_sku_countryCode_langCode_fieldId_origValue)[sku][m_countryCodeFrom][m_langCodeFrom].contains(fieldId))
                                                {
                                                    if (!fieldId.contains("size_system"))
                                                    {
                                                        QJsonObject origValue;
                                                        origValue["countryCodeFrom"] = m_countryCodeFrom;
                                                        origValue["langCodeFrom"] = m_langCodeFrom;
                                                        origValue["value"] = (*m_sku_countryCode_langCode_fieldId_origValue)[sku][m_countryCodeFrom][m_langCodeFrom][fieldId].toString();
                                                        jsonSourceInfos.object["fromValue"] = origValue;
                                                    }
                                                }
                                            }
                                            if (!jsonSourceInfos.object.contains(countryCodeTo))
                                            {
                                                jsonSourceInfos.object[countryCodeTo] = QJsonObject{};
                                            }
                                            auto countryObject = jsonSourceInfos.object[countryCodeTo].toObject();
                                            QJsonObject valuesObject;
                                            valuesObject["values"] = jsonPossibleValues;
                                            countryObject[langCodeTo] = valuesObject;
                                            jsonSourceInfos.object[countryCodeTo] = countryObject;
                                        }
                                    }
                                    else
                                    {
                                        Q_ASSERT(!fieldId.contains("apparel_size"));
                                        if (!m_jsonReplyText->reloadJson(
                                                    countryCodeTo,
                                                    langCodeTo,
                                                    skuParent,
                                                    colorOrig,
                                                    fieldId,
                                                    m_sku_countryCode_langCode_varTitleInfos))
                                            //|| (*m_sku_countryCode_langCode_fieldId_origValue)[sku][m_countryCodeFrom][m_langCodeFrom].contains(fieldId))
                                        {
                                            JsonSourceInfos &jsonSourceInfos = skuParent_color_fieldId_jsonSourceText[skuParent][colorOrig][fieldId];
                                            jsonSourceInfos.countryCodesTo << countryCodeTo;
                                            jsonSourceInfos.langCodesTo << langCodeTo;
                                            if (jsonSourceInfos.object.size() == 0)
                                            {
                                                if ((*m_sku_countryCode_langCode_fieldId_origValue)[sku][m_countryCodeFrom][m_langCodeFrom].contains(fieldId))
                                                {
                                                    QJsonObject origValue;
                                                    origValue["countryCodeFrom"] = m_countryCodeFrom;
                                                    origValue["langCodeFrom"] = m_langCodeFrom;
                                                    origValue["value"] = (*m_sku_countryCode_langCode_fieldId_origValue)[sku][m_countryCodeFrom][m_langCodeFrom][fieldId].toString();
                                                    jsonSourceInfos.object["fromValue"] = origValue;
                                                }
                                                if ((*m_countryCode_langCode_fieldId_hint)[countryCodeTo][langCodeTo].contains(fieldId))
                                                {
                                                    jsonSourceInfos.object["documentation_with_exemple"] = (*m_countryCode_langCode_fieldId_hint)[countryCodeTo][langCodeTo][fieldId];
                                                }
                                            }
                                            if (!jsonSourceInfos.object.contains(countryCodeTo))
                                            {
                                                jsonSourceInfos.object[countryCodeTo] = QJsonObject{};
                                            }
                                            auto countryObject = jsonSourceInfos.object[countryCodeTo].toObject();
                                            QJsonObject valueObject;
                                            valueObject["value"] = "TODO";
                                            countryObject[langCodeTo] = valueObject;
                                            jsonSourceInfos.object[countryCodeTo] = countryObject;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        skuParents_colorsDone[skuParent].insert(colorOrig);
    }

    _askFillingSelects(skuParent_color_countryCode_langCode_fieldId_jsonSourceSelect, callbackFinishedSuccess, callbackFinishedFailure);
    _askFillingTexts(skuParent_color_fieldId_jsonSourceText, callbackFinishedSuccess, callbackFinishedFailure);

    if (m_nQueries == 0)
    {
        callbackFinishedSuccess();
    }
    else
    {
        qDebug() << "Starting GptFiller::askFillingSelectsAndTexts number of queries:" << m_nQueries;
    }
}

void GptFiller::_askFillingSelects(
        const QHash<QString, QHash<QString, QHash<QString, QHash<QString, QHash<QString, JsonSourceInfos>>>>>
        &skuParent_color_countryCode_langCode_fieldId_jsonSourceSelect
        , std::function<void ()> callbackFinishedSuccess
        , std::function<void (const QString &)> callbackFinishedFailure)
{
    for (auto itSkuParent = skuParent_color_countryCode_langCode_fieldId_jsonSourceSelect.begin();
         itSkuParent != skuParent_color_countryCode_langCode_fieldId_jsonSourceSelect.end(); ++itSkuParent)
    {
        const auto &skuParent = itSkuParent.key();
        for (auto itColor = itSkuParent.value().begin();
             itColor != itSkuParent.value().end(); ++itColor)
        {
            const auto &colorOrig = itColor.key();
            for (auto itCountryCode = itColor.value().begin();
                 itCountryCode != itColor.value().end(); ++itCountryCode)
            {
                for (auto itLangCode = itCountryCode.value().begin();
                     itLangCode != itCountryCode.value().end(); ++itLangCode)
                {
                    for (auto itFieldId = itLangCode.value().begin();
                         itFieldId != itLangCode.value().end(); ++itFieldId)
                    {
                        const auto &fieldId = itFieldId.key();
                        const auto &jsonObjectSource = itFieldId.value();
                        const auto &jsonSource = _getStringFromJson(jsonObjectSource.object);
                        const auto &skuChild = m_skuParent_skus.values(skuParent);
                        const auto &skuFirst = skuChild[0];
                        const auto &countryCodesTo = jsonObjectSource.countryCodesTo;
                        const auto &langCodesTo = jsonObjectSource.langCodesTo;
                        QString question;
                        if (jsonObjectSource.object.contains("fromValue"))
                        {
                            auto objectCopy = jsonObjectSource.object;
                            objectCopy.remove("documentation_with_exemple");
                            const auto &jsonSource = _getStringFromJson(objectCopy);
                            question = JsonReplySelect::PROMPT_TRANSLATE.arg(jsonSource);
                        }
                        else
                        {
                            const auto &customInstructions = (*m_sku_infos)[skuFirst].customInstructions;
                            const auto &aiDescription = m_jsonReplyAiDescription->get_description_ai(skuParent, (*m_sku_infos)[skuFirst].colorOrig);
                            Q_ASSERT(!aiDescription.isEmpty());
                            question = JsonReplySelect::PROMPT.arg
                                    (aiDescription, customInstructions, fieldId, jsonSource);
                        }
                        ++m_nQueries;
                        qDebug().noquote() << "\nQUESTION GptFiller::_askFillingSelects SELECTS:"
                                           << skuParent << "-" << fieldId << "-" << question;
                        OpenAi::instance()->askQuestion(
                                    question
                                    , skuParent
                                    , N_RETRY
                                    , "gpt-4.1-mini"
                                    , [this, skuParent, colorOrig, fieldId, countryCodesTo, langCodesTo](const QString &jsonReply) -> bool{
                            qDebug().noquote() << "REPLY GptFiller::_askFillingSelects SELECT:" << skuParent << "-" << fieldId << "-" << jsonReply;
                            QList<QSet<QString>> listPossibleValues;
                            for (int i=0; i<countryCodesTo.size(); ++i)
                            {
                                const auto &possibleValues = (*m_countryCode_langCode_fieldId_possibleValues)
                                        [countryCodesTo[i]][langCodesTo[i]][fieldId];
                                QSet<QString> setPossibleValues{possibleValues.begin(), possibleValues.end()};
                                listPossibleValues << setPossibleValues;
                            }
                            if (m_jsonReplySelect->isJsonReplyCorrect(
                                        skuParent
                                        , colorOrig
                                        , countryCodesTo
                                        , langCodesTo
                                        , listPossibleValues
                                        , fieldId
                                        , jsonReply))
                            {
                                return m_jsonReplySelect->tryToRecordReply(
                                            skuParent
                                            , colorOrig
                                            , countryCodesTo
                                            , langCodesTo
                                            , fieldId
                                            , m_sku_countryCode_langCode_varTitleInfos
                                            , jsonReply
                                            );
                            }
                            return false;
                        }
                        , [this, callbackFinishedSuccess, callbackFinishedFailure](const QString &jsonReply){
                            ++m_nDone;
                            qDebug() << "GptFiller::askFillingSelectsAndTexts OK" << m_nDone << "/" << m_nQueries;
                            if (m_nDone + m_nDoneFailed == m_nQueries)
                            {
                                if (m_nDoneFailed > 0)
                                {
                                    callbackFinishedFailure("Select: " + m_lastError);
                                }
                                else
                                {
                                    callbackFinishedSuccess();
                                }
                            }
                        }
                        , [this, callbackFinishedFailure](const QString &jsonReply){
                            ++m_nDoneFailed;
                            qDebug() << "GptFiller::askFillingSelectsAndTexts KO" << m_nDone << "/" << m_nQueries;
                            if (m_nDone + m_nDoneFailed == m_nQueries)
                            {
                                callbackFinishedFailure("Select: " + jsonReply);
                            }
                        }
                        );
                    }
                }
            }
        }
    }
}

void GptFiller::_askFillingTexts(
        const QHash<QString, QHash<QString, QHash<QString, JsonSourceInfos>>> &skuParent_color_fieldId_jsonSourceText
        , std::function<void ()> callbackFinishedSuccess
        , std::function<void (const QString &)> callbackFinishedFailure)
{
    for (auto itSkuParent = skuParent_color_fieldId_jsonSourceText.begin();
         itSkuParent != skuParent_color_fieldId_jsonSourceText.end(); ++itSkuParent)
    {
        const auto &skuParent = itSkuParent.key();
        for (auto itColor = itSkuParent.value().begin();
             itColor != itSkuParent.value().end(); ++itColor)
        {
            const auto &colorOrig = itColor.key();
            for (auto itFieldId = itColor.value().begin();
                 itFieldId != itColor.value().end(); ++itFieldId)
            {
                const auto &fieldId = itFieldId.key();
                auto &jsonObjectSource = itFieldId.value();
                const auto &skuChild = m_skuParent_skus.values(skuParent);
                const auto &skuFirst = skuChild[0];
                const auto &countryCodesTo = jsonObjectSource.countryCodesTo;
                const auto &langCodesTo = jsonObjectSource.langCodesTo;
                QString question;
                if (jsonObjectSource.object.contains("fromValue"))
                {
                    auto objectCopy = jsonObjectSource.object;
                    objectCopy.remove("documentation_with_exemple");
                    const auto &jsonSource = _getStringFromJson(objectCopy);
                    question = JsonReplyText::PROMPT_TRANSLATE.arg(jsonSource);
                }
                else
                {
                    const auto &jsonSource = _getStringFromJson(jsonObjectSource.object);
                    auto customInstructions = (*m_sku_infos)[skuFirst].customInstructions;
                    if (fieldId.contains("style"))
                    {
                        customInstructions += ". Your reply \"value\" should be under 50 characters.";
                    }
                    else if (fieldId.contains("pattern"))
                    {
                        customInstructions += ". Your reply \"value\" should be under 50 characters.";
                    }
                    else if (fieldId.contains("material"))
                    {
                        customInstructions += ". Your reply \"value\" should be under 40 characters.";
                    }
                    else if (fieldId.contains("color") || fieldId.contains("type"))
                    {
                        customInstructions += ". Your reply \"value\" should be under 30 characters.";
                    }
                    const auto &aiDescription = m_jsonReplyAiDescription->get_description_ai(skuParent, (*m_sku_infos)[skuFirst].colorOrig);
                    Q_ASSERT(!aiDescription.isEmpty());
                    question = JsonReplyText::PROMPT.arg
                            (aiDescription, customInstructions, jsonSource);
                }
                ++m_nQueries;
                qDebug().noquote() << "\nQUESTION GptFiller::_askFillingTexts TEXT QUESTIONS:" << fieldId << "-" << question;
                OpenAi::instance()->askQuestion(
                            question
                            , skuParent
                            , N_RETRY
                            , "gpt-4.1-mini"
                            , [this, skuParent, colorOrig, countryCodesTo, fieldId, langCodesTo](const QString &jsonReply) -> bool{
                    qDebug().noquote() << "\nREPLY GptFiller::_askFillingTexts TEXT:" << fieldId << "-" << jsonReply;
                    return m_jsonReplyText->tryToRecordReply(
                                skuParent
                                , colorOrig
                                , countryCodesTo
                                , langCodesTo
                                , fieldId
                                , m_sku_countryCode_langCode_varTitleInfos
                                , jsonReply
                                );
                }
                , [this, callbackFinishedSuccess, callbackFinishedFailure](const QString &jsonReply){
                    ++m_nDone;
                    qDebug() << "GptFiller::askFillingSelectsAndTexts OK" << m_nDone << "/" << m_nQueries;
                    if (m_nDone + m_nDoneFailed == m_nQueries)
                    {
                        if (m_nDoneFailed > 0)
                        {
                            callbackFinishedFailure(m_lastError);
                        }
                        else
                        {
                            callbackFinishedSuccess();
                        }
                    }
                }
                , [this, callbackFinishedFailure](const QString &jsonReply){
                    ++m_nDoneFailed;
                    m_lastError = jsonReply;
                    qDebug() << "GptFiller::askFillingSelectsAndTexts KO" << m_nDone << "/" << m_nQueries;
                    if (m_nDone + m_nDoneFailed == m_nQueries)
                    {
                        callbackFinishedFailure("Text: " + jsonReply);
                    }
                }
                );
            }
        }
    }
}

void GptFiller::askFillingTitles(
        std::function<void ()> callbackFinishedSuccess
        , std::function<void (const QString &)> callbackFinishedFailure)
{
    m_sku_countryCode_langCode_varTitleInfos = _get_sku_countryCode_langCode_varTitleInfos();
    m_nDone = 0;
    m_nQueries = 0;
    m_nDoneFailed = 0;
    QSet<QString> skuParentsDone;
    QHash<QString, JsonSourceInfos> skuParent_jsonSourceTitles;

    for (auto itSku = m_sku_countryCode_langCode_fieldId_value->begin();
         itSku != m_sku_countryCode_langCode_fieldId_value->end(); ++itSku)
    {
        const auto &sku = itSku.key();
        bool isParent = m_skuParent_skus.contains(sku);
        if (isParent)
        {
            const auto &skuParent = sku;
            if (!skuParentsDone.contains(skuParent))
            {
                for (auto itCountryCode = m_countryCode_langCode_fieldIdMandatory->begin();
                     itCountryCode != m_countryCode_langCode_fieldIdMandatory->end(); ++itCountryCode)
                {
                    const auto &countryCodeTo = itCountryCode.key();
                    for (auto itLangCode = itCountryCode.value().begin();
                         itLangCode != itCountryCode.value().end(); ++itLangCode)
                    {
                        const auto &langCodeTo = itLangCode.key();

                        if (!_containsTitle(skuParent, countryCodeTo, langCodeTo))
                        {
                            if (!m_jsonReplyTitles->reloadJson(
                                        countryCodeTo,
                                        langCodeTo,
                                        skuParent,
                                        QString{}, // Color
                                        QString{}, // field id
                                        m_sku_countryCode_langCode_varTitleInfos))
                            {
                                JsonSourceInfos &jsonSourceInfos = skuParent_jsonSourceTitles[skuParent];
                                jsonSourceInfos.countryCodesTo << countryCodeTo;
                                jsonSourceInfos.langCodesTo << langCodeTo;
                                if (!jsonSourceInfos.object.contains(countryCodeTo))
                                {
                                    jsonSourceInfos.object[countryCodeTo] = QJsonObject{};
                                }
                                auto countryObject = jsonSourceInfos.object[countryCodeTo].toObject();
                                QJsonObject valuesObject;
                                valuesObject["title"] = "TODO";
                                countryObject[langCodeTo] = valuesObject;
                                jsonSourceInfos.object[countryCodeTo] = countryObject;
                            }
                        }
                    }
                }
            }
            skuParentsDone.insert(skuParent);
        }
    }
    _askFillingTitles(skuParent_jsonSourceTitles
                           , callbackFinishedSuccess
                           , callbackFinishedFailure);
    if (m_nQueries == 0)
    {
        callbackFinishedSuccess();
    }
    else
    {
        qDebug() << "Starting GptFiller::askFillingTitles number of queries:" << m_nQueries;
    }
}

void GptFiller::_askFillingTitles(
        const QHash<QString, JsonSourceInfos> &skuParent_jsonSourceTitles
        , std::function<void ()> callbackFinishedSuccess
        , std::function<void (const QString &)> callbackFinishedFailure)
{
    for (auto itSkuParent = skuParent_jsonSourceTitles.begin();
         itSkuParent != skuParent_jsonSourceTitles.end(); ++itSkuParent)
    {
        const auto &skuParent = itSkuParent.key();
        const auto &jsonObjectSource = itSkuParent.value();
        const auto &jsonSource = _getStringFromJson(jsonObjectSource.object);
        const auto &skuChild = m_skuParent_skus.values(skuParent);
        const auto &skuFirst = skuChild[0];
        QString colorOrig = (*m_sku_infos)[skuFirst].colorOrig;
        //const auto &customInstructions = (*m_sku_infos)[skuFirst].customInstructions;
        const auto &aiDescription = m_jsonReplyAiDescription->get_description_ai(skuParent, colorOrig);
        const auto &countryCodesTo = jsonObjectSource.countryCodesTo;
        const auto &langCodesTo = jsonObjectSource.langCodesTo;
        QString question{
            JsonReplyTitles::PROMPT.arg
                    (aiDescription, jsonSource)};
        ++m_nQueries;
        qDebug().noquote() << "\nQUESTION GptFiller::_askFillingTitles TITLE:" << question;
        OpenAi::instance()->askQuestion(
                    question
                    , skuParent
                    , N_RETRY + 2
                    , "gpt-4.1-mini"
                    , [this, skuParent, colorOrig, countryCodesTo, langCodesTo](const QString &jsonReply) -> bool{
            qDebug().noquote() << "\nREPLY GptFiller::askFillingSelectsAndTexts TEXT:" << jsonReply;
            return m_jsonReplyTitles->tryToRecordReply(
                        skuParent
                        , QString{}
                        , countryCodesTo
                        , langCodesTo
                        , QString{}
                        , m_sku_countryCode_langCode_varTitleInfos
                        , jsonReply
                        );
        }
        , [this, callbackFinishedSuccess](const QString &jsonReply){
            ++m_nDone;
            qDebug() << "GptFiller::askFillingTitles OK" << m_nDone << "/" << m_nQueries;
            if (m_nDone == m_nQueries)
            {
                callbackFinishedSuccess();
            }
        }
        , [this, callbackFinishedFailure](const QString &jsonReply){
            ++m_nDone;
            qDebug() << "GptFiller::askFillingTitles KO" << m_nDone << "/" << m_nQueries;
            if (m_nDone == m_nQueries)
            {
                callbackFinishedFailure("Text: " + jsonReply);
            }
        }
        );
    }
}

QHash<QString, QHash<QString, QHash<QString, QString> > > GptFiller::_get_sku_countryCode_langCode_varTitleInfos() const
{
    QHash<QString, QHash<QString, QHash<QString, QString>>> sku_countryCode_langCode_varTitleInfos;
    for (auto itSku = m_sku_countryCode_langCode_fieldId_value->begin();
         itSku != m_sku_countryCode_langCode_fieldId_value->end(); ++itSku)
    {
        const auto &sku = itSku.key();
        if (!m_skuParent_skus.contains(sku))
        {
            const auto &infos = (*m_sku_infos)[sku];
            for (auto itCountryCode = itSku.value().begin();
                 itCountryCode != itSku.value().end(); ++itCountryCode)
            {
                const auto &countryCode = itCountryCode.key();
                for (auto itLangCode = itCountryCode.value().begin();
                     itLangCode != itCountryCode.value().end(); ++itLangCode)
                {
                    const auto &langCode = itLangCode.key();
                    if (!sku_countryCode_langCode_varTitleInfos[sku][countryCode].contains(langCode))
                    {
                        QStringList varElements;
                        const auto &fieldId_value = itLangCode.value();
                        if (!infos.colorOrig.isEmpty())
                        {
                            for (const auto &fieldIdColor : TemplateMergerFiller::FIELD_IDS_COLOR_NAME)
                            {
                                if (fieldId_value.contains(fieldIdColor))
                                {
                                    varElements << fieldId_value[fieldIdColor].toString();
                                    break;
                                }
                            }
                        }
                        if (!infos.sizeOrig.isEmpty())
                        {
                            QString sizeTitle;
                            if (infos.sizeTitleOrig.contains("="))
                            {
                                const auto &elementsSize = infos.sizeTitleOrig.split("=");
                                sizeTitle = elementsSize[0] + "=";
                            }
                            for (const auto &fieldIdSize : TemplateMergerFiller::FIELD_IDS_SIZE)
                            {
                                if (fieldId_value.contains(fieldIdSize))
                                {
                                    if (!sizeTitle.endsWith("="))
                                    {
                                        const auto &countrySizeTitle = fieldId_value[fieldIdSize].toString();
                                        if (countrySizeTitle != infos.sizeOrig)
                                        {
                                            sizeTitle += infos.sizeOrig + "=";
                                        }
                                    }
                                    sizeTitle += countryCode + "-" + fieldId_value[fieldIdSize].toString();
                                    varElements << sizeTitle;
                                    break;
                                }
                            }
                            Q_ASSERT(!sizeTitle.isEmpty());
                        }
                        if (varElements.size() > 0)
                        {
                            sku_countryCode_langCode_varTitleInfos[sku][countryCode][langCode] = "(" + varElements.join(", ") + ")";
                        }
                    }
                }
            }
        }
    }
    return sku_countryCode_langCode_varTitleInfos;
}

JsonReplyAiDescription *GptFiller::jsonReplyAiDescription() const
{
    return m_jsonReplyAiDescription;
}

void GptFiller::askTrueMandatory(const QString &productType,
                                 const QSet<QString> &mandatordyFieldIds,
                                 std::function<void (const QSet<QString> &)> callbackFinishedSuccess,
                                 std::function<void (const QString &)> callbackFinishedFailure)
{
    if (settings()->contains(SETTINGS_KEY_MANDATORY))
    {
        const auto &notMandatoryFieldIds
                = settings()->value(SETTINGS_KEY_MANDATORY).value<QSet<QString>>();
        callbackFinishedSuccess(notMandatoryFieldIds);
    }
    else
    {
    QStringList mandatordyFieldIdsList{mandatordyFieldIds.begin(), mandatordyFieldIds.end()};
    std::sort(mandatordyFieldIdsList.begin(), mandatordyFieldIdsList.end());
    const QString &question = PROMPT_ASK_NOT_MANDATORY.arg(
                productType,
                mandatordyFieldIdsList.join("\n"));
    OpenAi::instance()->askQuestion(
                question
                , productType
                , N_RETRY
                , "gpt-4.1"
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
        settings()->setValue(
                    SETTINGS_KEY_MANDATORY
                    , QVariant::fromValue(notMandatoryFieldIds));
        callbackFinishedSuccess(notMandatoryFieldIds);
    }
    , [this, callbackFinishedFailure](const QString &jsonReply){
        callbackFinishedFailure(jsonReply);
    }
    ); // We take a better model as this question is important
    }
}

void GptFiller::clear()
{
    QFile::remove(m_jsonFilePath);
    m_skuParent_fieldId_jsonReplySelect.clear();
    m_skuParent_colorOrig_fieldId_jsonReplyText.clear();
}

QSharedPointer<QSettings> GptFiller::settings() const
{
    const QString settingsPath = QDir{m_workingDir}.absoluteFilePath("settings.ini");
    QSharedPointer<QSettings> settings{new QSettings{settingsPath, QSettings::IniFormat}};
    return settings;
}

QString GptFiller::_getStringFromJson(const QJsonObject &jsonObject) const
{
    return QString::fromUtf8(QJsonDocument(jsonObject).toJson(QJsonDocument::Compact));
}

bool GptFiller::_containsBullets(
            const QHash<QString, QVariant> &fieldId_value) const
{
    static const QSet<QString> bulletPatternFieldIds{
        "bullet_point#%1.value"
        , "bullet_point%1"
    };
    for (int i=1; i<=5; ++i)
    {
        bool containsOne = false;
        for (const auto &bulletPatternFieldId : bulletPatternFieldIds)
        {
            const auto &bulletFieldId = bulletPatternFieldId.arg(i);
            if (fieldId_value.contains(bulletFieldId))
            {
                containsOne = true;
            }
        }
        if (!containsOne)
        {
            return false;
        }
    }
    return true;
}

bool GptFiller::_containsDescription(
            const QHash<QString, QVariant> &fieldId_value) const
{
    return fieldId_value.contains("product_description")
            || fieldId_value.contains("product_description#1.value");
}

bool GptFiller::_containsTitle(
        const QString &skuParent
        , const QString &countryCode
        , const QString &langCode) const
{
    auto skus = m_skuParent_skus.values(skuParent);
    skus << skuParent;
    for (const auto &sku : skus)
    {
        if (!(*m_sku_countryCode_langCode_fieldId_value)[sku][countryCode][langCode].contains("item_name"))
        {
            return false;
        }
    }
    return true;
}





