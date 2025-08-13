#ifndef WIDGETADDESCRIPTION_H
#define WIDGETADDESCRIPTION_H

#include <QWidget>
#include <QJsonObject>
#include <QJsonArray>

namespace Ui {
class WidgetAdDescription;
}

class WidgetAdDescription : public QWidget
{
    Q_OBJECT

public:
    explicit WidgetAdDescription(QWidget *parent = nullptr);
    ~WidgetAdDescription();
    void displayObject(const QJsonObject &jsonObject);
    void clear();
    void addJsonArray(const QString &label, const QJsonArray &array);
    void addText(const QString &label, const QString &text);
    void addTextSmall(const QString &label, const QString &text);
    void addTextBig(const QString &label, const QString &text);

private:
    Ui::WidgetAdDescription *ui;
};

#endif // WIDGETADDESCRIPTION_H
