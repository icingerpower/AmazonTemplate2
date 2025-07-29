#include "../common/workingdirectory/WorkingDirectoryManager.h"

#include "ColMapping.h"

const QStringList ColMapping::COL_NAMES{QObject::tr("from"), QObject::tr("to")};

ColMapping::ColMapping(QObject *parent)
    : QAbstractTableModel(parent)
{
    m_settingsKeysFrom = "ColMappingFrom";
    m_settingsKeysFromTo = "ColMappingFromTo";
    _loadFromSettings();
}

void ColMapping::_saveInSettings()
{
    auto settings = WorkingDirectoryManager::instance()->settings();
    settings->setValue(m_settingsKeysFrom, m_colFrom);
    settings->setValue(m_settingsKeysFromTo, QVariant::fromValue(m_fromTo));
}

void ColMapping::_loadFromSettings()
{
    auto settings = WorkingDirectoryManager::instance()->settings();
    m_colFrom = settings->value(m_settingsKeysFrom).toStringList();
    m_fromTo = settings->value(m_settingsKeysFrom).value<QHash<QString, QString>>();
}

ColMapping *ColMapping::instance()
{
    static ColMapping instance;
    return &instance;
}

const QHash<QString, QString> &ColMapping::colNameFromTo() const
{
    return m_fromTo;
}

void ColMapping::addMapping(const QString &from, const QString &to)
{
    beginInsertRows(QModelIndex{}, 0, 0);
    m_colFrom << from;
    m_fromTo[from] = to;
    _saveInSettings();
    endInsertRows();
}

void ColMapping::removeMapping(const QModelIndex &index)
{
    beginRemoveRows(QModelIndex{}, index.row(), index.row());
    m_fromTo.remove(m_colFrom.takeAt(index.row()));
    _saveInSettings();
    endRemoveRows();
}

QVariant ColMapping::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
    {
        return COL_NAMES[section];
    }
    return QVariant{};
}

int ColMapping::rowCount(const QModelIndex &) const
{
    return m_colFrom.size();
}

int ColMapping::columnCount(const QModelIndex &) const
{
    return COL_NAMES.size();
}

QVariant ColMapping::data(const QModelIndex &index, int role) const
{
    if (role == Qt::DisplayRole || role == Qt::EditRole)
    {
        const auto &from = m_colFrom[index.row()];
        if(index.column() == 0)
        {
            return from;
        }
        else if(index.column() == 1)
        {
            return m_fromTo[from];
        }
        Q_ASSERT(false);
    }
    return QVariant{};
}

bool ColMapping::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (data(index, role) != value)
    {
        const auto &from = m_colFrom[index.row()];
        if(index.column() == 0)
        {
            const auto &fromNew = value.toString();
            m_colFrom[index.row()] = fromNew;
            m_fromTo[fromNew] = m_fromTo[from];
            m_fromTo.remove(from);
        }
        else if(index.column() == 1)
        {
            m_fromTo[from] = value.toString();
        }
        _saveInSettings();
        emit dataChanged(index, index, {role});
        return true;
    }
    return false;
}

Qt::ItemFlags ColMapping::flags(const QModelIndex &index) const
{
    if (!index.isValid())
    {
        return Qt::NoItemFlags;
    }
    return Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable;
}

