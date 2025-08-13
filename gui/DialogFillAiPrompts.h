#ifndef DIALOGFILLAIPROMPTS_H
#define DIALOGFILLAIPROMPTS_H

#include <QDialog>

#include <QItemSelection>

#include "model/PromptInfo.h"

namespace Ui {
class DialogFillAiPrompts;
}

class GptFiller;

class DialogFillAiPrompts : public QDialog
{
    Q_OBJECT

public:
    explicit DialogFillAiPrompts(
            const QList<PromptInfo> &promptInfos,
            GptFiller *gptFiller,
            QWidget *parent = nullptr);
    ~DialogFillAiPrompts();

public slots:
    void copyPrompt();
    void copyImagePath();
    void pasteReply();
    void onPromptSelected(const QItemSelection &selected
                          , const QItemSelection &deselected);

private:
    Ui::DialogFillAiPrompts *ui;
    GptFiller *m_gptFiller;
    void _connectSlots();
    QList<PromptInfo> m_promptInfos;
    QSet<int> m_promptRowDone;
    void _setButtonEnabled(bool enabled);
};

#endif // DIALOGFILLAIPROMPTS_H
