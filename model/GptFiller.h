#ifndef GPTFILLER_H
#define GPTFILLER_H

#include <QObject>
#include <QMutex>
#include <QSettings>
#include <QDir>
#include <QPair>
#include <QList>
#include <QJsonObject>

class GptFiller : public QObject
{
    Q_OBJECT
public:
    struct SkuInfo{
        QString skuParent;
        QString colorOrig;
        QString sizeTitleOrig;
        QString sizeOrig;
        QString customInstructions;
        QString imageFilePath;
    };
    static const int N_RETRY;
    static const QString PROMPT_FIRST; // First field of an SKU parent showing also an image of the product. Then, json will be sent only
    static const QString PROMPT_INTRODUCE_JSON;
    static const QString PROMPT_ASK_NOT_MANDATORY;
    static const QString PROMPT_TEXT_START;
    static const QString PROMPT_TEXT_END_DESCRIPTION;
    static const QString PROMPT_TEXT_END_BULLET_POINTS;
    static const QString PROMPT_TEXT_END_TITLE;
    explicit GptFiller(const QString &workingDir,
                       const QString &apiKey,
                       QObject *parent = nullptr);
    void askFillingTitles(const QString &countryCodeFrom
                          , const QString &langCodeFrom
                          , const QHash<QString, QSet<QString> > &countryCode_sourceSkus
                          , const QHash<QString, SkuInfo> &sku_infos
                          , const QHash<QString, QHash<QString, QString> > &sku_langCode_varTitleInfos
                          , QHash<QString, QHash<QString, QHash<QString, QHash<QString, QVariant>>>> &sku_countryCode_langCode_fieldId_value
                          , std::function<void()> callbackFinishedSuccess
                          , std::function<void(const QString &)> callbackFinishedFailure);
    void askFillingDescBullets(const QHash<QString, SkuInfo> &sku_infos
                               , const QHash<QString, QHash<QString, QSet<QString>>> &countryCode_langCode_fieldIdMandatory
                               , QHash<QString, QHash<QString, QHash<QString, QHash<QString, QVariant>>>> &sku_countryCode_langCode_fieldId_value
                               , const QSet<QString> &langCodes
                               , std::function<void()> callbackFinishedSuccess
                               , std::function<void(const QString &)> callbackFinishedFailure
                               );
    void askFilling(const QString &countryCodeFrom
                    , const QString &langCodeFrom
                    , const QSet<QString> &fieldIdsToIgnore
                    , const QHash<QString, SkuInfo> &sku_infos
                    , const QHash<QString, QHash<QString, QHash<QString, QHash<QString, QVariant>>>> &sku_countryCode_langCode_fieldId_origValue
                    , const QHash<QString, QHash<QString, QHash<QString, QStringList>>> &countryCode_langCode_fieldId_possibleValues
                    , const QHash<QString, QHash<QString, QSet<QString>>> &countryCode_langCode_fieldIdMandatory
                    , const QHash<QString, QHash<QString, QSet<QString>>> &countryCode_langCode_fieldIdChildOnly
                    , QHash<QString, QHash<QString, QHash<QString, QHash<QString, QVariant>>>> &sku_countryCode_langCode_fieldId_value // Then we will this using the previous
                    , std::function<void (int, int)> callBackProgress
                    , std::function<void()> callbackFinishedSuccess
                    , std::function<void(const QString &)> callbackFinishedFailure
                    );
    void askTrueMandatory(
        const QString &productType
        , const QSet<QString> &mandatordyFieldIds
        , std::function<void(const QSet<QString> &notMandatoryFieldIds)> callbackFinishedSuccess
        , std::function<void(const QString &)> callbackFinishedFailure
        );

public slots:
    void clear();
    void askStop();

signals:
    void finished();

private:
    void _saveReplies();
    void _loadReplies();
    QString m_jsonFilePath;
    QList<QPair<QString, QString>> m_listCountryCode_langCode;
    QString m_countryCodeFrom;
    QString m_langCodeFrom;
    const QHash<QString, QHash<QString, QHash<QString, QHash<QString, QVariant>>>> *m_sku_countryCode_langCode_fieldId_origValue;
    const QHash<QString, QHash<QString, QHash<QString, QStringList>>> *m_countryCode_langCode_fieldId_possibleValues;
    const QHash<QString, QHash<QString, QSet<QString>>> *m_countryCode_langCode_fieldIdMandatory;
    const QHash<QString, QHash<QString, QSet<QString>>> *m_countryCode_langCode_fieldIdChildOnly;
    const QSet<QString> *m_fieldIdsToIgnore;
    const QHash<QString, SkuInfo> *m_sku_infos;
    QHash<QString, QHash<QString, QString>> m_sku_langCode_varTitleInfos;
    QMultiHash<QString, QString> m_skuParent_skus;
    QHash<QString, QHash<QString, QHash<QString, QHash<QString, QVariant>>>> *m_sku_countryCode_langCode_fieldId_value;
    QHash<QString, QHash<QString, QJsonObject>> m_skuParent_fieldId_jsonSelect;
    QHash<QString, QHash<QString, QHash<QString, QJsonObject>>> m_skuParent_colorOrig_fieldId_jsonText;

