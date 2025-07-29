#include "FileModelToFill.h"

FileModelToFill::FileModelToFill(const QString &dirPath, QObject *parent)
    : QFileSystemModel(parent)
{
    setRootPath(dirPath);
    setNameFilters(QStringList{"*TOFILL*.xlsm", "*TOFILL.xlsm", "*TOFILL*.xlsx", "*TOFILL.xlsx"});
    setNameFilterDisables(false);
}

QList<QFileInfo> FileModelToFill::getFilePaths() const
{
    return rootDirectory().entryInfoList(nameFilters(), QDir::Files);
}
