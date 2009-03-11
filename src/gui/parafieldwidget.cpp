/***************************************************************************
    copyright            : (C) 2005-2008 by Robby Stephenson
    email                : robby@periapsis.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of version 2 of the GNU General Public License as  *
 *   published by the Free Software Foundation;                            *
 *                                                                         *
 ***************************************************************************/

#include "parafieldwidget.h"
#include "../field.h"

#include <ktextedit.h>

using Tellico::GUI::ParaFieldWidget;

ParaFieldWidget::ParaFieldWidget(Tellico::Data::FieldPtr field_, QWidget* parent_)
    : FieldWidget(field_, parent_) {

  m_textEdit = new KTextEdit(this);
  m_textEdit->setAcceptRichText(false);
  if(field_->property(QLatin1String("spellcheck")) != QLatin1String("false")) {
    m_textEdit->setCheckSpellingEnabled(true);
  }
  connect(m_textEdit, SIGNAL(textChanged()), SLOT(checkModified()));

  registerWidget();
}

QString ParaFieldWidget::text() const {
  QString text = m_textEdit->toPlainText();
  text.replace(QLatin1Char('\n'), QLatin1String("<br/>"));
  return text;
}

void ParaFieldWidget::setTextImpl(const QString& text_) {
  QRegExp rx(QLatin1String("<br/?>"), Qt::CaseInsensitive);
  QString s = text_;
  s.replace(rx, QLatin1String("\n"));
  m_textEdit->setText(s);
}

void ParaFieldWidget::clearImpl() {
  m_textEdit->clear();
  editMultiple(false);
}

QWidget* ParaFieldWidget::widget() {
  return m_textEdit;
}

#include "parafieldwidget.moc"
