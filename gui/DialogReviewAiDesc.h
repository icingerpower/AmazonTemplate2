#ifndef DIALOGREVIEWAIDESC_H
#define DIALOGREVIEWAIDESC_H

#include <QDialog>

#include <QItemSelection>

#include <model/GptFiller.h>

namespace Ui {
class DialogReviewAiDesc;
}

class GptFiller;
class JsonReplyAiDescription;

class DialogReviewAiDesc : public QDialog
{
    Q_OBJECT

public:
    explicit DialogReviewAiDesc(
            GptFiller *gptFiller,
            QWidget *parent = nullptr);
    ~DialogReviewAiDesc();

public slots:
    void remove();
    void askToFix();
    void onPromptSelected(const QItemSelection &selected
                          , const QItemSelection &deselected);

private:
    Ui::DialogReviewAiDesc *ui;
    GptFiller *m_gptFiller;
    void _connectSlots();
    JsonReplyAiDescription *m_jsonReplyAiDescription;
    QHash<QString, QHash<QString, QJsonObject>> m_skuParent_color_jsonReply;
};

#endif // DIALOGREVIEWAIDESC_H
