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
        QString customInstructions;
        QString imageFilePath;
    };
    static const QString PROMPT_FIRST; // First field of an SKU parent showing also an image of the product. Then, json will be sent only
    static const QString PROMPT_INTRODUCE_JSON;
    static const QString PROMPT_ASK_NOT_MANDATORY;
    explicit GptFiller(const QString &workingDir,
                       const QString &apiKey,
                       QObject *parent = nullptr);
    void askFilling(const QString &countryCodeFrom
                    , const QString &langCodeFrom
                    , const QSet<QString> &fieldIdsToIgnore
                    , const QHash<QString, SkuInfo> &sku_infos
                    , const QHash<QString, QHash<QString, QHash<QString, QHash<QString, QVariant>>>> &sku_countryCode_langCode_fieldId_origValue
                    , const QHash<QString, QHash<QString, QHash<QString, QStringList>>> &countryCode_langCode_fieldId_possibleValues
                    , const QHash<QString, QHash<QString, QSet<QString>>> &countryCode_langCode_fieldIdMandatory
                    , QHash<QString, QHash<QString, QHash<QString, QHash<QString, QVariant>>>> &skuParent_colorOrig_langCode_fieldId_valueText // Fill this first and ChatGpt will either reply for all or by color
                    , QHash<QString, QHash<QString, QHash<QString, QHash<QString, QVariant>>>> &sku_countryCode_langCode_fieldId_value // Then we will this using the previous
                    , std::function<void()> callbackFinished
                    , std::function<void (int, int)> callBackProgress
                    );
    void askTrueMandatory(
        const QString &productType
        , const QSet<QString> &mandatordyFieldIds
        , std::function<void(const QSet<QString> &notMandatoryFieldIds)> callbackFinished
        );

public slots:
    void clear();
    void askStop();

signals:
    void finished();

private:
    QString m_jsonFilePath;
    QList<QPair<QString, QString>> m_listCountryCode_langCode;
    QString m_countryCodeFrom;
    QString m_langCodeFrom;
    const QHash<QString, QHash<QString, QHash<QString, QHash<QString, QVariant>>>> *m_sku_countryCode_langCode_fieldId_origValue;
    const QHash<QString, QHash<QString, QHash<QString, QStringList>>> *m_countryCode_langCode_fieldId_possibleValues;
    const QHash<QString, QHash<QString, QSet<QString>>> *m_countryCode_langCode_fieldIdMandatory;
    const QSet<QString> *m_fieldIdsToIgnore;
    QHash<QString, SkuInfo> m_sku_infos;
    QMultiHash<QString, QString> m_skuParent_skus;
    QHash<QString, QHash<QString, QHash<QString, QHash<QString, QVariant>>>> *m_skuParent_colorOrig_langCode_fieldId_valueText;
    QHash<QString, QHash<QString, QHash<QString, QHash<QString, QVariant>>>> *m_sku_countryCode_langCode_fieldId_value;
    QHash<QString, QHash<QString, QJsonObject>> m_skuParent_fieldId_jsonSelect;
    QHash<QString, QHash<QString, QHash<QString, QJsonObject>>> m_skuParent_colorOrig_fieldId_jsonText;
    QHash<QString, QHash<QString, QJsonObject>> m_skuParent_fieldId_jsonSelectReply;
    QHash<QString, QHash<QString, QHash<QString, QJsonObject>>> m_skuParent_colorOrig_fieldId_jsonTextReply;
    void _prepareQueries();
    void _processQueries();
    void _saveReplies();
    void _loadReplies();
    bool _isJsonTextDone(const QString &skuParent, const QString &colorOrig, const QString &fieldId) const;
    bool _isJsonSelectDone(const QString &skuParent, const QString &fieldId) const;
    bool _recordJsonSelect(const QString &skuParent, const QString &jsonReply);
    bool _recordJsonText(const QString &skuParent, const QString &colorOrig, const QString &jsonReply);
    int m_nQueries;
    std::atomic_int m_nDone;
    std::function<void (int, int)> m_callBackProgress;
    std::function<void ()> m_callbackFinished;
    bool m_stopAsked;
};

#endif // GPTFILLER_H
