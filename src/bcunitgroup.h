/***************************************************************************
                                bcunitgroup.h
                             -------------------
    begin                : Sun Nov 3 2002
    copyright            : (C) 2002 by Robby Stephenson
    email                : robby@periapsis.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef BCUNITGROUP_H
#define BCUNITGROUP_H

#include "bcunit.h"

#include <qptrlist.h>
#include <qstring.h>

/**
 * The BCUnitGroup is simply a QPtrList of BCUnits which knows the name of its group,
 * and the name of the attribute to which that group belongs.
 *
 * An example for a book collection would be a group of books, all written by
 * David Weber. The @ref groupName() would be "Weber, David" and the
 * @ref attributeName() would be "author".
 *
 * @author Robby Stephenson
 * @version $Id: bcunitgroup.h,v 1.2 2002/11/10 00:38:29 robby Exp $
 */

class BCUnitGroup:public QPtrList<BCUnit> {
public:
  BCUnitGroup( const QString& group, const QString& att )
    : m_group(group), m_attribute(att) {}

  const QString & groupName() const { return m_group; }
  const QString & attributeName() const { return m_attribute; }

private:
  QString m_group;
  QString m_attribute;
};

#endif
