#include <QClipboard>
#include <QApplication>
#include <QMessageBox>

#include "model/GptFiller.h"

#include "DialogFillAiPrompts.h"
#include "ui_DialogFillAiPrompts.h"

DialogFillAiPrompts::DialogFillAiPrompts(
        const QList<PromptInfo> &promptInfos,
        GptFiller *gptFiller,
        QWidget *parent) :
    QDialog(parent),
    ui(new Ui::DialogFillAiPrompts)
{
    ui->setupUi(this);
    m_gptFiller = gptFiller;
    m_promptInfos = promptInfos;
    int i = 1;
    for (const auto &promptInfo : m_promptInfos)
    {
        const QString &label
                = QString::number(i) + " - "
                + promptInfo.skuParent + " | "
                + promptInfo.colorOrig;
        ui->listWidgetPrompts->addItem(label);
        ++i;
    }
    _setButtonEnabled(false);
    _connectSlots();
}

DialogFillAiPrompts::~DialogFillAiPrompts()
{
    delete ui;
}

void DialogFillAiPrompts::_connectSlots()
{
    connect(ui->buttonCopyPrompt,
            &QPushButton::clicked,
            this,
            &DialogFillAiPrompts::copyPrompt);
    connect(ui->buttonCopyImagePath,
            &QPushButton::clicked,
            this,
            &DialogFillAiPrompts::copyImagePath);
    connect(ui->buttonPasteReply,
            &QPushButton::clicked,
            this,
            &DialogFillAiPrompts::pasteReply);
    connect(ui->listWidgetPrompts->selectionModel(),
            &QItemSelectionModel::selectionChanged,
            this,
            &DialogFillAiPrompts::onPromptSelected);
}

void DialogFillAiPrompts::_setButtonEnabled(bool enabled)
{
    ui->buttonPasteReply->setEnabled(enabled);
    ui->buttonCopyPrompt->setEnabled(enabled);
    ui->buttonCopyImagePath->setEnabled(enabled);
}

void DialogFillAiPrompts::copyPrompt()
{
    auto clipboard = QApplication::clipboard();
    const auto &text = ui->textEditQuestion->toPlainText();
    if (!text.isEmpty())
    {
        clipboard->setText(text);
    }
}

void DialogFillAiPrompts::copyImagePath()
{
    auto clipboard = QApplication::clipboard();
    const auto &text = ui->lineEditImageFilePath->text();
    if (!text.isEmpty())
    {
        clipboard->setText(text);
    }
}

void DialogFillAiPrompts::pasteReply()
{
    auto clipboard = QApplication::clipboard();
    const auto &text = clipboard->text();
    if (text.trimmed().isEmpty())
    {
        QMessageBox::information(
                    this,
                    tr("No text"),
                    tr("You pasted en empty text."));
    }
    else
    {
        const auto &selIndexes = ui->listWidgetPrompts->selectionModel()->selectedIndexes();
        if (selIndexes.size() > 0)
        {
            int row = selIndexes.first().row();
            auto &promptInfo = m_promptInfos[row];
            promptInfo.reply = text;
            promptInfo.reply.replace("\\_", "_");
            promptInfo.reply.replace("\\[", "[");
            promptInfo.reply.replace("\\_", "_");
            //promptInfo.reply.replace("\\", ""); // May be this would be more reliable?
            ui->textEditReply->setPlainText(text);
            qDebug().noquote() << "\nDialogFillAiPrompts::pasteReply - Trying to paste reply:" << promptInfo.reply;
            if (m_gptFiller->recordProductAiDescriptionsPrompt(promptInfo))
            {
                ui->listWidgetPrompts->setRowHidden(row, true);
                if (m_promptRowDone.size() == m_promptRowDone.size())
                {
                    ui->listWidgetPrompts->clearSelection();
                }
                else
                {
                    for (int i=0; i<m_promptInfos.size(); ++i)
                    {
                        if (!ui->listWidgetPrompts->isRowHidden(i))
                        {
                            ui->listWidgetPrompts->setCurrentRow(i);
                            break;
                        }
                    }
                }
                m_promptRowDone.insert(row);
            }
            else
            {
                QMessageBox::information(
                            this,
                            tr("Rejected"),
                            tr("The reply was rejected."));
            }
        }
    }
}

void DialogFillAiPrompts::onPromptSelected(
        const QItemSelection &selected, const QItemSelection &deselected)
{
    if (selected.size() > 0)
    {
        int row = selected.indexes().first().row();
        ui->lineEditImageColor->setText(m_promptInfos[row].colorOrig);
        ui->lineEditImageFilePath->setText(m_promptInfos[row].imageFilePath);
        QPixmap image{m_promptInfos[row].imageFilePath};
        ui->labelImage->setPixmap(image.scaledToHeight(ui->labelImage->height()));
        ui->lineEditSkuParent->setText(m_promptInfos[row].skuParent);
        ui->textEditQuestion->setText(m_promptInfos[row].question);
        ui->textEditReply->setText(m_promptInfos[row].reply);
        _setButtonEnabled(true);
    }
    else if (deselected.size() > 0)
    {
        ui->lineEditImageColor->clear();
        ui->lineEditImageFilePath->clear();
        ui->lineEditSkuParent->clear();
        ui->textEditQuestion->clear();
        ui->textEditReply->clear();
        ui->labelImage->clear();
        _setButtonEnabled(false);
    }
}
