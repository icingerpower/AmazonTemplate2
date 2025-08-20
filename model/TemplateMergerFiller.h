#ifndef TemplateMergerFiller_H
#define TemplateMergerFiller_H

#include <QString>
#include <QStringList>
#include <QDir>

#include <xlsxdocument.h>

#include "GptFiller.h"
#include "SkuInfo.h"
#include "PromptInfo.h"

class TemplateMergerFiller
{
public:
    static const QStringList SHEETS_TEMPLATE;
    static const QStringList SHEETS_VALID_VALUES;
    static const QHash<QString, QString> SHEETS_MANDATORY;
    static const QSet<QString> VALUES_MANDATORY;
    static const QSet<QString> FIELD_IDS_NOT_AI;
    static const QSet<QString> FIELD_IDS_AI_BUT_REQUIRED_IN_FIRST;
    static const QSet<QString> FIELD_IDS_PUT_FIRST_VALUE;
    static const QSet<QString> FIELD_IDS_EXTRA_MANDATORY;
    static const QHash<QString, QSet<QString>> PRODUCT_TYPE_FIELD_IDS_EXTRA_MANDATORY;
    static const QSet<QString> FIELD_IDS_PATTERN_REMOVE_AS_MANDATORY;
    static const QSet<QString> FIELD_IDS_ALWAY_SAME_VALUE;
    static const QSet<QString> FIELD_IDS_ALWAY_SAME_VALUE_CHILD;
    static const QStringList FIELD_IDS_COLOR_NAME;
    static const QStringList FIELD_IDS_SIZE;
    static const QSet<QString> FIELD_IDS_CHILD_ONLY;
    static const QMultiHash<QString, QSet<QString>> AUTO_SELECT_PATTERN_POSSIBLE_VALUES;
    static const QHash<QString, QString> MAPPING_FIELD_ID;
    static const QHash<QString, QHash<QString, QSet<QString>>> COUNTRY_LANG_FIELD_ID_TO_REMOVE;
    static const QHash<QString, QHash<QString, QHash<QString, QHash<QString, QSet<QString>>>>> TYPE_COUNTRY_LANG_FIELD_ID_POSSIBLE_VALUES;
    enum Version{
        V01
        , V02
    };

    enum Gender{
        Female,
        Male,
        Unisex,
        UndefinedGender
    };
    enum Age{
        Adult,
        kid,
        baby,
        UndefinedAge
    };

    typedef std::function<QVariant(const QString &countryFrom,
                                   const QString &countryTo,
                                   const QString &langTo,
                                   const QHash<QString, QHash<QString, QString>> &countryCode_langCode_keywords,
                                   Gender targetGender,
                                   Age age_range_description,
                                   const QVariant &origValue)> FuncFiller;
    static const QHash<QString, FuncFiller> FIELD_IDS_FILLER_NO_SOURCES;
    static const QSet<QString> FIELD_IDS_NO_SOURCES;
    static const QHash<QString, FuncFiller> FIELD_IDS_FILLER;
    struct CopyStrategy{
        QSet<QString> otherFieldId;
        FuncFiller funcFillerFromsource;
    };
    static const QHash<QString, QStringList> FIELD_IDS_COPY_FROM_OTHER;
    TemplateMergerFiller(
            const QString &filePathFrom,
            const QString &customInstructions,
            const QString &apiKey,
            std::function<void(QString &logMessage)> callBackLog);
    ~TemplateMergerFiller();
    void clearPreviousChatgptReplies();
    void preFillExcelFiles(const QString &keywordFilePath
                        , const QStringList &sourceFilePaths
                        , const QStringList &toFillFilePaths
                        , std::function<void()> callBackFinishedSuccess
                        , std::function<void(const QString &)> callbackFinishedFailure
                        );
    void fillExcelFiles(const QString &keywordFilePath
                        , const QStringList &sourceFilePaths
                        , const QStringList &toFillFilePaths
                        , std::function<void(int, int)> callBackProgress
                        , std::function<void()> callBackFinishedSuccess
                        , std::function<void(const QString &)> callbackFinishedFailure
                        );
    void fillAiDescOnly(const QString &keywordFilePath
                        , const QStringList &sourceFilePaths
                        , const QStringList &toFillFilePaths
                        , std::function<void(int, int)> callBackProgress
                        , std::function<void()> callBackFinishedSuccess
                        , std::function<void(const QString &)> callbackFinishedFailure
                        );
    QSet<QString> getLangCodesTo() const;
    // ChatGpt, for each field mandatory, will say if needed for child only or both
    // ChatGpt, will be asked value, for each lang
    // We will use QSettings for crash retake

    GptFiller *gptFiller() const;
    void initGptFiller();

