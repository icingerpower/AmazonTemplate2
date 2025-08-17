#ifndef DIALOGPOSSIBLEVALUES_H
#define DIALOGPOSSIBLEVALUES_H

#include <QDialog>
#include <QItemSelection>

class TemplateMergerFiller;

namespace Ui {
class DialogPossibleValues;
}

class DialogPossibleValues : public QDialog
{
    Q_OBJECT

public:
    explicit DialogPossibleValues(
            TemplateMergerFiller *templateMergerFille, QWidget *parent = nullptr);
    ~DialogPossibleValues();
    void onPromptSelected(const QItemSelection &selected
                          , const QItemSelection &deselected);
public slots:
    void copyColumn();
    void copyAll();

private:
    Ui::DialogPossibleValues *ui;
    QMap<QString, QHash<QString, QStringList>> m_fieldIds_countryLang_possibleValues;
    QStringList m_countryLangs;
    void _connectSlots();
};

#endif // DIALOGPOSSIBLEVALUES_H
