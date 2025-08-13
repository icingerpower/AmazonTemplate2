#include <QApplication>
#include <QClipboard>
#include <QFile>

#include <xlsxdocument.h>

#include "TableInfoExtractor.h"

const QStringList TableInfoExtractor::HEADER{
                         "SKU"
                         , "Title"
                         , "Color map"
                         , "Color name"
                         , "Size 1"
                         , "Size 2"
                         , "Size num"
};
const int TableInfoExtractor::IND_SKU{0};
const int TableInfoExtractor::IND_TITLE{1};
const int TableInfoExtractor::IND_COLOR_MAP{2};
const int TableInfoExtractor::IND_COLOR_NAME{3};
const int TableInfoExtractor::IND_SIZE_1{4};
const int TableInfoExtractor::IND_SIZE_2{5};
const int TableInfoExtractor::IND_SIZE_NUM{6};

TableInfoExtractor::TableInfoExtractor(QObject *parent)
    : QAbstractTableModel(parent)
{}

void TableInfoExtractor::fillGtinTemplate(
    const QString &filePathFrom, //Empty GS1 France template
    const QString &filePathTo, //New GS1 France template filled
    const QString &brand,
    const QString &categoryCode) const
{
    QXlsx::Document document(filePathFrom);
    const auto &dim = document.dimension();
    QHash<QString, int> colName_index;
    for (int i=0; i<dim.lastColumn(); ++i)
    {
        auto cell = document.cellAt(1, i+1);
        colName_index[cell->value().toString()] = i;
    }
    int curRow = 2;
    for (const auto &stringList : m_listOfStringList)
    {
        const auto &sku = stringList[IND_SKU];
        if (!sku.startsWith("P-"))
        {
            const auto &title = stringList[IND_TITLE];
            if (!title.isEmpty())
            {
                QString colProductCategory{"Code de la catégorie du produit"};
                Q_ASSERT(colName_index.contains(colProductCategory));
                document.write(curRow + 1, colName_index[colProductCategory] + 1, categoryCode);
                QString colProductName{"Nom produit"};
                Q_ASSERT(colName_index.contains(colProductName));
                document.write(curRow + 1, colName_index[colProductName] + 1, title);
                QString colMainBrand{"Marque principale"};
                Q_ASSERT(colName_index.contains(colMainBrand));
                document.write(curRow + 1, colName_index[colMainBrand] + 1, brand);
                QString colContentQuantity{"Valeur du contenu net"};
                Q_ASSERT(colName_index.contains(colContentQuantity));
                document.write(curRow + 1, colName_index[colContentQuantity] + 1, 1);
                QString colContentQuantityType{"Unité de mesure du contenu net"};
                Q_ASSERT(colName_index.contains(colContentQuantityType));
                document.write(curRow + 1, colName_index[colContentQuantityType] + 1, "PIECE");
                QString colContentSKU{"SKU"};
                Q_ASSERT(colName_index.contains(colContentSKU));
                document.write(curRow + 1, colName_index[colContentSKU] + 1, stringList[IND_SKU]);
                ++curRow;
            }
        }
    }
    document.saveAs(filePathTo);
}

QString TableInfoExtractor::pasteSKUs()
{
    return _paste(IND_SKU);
}

