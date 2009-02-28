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

#include "borrower.h"
#include "entry.h"
#include "tellico_utils.h"

using Tellico::Data::Loan;
using Tellico::Data::Borrower;

Loan::Loan(Tellico::Data::EntryPtr entry, const QDate& loanDate, const QDate& dueDate, const QString& note)
    : QSharedData(), m_uid(Tellico::uid()), m_borrower(0), m_entry(entry), m_loanDate(loanDate), m_dueDate(dueDate),
      m_note(note), m_inCalendar(false) {
}

Loan::Loan(const Loan& other) : QSharedData(other), m_uid(Tellico::uid()), m_borrower(other.m_borrower),
      m_entry(other.m_entry), m_loanDate(other.m_loanDate), m_dueDate(other.m_dueDate),
      m_note(other.m_note), m_inCalendar(false) {
}

Tellico::Data::BorrowerPtr Loan::borrower() const {
  return m_borrower;
}

Tellico::Data::EntryPtr Loan::entry() const {
  return m_entry;
}

Borrower::Borrower(const QString& name_, const QString& uid_)
    : QSharedData(), m_name(name_), m_uid(uid_) {
}

Borrower::Borrower(const Borrower& b)
    : QSharedData(b), m_name(b.m_name), m_uid(b.m_uid), m_loans(b.m_loans) {
}

Borrower& Borrower::operator=(const Borrower& other_) {
  if(this == &other_) return *this;

//  static_cast<QSharedData&>(*this) = static_cast<const QSharedData&>(other_);
  m_name = other_.m_name;
  m_uid = other_.m_uid;
  m_loans = other_.m_loans;
  return *this;
}

Tellico::Data::LoanPtr Borrower::loan(Tellico::Data::EntryPtr entry_) {
  foreach(LoanPtr loan, m_loans) {
    if(loan->entry() == entry_) {
      return loan;
    }
  }
  return LoanPtr();
}

void Borrower::addLoan(Tellico::Data::LoanPtr loan_) {
  if(loan_) {
    m_loans.append(loan_);
    loan_->setBorrower(BorrowerPtr(this));
  }
}

bool Borrower::removeLoan(Tellico::Data::LoanPtr loan_) {
  m_loans.removeAll(loan_);
  return true;
}
