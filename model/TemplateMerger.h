#ifndef TEMPLATEMERGER_H
#define TEMPLATEMERGER_H

#include <QString>
#include <QStringList>

#include <xlsxdocument.h>

class TemplateMerger
{
public:
    static const QStringList SHEETS_TEMPLATE;
    static const QStringList SHEETS_VALID_VALUES;
    static const QHash<QString, QString> SHEETS_MANDATORY;
    TemplateMerger(
        const QString &filePathTo);
    void readSkus();
    void readInfoSources(const QStringList &sourceFilePaths);
    void setFilePathsToFill(const QStringList &toFillFilePath);
    // ChatGpt, for each field mandatory, will say if needed for child only or both
    // ChatGpt, will be asked value, for each lang
    // We will use QSettings for crash retake
    void askChatGptInformations();
    void createToFillXlsx();

private:
    void selectTemplateSheet(QXlsx::Document &doc);
    QString m_filePathTo;
    QStringList m_toFillFilePath;
    QHash<QString, QHash<QString, QHash<QString, QString>>> m_countryCode_skuParent_field_value;
    QHash<QString, QHash<QString, QHash<QString, QString>>> m_countryCode_sku_field_value;
    QHash<QString, QHash<QString, QHash<QString, QString>>> m_fieldId_countryCode_fieldName;
    QHash<QString, bool> m_fieldId_childOnly;
};

#endif // TEMPLATEMERGER_H
