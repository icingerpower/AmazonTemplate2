#ifndef JSONREPLYTEXT_H
#define JSONREPLYTEXT_H

#include "JsonReplySelect.h"

class JsonReplyText : public JsonReplySelect
{
public:
    static const QString PROMPT;
    static const QString PROMPT_TRANSLATE;
    JsonReplyText(const QString &workingDir
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

};

#endif // JSONREPLYTEXT_H
