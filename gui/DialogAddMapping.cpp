#include <QMessageBox>

#include "DialogAddMapping.h"
#include "ui_DialogAddMapping.h"

DialogAddMapping::DialogAddMapping(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::DialogAddMapping)
{
    ui->setupUi(this);
}

DialogAddMapping::~DialogAddMapping()
{
    delete ui;
}

QString DialogAddMapping::getFrom() const
{
    return ui->lineEditFrom->text();
}

QString DialogAddMapping::getTo() const
{
    return ui->lineEditTo->text();
}

void DialogAddMapping::accept()
{
    if (ui->lineEditFrom->text().trimmed().isEmpty())
    {
        QMessageBox::information(this,
                                 tr("From col name"),
                                 tr("You need to input a from column name"));
    }
    else if (ui->lineEditTo->text().trimmed().isEmpty())
    {
        QMessageBox::information(this,
                                 tr("To col name"),
                                 tr("You need to input a to column name"));
    }
    else
    {
        QDialog::accept();
    }
}
