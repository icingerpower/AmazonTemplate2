#ifndef JSONREPLYDESCBULLETS_H
#define JSONREPLYDESCBULLETS_H

#include "JsonReplyAbstract.h"

class JsonReplyDescBullets : public JsonReplyAbstract
{
public:
    static const QString PROMPT;
    JsonReplyDescBullets(
            const QString &workingDir
            , const QMultiHash<QString, QString> &skuParent_skus
            , const QHash<QString, SkuInfo> *sku_info
            , const QHash<QString, QHash<QString, QSet<QString>>> *countryCode_langCode_fieldIdChildOnly
            , QHash<QString, QHash<QString, QHash<QString, QHash<QString, QVariant>>>> *sku_countryCode_langCode_fieldId_value);
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

};

#endif // JSONREPLYDESCBULLETS_H
