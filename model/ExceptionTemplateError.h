#ifndef EXCEPTIONTEMPLATEERROR_H
#define EXCEPTIONTEMPLATEERROR_H

#include <QException>

class ExceptionTemplateError : public QException
{
public:
    void raise() const override;
    ExceptionTemplateError *clone() const override;
    void setInfos(const QString &title, const QString &error);

    const QString &title() const;
    const QString &error() const;

private:
    QString m_title;
    QString m_error;
};

#endif // EXCEPTIONTEMPLATEERROR_H
