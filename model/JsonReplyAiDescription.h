#ifndef JSONREPLYAIDESCRIPTION_H
#define JSONREPLYAIDESCRIPTION_H

#include "JsonReplyAbstract.h"

class JsonReplyAiDescription : public JsonReplyAbstract
{
public:
    static const QString PROMPT_DESC_PRODUCT;
    JsonReplyAiDescription(
            const QString &workingDir
            , const QMultiHash<QString, QString> &skuParent_skus
            , const QHash<QString, SkuInfo> *sku_infos
            , const QHash<QString, QHash<QString, QSet<QString>>> *countryCode_langCode_fieldIdChildOnly
            , QHash<QString, QHash<QString, QHash<QString, QHash<QString, QVariant>>>> *sku_countryCode_langCode_fieldId_value);
    void remove(const QString &skuParent,
                const QString &color);
    QHash<QString, QHash<QString, QJsonObject>> get_skuParent_color_jsonReply() const;
    QString getName() const override;
    bool isJsonReplyCorrect(const QString &skuParent
                            , const QString &color
                            , const QStringList &countryCodes
                            , const QStringList &langCodes
                            , const QString &fieldId
                            , const QJsonObject &jsonReply) const override;
    void record_fieldId_values(
            const QJsonObject &jsonReplyOfOneCountryLang,
            const QString &countryCode,
            const QString &langCode,
            const QString &skuParent,
            const QString &colorOrig,
            const QString &fieldId,
            const QHash<QString, QHash<QString, QHash<QString, QString>>> &sku_countryCode_langCode_varTitleInfos) const override;
    QString get_description_ai(const QString &skuParent
                               , const QString &color) const;
    QString get_description_amz_imperial_and_metric(const QString &skuParent
                                                    , const QString &color) const;
    QString get_description_amz_metric_only(const QString &skuParent
                                            , const QString &color) const;
    QStringList get_bullets_imperial_and_metric(const QString &skuParent
                                                , const QString &color) const;
    QStringList get_bullets_metric_only(const QString &skuParent
                                        , const QString &color) const;

};

#endif // JSONREPLYAIDESCRIPTION_H