QString TableInfoExtractor::_paste(int colIndex)
{
    auto *clipboard = QApplication::clipboard();
    const auto &text = clipboard->text();
    const auto &lines = text.split("\n");
    if (lines.size() == 1 && lines[0].trimmed().isEmpty())
    {
        return "No lines pasted";
    }
    QList<QStringList> listOfStringList;
    for (const auto &line : lines)
    {
        const auto &lineTrimmed = line.trimmed();
        //if (!lineTrimmed.isEmpty())
        //{
        listOfStringList << QStringList{HEADER.size()};
        listOfStringList.last()[colIndex] = lineTrimmed;
        //}
    }
    if (m_listOfStringList.size() == 0)
    {
        beginInsertRows(QModelIndex{}, 0, listOfStringList.size()-1);
        m_listOfStringList = std::move(listOfStringList);
        endInsertRows();
    }
    else if (m_listOfStringList.size() != listOfStringList.size())
    {
        return "The size doesn't match. Current is "
               + QString::number(m_listOfStringList.size())
               + ". Pasted text size is: "
               + QString::number(listOfStringList.size());
    }
    else
    {
        for (int i=0; i<m_listOfStringList.size(); ++i)
        {
            m_listOfStringList[i][colIndex] = listOfStringList[i][colIndex];
        }
        emit dataChanged(index(0, colIndex), index(rowCount()-1, colIndex));
    }
    return QString{};
}

QString TableInfoExtractor::pasteTitles()
{
    const auto &error = _paste(IND_TITLE);
    if (error.isEmpty())
    {
        auto clearColumns = [this](){
            _clearColumn(IND_TITLE);
            _clearColumn(IND_COLOR_MAP);
            _clearColumn(IND_COLOR_NAME);
            _clearColumn(IND_SIZE_1);
            _clearColumn(IND_SIZE_2);

        };
        for (auto &stringList : m_listOfStringList)
        {
            const auto &title = stringList[IND_TITLE];
            if (!title.isEmpty())
            {
                if (!title.contains("(") && title.contains(")"))
                {
                    clearColumns();
                    return "( is missing in title";
                }
                if (title.contains("(") && !title.contains(")"))
                {
                    clearColumns();
                    return ") is missing in title";
                }
                if (title.contains("("))
                {
                    const auto &elements = title.split("(");
                    auto varInfo = elements.last().split(")").first();
                    if (varInfo.contains(","))
                    {
                        const QStringList &color_size = varInfo.split(",");
                        stringList[IND_COLOR_NAME] = color_size[0].trimmed();
                        stringList[IND_COLOR_MAP] = stringList[IND_COLOR_NAME].split(" ").first();
                        const auto &sizeInfos = color_size.last();
                        QHash<int, int> ind_colInd{{0, IND_SIZE_1}, {1, IND_SIZE_1}};
                        const auto &sizeElements = sizeInfos.split(("="));
                        QStringList sizeToTryConvertNum;
                        for (int j=0; j<sizeElements.size(); ++j)
                        {
                            stringList[ind_colInd[j]] = sizeElements[j];
                            if (sizeElements[j].contains(" "))
                            {
                                sizeToTryConvertNum << sizeElements[j].split(" ");
                            }
                            else
                            {
                                sizeToTryConvertNum.append(sizeElements[j]);
                            }
                        }
                        for (const auto &sizeToConvert : sizeToTryConvertNum)
                        {
                            bool isNum = false;
                            if (sizeToConvert.toInt(&isNum)
                                || sizeToConvert.toDouble(&isNum))
                            {
                                stringList[IND_SIZE_NUM] = sizeToConvert;
                                break;
                            }
                        }
                    }
                }
            }
        }
        emit dataChanged(index(0, 0), index(rowCount() - 1, columnCount() - 1));
    }
    else
    {
        return error;
    }
    return QString{};
}

void TableInfoExtractor::generateModelNames()
{
    QStringList modelNames;
    for (const auto &stringList : m_listOfStringList)
    {
        auto modelName = stringList[IND_SKU];
        if (modelName.startsWith("P-"))
        {
            modelName.clear();
        }
        else if (modelName.startsWith("CJ"))
        {
            modelName.remove(modelName.size()-4, 4);
        }
        else if (modelName.contains("-"))
        {
            modelName = modelName.split("-")[0];
        }

        modelNames << modelName;
    }
    auto *clipboard = QApplication::clipboard();
    clipboard->setText(modelNames.join("\n"));
}

