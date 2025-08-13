#include <QJsonDocument>
#include <QSaveFile>
#include <QDir>

#include "JsonReplyAbstract.h"


JsonReplyAbstract::JsonReplyAbstract(
        const QString &workingDir
        , const QMultiHash<QString, QString> &skuParent_skus
        , const QHash<QString, SkuInfo> *sku_infos
        , const QHash<QString, QHash<QString, QSet<QString>>> *countryCode_langCode_fieldIdChildOnly,
        QHash<QString, QHash<QString, QHash<QString, QHash<QString, QVariant>>>> *sku_countryCode_langCode_fieldId_value)
{
    m_workingDir = workingDir;
    for (auto it = skuParent_skus.begin();
         it != skuParent_skus.end(); ++it)
    {
        m_skuParent_skus[it.key()].insert(it.value());
    }
    m_countryCode_langCode_fieldIdChildOnly = countryCode_langCode_fieldIdChildOnly;
    m_sku_countryCode_langCode_fieldId_value = sku_countryCode_langCode_fieldId_value;
    m_sku_infos = sku_infos;
}

bool JsonReplyAbstract::contains(
        const QString &skuParent
        , const QString &colorOrig
        , const QString &countryCode
        , const QString &langCode
        , const QString &fieldId) const
{
    auto itSkuParent = m_skuParent_color_countryCode_langCode_fieldId_jsonObject.constFind(skuParent);
    if (itSkuParent != m_skuParent_color_countryCode_langCode_fieldId_jsonObject.cend())
    {
        const auto &mapColors = itSkuParent.value();
        auto itColor = mapColors.constFind(colorOrig);
        if (itColor != mapColors.cend())
        {
            const auto &itCountryCodes = itColor.value();
            auto itCountryCode = itCountryCodes.constFind(countryCode);
            if (itCountryCode != itCountryCodes.cend())
            {
                const auto &itLangCodes = itCountryCode.value();
                auto itLangCode = itLangCodes.constFind(langCode);
                if (itLangCode != itLangCodes.cend())
                {
                    const auto &itFieldIds = itLangCode.value();
                    return itFieldIds.contains(fieldId);
                }
            }
        }
    }
    return false;
}

bool JsonReplyAbstract::reloadJson(
        const QString &countryCode
        , const QString &langCode
        , const QString &skuParent
        , const QString &colorOrig
        , const QString &fieldId
        , const QHash<QString, QHash<QString, QHash<QString, QString>>> &sku_countryCode_langCode_varTitleInfos)
{
    auto itSkuParent = m_skuParent_color_countryCode_langCode_fieldId_jsonObject.find(skuParent);
    if (itSkuParent != m_skuParent_color_countryCode_langCode_fieldId_jsonObject.end())
    {
        auto itColor = itSkuParent.value().find(colorOrig);
        if (itColor != itSkuParent.value().end())
        {
            auto itCountryCode = itColor.value().find(countryCode);
            if (itCountryCode != itColor.value().end())
            {
                const auto &countryCode = itCountryCode.key();
                auto itLangCode = itCountryCode.value().find(langCode);
                if (itLangCode != itCountryCode.value().end())
                {
                    const auto &langCode = itLangCode.key();
                    auto itFieldId = itLangCode.value().find(fieldId);
                    if (itFieldId != itLangCode.value().end())
                    {
                        const QJsonObject &replyOneCountryLange = itFieldId.value();
                        QJsonObject objectLang;
                        objectLang[langCode] = replyOneCountryLange;
                        QJsonObject objectCountry;
                        objectCountry[countryCode] = objectLang;
                        if (tryToRecordReply(skuParent
                                             , colorOrig
                                             , QStringList{countryCode}
                                             , QStringList{langCode}
                                             , fieldId
                                             , sku_countryCode_langCode_varTitleInfos
                                             , objectCountry
                                             , false))
                        {
                            record_fieldId_values(
                                        replyOneCountryLange,
                                        countryCode,
                                        langCode,
                                        skuParent,
                                        colorOrig,
                                        fieldId,
                                        sku_countryCode_langCode_varTitleInfos);
                            return true;
                        }
                    }
                }
            }

        }
    }
    return false;
}

