#ifndef COLMAPPING_H
#define COLMAPPING_H

#include <QAbstractTableModel>

class ColMapping : public QAbstractTableModel
{
    Q_OBJECT

public:
    static ColMapping *instance();
    const QHash<QString, QString> &colNameFromTo() const;
    void addMapping(const QString &from, const QString &to);
    void removeMapping(const QModelIndex &index);

    QVariant headerData(int section,
                        Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;

    Qt::ItemFlags flags(const QModelIndex &index) const override;

private:
    explicit ColMapping(QObject *parent = nullptr);
    static const QStringList COL_NAMES;
    QStringList m_colFrom;
    QHash<QString, QString> m_fromTo;
    void _saveInSettings();
    void _loadFromSettings();
    QString m_settingsKeysFrom;
    QString m_settingsKeysFromTo;
};

#endif // COLMAPPING_H
