#include <QListWidget>
#include <QLineEdit>
#include <QTextEdit>

#include "WidgetAdDescription.h"
#include "ui_WidgetAdDescription.h"

WidgetAdDescription::WidgetAdDescription(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::WidgetAdDescription)
{
    ui->setupUi(this);
}

WidgetAdDescription::~WidgetAdDescription()
{
    delete ui;
}

void WidgetAdDescription::displayObject(const QJsonObject &jsonObject)
{
    clear();
    auto keys = jsonObject.keys();
    std::sort(keys.begin(), keys.end());
    //for (auto it = jsonObject.begin();
         //it != jsonObject.end(); ++it)
    for (const auto &key : keys)
    {
        const auto &value = jsonObject[key];
        if (value.isArray())
        {
            addJsonArray(key, value.toArray());

        }
        else if (value.isString())
        {
            addText(key, value.toString());
        }
        else if (value.isDouble())
        {
            addText(key, QString::number(value.toInt()));
        }
        else
        {
            Q_ASSERT(false);
        }
    }
}

void WidgetAdDescription::clear()
{
    if (ui->formLayout == nullptr)
    {
        return;
    }

    while (ui->formLayout->count() > 0)
    {
        QLayoutItem *layoutItem = ui->formLayout->takeAt(0);
        if (layoutItem != nullptr)
        {
            if (layoutItem->widget() != nullptr)
            {
                layoutItem->widget()->deleteLater();
            }
            if (layoutItem->layout() != nullptr)
            {
                delete layoutItem->layout();
            }
            delete layoutItem;
        }
    }
}

void WidgetAdDescription::addJsonArray(const QString &label, const QJsonArray &array)
{
    QStringList jsonArrayStringList;
    for (const QJsonValue &arrayValue : array)
    {
        Q_ASSERT(arrayValue.isString());
        jsonArrayStringList << arrayValue.toString();
    }

    QListWidget *listWidget = new QListWidget(this);
    listWidget->setSelectionMode(QAbstractItemView::NoSelection);
    listWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    listWidget->setFocusPolicy(Qt::NoFocus);
    listWidget->setWordWrap(true);
    listWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    listWidget->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);

    for (const QString &textLine : jsonArrayStringList)
    {
        QListWidgetItem *listItem = new QListWidgetItem(textLine, listWidget);
        listItem->setToolTip(textLine);
        listItem->setFlags(listItem->flags() & ~Qt::ItemIsSelectable & ~Qt::ItemIsEditable);
    }

    ui->formLayout->addRow(label, listWidget);
}

void WidgetAdDescription::addText(const QString &label, const QString &text)
{
    if (text.size() < 240)
    {
        addTextSmall(label, text);
    }
    else
    {
        addTextBig(label, text);
    }
}

void WidgetAdDescription::addTextSmall(const QString &label, const QString &text)
{
    QLineEdit *lineEditReadOnly = new QLineEdit(this);
    lineEditReadOnly->setReadOnly(true);
    lineEditReadOnly->setText(text);
    lineEditReadOnly->setCursorPosition(0);
    ui->formLayout->addRow(label, lineEditReadOnly);
}

void WidgetAdDescription::addTextBig(const QString &label, const QString &text)
{
    QTextEdit *textEditReadOnly = new QTextEdit(this);
    textEditReadOnly->setReadOnly(true);
    textEditReadOnly->setLineWrapMode(QTextEdit::WidgetWidth);
    textEditReadOnly->setPlainText(text);
    textEditReadOnly->setMinimumHeight(160);
    textEditReadOnly->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    ui->formLayout->addRow(label, textEditReadOnly);
}