void TableInfoExtractor::_clearColumn(int colIndex)
{
    if (m_listOfStringList.size() > 0)
    {
        for (auto &stringList : m_listOfStringList)
        {
            stringList[colIndex] = QString{};
        }
        emit dataChanged(index(0, colIndex), index(rowCount()-1, colIndex));
    }
}

void TableInfoExtractor::copyColumn(const QModelIndex &index)
{
    QStringList values;
    for (const auto &stringList : m_listOfStringList)
    {
        values << stringList[index.column()];
    }
    auto *clipboard = QApplication::clipboard();
    clipboard->setText(values.join("\n"));
}

QVariant TableInfoExtractor::headerData(
    int section, Qt::Orientation orientation, int role) const
{
    if (role == Qt::DisplayRole)
    {
        if (orientation == Qt::Horizontal)
        {
            return HEADER[section];
        }
        else if (orientation == Qt::Vertical)
        {
            return QString::number(section + 1);
        }
    }
    return QVariant{};
}

int TableInfoExtractor::rowCount(const QModelIndex &) const
{
    return m_listOfStringList.size();
}

int TableInfoExtractor::columnCount(const QModelIndex &) const
{
    return HEADER.count();
}

QVariant TableInfoExtractor::data(const QModelIndex &index, int role) const
{
    if (index.isValid())
    {
        if (role == Qt::DisplayRole || role == Qt::EditRole)
        {
            return m_listOfStringList[index.row()][index.column()];
        }
    }
    return QVariant();
}

bool TableInfoExtractor::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (data(index, role) != value)
    {
        m_listOfStringList[index.row()][index.column()] = value.toString();
        emit dataChanged(index, index, {role});
        return true;
    }
    return false;
}

Qt::ItemFlags TableInfoExtractor::flags(const QModelIndex &index) const
{
    if (!index.isValid())
    {
        return Qt::NoItemFlags;
    }
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable;
}

QStringList TableInfoExtractor::readGtin(const QString &gtinFilePath) const
{
    QXlsx::Document document(gtinFilePath);
    const auto &dim = document.dimension();
    auto sheet = document.currentSheet()->sheetName();
    QHash<QString, int> colName_index;
    int nCols = qMax(14, dim.lastColumn());
    for (int i=0; i<nCols; ++i)
    {
        auto cell = document.cellAt(1, i+1);
        colName_index[cell->value().toString()] = i;
    }
    QString colContentSKU{"SKU"};
    Q_ASSERT(colName_index.contains(colContentSKU));
    int indColSku = colName_index[colContentSKU];
    QString colContentGTIN{"GTIN"};
    Q_ASSERT(colName_index.contains(colContentGTIN));
    int indColGtin = colName_index[colContentGTIN];
    QHash<QString, QString> sku_gtin;
    for (int row = 2; row<dim.lastRow(); ++row)
    {
        auto cellSku = document.cellAt(row + 1, indColSku + 1);
        auto cellGtin = document.cellAt(row + 1, indColGtin + 1);
        if (cellSku && cellGtin)
        {
            QString sku = cellSku->value().toString();
            QString gtin = cellGtin->value().toString();
            if (!gtin.startsWith("0"))
            {
                gtin = "0" + gtin;
            }
            if (!sku.isEmpty() && !gtin.isEmpty())
            {
                sku_gtin[sku] = gtin;
            }
        }
    }
    QStringList GTINs;
    for (const auto &stringList : m_listOfStringList)
    {
        const auto &sku = stringList[IND_SKU];
        if (!sku.startsWith("P-"))
        {
            GTINs << sku_gtin[sku];
        }
        else
        {
            GTINs << QString{};
        }
    }
    return GTINs;
}


void TableInfoExtractor::clear()
{
    if (m_listOfStringList.size() > 0)
    {
        beginRemoveRows(QModelIndex{}, 0, m_listOfStringList.size()-1);
        m_listOfStringList.clear();
        endRemoveRows();
    }
}