void JsonReplyAbstract::save()
{
    qDebug() << "JsonReplyAbstract::save()...SAVE";
    QJsonObject jsonSkuParents;
    for (auto itSkuParent = m_skuParent_color_countryCode_langCode_fieldId_jsonObject.begin();
         itSkuParent != m_skuParent_color_countryCode_langCode_fieldId_jsonObject.end(); ++itSkuParent)
    {
        const auto &skuParent = itSkuParent.key();
        QJsonObject jsonColors;
        for (auto itColor = itSkuParent.value().begin();
             itColor != itSkuParent.value().end(); ++itColor)
        {
            const auto &colorOrig = itColor.key();
            QJsonObject jsonCountryCodes;
            for (auto itCountryCode = itColor.value().begin();
                itCountryCode != itColor.value().end(); ++itCountryCode)
            {
                const auto &countryCode = itCountryCode.key();
                QJsonObject jsonLangcodes;
                for (auto itLangCode = itCountryCode.value().begin();
                     itLangCode != itCountryCode.value().end(); ++itLangCode)
                {
                    const auto &langCode = itLangCode.key();
                    QJsonObject jsonFieldIds;
                    for (auto itFieldId = itLangCode.value().begin();
                         itFieldId != itLangCode.value().end(); ++itFieldId)
                    {
                        const auto fieldId = itFieldId.key();
                        const auto &jsonObjectToSave = itFieldId.value();
                        jsonFieldIds[fieldId] = jsonObjectToSave;
                    }
                    jsonLangcodes[langCode] = jsonFieldIds;
                }
                jsonCountryCodes[countryCode] = jsonLangcodes;
            }
            jsonColors[colorOrig] = jsonCountryCodes;
        }
        jsonSkuParents[skuParent] = jsonColors;
    }

    const auto &jsonFileName = getName() + ".json";
    QDir dir{m_workingDir};
    if (!dir.exists())
    {
        dir.mkpath(".");
    }
    const auto &jsonFilePath = dir.absoluteFilePath(jsonFileName);

    QSaveFile jsonFile{jsonFilePath};
    if (!jsonFile.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        qWarning() << "JsonReplyAbstract::save cannot open file for write:" << jsonFilePath << jsonFile.errorString();
        return;
    }

    const QJsonDocument jsonDocument{jsonSkuParents};
    const QByteArray jsonBytes = jsonDocument.toJson(QJsonDocument::Indented);

    if (jsonFile.write(jsonBytes) != jsonBytes.size())
    {
        qWarning() << "JsonReplyAbstract::save failed to write full JSON to:" << jsonFilePath;
        return;
    }
    if (!jsonFile.commit())
    {
        qWarning() << "JsonReplyAbstract::save failed to commit file:" << jsonFilePath << jsonFile.errorString();
        return;
    }
    qDebug() << "JsonReplyAbstract::save()...SAVE...DONE";
}

