/***************************************************************************
    Copyright (C) 2009 Robby Stephenson <robby@periapsis.org>
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or         *
 *   modify it under the terms of the GNU General Public License as        *
 *   published by the Free Software Foundation; either version 2 of        *
 *   the License or (at your option) version 3 or any later version        *
 *   accepted by the membership of KDE e.V. (or its successor approved     *
 *   by the membership of KDE e.V.), which shall act as a proxy            *
 *   defined in Section 14 of version 3 of the license.                    *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 *                                                                         *
 ***************************************************************************/

#undef QT_NO_CAST_FROM_ASCII

#include "qtest_kde.h"
#include "formattest.h"
#include "formattest.moc"
#include "../fieldformat.h"
#include "../core/tellico_config.h"

QTEST_KDEMAIN_CORE( FormatTest )

void FormatTest::initTestCase() {
  Tellico::Config::setArticlesString(QString("the,l'"));
  // have to update apostrophe list
  Tellico::FieldFormat::articlesUpdated();
  Tellico::Config::setNoCapitalizationString(QString("the,of,et,de"));
}

void FormatTest::testCapitalization() {
  QFETCH(QString, string);
  QFETCH(QString, capitalized);

  QCOMPARE(Tellico::FieldFormat::capitalize(string), capitalized);
}

void FormatTest::testCapitalization_data() {
  QTest::addColumn<QString>("string");
  QTest::addColumn<QString>("capitalized");

  QTest::newRow("test1") << "the title" << "The Title";
  // right now, 'the' is in the no capitalization string
  QTest::newRow("test2") << "title, the" << "Title, the";
  QTest::newRow("test3") << "the return of the king" << "The Return of the King";
  QTest::newRow("test4") << QString::fromUtf8("école nationale supérieure de l'aéronautique et de l'espace")
                         << QString::fromUtf8("École Nationale Supérieure de l'Aéronautique et de l'Espace");
}

void FormatTest::testTitle() {
  QFETCH(QString, string);
  QFETCH(QString, formatted);
  QFETCH(bool, capitalize);
  QFETCH(bool, format);

  Tellico::Config::setAutoCapitalization(capitalize);
  Tellico::Config::setAutoFormat(format);

  QCOMPARE(Tellico::FieldFormat::title(string), formatted);
}

void FormatTest::testTitle_data() {
  QTest::addColumn<QString>("string");
  QTest::addColumn<QString>("formatted");
  QTest::addColumn<bool>("capitalize");
  QTest::addColumn<bool>("format");

  QTest::newRow("test1") << "the title" << "the title" << false << false;
  QTest::newRow("test2") << "the title" << "The Title" << true << false;
  // right now, 'the' is in the no capitalization string
  QTest::newRow("test3") << "title, the" << "Title, the" << true << false;
  QTest::newRow("test4") << "the Title" << "the Title" << false << false;
  QTest::newRow("test5") << "the Title" << "The Title" << true << false;
  QTest::newRow("test6") << "the title" << "title, the" << false << true;
  QTest::newRow("test7") << "the title" << "Title, The" << true << true;
  // right now, 'the' is in the no capitalization string
  QTest::newRow("test8") << "title,the" << "Title, the" << true << true;
  QTest::newRow("test9") << "the return of the king" << "Return of the King, The" << true << true;
  // test the formatting for table rows
  QTest::newRow("test9") << "the return of the king::Frodo" << "Return of the King, The::Frodo" << true << true;
}

void FormatTest::testName() {
  QFETCH(QString, string);
  QFETCH(QString, formatted);
  QFETCH(bool, capitalize);
  QFETCH(bool, format);

  Tellico::Config::setAutoCapitalization(capitalize);
  Tellico::Config::setAutoFormat(format);

   QCOMPARE(Tellico::FieldFormat::name(string), formatted);
}

void FormatTest::testName_data() {
  QTest::addColumn<QString>("string");
  QTest::addColumn<QString>("formatted");
  QTest::addColumn<bool>("capitalize");
  QTest::addColumn<bool>("format");

  QTest::newRow("test1") << "name" << "Name" << true << false;
  QTest::newRow("test2") << "first name" << "name, first" << false << true;
  QTest::newRow("test3") << "first Name" << "Name, First" << true << true;
  QTest::newRow("test4") << "tom swift, jr." << "Swift, Jr., Tom" << true << true;
  QTest::newRow("test5") << "swift, jr., tom" << "Swift, Jr., Tom" << true << false;
  QTest::newRow("test6") << "tom de swift, jr." << "de Swift, Jr., Tom" << true << true;
}
