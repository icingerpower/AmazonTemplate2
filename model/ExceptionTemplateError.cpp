#include "ExceptionTemplateError.h"

void ExceptionTemplateError::raise() const
{
    throw *this;
}

ExceptionTemplateError *ExceptionTemplateError::clone() const
{
    return new ExceptionTemplateError(*this);
}

void ExceptionTemplateError::setInfos(const QString &title, const QString &error)
{
    m_title = title;
    m_error = error;
}

const QString &ExceptionTemplateError::title() const
{
    return m_title;
}

const QString &ExceptionTemplateError::error() const
{
    return m_error;
}
