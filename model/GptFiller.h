#ifndef GPTFILLER_H
#define GPTFILLER_H

#include <QObject>
#include <QMutex>
#include <QSettings>
#include <QDir>
#include <QPair>
#include <QList>
#include <QJsonObject>
#include <QSharedPointer>

#include "SkuInfo.h"
#include "PromptInfo.h"

class JsonReplyAiDescription;
class JsonReplySelect;
class JsonReplyText;
class JsonReplyDescBullets;
class JsonReplyTitles;
class JsonReplyTransBullets;

class GptFiller : public QObject
{
    Q_OBJECT
public:
    static const int N_RETRY;
    static const QString PROMPT_ASK_NOT_MANDATORY;
    explicit GptFiller(const QString &workingDir,
                       const QString &apiKey,
                       QObject *parent = nullptr);
    void init(const QString &countryCodeFrom
              , const QString &langCodeFrom
              , const QString &productType
              , const QSet<QString> &fieldIdsToIgnore
              , const QHash<QString, SkuInfo> &sku_infos
              , const QHash<QString, QHash<QString, QHash<QString, QHash<QString, QVariant>>>> &sku_countryCode_langCode_fieldId_origValue
              , const QHash<QString, QHash<QString, QHash<QString, QStringList>>> &countryCode_langCode_fieldId_possibleValues
              , const QHash<QString, QHash<QString, QSet<QString>>> &countryCode_langCode_fieldIdMandatory
              , const QHash<QString, QHash<QString, QSet<QString>>> &countryCode_langCode_fieldIdChildOnly
              , const QHash<QString, QHash<QString, QHash<QString, QString>>> &countryCode_langCode_fieldId_hint
              , QHash<QString, QHash<QString, QHash<QString, QHash<QString, QVariant>>>> &sku_countryCode_langCode_fieldId_value
              );
    QList<PromptInfo> getProductAiDescriptionsPrompts() const;
    int recordProductAiDescriptionsPrompts(const QList<PromptInfo> &promptInfos); // Return the number for failed record
    bool recordProductAiDescriptionsPrompt(const PromptInfo &promptInfos); // Return the number for failed record
    void askProductAiDescriptions(
            std::function<void()> callbackFinishedSuccess
            , std::function<void(const QString &)> callbackFinishedFailure);
    void askFillingSelectsAndTexts(
            std::function<void()> callbackFinishedSuccess
            , std::function<void(const QString &)> callbackFinishedFailure);
    void askFillingTransBullets(
            std::function<void()> callbackFinishedSuccess
            , std::function<void(const QString &)> callbackFinishedFailure);
    void askFillingDescBullets(
            std::function<void()> callbackFinishedSuccess
            , std::function<void(const QString &)> callbackFinishedFailure);
    void askFillingTitles(
            std::function<void()> callbackFinishedSuccess
            , std::function<void(const QString &)> callbackFinishedFailure);
    void askTrueMandatory(
        const QString &productType
        , const QSet<QString> &mandatordyFieldIds
        , std::function<void(const QSet<QString> &notMandatoryFieldIds)> callbackFinishedSuccess
        , std::function<void(const QString &)> callbackFinishedFailure
        );

    JsonReplyAiDescription *jsonReplyAiDescription() const;

public slots:
    void clear();

signals:
    void finished();

private:
    struct JsonSourceInfos{
        QJsonObject object;
        QStringList countryCodesTo;
        QStringList langCodesTo;
    };
    QString m_jsonFilePath;
    QString m_workingDir;
    QList<QPair<QString, QString>> m_listCountryCode_langCode;
    QString m_countryCodeFrom;
    QString m_langCodeFrom;
    const QHash<QString, QHash<QString, QHash<QString, QHash<QString, QVariant>>>> *m_sku_countryCode_langCode_fieldId_origValue;
    const QHash<QString, QHash<QString, QHash<QString, QStringList>>> *m_countryCode_langCode_fieldId_possibleValues;
    const QHash<QString, QHash<QString, QSet<QString>>> *m_countryCode_langCode_fieldIdMandatory;
    const QHash<QString, QHash<QString, QSet<QString>>> *m_countryCode_langCode_fieldIdChildOnly;
    const QSet<QString> *m_fieldIdsToIgnore;
    const QHash<QString, SkuInfo> *m_sku_infos;
    const QHash<QString, QHash<QString, QHash<QString, QString>>> *m_countryCode_langCode_fieldId_hint;
    QHash<QString, QHash<QString, QHash<QString, QString>>> m_sku_countryCode_langCode_varTitleInfos;
    QMultiHash<QString, QString> m_skuParent_skus;
    QHash<QString, QHash<QString, QHash<QString, QHash<QString, QVariant>>>> *m_sku_countryCode_langCode_fieldId_value;
    QHash<QString, QHash<QString, QJsonObject>> m_skuParent_fieldId_jsonSelect;
    QHash<QString, QHash<QString, QHash<QString, QJsonObject>>> m_skuParent_colorOrig_fieldId_jsonText;