    QHash<QString, QHash<QString, QJsonObject>> m_skuParent_fieldId_jsonReplySelect;
    QHash<QString, QHash<QString, QHash<QString, QJsonObject>>> m_skuParent_colorOrig_fieldId_jsonReplyText;
    QHash<QString, QHash<QString, QJsonObject>> m_skuParent_langCodesToJoined_jsonReplyTitles;
    QHash<QString, QHash<QString, QJsonObject>> m_skuParentColor_langCode_jsonReplyDesc;
    QHash<QString, QHash<QString, QJsonObject>> m_skuParentColor_langCode_jsonReplyBullets;
    void _prepareQueries();
    void _processQueries();
    bool _reloadJsonText(const QString &skuParent, const QString &colorOrig, const QString &fieldId);
    bool _reloadJsonSelect(const QString &skuParent, const QString &fieldId);
    bool _reloadJsonTitles(const QString &skuParent, const QStringList &langCodesTo, const QString &langCodesToJoined);
    bool _reloadJsonDesc(const QString &skuParent, const QString &skuColor, const QString &langCode);
    bool _reloadJsonBullets(const QString &skuParentColor, const QString &langCode);
    QString _tryToFixJson(const QString &jsonReply) const;
    QJsonObject _getReplyObject(const QJsonDocument &jsonDoc) const;
    QJsonObject _getReplyObject(const QString &jsonReply) const;
    bool _recordJsonTitles(const QString &skuParent
                           , const QStringList &langCodesTo
                           , const QString &jsonReply);
    bool _recordJsonTitles(const QString &skuParent
                           , const QStringList &langCodesTo
                           , const QJsonObject &jsonObject);
    bool _recordJsonSelect(const QString &skuParent, const QString &jsonReply);
    bool _recordJsonSelect(const QString &skuParent, const QJsonObject &jsonObject);
    bool _recordJsonText(const QString &skuParent, const QString &colorOrig, const QString &jsonReply);
    bool _recordJsonText(const QString &skuParent, const QString &colorOrig, const QJsonObject &jsonObject);
    bool _recordJsonDescription(const QString &skuParent,
                                const QString &colorOrig,
                                const QString &langCode,
                                const QString &jsonReply);
    bool _recordJsonDescription(const QString &skuParent,
                                const QString &colorOrig,
                                const QString &langCode,
                                const QJsonObject &jsonObject);
    bool _recordJsonBulletPoints(const QString &skuParent,
                                 const QString &colorOrig,
                                 const QString &langCode,
                                 const QString &jsonReply);
    bool _recordJsonBulletPoints(const QString &skuParent,
                                 const QString &colorOrig,
                                 const QString &langCode,
                                 const QJsonObject &jsonObject);
    int m_nQueries;
    std::atomic_int m_nDone;
    std::function<void (int, int)> m_callBackProgress;
    std::function<void ()> m_callbackFinishedSuccess;
    std::function<void (const QString &)> m_callbackFinishedFailure;
    bool m_stopAsked;
};

#endif // GPTFILLER_H
