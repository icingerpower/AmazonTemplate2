#ifndef TemplateMergerFiller_H
#define TemplateMergerFiller_H

#include <QString>
#include <QStringList>

#include <xlsxdocument.h>

class TemplateMergerFiller
{
public:
    static const QString ID_SKU;
    static const QStringList SHEETS_TEMPLATE;
    static const QStringList SHEETS_VALID_VALUES;
    static const QHash<QString, QString> SHEETS_MANDATORY;
    static const QSet<QString> VALUES_MANDATORY;
    static const QSet<QString> FIELD_IDS_NOT_AI;
    static const QSet<QString> FIELD_IDS_PUT_FIRST_VALUE;
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
                                   const QString &countryTo, // TODO add gender + age_range_description + management of EU-46=CN-47
                                   Gender targetGender,
                                   Age age_range_description,
                                   const QVariant &origValue)> FuncFiller;
    static const QHash<QString, FuncFiller> FIELD_IDS_FILLER_NO_SOURCES;
    static const QHash<QString, FuncFiller> FIELD_IDS_FILLER;
    struct CopyStrategy{
        QSet<QString> otherFieldId;
        FuncFiller funcFillerFromsource;
    };
    static const QHash<QString, QSet<QString>> FIELD_IDS_COPY_FROM_OTHER;
    TemplateMergerFiller(
        const QString &filePathFrom);
    void fillExcelFiles(const QStringList &sourceFilePaths, const QStringList &toFillFilePaths);
    // ChatGpt, for each field mandatory, will say if needed for child only or both
    // ChatGpt, will be asked value, for each lang
    // We will use QSettings for crash retake

private:
    void readSkus(const QString &countryCode,
                  QStringList &skus,
                  QHash<QString, QHash<QString, QHash<QString, QVariant>>> &sku_countryCode_fieldId_origValue);
    void setFilePathsToFill(const QStringList &toFillFilePaths);
    void readInfoSources(const QStringList &sourceFilePaths);
    void fillDataAutomatically();
    void fillDataLeftChatGpt();
    void createToFillXlsx();

    QString _getCountryCode(const QString &templateFilePath) const;
    void _selectTemplateSheet(QXlsx::Document &doc);
    void _selectMandatorySheet(QXlsx::Document &doc);
    void _selectValidValuesSheet(QXlsx::Document &doc);

    void _readFields(QXlsx::Document &document, const QString &countryCode);
    void _readMandatory(QXlsx::Document &document, const QString &countryCode);
    void _readValidValues(QXlsx::Document &document, const QString &countryCode);
    void _preFillChildOny();
    QHash<QString, int> _get_fieldId_index(QXlsx::Document &doc) const;
    QString m_filePathFrom;
    QStringList m_toFillFilePaths;
    QStringList m_skus;

    QHash<QString, QHash<QString, QString>> m_countryCode_fieldName_fieldId;
    QHash<QString, QSet<QString>> m_countryCode_fieldIdMandatory;
    QHash<QString, QHash<QString, QStringList>> m_countryCode_fieldId_possibleValues;
    QHash<QString, QSet<QString>> m_countryCode_fieldIdChildOnly; //AI

    QHash<QString, QHash<QString, QHash<QString, QVariant>>> m_sku_countryCode_fieldId_origValue;
    QHash<QString, QHash<QString, QHash<QString, QVariant>>> m_sku_countryCode_fieldId_value;
    //QHash<QString, QHash<QString, QHash<QString, QString>>> m_countryCode_skuParent_fieldId_value;
    //QHash<QString, QHash<QString, QHash<QString, QString>>> m_countryCode_sku_fieldId_value;
    static FuncFiller FUNC_FILLER_COPY;
    static FuncFiller FUNC_FILLER_CONVERT_CLOTHE_SIZE;
    static FuncFiller FUNC_FILLER_CONVERT_SHOE_SIZE;
    Gender m_gender;
    Age m_age;
};

#endif // TemplateMergerFiller_H