void JsonReplyAbstract::load()
{
    m_skuParent_color_countryCode_langCode_fieldId_jsonObject.clear();

    const auto &jsonFileName = getName() + ".json";
    QDir dir{m_workingDir};
    const auto &jsonFilePath = dir.absoluteFilePath(jsonFileName);

    QFile jsonFile{jsonFilePath};
    if (!jsonFile.exists())
    {
        qInfo() << "JsonReplyAbstract::load file does not exist yet:" << jsonFilePath;
        return;
    }
    if (!jsonFile.open(QIODevice::ReadOnly))
    {
        qWarning() << "JsonReplyAbstract::load cannot open file for read:" << jsonFilePath << jsonFile.errorString();
        return;
    }

    const QByteArray jsonBytes = jsonFile.readAll();
    jsonFile.close();

    QJsonParseError jsonParseError;
    const QJsonDocument jsonDocument = QJsonDocument::fromJson(jsonBytes, &jsonParseError);
    if (jsonParseError.error != QJsonParseError::NoError)
    {
        qWarning() << "JsonReplyAbstract::load JSON parse error:" << jsonParseError.errorString() << "at offset" << jsonParseError.offset;
        return;
    }
    if (!jsonDocument.isObject())
    {
        qWarning() << "JsonReplyAbstract::load root is not an object in:" << jsonFilePath;
        return;
    }

    const QJsonObject jsonSkuParents = jsonDocument.object();
    for (auto itSkuParent = jsonSkuParents.begin();
         itSkuParent != jsonSkuParents.end(); ++itSkuParent)
    {
        const QString skuParent = itSkuParent.key();
        const QJsonValue jsonColorsValue = itSkuParent.value();
        if (!jsonColorsValue.isObject())
        {
            qWarning() << "JsonReplyAbstract::load expected object for skuParent:" << skuParent;
            continue;
        }
        const QJsonObject jsonColors = jsonColorsValue.toObject();

        for (auto itColor = jsonColors.begin();
             itColor != jsonColors.end(); ++itColor)
        {
            const QString colorOrig = itColor.key();
            const QJsonValue jsonCountryCodesValue = itColor.value();
            if (!jsonCountryCodesValue.isObject())
            {
                qWarning() << "JsonReplyAbstract::load expected object for color:" << skuParent << colorOrig;
                continue;
            }
            const QJsonObject jsonCountryCodes = jsonCountryCodesValue.toObject();

            for (auto itCountryCode = jsonCountryCodes.begin();
                 itCountryCode != jsonCountryCodes.end(); ++itCountryCode)
            {
                const QString countryCode = itCountryCode.key();
                const QJsonValue jsonLangcodesValue = itCountryCode.value();
                if (!jsonLangcodesValue.isObject())
                {
                    qWarning() << "JsonReplyAbstract::load expected object for countryCode:" << skuParent << colorOrig << countryCode;
                    continue;
                }
                const QJsonObject jsonLangcodes = jsonLangcodesValue.toObject();

                for (auto itLangCode = jsonLangcodes.begin();
                     itLangCode != jsonLangcodes.end(); ++itLangCode)
                {
                    const QString langCode = itLangCode.key();
                    const QJsonValue jsonFieldIdsValue = itLangCode.value();
                    if (!jsonFieldIdsValue.isObject())
                    {
                        qWarning() << "JsonReplyAbstract::load expected object for langCode:" << skuParent << colorOrig << countryCode << langCode;
                        continue;
                    }
                    const QJsonObject jsonFieldIds = jsonFieldIdsValue.toObject();
                    for (auto itFieldId = jsonFieldIds.begin();
                         itFieldId != jsonFieldIds.end(); ++itFieldId)
                    {
                        const auto &fieldId = itFieldId.key();
                        const QJsonValue jsonObjectValue = itFieldId.value();
                        if (!jsonObjectValue.isObject())
                        {
                            qWarning() << "JsonReplyAbstract::load expected object for langCode:" << skuParent << colorOrig << countryCode << langCode;
                            continue;
                        }
                        const QJsonObject jsonObjectLoaded = jsonObjectValue.toObject();
                        m_skuParent_color_countryCode_langCode_fieldId_jsonObject
                                [skuParent]
                                [colorOrig]
                                [countryCode]
                                [langCode]
                                [fieldId] = jsonObjectLoaded;
                    }
                }
            }
        }
    }
}