    const QHash<QString, QHash<QString, QHash<QString, QStringList> > > &countryCode_langCode_fieldId_possibleValues() const;

private:
    Version _getDocumentVersion(QXlsx::Document &document) const;
    void _readParentSkus(QXlsx::Document &document,
                  const QString &countryCode,
                  const QString &langCode,
                  QMultiHash<QString, QString> &skuParent_skus);
    void _readSkus(QXlsx::Document &document,
                  const QString &countryCode,
                  const QString &langCode,
                  QStringList &skus,
                  QHash<QString, SkuInfo> &sku_infos,
                  QHash<QString, QHash<QString, QHash<QString, QHash<QString, QVariant>>>> &sku_countryCode_langCode_fieldId_origValue,
                  bool isMainFile = false);
    void _setFilePathsToFill(const QString &keywordFilePath, const QStringList &toFillFilePaths);
    void _readInfoSources(const QStringList &sourceFilePaths);
    void _checkMinimumValues(const QString &toFillFilePath);
    void _checkVarationsNotMissing();
    void _fillDataAutomatically();
    void _fillDataLeftChatGpt(
        std::function<void(int, int)> callBackProgress
        , std::function<void()> callBackFinishedSuccess
        , std::function<void(const QString &)> callBackFinishedError
        );
    void _fillDataLeftChatGptAiDescOnly(
        std::function<void(int, int)> callBackProgress
        , std::function<void()> callBackFinishedSuccess
        , std::function<void(const QString &)> callBackFinishedError
        );
    void _fixAlwaysSameValue();
    void _createToFillXlsx();

    void _readKeywords(const QString &filePath);
    QString _getCountryCode(const QString &templateFilePath) const;
    QString _getLangCode(const QString &templateFilePath) const;
    QString _getLangCodeFromText(const QString &text) const;
    void _selectTemplateSheet(QXlsx::Document &doc) const;
    void _selectMandatorySheet(QXlsx::Document &doc) const;
    void _selectValidValuesSheet(QXlsx::Document &doc) const;

    void _readFields(QXlsx::Document &document, const QString &countryCode, const QString &langCode);
    void _readMandatory(QXlsx::Document &document, const QString &countryCode, const QString &langCode);
    void _readValidValues(QXlsx::Document &document, const QString &countryCode, const QString &langCode, const QString &countryCodeFrom, const QString &langCodeFrom);
    void _preFillChildOny();
    void _preFillTitles();
    bool _isSkuParent(const QString &sku) const;
    QHash<QString, int> _get_fieldId_index(QXlsx::Document &doc) const;
    void _formatFieldId(QString &fieldId) const;
    int _getIndColSku(const QHash<QString, int> &fieldId_index) const;
    int _getIndColSkuParent(const QHash<QString, int> &fieldId_index) const;
    int _getIndColColorName(const QHash<QString, int> &fieldId_index) const;
    int _getIndColSize(const QHash<QString, int> &fieldId_index) const;
    int _getIndColTitle(const QHash<QString, int> &fieldId_index) const;
    int _getIndCol(const QHash<QString, int> &fieldId_index, const QStringList &possibleValues) const;
    int _getRowFieldId(Version version) const;
    void _recordValueAllVersion(QHash<QString, QVariant> &fieldId_value,
                                const QString fieldId,
                                const QVariant &value);
    void _preFillExcelFiles(const QString &keywordFilePath
                            , const QStringList &sourceFilePaths
                            , const QStringList &toFillFilePaths);
    QString _getCustomInstructions(const QString &sku) const;
    QSet<QString> getAllMandatoryFieldIds() const;
    QString m_filePathFrom;
    QStringList m_toFillFilePaths;
    QStringList m_skus;
    QDir m_workdingDirImages;
    QHash<QString, QSet<QString>> m_countryCode_sourceSkus;
    QHash<QString, QHash<QString, QString>> m_countryCode_langCode_keywords;

    QHash<QString, QHash<QString, QHash<QString, QString>>> m_countryCode_langCode_fieldName_fieldId;
    QHash<QString, QHash<QString, QSet<QString>>> m_countryCode_langCode_fieldIdMandatory;
    QHash<QString, QHash<QString, QHash<QString, QStringList>>> m_countryCode_langCode_fieldId_possibleValues;
    QHash<QString, QHash<QString, QSet<QString>>> m_countryCode_langCode_fieldIdChildOnly;
    QHash<QString, QHash<QString, QHash<QString, QString>>> m_countryCode_langCode_fieldId_hint;

    QHash<QString, SkuInfo> m_sku_skuInfos;
    QMultiHash<QString, QString> m_skuParent_skus;
    QHash<QString, QHash<QString, QHash<QString, QHash<QString, QVariant>>>> m_sku_countryCode_langCode_fieldId_origValue;
    //QHash<QString, QHash<QString, QHash<QString, QVariant>>> m_skuParent_langCode_fieldId_value;
    QHash<QString, QHash<QString, QHash<QString, QHash<QString, QVariant>>>> m_sku_countryCode_langCode_fieldId_value;

    static FuncFiller FUNC_FILLER_COPY;
    static FuncFiller FUNC_FILLER_CONVERT_CLOTHE_SIZE;
    static FuncFiller FUNC_FILLER_CONVERT_SHOE_SIZE;
    static FuncFiller FUNC_FILLER_PUT_KEYWORDS;
    static FuncFiller FUNC_FILLER_PRICE;
    QString m_customInstructions;
    QHash<QString, QString> m_skuPattern_customInstructions;
    Gender m_gender;
    Age m_age;
    QString m_productType;
    GptFiller *m_gptFiller;
    std::function<void(QString &logMessage)> m_callBackLog;
};

#endif // TemplateMergerFiller_H
