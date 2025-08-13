#include "model/GptFiller.h"
#include "model/JsonReplyAiDescription.h"

#include "DialogReviewAiDesc.h"
#include "ui_DialogReviewAiDesc.h"


DialogReviewAiDesc::DialogReviewAiDesc(
        GptFiller *gptFiller
        , QWidget *parent)
    : QDialog(parent),
    ui(new Ui::DialogReviewAiDesc)
{
    ui->setupUi(this);
    m_gptFiller = gptFiller;
    m_jsonReplyAiDescription = m_gptFiller->jsonReplyAiDescription();
    m_skuParent_color_jsonReply = m_jsonReplyAiDescription->get_skuParent_color_jsonReply();
    QList<QStringList> list_stringList;
    for (auto itSkuParent = m_skuParent_color_jsonReply.begin();
         itSkuParent != m_skuParent_color_jsonReply.end(); ++itSkuParent)
    {
        for (auto itColor = itSkuParent->begin();
             itColor != itSkuParent->end(); ++itColor)
        {
            list_stringList << QStringList{itSkuParent.key(), itColor.key()};
        }
    }
    std::sort(list_stringList.begin(), list_stringList.end(), [](const QStringList &stringList1, const QStringList &stringList2) -> bool {
        return stringList1.join("-") < stringList2.join("-");
    });
    if (list_stringList.size() > 0)
    {
        ui->tableWidgetPrompts->setRowCount(list_stringList.size());
        int nColumns = list_stringList[0].size();
        ui->tableWidgetPrompts->setColumnCount(nColumns);
        for (int i=0; i<list_stringList.size(); ++i)
        {
            for (int j=0; j<nColumns; ++j)
            {
                ui->tableWidgetPrompts->setItem(
                            i, j, new QTableWidgetItem{list_stringList[i][j]});
            }
        }
    }
    _connectSlots();
}

DialogReviewAiDesc::~DialogReviewAiDesc()
{
    delete ui;
}

void DialogReviewAiDesc::remove()
{
    const auto &selIndexes = ui->tableWidgetPrompts->selectionModel()->selectedIndexes();
    if (selIndexes.size() > 0)
    {
        int row = selIndexes.first().row();
        auto skuParent = ui->tableWidgetPrompts->item(row, 0)->data(Qt::DisplayRole).toString();
        auto color = ui->tableWidgetPrompts->item(row, 1)->data(Qt::DisplayRole).toString();
        m_jsonReplyAiDescription->remove(skuParent, color);
        ui->tableWidgetPrompts->removeRow(row);
        if (ui->tableWidgetPrompts->rowCount() > 0)
        {
            ui->tableWidgetPrompts->setCurrentCell(0, 0);
        }
    }
}

void DialogReviewAiDesc::askToFix()
{
    const auto &selIndexes = ui->tableWidgetPrompts->selectionModel()->selectedIndexes();
    if (selIndexes.size() > 0)
    {
        int row = selIndexes.first().row();
    }
}

void DialogReviewAiDesc::onPromptSelected(
        const QItemSelection &selected, const QItemSelection &deselected)
{
    if (selected.size() > 0)
    {
        int row = selected.indexes().first().row();
        auto skuParent = ui->tableWidgetPrompts->item(row, 0)->data(Qt::DisplayRole).toString();
        auto color = ui->tableWidgetPrompts->item(row, 1)->data(Qt::DisplayRole).toString();
        ui->widgetAiDesc->displayObject(m_skuParent_color_jsonReply[skuParent][color]);
    }
    else if (deselected.size() > 0)
    {
        ui->widgetAiDesc->clear();
    }
}

void DialogReviewAiDesc::_connectSlots()
{
    connect(ui->buttonRemove,
            &QPushButton::clicked,
            this,
            &DialogReviewAiDesc::remove);
    connect(ui->buttonAskToFix,
            &QPushButton::clicked,
            this,
            &DialogReviewAiDesc::askToFix);
    connect(ui->tableWidgetPrompts->selectionModel(),
            &QItemSelectionModel::selectionChanged,
            this,
            &DialogReviewAiDesc::onPromptSelected);

}