bool JsonReplyAbstract::tryToRecordReply(
        const QString &skuParent
        , const QString &color
        , const QStringList &countryCodes
        , const QStringList &langCodes
        , const QString &fieldId
        , const QHash<QString, QHash<QString, QHash<QString, QString>>> &sku_countryCode_langCode_varTitleInfos
        , const QString &jsonReply)
{
    const QJsonDocument &jsonDoc = QJsonDocument::fromJson(_tryToFixJson(jsonReply).toUtf8());
    bool correctReply = true;
    if (jsonDoc.isObject())
    {
        const auto &jsonReply = _getReplyObject(jsonDoc);
        return tryToRecordReply(skuParent
                                , color
                                , countryCodes
                                , langCodes
                                , fieldId
                                , sku_countryCode_langCode_varTitleInfos
                                , jsonReply);
    }
    else
    {
        correctReply = false;
    }
    return correctReply;
}

bool JsonReplyAbstract::tryToRecordReply(
        const QString &skuParent
        , const QString &colorOrig
        , const QStringList &countryCodes
        , const QStringList &langCodes
        , const QString &fieldId
        , const QHash<QString, QHash<QString, QHash<QString, QString>>> &sku_countryCode_langCode_varTitleInfos
        , const QJsonObject &jsonReply
        , bool saveJson)
{
    bool correctReply = true;
    if (isJsonReplyCorrect(skuParent, colorOrig, countryCodes, langCodes, fieldId, jsonReply))
    {
        for (int i=0; i<countryCodes.size(); ++i)
        {
            const auto &countryCode = countryCodes[i];
            const auto &langCode = langCodes[i];
            Q_ASSERT(jsonReply.contains(countryCode));
            Q_ASSERT(jsonReply[countryCode].isObject());
            auto jsonCountry = jsonReply[countryCode].toObject();
            Q_ASSERT(jsonCountry.contains(langCode));
            Q_ASSERT(jsonCountry[langCode].isObject());
            auto jsonReplyOneLang = jsonCountry[langCode].toObject();
            m_skuParent_color_countryCode_langCode_fieldId_jsonObject[skuParent][colorOrig][countryCode][langCode][fieldId] = jsonReplyOneLang;
            record_fieldId_values(jsonReplyOneLang, countryCode, langCode, skuParent, colorOrig, fieldId, sku_countryCode_langCode_varTitleInfos);
        }
        if (saveJson)
        {
            save();
        }
    }
    else
    {
        correctReply = false;
    }
    return correctReply;
}

void JsonReplyAbstract::_fileLangCountry(
        const QString &localCode, QString &countryCode, QString &langcode)
{
    if (localCode.contains("_"))
    {
        const QStringList &elements = localCode.split("_");
        countryCode = elements.last().toUpper();
        langcode = elements.first().toUpper();
    }
    else
    {
        countryCode = localCode;
        langcode = localCode;
    }
}

QString JsonReplyAbstract::_tryToFixJson(const QString &jsonReply) const
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
    else if (jsonReply.contains("\n{\"description_ai"))
    {
        QString correctReply{jsonReply};
        correctReply.replace("\n{\"description_ai", "\"description_ai");
        correctReply.replace("}\n,{\"desc", ",\"desc");
        correctReply.replace("}\n,{\"bullets", ",\"bullets");
        correctReply.replace("}\n,{\"title", ",\"title");
        correctReply.replace("}\n,{\"max", ",\"max");
        correctReply.replace("}\n}}}", "}}}");
        qDebug() << "Corrected reply:" << correctReply;
        return correctReply;
    }
    else if (jsonReply.count('{') + 1 == jsonReply.count('}'))
    {
        QString fixed = jsonReply;
        // remove only the last closing brace (ignore trailing spaces)
        int i = fixed.size() - 1;
        while (i >= 0 && fixed.at(i).isSpace()) --i;
        if (i >= 0 && fixed.at(i) == '}')
            fixed.remove(i, 1);
        return fixed;
    }
    return jsonReply;
}

QJsonObject JsonReplyAbstract::_getReplyObject(const QJsonDocument &jsonDoc) const
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

QJsonObject JsonReplyAbstract::_getReplyObject(const QString &jsonReply) const
{
    const QJsonDocument &jsonDoc = QJsonDocument::fromJson(
        _tryToFixJson(jsonReply).toUtf8());
    return _getReplyObject(jsonDoc);
}
