/***************************************************************************
    copyright            : (C) 2003-2006 by Robby Stephenson
    email                : robby@periapsis.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of version 2 of the GNU General Public License as  *
 *   published by the Free Software Foundation;                            *
 *                                                                         *
 ***************************************************************************/

#include "videocollection.h"

#include <klocale.h>

namespace {
  static const char* video_general = I18N_NOOP("General");
  static const char* video_people = I18N_NOOP("Other People");
  static const char* video_features = I18N_NOOP("Features");
  static const char* video_personal = I18N_NOOP("Personal");
}

using Tellico::Data::VideoCollection;

VideoCollection::VideoCollection(bool addFields_, const QString& title_ /*=null*/)
   : Collection(title_.isEmpty() ? i18n("My Videos") : title_) {
  if(addFields_) {
    addFields(defaultFields());
  }
  setDefaultGroupField(QString::fromLatin1("genre"));
}

Tellico::Data::FieldVec VideoCollection::defaultFields() {
  FieldVec list;
  FieldPtr field;

  field = new Field(QString::fromLatin1("title"), i18n("Title"));
  field->setCategory(i18n("General"));
  field->setFlags(Field::NoDelete);
  field->setFormatFlag(Field::FormatTitle);
  list.append(field);

  QStringList media;
  media << i18n("DVD") << i18n("VHS") << i18n("VCD") << i18n("DivX") << i18n("Blu-ray") << i18n("HD DVD");
  field = new Field(QString::fromLatin1("medium"), i18n("Medium"), media);
  field->setCategory(i18n(video_general));
  field->setFlags(Field::AllowGrouped);
  list.append(field);

  field = new Field(QString::fromLatin1("year"), i18n("Production Year"), Field::Number);
  field->setCategory(i18n(video_general));
  field->setFlags(Field::AllowGrouped);
  list.append(field);

  QStringList cert = QStringList::split(QRegExp(QString::fromLatin1("\\s*,\\s*")),
                                        i18n("Movie ratings - "
                                             "G (USA),PG (USA),PG-13 (USA),R (USA), U (USA)",
                                             "G (USA),PG (USA),PG-13 (USA),R (USA), U (USA)"),
                                        false);
  field = new Field(QString::fromLatin1("certification"), i18n("Certification"), cert);
  field->setCategory(i18n(video_general));
  field->setFlags(Field::AllowGrouped);
  list.append(field);

  field = new Field(QString::fromLatin1("genre"), i18n("Genre"));
  field->setCategory(i18n(video_general));
  field->setFlags(Field::AllowCompletion | Field::AllowMultiple | Field::AllowGrouped);
  field->setFormatFlag(Field::FormatPlain);
  list.append(field);

  QStringList region;
  region << i18n("Region 1")
         << i18n("Region 2")
         << i18n("Region 3")
         << i18n("Region 4")
         << i18n("Region 5")
         << i18n("Region 6")
         << i18n("Region 7")
         << i18n("Region 8");
  field = new Field(QString::fromLatin1("region"), i18n("Region"), region);
  field->setCategory(i18n(video_general));
  field->setFlags(Field::AllowGrouped);
  list.append(field);

  field = new Field(QString::fromLatin1("nationality"), i18n("Nationality"));
  field->setCategory(i18n(video_general));
  field->setFlags(Field::AllowCompletion | Field::AllowMultiple | Field::AllowGrouped);
  field->setFormatFlag(Field::FormatPlain);
  list.append(field);

  QStringList format;
  format << i18n("NTSC") << i18n("PAL") << i18n("SECAM");
  field = new Field(QString::fromLatin1("format"), i18n("Format"), format);
  field->setCategory(i18n(video_general));
  field->setFlags(Field::AllowGrouped);
  list.append(field);

  field = new Field(QString::fromLatin1("cast"), i18n("Cast"), Field::Table);
  field->setProperty(QString::fromLatin1("columns"), QChar('2'));
  field->setProperty(QString::fromLatin1("column1"), i18n("Actor/Actress"));
  field->setProperty(QString::fromLatin1("column2"), i18n("Role"));
  field->setFormatFlag(Field::FormatName);
  field->setFlags(Field::AllowGrouped);
  field->setDescription(i18n("A table for the cast members, along with the roles they play"));
  list.append(field);

  field = new Field(QString::fromLatin1("director"), i18n("Director"));
  field->setCategory(i18n(video_people));
  field->setFlags(Field::AllowCompletion | Field::AllowMultiple | Field::AllowGrouped);
  field->setFormatFlag(Field::FormatName);
  list.append(field);

  field = new Field(QString::fromLatin1("producer"), i18n("Producer"));
  field->setCategory(i18n(video_people));
  field->setFlags(Field::AllowCompletion | Field::AllowMultiple | Field::AllowGrouped);
  field->setFormatFlag(Field::FormatName);
  list.append(field);

  field = new Field(QString::fromLatin1("writer"), i18n("Writer"));
  field->setCategory(i18n(video_people));
  field->setFlags(Field::AllowCompletion | Field::AllowMultiple | Field::AllowGrouped);
  field->setFormatFlag(Field::FormatName);
  list.append(field);

  field = new Field(QString::fromLatin1("composer"), i18n("Composer"));
  field->setCategory(i18n(video_people));
  field->setFlags(Field::AllowCompletion | Field::AllowMultiple | Field::AllowGrouped);
  field->setFormatFlag(Field::FormatName);
  list.append(field);

  field = new Field(QString::fromLatin1("studio"), i18n("Studio"));
  field->setCategory(i18n(video_people));
  field->setFlags(Field::AllowCompletion | Field::AllowMultiple | Field::AllowGrouped);
  field->setFormatFlag(Field::FormatPlain);
  list.append(field);

  field = new Field(QString::fromLatin1("language"), i18n("Language Tracks"));
  field->setCategory(i18n(video_features));
  field->setFlags(Field::AllowCompletion | Field::AllowMultiple | Field::AllowGrouped);
  field->setFormatFlag(Field::FormatPlain);
  list.append(field);

  field = new Field(QString::fromLatin1("subtitle"), i18n("Subtitle Languages"));
  field->setCategory(i18n(video_features));
  field->setFlags(Field::AllowCompletion | Field::AllowMultiple | Field::AllowGrouped);
  field->setFormatFlag(Field::FormatPlain);
  list.append(field);

  field = new Field(QString::fromLatin1("audio-track"), i18n("Audio Tracks"));
  field->setCategory(i18n(video_features));
  field->setFlags(Field::AllowCompletion | Field::AllowMultiple | Field::AllowGrouped);
  field->setFormatFlag(Field::FormatPlain);
  list.append(field);

  field = new Field(QString::fromLatin1("running-time"), i18n("Running Time"), Field::Number);
  field->setCategory(i18n(video_features));
  field->setDescription(i18n("The running time of the video (in minutes)"));
  list.append(field);

  field = new Field(QString::fromLatin1("aspect-ratio"), i18n("Aspect Ratio"));
  field->setCategory(i18n(video_features));
  field->setFlags(Field::AllowCompletion | Field::AllowMultiple | Field::AllowGrouped);
  list.append(field);

  field = new Field(QString::fromLatin1("widescreen"), i18n("Widescreen"), Field::Bool);
  field->setCategory(i18n(video_features));
  list.append(field);

  QStringList color;
  color << i18n("Color") << i18n("Black & White");
  field = new Field(QString::fromLatin1("color"), i18n("Color Mode"), color);
  field->setCategory(i18n(video_features));
  field->setFlags(Field::AllowGrouped);
  list.append(field);

  field = new Field(QString::fromLatin1("directors-cut"), i18n("Director's Cut"), Field::Bool);
  field->setCategory(i18n(video_features));
  list.append(field);

  field = new Field(QString::fromLatin1("plot"), i18n("Plot Summary"), Field::Para);
  list.append(field);

  field = new Field(QString::fromLatin1("rating"), i18n("Personal Rating"), Field::Rating);
  field->setCategory(i18n(video_personal));
  field->setFlags(Field::AllowGrouped);
  list.append(field);

  field = new Field(QString::fromLatin1("pur_date"), i18n("Purchase Date"));
  field->setCategory(i18n(video_personal));
  field->setFormatFlag(Field::FormatDate);
  list.append(field);

  field = new Field(QString::fromLatin1("gift"), i18n("Gift"), Field::Bool);
  field->setCategory(i18n(video_personal));
  list.append(field);

  field = new Field(QString::fromLatin1("pur_price"), i18n("Purchase Price"));
  field->setCategory(i18n(video_personal));
  list.append(field);

  field = new Field(QString::fromLatin1("loaned"), i18n("Loaned"), Field::Bool);
  field->setCategory(i18n(video_personal));
  list.append(field);

  field = new Field(QString::fromLatin1("keyword"), i18n("Keywords"));
  field->setCategory(i18n(video_personal));
  field->setFlags(Field::AllowCompletion | Field::AllowMultiple | Field::AllowGrouped);
  list.append(field);

  field = new Field(QString::fromLatin1("cover"), i18n("Cover"), Field::Image);
  list.append(field);

  field = new Field(QString::fromLatin1("comments"), i18n("Comments"), Field::Para);
  list.append(field);

  return list;
}

int VideoCollection::sameEntry(Data::EntryPtr entry1_, Data::EntryPtr entry2_) const {
  // not enough for title to be equal, must also have another field
  // ever possible for a studio to do two movies with identical titles?
  int res = 3*Entry::compareValues(entry1_, entry2_, QString::fromLatin1("title"), this);
//  if(res == 0) {
//    myDebug() << "VideoCollection::sameEntry() - different titles for " << entry1_->title() << " vs. "
//              << entry2_->title() << endl;
//  }
  res += Entry::compareValues(entry1_, entry2_, QString::fromLatin1("year"), this);
  res += Entry::compareValues(entry1_, entry2_, QString::fromLatin1("director"), this);
  res += Entry::compareValues(entry1_, entry2_, QString::fromLatin1("studio"), this);
  res += Entry::compareValues(entry1_, entry2_, QString::fromLatin1("medium"), this);
  return res;
}

#include "videocollection.moc"
