/***************************************************************************
    Copyright (C) 2003-2009 Robby Stephenson <robby@periapsis.org>
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


#include "bibtexexporter.h"
#include "bibtexhandler.h"
#include "../document.h"
#include "../collections/bibtexcollection.h"
#include "../core/filehandler.h"
#include "../utils/stringset.h"
#include "../fieldformat.h"
#include "../tellico_debug.h"

#include <config.h>

#include <klocale.h>
#include <KConfigGroup>
#include <kcombobox.h>

#include <QRegExp>
#include <QCheckBox>
#include <QGroupBox>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>

using Tellico::Export::BibtexExporter;

BibtexExporter::BibtexExporter() : Tellico::Export::Exporter(),
   m_expandMacros(false),
   m_packageURL(true),
   m_skipEmptyKeys(false),
   m_widget(0) {
}

QString BibtexExporter::formatString() const {
  return i18n("Bibtex");
}

QString BibtexExporter::fileFilter() const {
  return i18n("*.bib|Bibtex Files (*.bib)") + QLatin1Char('\n') + i18n("*|All Files");
}

bool BibtexExporter::exec() {
  Data::CollPtr c = collection();
  if(!c || c->type() != Data::Collection::Bibtex) {
    return false;
  }
  const Data::BibtexCollection* coll = static_cast<const Data::BibtexCollection*>(c.data());

// there are some special attributes
// the entry-type specifies the entry type - book, inproceedings, whatever
  QString typeField;
// the key specifies the cite-key
  QString keyField;
// the crossref bibtex field can reference another entry
  QString crossRefField;
  bool hasCrossRefs = false;

  const QString bibtex = QLatin1String("bibtex");
// keep a list of all the 'ordinary' fields to iterate through later
  Data::FieldList fields;
  Data::FieldList vec = coll->fields();
  foreach(Data::FieldPtr it, vec) {
    QString bibtexField = it->property(bibtex);
    if(bibtexField == QLatin1String("entry-type")) {
      typeField = it->name();
    } else if(bibtexField == QLatin1String("key")) {
      keyField = it->name();
    } else if(bibtexField == QLatin1String("crossref")) {
      fields.append(it); // still output crossref field
      crossRefField = it->name();
      hasCrossRefs = true;
    } else if(!bibtexField.isEmpty()) {
      fields.append(it);
    }
  }

  if(typeField.isEmpty() || keyField.isEmpty()) {
    kWarning() << "BibtexExporter::exec() - the collection must have fields defining "
                   "the entry-type and the key of the entry" << endl;
    return false;
  }
  if(fields.isEmpty()) {
    kWarning() << "BibtexExporter::exec() - no bibtex field mapping exists in the collection.";
    return false;
  }

  QString text = QLatin1String("@comment{Generated by Tellico ")
               + QLatin1String(VERSION)
               + QLatin1String("}\n\n");

  if(!coll->preamble().isEmpty()) {
    text += QLatin1String("@preamble{") + coll->preamble() + QLatin1String("}\n\n");
  }

  const QStringList macros = coll->macroList().keys();
  if(!m_expandMacros) {
    QMap<QString, QString>::ConstIterator macroIt;
    for(macroIt = coll->macroList().constBegin(); macroIt != coll->macroList().constEnd(); ++macroIt) {
      if(!macroIt.value().isEmpty()) {
        text += QLatin1String("@string{")
                + macroIt.key()
                + QLatin1String("=")
                + BibtexHandler::exportText(macroIt.value(), macros)
                + QLatin1String("}\n\n");
      }
    }
  }

  // if anything is crossref'd, we have to do an initial scan through the
  // whole collection first
  StringSet crossRefKeys;
  if(hasCrossRefs) {
    foreach(Data::EntryPtr entryIt, entries()) {
      crossRefKeys.add(entryIt->field(crossRefField));
    }
  }

  StringSet usedKeys;
  Data::EntryList crossRefs;
  QString type, key, newKey, value;
  foreach(Data::EntryPtr entryIt, entries()) {
    type = entryIt->field(typeField);
    if(type.isEmpty()) {
      kWarning() << "BibtexExporter::text() - the entry for '" << entryIt->title()
                  << "' has no entry-type, skipping it!" << endl;
      continue;
    }

    key = entryIt->field(keyField);
    if(key.isEmpty()) {
      if(m_skipEmptyKeys) {
        continue;
      }
      key = BibtexHandler::bibtexKey(entryIt);
    } else {
      // check crossrefs, only counts for non-empty keys
      // if this entry is crossref'd, add it to the list, and skip it
      if(hasCrossRefs && crossRefKeys.has(key)) {
        crossRefs.append(entryIt);
        continue;
      }
    }

    newKey = key;
    char c = 'a';
    while(usedKeys.has(newKey)) {
      // duplicate found!
      newKey = key + QLatin1Char(c);
      ++c;
    }
    key = newKey;
    usedKeys.add(key);

    writeEntryText(text, fields, *entryIt, type, key);
  }

  // now write out crossrefs
  foreach(Data::EntryPtr entryIt, crossRefs) {
    // no need to check type

    key = entryIt->field(keyField);
    newKey = key;
    char c = 'a';
    while(usedKeys.has(newKey)) {
      // duplicate found!
      newKey = key + QLatin1Char(c);
      ++c;
    }
    key = newKey;
    usedKeys.add(key);

    writeEntryText(text, fields, *entryIt, entryIt->field(typeField), key);
  }

  return FileHandler::writeTextURL(url(), text, options() & ExportUTF8, options() & Export::ExportForce);
}

QWidget* BibtexExporter::widget(QWidget* parent_) {
  if(m_widget && m_widget->parent() == parent_) {
    return m_widget;
  }

  m_widget = new QWidget(parent_);
  QVBoxLayout* l = new QVBoxLayout(m_widget);

  QGroupBox* gbox = new QGroupBox(i18n("Bibtex Options"), m_widget);
  QVBoxLayout* vlay = new QVBoxLayout(gbox);

  m_checkExpandMacros = new QCheckBox(i18n("Expand string macros"), gbox);
  m_checkExpandMacros->setChecked(m_expandMacros);
  m_checkExpandMacros->setWhatsThis(i18n("If checked, the string macros will be expanded and no "
                                         "@string{} entries will be written."));

  m_checkPackageURL = new QCheckBox(i18n("Use URL package"), gbox);
  m_checkPackageURL->setChecked(m_packageURL);
  m_checkPackageURL->setWhatsThis(i18n("If checked, any URL fields will be wrapped in a "
                                       "\\url declaration."));

  m_checkSkipEmpty = new QCheckBox(i18n("Skip entries with empty citation keys"), gbox);
  m_checkSkipEmpty->setChecked(m_skipEmptyKeys);
  m_checkSkipEmpty->setWhatsThis(i18n("If checked, any entries without a bibtex citation key "
                                      "will be skipped."));

  QHBoxLayout* hlay = new QHBoxLayout(gbox);
  vlay->addLayout(hlay);

  QLabel* l1 = new QLabel(i18n("Bibtex quotation style:") + QLatin1Char(' '), gbox); // add a space for asthetics
  m_cbBibtexStyle = new KComboBox(gbox);
  m_cbBibtexStyle->addItem(i18n("Braces"));
  m_cbBibtexStyle->addItem(i18n("Quotes"));
  QString whats = i18n("<qt>The quotation style used when exporting bibtex. All field values will "
                       " be escaped with either braces or quotation marks.</qt>");
  l1->setWhatsThis(whats);
  m_cbBibtexStyle->setWhatsThis(whats);
  if(BibtexHandler::s_quoteStyle == BibtexHandler::BRACES) {
    m_cbBibtexStyle->setCurrentItem(i18n("Braces"));
  } else {
    m_cbBibtexStyle->setCurrentItem(i18n("Quotes"));
  }

  hlay->addWidget(l1);
  hlay->addWidget(m_cbBibtexStyle);

  vlay->addWidget(m_checkExpandMacros);
  vlay->addWidget(m_checkPackageURL);
  vlay->addWidget(m_checkSkipEmpty);

  l->addWidget(gbox);
  l->addStretch(1);
  return m_widget;
}

void BibtexExporter::readOptions(KSharedConfigPtr config_) {
  KConfigGroup group(config_, QString::fromLatin1("ExportOptions - %1").arg(formatString()));
  m_expandMacros = group.readEntry("Expand Macros", m_expandMacros);
  m_packageURL = group.readEntry("URL Package", m_packageURL);
  m_skipEmptyKeys = group.readEntry("Skip Empty Keys", m_skipEmptyKeys);

  if(group.readEntry("Use Braces", true)) {
    BibtexHandler::s_quoteStyle = BibtexHandler::BRACES;
  } else {
    BibtexHandler::s_quoteStyle = BibtexHandler::QUOTES;
  }
}

void BibtexExporter::saveOptions(KSharedConfigPtr config_) {
  KConfigGroup group(config_, QString::fromLatin1("ExportOptions - %1").arg(formatString()));
  m_expandMacros = m_checkExpandMacros->isChecked();
  group.writeEntry("Expand Macros", m_expandMacros);
  m_packageURL = m_checkPackageURL->isChecked();
  group.writeEntry("URL Package", m_packageURL);
  m_skipEmptyKeys = m_checkSkipEmpty->isChecked();
  group.writeEntry("Skip Empty Keys", m_skipEmptyKeys);

  bool useBraces = m_cbBibtexStyle->currentText() == i18n("Braces");
  group.writeEntry("Use Braces", useBraces);
  if(useBraces) {
    BibtexHandler::s_quoteStyle = BibtexHandler::BRACES;
  } else {
    BibtexHandler::s_quoteStyle = BibtexHandler::QUOTES;
  }
}

void BibtexExporter::writeEntryText(QString& text_, const Tellico::Data::FieldList& fields_, const Tellico::Data::Entry& entry_,
                                    const QString& type_, const QString& key_) {
  const QStringList macros = static_cast<const Data::BibtexCollection*>(Data::Document::self()->collection().data())->macroList().keys();
  const QString bibtex = QLatin1String("bibtex");
  const QString bibtexSep = QLatin1String("bibtex-separator");

  text_ += QLatin1Char('@') + type_ + QLatin1Char('{') + key_;

  QString value;
  bool format = options() & Export::ExportFormatted;
  foreach(Data::FieldPtr fIt, fields_) {
    value = entry_.field(fIt->name(), format);
    if(value.isEmpty()) {
      continue;
    }

    // If the entry is formatted as a name and allows multiple values
    // insert "and" in between them (e.g. author and editor)
    if(fIt->formatFlag() == Data::Field::FormatName
       && fIt->flags() & Data::Field::AllowMultiple) {
      value.replace(FieldFormat::delimiter(), QLatin1String(" and "));
    } else if(fIt->flags() & Data::Field::AllowMultiple) {
      QString bibsep = fIt->property(bibtexSep);
      if(!bibsep.isEmpty()) {
        value.replace(FieldFormat::delimiter(), bibsep);
      }
    } else if(fIt->type() == Data::Field::Para) {
      // strip HTML from bibtex export
      QRegExp stripHTML(QLatin1String("<.*>"));
      stripHTML.setMinimal(true);
      value.remove(stripHTML);
    } else if(fIt->property(bibtex) == QLatin1String("pages")) {
      QRegExp rx(QLatin1String("(\\d)-(\\d)"));
      for(int pos = rx.indexIn(value); pos > -1; pos = rx.indexIn(value, pos+2)) {
        value.replace(pos, 3, rx.cap(1) + QLatin1String("--") + rx.cap(2));
      }
    }

    if(m_packageURL && fIt->type() == Data::Field::URL) {
      bool b = BibtexHandler::s_quoteStyle == BibtexHandler::BRACES;
      value = (b ? QLatin1Char('{') : QLatin1Char('"'))
            + QLatin1String("\\url{") + BibtexHandler::exportText(value, macros) + QLatin1Char('}')
            + (b ? QLatin1Char('}') : QLatin1Char('"'));
    } else if(fIt->type() != Data::Field::Number) {
      // numbers aren't escaped, nor will they have macros
      // if m_expandMacros is true, then macros is empty, so this is ok even then
      value = BibtexHandler::exportText(value, macros);
    }
    text_ += QLatin1String(",\n  ")
           + fIt->property(bibtex)
           + QLatin1String(" = ")
           + value;
  }
  text_ += QLatin1String("\n}\n\n");
}

#include "bibtexexporter.moc"
