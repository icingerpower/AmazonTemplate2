#ifndef DIALOGADDMAPPING_H
#define DIALOGADDMAPPING_H

#include <QDialog>

namespace Ui {
class DialogAddMapping;
}

class DialogAddMapping : public QDialog
{
    Q_OBJECT

public:
    explicit DialogAddMapping(QWidget *parent = nullptr);
    ~DialogAddMapping();
    QString getFrom() const;
    QString getTo() const;

public slots:
    void accept() override;

private:
    Ui::DialogAddMapping *ui;
};

#endif // DIALOGADDMAPPING_H