    QSharedPointer<QSettings> settings() const;
    QHash<QString, QHash<QString, QJsonObject>> m_skuParent_fieldId_jsonReplySelect;
    QHash<QString, QHash<QString, QHash<QString, QJsonObject>>> m_skuParent_colorOrig_fieldId_jsonReplyText;
    QHash<QString, QHash<QString, QJsonObject>> m_skuParent_langCode_jsonReplyTitles;
    QHash<QString, QHash<QString, QJsonObject>> m_skuParentColor_langCode_jsonReplyDesc;
    QHash<QString, QHash<QString, QJsonObject>> m_skuParentColor_langCode_jsonReplyBullets;
    QString _getStringFromJson(const QJsonObject &jsonObject) const;
    QSet<QString> _getAllFieldIds() const;
    bool _containsBullets(const QHash<QString, QVariant> &fieldId_value) const;
    bool _containsDescription(const QHash<QString, QVariant> &fieldId_value) const;
    bool _containsTitle(const QString &skuParent, const QString &countryCode, const QString &langCode) const;
    void _askFillingSelects(const QHash<QString, QHash<QString, QHash<QString, QHash<QString, QHash<QString, JsonSourceInfos> > > > > &skuParent_color_countryCode_langCode_fieldId_jsonSourceSelect
            , std::function<void()> callbackFinishedSuccess
            , std::function<void(const QString &)> callbackFinishedFailure);
    void _askFillingTexts(const QHash<QString, QHash<QString, QHash<QString, JsonSourceInfos> > > &skuParent_color_fieldId_jsonSourceText
            , std::function<void()> callbackFinishedSuccess
            , std::function<void(const QString &)> callbackFinishedFailure);
    void _askFillingTransBullets(
            const QHash<QString, QHash<QString, QHash<QString, JsonSourceInfos> > > &skuParent_color_fieldId_jsonSourceTransBullets
            , std::function<void()> callbackFinishedSuccess
            , std::function<void(const QString &)> callbackFinishedFailure);
    void _askFillingDescBullets(
            const QHash<QString, QHash<QString, JsonSourceInfos>> &skuParent_color_jsonSourceDescBullets
            , std::function<void()> callbackFinishedSuccess
            , std::function<void(const QString &)> callbackFinishedFailure);
    void _askFillingTitles(
            const QHash<QString, JsonSourceInfos> &skuParent_jsonSourceDescBullets
            , std::function<void()> callbackFinishedSuccess
            , std::function<void(const QString &)> callbackFinishedFailure);
    QHash<QString, QHash<QString, QHash<QString, QString> > > _get_sku_countryCode_langCode_varTitleInfos() const;
    int m_nQueries;
    QString m_lastError;
    std::atomic_int m_nDone;
    std::atomic_int m_nDoneFailed;
    std::function<void (int, int)> m_callBackProgress;
    std::function<void ()> m_callbackFinishedSuccess;
    std::function<void (const QString &)> m_callbackFinishedFailure;
    QString m_productType;
    JsonReplyAiDescription *m_jsonReplyAiDescription;
    JsonReplySelect *m_jsonReplySelect;
    JsonReplyText *m_jsonReplyText;
    JsonReplyDescBullets *m_jsonReplyDescBullets;
    JsonReplyTransBullets *m_jsonReplyTransBullets;
    JsonReplyTitles *m_jsonReplyTitles;
    static const QString SETTINGS_KEY_MANDATORY;
};

#endif // GPTFILLER_H
