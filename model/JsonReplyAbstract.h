#ifndef JSONREPLYABSTRACT_H
#define JSONREPLYABSTRACT_H

#include <QString>
#include <QJsonObject>
#include <QSharedPointer>

#include "SkuInfo.h"

class JsonReplyAbstract
{
public:
    JsonReplyAbstract(const QString &workingDir,
            const QMultiHash<QString, QString> &skuParent_skus
            , const QHash<QString, SkuInfo> *sku_infos
            , const QHash<QString, QHash<QString, QSet<QString>>> *countryCode_langCode_fieldIdChildOnly
            , QHash<QString, QHash<QString, QHash<QString, QHash<QString, QVariant>>>> *sku_countryCode_langCode_fieldId_value);

    virtual QString getName() const = 0;
    virtual bool isJsonReplyCorrect(const QString &skuParent // Those parameters are needed to check json
                                    , const QString &color
                                    , const QStringList &countryCodes
                                    , const QStringList &langCodes
                                    , const QString &fieldId
                                    , const QJsonObject &jsonReply) const = 0;
    virtual void record_fieldId_values(
            const QJsonObject &jsonReplyOfOneCountryLang,
            const QString &countryCode,
            const QString &langCode,
            const QString &skuParent,
            const QString &colorOrig,
            const QString &fieldId,
            const QHash<QString, QHash<QString, QHash<QString, QString>>> &sku_countryCode_langCode_varTitleInfos) const = 0;


    bool contains(const QString &skuParent,
                  const QString &colorOrig,
                  const QString &countryCode,
                  const QString &langCode,
                  const QString &fieldId) const;
    bool reloadJson(const QString &countryCode,
                    const QString &langCode,
                    const QString &skuParent,
                    const QString &colorOrig,
                    const QString &fieldId,
                    const QHash<QString, QHash<QString, QHash<QString, QString>>> &sku_countryCode_langCode_varTitleInfos);

    void save();
    void load();
    bool tryToRecordReply(const QString &skuParent
                          , const QString &color
                          , const QStringList &countryCodes
                          , const QStringList &langCodes // Can also have country information like "fr_FR", "fr_BE" TODO rename localCodes
                          , const QString &fieldId
                          , const QHash<QString, QHash<QString, QHash<QString, QString>>> &sku_countryCode_langCode_varTitleInfos
                          , const QString &jsonReply);
    bool tryToRecordReply(const QString &skuParent
                          , const QString &colorOrig
                          , const QStringList &countryCodes
                          , const QStringList &langCodes
                          , const QString &fieldId
                          , const QHash<QString, QHash<QString, QHash<QString, QString>>> &sku_countryCode_langCode_varTitleInfos
                          , const QJsonObject &jsonReply
                          , bool saveJson = true);

protected:
    QHash<QString, QHash<QString, QHash<QString, QHash<QString, QHash<QString, QJsonObject>>>>> m_skuParent_color_countryCode_langCode_fieldId_jsonObject;
    QHash<QString, QHash<QString, QHash<QString, QHash<QString, QVariant>>>> *m_sku_countryCode_langCode_fieldId_value;
    const QHash<QString, QHash<QString, QSet<QString>>> *m_countryCode_langCode_fieldIdChildOnly;
    const QHash<QString, SkuInfo> *m_sku_infos;
    QHash<QString, QSet<QString>> m_skuParent_skus;
    void _fileLangCountry(const QString &localCode,
                          QString &countryCode,
                          QString &langcode);
    QString m_workingDir;
    QString _tryToFixJson(const QString &jsonReply) const;
    QJsonObject _getReplyObject(const QJsonDocument &jsonDoc) const;
    QJsonObject _getReplyObject(const QString &jsonReply) const;
};

#endif // JSONREPLYABSTRACT_H
