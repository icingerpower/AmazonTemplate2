#include "FileModelSources.h"

FileModelSources::FileModelSources(const QString &dirPath, QObject *parent)
    : QFileSystemModel(parent)
{
    setRootPath(dirPath);
    setNameFilters(QStringList{"*SOURCES*.xlsm", "*SOURCES.xlsm", "*SOURCES*.xlsx", "*SOURCES.xlsx"});
    setNameFilterDisables(false);
}

QList<QFileInfo> FileModelSources::getFilePaths() const
{
    return rootDirectory().entryInfoList(nameFilters(), QDir::Files);
}
