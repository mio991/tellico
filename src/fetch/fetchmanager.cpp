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

#include <config.h>

#include "fetchmanager.h"
#include "configwidget.h"
#include "messagehandler.h"
#include "../tellico_kernel.h"
#include "../entry.h"
#include "../collection.h"
#include "../tellico_utils.h"
#include "../tellico_debug.h"

#ifdef AMAZON_SUPPORT
#include "amazonfetcher.h"
#endif
#ifdef IMDB_SUPPORT
#include "imdbfetcher.h"
#endif
#ifdef HAVE_YAZ
#include "z3950fetcher.h"
#endif
#include "srufetcher.h"
#include "entrezfetcher.h"
#include "execexternalfetcher.h"
#include "yahoofetcher.h"
#include "animenfofetcher.h"
#include "ibsfetcher.h"
#include "isbndbfetcher.h"
#include "gcstarpluginfetcher.h"
#include "crossreffetcher.h"
#include "arxivfetcher.h"
#include "citebasefetcher.h"
#include "bibsonomyfetcher.h"
#include "googlescholarfetcher.h"
#include "discogsfetcher.h"

#include <kglobal.h>
#include <kconfig.h>
#include <klocale.h>
#include <kiconloader.h>
#include <kmimetype.h>
#include <kstandarddirs.h>
#include <dcopref.h>
#include <ktempfile.h>
#include <kio/netaccess.h>

#include <qfileinfo.h>
#include <qdir.h>

#define LOAD_ICON(name, group, size) \
  KGlobal::iconLoader()->loadIcon(name, static_cast<KIcon::Group>(group), size_)

using Tellico::Fetch::Manager;
Manager* Manager::s_self = 0;

Manager::Manager() : QObject(), m_currentFetcherIndex(-1), m_messager(new ManagerMessage()),
                     m_count(0), m_loadDefaults(false) {
  loadFetchers();

//  m_keyMap.insert(FetchFirst, QString::null);
  m_keyMap.insert(Title,      i18n("Title"));
  m_keyMap.insert(Person,     i18n("Person"));
  m_keyMap.insert(ISBN,       i18n("ISBN"));
  m_keyMap.insert(UPC,        i18n("UPC/EAN"));
  m_keyMap.insert(Keyword,    i18n("Keyword"));
  m_keyMap.insert(DOI,        i18n("DOI"));
  m_keyMap.insert(ArxivID,    i18n("arXiv ID"));
  m_keyMap.insert(PubmedID,   i18n("Pubmed ID"));
  // to keep from having a new i18n string, just remove octothorpe
  m_keyMap.insert(LCCN,       i18n("LCCN#").remove('#'));
  m_keyMap.insert(Raw,        i18n("Raw Query"));
//  m_keyMap.insert(FetchLast,  QString::null);
}

Manager::~Manager() {
  delete m_messager;
}

void Manager::loadFetchers() {
//  myDebug() << "Manager::loadFetchers()" << endl;
  m_fetchers.clear();
  m_configMap.clear();

  KConfig* config = KGlobal::config();
  if(config->hasGroup(QString::fromLatin1("Data Sources"))) {
    KConfigGroup configGroup(config, QString::fromLatin1("Data Sources"));
    int nSources = configGroup.readNumEntry("Sources Count", 0);
    for(int i = 0; i < nSources; ++i) {
      QString group = QString::fromLatin1("Data Source %1").arg(i);
      Fetcher::Ptr f = createFetcher(config, group);
      if(f) {
        m_configMap.insert(f, group);
        m_fetchers.append(f);
        f->setMessageHandler(m_messager);
      }
    }
    m_loadDefaults = false;
  } else { // add default sources
    m_fetchers = defaultFetchers();
    m_loadDefaults = true;
  }
}

Tellico::Fetch::FetcherVec Manager::fetchers(int type_) {
  FetcherVec vec;
  for(FetcherVec::Iterator it = m_fetchers.begin(); it != m_fetchers.end(); ++it) {
    if(it->canFetch(type_)) {
      vec.append(it.data());
    }
  }
  return vec;
}

Tellico::Fetch::KeyMap Manager::keyMap(const QString& source_) const {
  // an empty string means return all
  if(source_.isEmpty()) {
    return m_keyMap;
  }

  // assume there's only one fetcher match
  KSharedPtr<const Fetcher> f = 0;
  for(FetcherVec::ConstIterator it = m_fetchers.constBegin(); it != m_fetchers.constEnd(); ++it) {
    if(source_ == it->source()) {
      f = it.data();
      break;
    }
  }
  if(!f) {
    kdWarning() << "Manager::keyMap() - no fetcher found!" << endl;
    return KeyMap();
  }

  KeyMap map;
  for(KeyMap::ConstIterator it = m_keyMap.begin(); it != m_keyMap.end(); ++it) {
    if(f->canSearch(it.key())) {
      map.insert(it.key(), it.data());
    }
  }
  return map;
}

void Manager::startSearch(const QString& source_, FetchKey key_, const QString& value_) {
  if(value_.isEmpty()) {
    emit signalDone();
    return;
  }

  // assume there's only one fetcher match
  int i = 0;
  m_currentFetcherIndex = -1;
  for(FetcherVec::Iterator it = m_fetchers.begin(); it != m_fetchers.end(); ++it, ++i) {
    if(source_ == it->source()) {
      ++m_count; // Fetcher::search() might emit done(), so increment before calling search()
      connect(it.data(), SIGNAL(signalResultFound(Tellico::Fetch::SearchResult*)),
              SIGNAL(signalResultFound(Tellico::Fetch::SearchResult*)));
      connect(it.data(), SIGNAL(signalDone(Tellico::Fetch::Fetcher::Ptr)),
              SLOT(slotFetcherDone(Tellico::Fetch::Fetcher::Ptr)));
      it->search(key_, value_);
      m_currentFetcherIndex = i;
      break;
    }
  }
}

void Manager::continueSearch() {
  if(m_currentFetcherIndex < 0 || m_currentFetcherIndex >= static_cast<int>(m_fetchers.count())) {
    myDebug() << "Manager::continueSearch() - can't continue!" << endl;
    emit signalDone();
    return;
  }
  Fetcher::Ptr f = m_fetchers[m_currentFetcherIndex];
  if(f && f->hasMoreResults()) {
    ++m_count;
    connect(f, SIGNAL(signalResultFound(Tellico::Fetch::SearchResult*)),
            SIGNAL(signalResultFound(Tellico::Fetch::SearchResult*)));
    connect(f, SIGNAL(signalDone(Tellico::Fetch::Fetcher::Ptr)),
            SLOT(slotFetcherDone(Tellico::Fetch::Fetcher::Ptr)));
    f->continueSearch();
  } else {
    emit signalDone();
  }
}

bool Manager::hasMoreResults() const {
  if(m_currentFetcherIndex < 0 || m_currentFetcherIndex >= static_cast<int>(m_fetchers.count())) {
    return false;
  }
  Fetcher::Ptr f = m_fetchers[m_currentFetcherIndex];
  return f && f->hasMoreResults();
}

void Manager::stop() {
//  myDebug() << "Manager::stop()" << endl;
  for(FetcherVec::Iterator it = m_fetchers.begin(); it != m_fetchers.end(); ++it) {
    if(it->isSearching()) {
      it->stop();
    }
  }
#ifndef NDEBUG
  if(m_count != 0) {
    myDebug() << "Manager::stop() - count should be 0!" << endl;
  }
#endif
  m_count = 0;
}

void Manager::slotFetcherDone(Fetcher::Ptr fetcher_) {
//  myDebug() << "Manager::slotFetcherDone() - " << (fetcher_ ? fetcher_->source() : QString::null)
//            << " :" << m_count << endl;
  fetcher_->disconnect(); // disconnect all signals
  --m_count;
  if(m_count <= 0) {
    emit signalDone();
  }
}

bool Manager::canFetch() const {
  for(FetcherVec::ConstIterator it = m_fetchers.constBegin(); it != m_fetchers.constEnd(); ++it) {
    if(it->canFetch(Kernel::self()->collectionType())) {
      return true;
    }
  }
  return false;
}

Tellico::Fetch::Fetcher::Ptr Manager::createFetcher(KConfig* config_, const QString& group_) {
  if(!config_->hasGroup(group_)) {
    myDebug() << "Manager::createFetcher() - no config group for " << group_ << endl;
    return 0;
  }

  KConfigGroup config(config_, group_);

  int fetchType = config.readNumEntry("Type", Fetch::Unknown);
  if(fetchType == Fetch::Unknown) {
    myDebug() << "Manager::createFetcher() - unknown type " << fetchType << ", skipping" << endl;
    return 0;
  }

  Fetcher::Ptr f = 0;
  switch(fetchType) {
    case Amazon:
#ifdef AMAZON_SUPPORT
      {
        int site = config.readNumEntry("Site", AmazonFetcher::Unknown);
        if(site == AmazonFetcher::Unknown) {
          myDebug() << "Manager::createFetcher() - unknown amazon site " << site << ", skipping" << endl;
        } else {
          f = new AmazonFetcher(static_cast<AmazonFetcher::Site>(site), this);
        }
      }
#endif
      break;

    case IMDB:
#ifdef IMDB_SUPPORT
      f = new IMDBFetcher(this);
#endif
      break;

    case Z3950:
#ifdef HAVE_YAZ
      f = new Z3950Fetcher(this);
#endif
      break;

    case SRU:
      f = new SRUFetcher(this);
      break;

    case Entrez:
      f = new EntrezFetcher(this);
      break;

    case ExecExternal:
      f = new ExecExternalFetcher(this);
      break;

    case Yahoo:
      f = new YahooFetcher(this);
      break;

    case AnimeNfo:
      f = new AnimeNfoFetcher(this);
      break;

    case IBS:
      f = new IBSFetcher(this);
      break;

    case ISBNdb:
      f = new ISBNdbFetcher(this);
      break;

    case GCstarPlugin:
      f = new GCstarPluginFetcher(this);
      break;

    case CrossRef:
      f = new CrossRefFetcher(this);
      break;

    case Arxiv:
      f = new ArxivFetcher(this);
      break;

    case Citebase:
      f = new CitebaseFetcher(this);
      break;

    case Bibsonomy:
      f = new BibsonomyFetcher(this);
      break;

    case GoogleScholar:
      f = new GoogleScholarFetcher(this);
      break;

    case Discogs:
      f = new DiscogsFetcher(this);
      break;

    case Unknown:
    default:
      break;
  }
  if(f) {
    f->readConfig(config, group_);
  }
  return f;
}

// static
Tellico::Fetch::FetcherVec Manager::defaultFetchers() {
  FetcherVec vec;
#ifdef AMAZON_SUPPORT
  vec.append(new AmazonFetcher(AmazonFetcher::US, this));
#endif
#ifdef IMDB_SUPPORT
  vec.append(new IMDBFetcher(this));
#endif
  vec.append(SRUFetcher::libraryOfCongress(this));
  vec.append(new ISBNdbFetcher(this));
  vec.append(new YahooFetcher(this));
  vec.append(new AnimeNfoFetcher(this));
  vec.append(new ArxivFetcher(this));
  vec.append(new GoogleScholarFetcher(this));
  vec.append(new DiscogsFetcher(this));
// only add IBS if user includes italian
  if(KGlobal::locale()->languagesTwoAlpha().contains(QString::fromLatin1("it"))) {
    vec.append(new IBSFetcher(this));
  }
  return vec;
}

Tellico::Fetch::FetcherVec Manager::createUpdateFetchers(int collType_) {
  if(m_loadDefaults) {
    return defaultFetchers();
  }

  FetcherVec vec;
  KConfigGroup config(KGlobal::config(), "Data Sources");
  int nSources = config.readNumEntry("Sources Count", 0);
  for(int i = 0; i < nSources; ++i) {
    QString group = QString::fromLatin1("Data Source %1").arg(i);
    // needs the KConfig*
    Fetcher::Ptr f = createFetcher(KGlobal::config(), group);
    if(f && f->canFetch(collType_) && f->canUpdate()) {
      vec.append(f);
    }
  }
  return vec;
}

Tellico::Fetch::FetcherVec Manager::createUpdateFetchers(int collType_, Type type_) {
  FetcherVec fetchers;
  // creates new fetchers
  FetcherVec allFetchers = createUpdateFetchers(collType_);
  for(Fetch::FetcherVec::Iterator it = allFetchers.begin(); it != allFetchers.end(); ++it) {
    if(it->type() == type_) {
      fetchers.append(it);
    }
  }
  return fetchers;
}

Tellico::Fetch::Fetcher::Ptr Manager::createUpdateFetcher(int collType_, const QString& source_) {
  Fetcher::Ptr fetcher = 0;
  // creates new fetchers
  FetcherVec fetchers = createUpdateFetchers(collType_);
  for(Fetch::FetcherVec::Iterator it = fetchers.begin(); it != fetchers.end(); ++it) {
    if(it->source() == source_) {
      fetcher = it;
      break;
    }
  }
  return fetcher;
}

void Manager::updateStatus(const QString& message_) {
  emit signalStatus(message_);
}

Tellico::Fetch::TypePairList Manager::typeList() {
  Fetch::TypePairList list;
#ifdef AMAZON_SUPPORT
  list.append(TypePair(AmazonFetcher::defaultName(), Amazon));
#endif
#ifdef IMDB_SUPPORT
  list.append(TypePair(IMDBFetcher::defaultName(),         IMDB));
#endif
#ifdef HAVE_YAZ
  list.append(TypePair(Z3950Fetcher::defaultName(),        Z3950));
#endif
  list.append(TypePair(SRUFetcher::defaultName(),          SRU));
  list.append(TypePair(EntrezFetcher::defaultName(),       Entrez));
  list.append(TypePair(ExecExternalFetcher::defaultName(), ExecExternal));
  list.append(TypePair(YahooFetcher::defaultName(),        Yahoo));
  list.append(TypePair(AnimeNfoFetcher::defaultName(),     AnimeNfo));
  list.append(TypePair(IBSFetcher::defaultName(),          IBS));
  list.append(TypePair(ISBNdbFetcher::defaultName(),       ISBNdb));
  list.append(TypePair(GCstarPluginFetcher::defaultName(), GCstarPlugin));
  list.append(TypePair(CrossRefFetcher::defaultName(),     CrossRef));
  list.append(TypePair(ArxivFetcher::defaultName(),        Arxiv));
  list.append(TypePair(CitebaseFetcher::defaultName(),     Citebase));
  list.append(TypePair(BibsonomyFetcher::defaultName(),    Bibsonomy));
  list.append(TypePair(GoogleScholarFetcher::defaultName(),GoogleScholar));
  list.append(TypePair(DiscogsFetcher::defaultName(),      Discogs));

  // now find all the scripts distributed with tellico
  QStringList files = KGlobal::dirs()->findAllResources("appdata", QString::fromLatin1("data-sources/*.spec"),
                                                        false, true);
  for(QStringList::Iterator it = files.begin(); it != files.end(); ++it) {
    KConfig spec(*it, false, false);
    QString name = spec.readEntry("Name");
    if(name.isEmpty()) {
      myDebug() << "Fetch::Manager::typeList() - no Name for " << *it << endl;
      continue;
    }

    if(!bundledScriptHasExecPath(*it, &spec)) { // no available exec
      continue;
    }

    list.append(TypePair(name, ExecExternal));
    m_scriptMap.insert(name, *it);
  }
  list.sort();
  return list;
}


// called when creating a new fetcher
Tellico::Fetch::ConfigWidget* Manager::configWidget(QWidget* parent_, Type type_, const QString& name_) {
  ConfigWidget* w = 0;
  switch(type_) {
#ifdef AMAZON_SUPPORT
    case Amazon:
      w = new AmazonFetcher::ConfigWidget(parent_);
      break;
#endif
#ifdef IMDB_SUPPORT
    case IMDB:
      w = new IMDBFetcher::ConfigWidget(parent_);
      break;
#endif
#ifdef HAVE_YAZ
    case Z3950:
      w = new Z3950Fetcher::ConfigWidget(parent_);
      break;
#endif
    case SRU:
      w = new SRUConfigWidget(parent_);
      break;
    case Entrez:
      w = new EntrezFetcher::ConfigWidget(parent_);
      break;
    case ExecExternal:
      w = new ExecExternalFetcher::ConfigWidget(parent_);
      if(!name_.isEmpty() && m_scriptMap.contains(name_)) {
        // bundledScriptHasExecPath() actually needs to write the exec path
        // back to the config so the configWidget can read it. But if the spec file
        // is not readablle, that doesn't work. So work around it with a copy to a temp file
        KTempFile tmpFile;
        tmpFile.setAutoDelete(true);
        KURL from, to;
        from.setPath(m_scriptMap[name_]);
        to.setPath(tmpFile.name());
        // have to overwrite since KTempFile already created it
        if(!KIO::NetAccess::file_copy(from, to, -1, true /*overwrite*/)) {
          myDebug() << KIO::NetAccess::lastErrorString() << endl;
        }
        KConfig spec(to.path(), false, false);
        // pass actual location of spec file
        if(name_ == spec.readEntry("Name") && bundledScriptHasExecPath(m_scriptMap[name_], &spec)) {
          static_cast<ExecExternalFetcher::ConfigWidget*>(w)->readConfig(&spec);
        } else {
          kdWarning() << "Fetch::Manager::configWidget() - Can't read config file for " << to.path() << endl;
        }
      }
      break;
    case Yahoo:
      w = new YahooFetcher::ConfigWidget(parent_);
      break;
    case AnimeNfo:
      w = new AnimeNfoFetcher::ConfigWidget(parent_);
      break;
    case IBS:
      w = new IBSFetcher::ConfigWidget(parent_);
      break;
    case ISBNdb:
      w = new ISBNdbFetcher::ConfigWidget(parent_);
      break;
    case GCstarPlugin:
      w = new GCstarPluginFetcher::ConfigWidget(parent_);
      break;
    case CrossRef:
      w = new CrossRefFetcher::ConfigWidget(parent_);
      break;
    case Arxiv:
      w = new ArxivFetcher::ConfigWidget(parent_);
      break;
    case Citebase:
      w = new CitebaseFetcher::ConfigWidget(parent_);
      break;
    case Bibsonomy:
      w = new BibsonomyFetcher::ConfigWidget(parent_);
      break;
    case GoogleScholar:
      w = new GoogleScholarFetcher::ConfigWidget(parent_);
      break;
    case Discogs:
      w = new DiscogsFetcher::ConfigWidget(parent_);
      break;
    case Unknown:
      kdWarning() << "Fetch::Manager::configWidget() - no widget defined for type = " << type_ << endl;
  }
  return w;
}

// static
QString Manager::typeName(Fetch::Type type_) {
  switch(type_) {
#ifdef AMAZON_SUPPORT
    case Amazon: return AmazonFetcher::defaultName();
#endif
#ifdef IMDB_SUPPORT
    case IMDB: return IMDBFetcher::defaultName();
#endif
#ifdef HAVE_YAZ
    case Z3950: return Z3950Fetcher::defaultName();
#endif
    case SRU: return SRUFetcher::defaultName();
    case Entrez: return EntrezFetcher::defaultName();
    case ExecExternal: return ExecExternalFetcher::defaultName();
    case Yahoo: return YahooFetcher::defaultName();
    case AnimeNfo: return AnimeNfoFetcher::defaultName();
    case IBS: return IBSFetcher::defaultName();
    case ISBNdb: return ISBNdbFetcher::defaultName();
    case GCstarPlugin: return GCstarPluginFetcher::defaultName();
    case CrossRef: return CrossRefFetcher::defaultName();
    case Arxiv: return ArxivFetcher::defaultName();
    case Citebase: return CitebaseFetcher::defaultName();
    case Bibsonomy: return BibsonomyFetcher::defaultName();
    case GoogleScholar: return GoogleScholarFetcher::defaultName();
    case Discogs: return DiscogsFetcher::defaultName();
    case Unknown: break;
  }
  myWarning() << "Manager::typeName() - none found for " << type_ << endl;
  return QString::null;
}

QPixmap Manager::fetcherIcon(Fetch::Fetcher::CPtr fetcher_, int group_, int size_) {
#ifdef HAVE_YAZ
  if(fetcher_->type() == Fetch::Z3950) {
    const Fetch::Z3950Fetcher* f = static_cast<const Fetch::Z3950Fetcher*>(fetcher_.data());
    KURL u;
    u.setProtocol(QString::fromLatin1("http"));
    u.setHost(f->host());
    QString icon = favIcon(u);
    if(u.isValid() && !icon.isEmpty()) {
      return LOAD_ICON(icon, group_, size_);
    }
  } else
#endif
  if(fetcher_->type() == Fetch::ExecExternal) {
    const Fetch::ExecExternalFetcher* f = static_cast<const Fetch::ExecExternalFetcher*>(fetcher_.data());
    const QString p = f->execPath();
    KURL u;
    if(p.find(QString::fromLatin1("allocine")) > -1) {
      u = QString::fromLatin1("http://www.allocine.fr");
    } else if(p.find(QString::fromLatin1("ministerio_de_cultura")) > -1) {
      u = QString::fromLatin1("http://www.mcu.es");
    } else if(p.find(QString::fromLatin1("dark_horse_comics")) > -1) {
      u = QString::fromLatin1("http://www.darkhorse.com");
    } else if(p.find(QString::fromLatin1("boardgamegeek")) > -1) {
      u = QString::fromLatin1("http://www.boardgamegeek.com");
    } else if(f->source().find(QString::fromLatin1("amarok"), 0, false /*case-sensitive*/) > -1) {
      return LOAD_ICON(QString::fromLatin1("amarok"), group_, size_);
    }
    if(!u.isEmpty() && u.isValid()) {
      QString icon = favIcon(u);
      if(!icon.isEmpty()) {
        return LOAD_ICON(icon, group_, size_);
      }
    }
  }
  return fetcherIcon(fetcher_->type(), group_);
}

QPixmap Manager::fetcherIcon(Fetch::Type type_, int group_, int size_) {
  QString name;
  switch(type_) {
    case Amazon:
      name = favIcon("http://amazon.com"); break;
    case IMDB:
      name = favIcon("http://imdb.com"); break;
    case Z3950:
      name = QString::fromLatin1("network"); break; // rather arbitrary
    case SRU:
      name = QString::fromLatin1("network_local"); break; // just to be different than z3950
    case Entrez:
      name = favIcon("http://www.ncbi.nlm.nih.gov"); break;
    case ExecExternal:
      name = QString::fromLatin1("exec"); break;
    case Yahoo:
      name = favIcon("http://yahoo.com"); break;
    case AnimeNfo:
      name = favIcon("http://animenfo.com"); break;
    case IBS:
      name = favIcon("http://internetbookshop.it"); break;
    case ISBNdb:
      name = favIcon("http://isbndb.com"); break;
    case GCstarPlugin:
      name = QString::fromLatin1("gcstar"); break;
    case CrossRef:
      name = favIcon("http://crossref.org"); break;
    case Arxiv:
      name = favIcon("http://arxiv.org"); break;
    case Citebase:
      name = favIcon("http://citebase.org"); break;
    case Bibsonomy:
      name = favIcon("http://bibsonomy.org"); break;
    case GoogleScholar:
      name = favIcon("http://scholar.google.com"); break;
    case Discogs:
      name = favIcon("http://www.discogs.com"); break;
    case Unknown:
      kdWarning() << "Fetch::Manager::fetcherIcon() - no pixmap defined for type = " << type_ << endl;
  }

  return name.isEmpty() ? QPixmap() : LOAD_ICON(name, group_, size_);
}

QString Manager::favIcon(const KURL& url_) {
  DCOPRef kded("kded", "favicons");
  DCOPReply reply = kded.call("iconForURL(KURL)", url_);
  QString iconName = reply.isValid() ? reply : QString();
  if(!iconName.isEmpty()) {
    return iconName;
  } else {
    // go ahead and try to download it for later
    kded.call("downloadHostIcon(KURL)", url_);
  }
  return KMimeType::iconForURL(url_);
}

bool Manager::bundledScriptHasExecPath(const QString& specFile_, KConfig* config_) {
  // make sure ExecPath is set and executable
  // for the bundled scripts, either the exec name is not set, in which case it is the
  // name of the spec file, minus the .spec, or the exec is set, and is local to the dir
  // if not, look for it
  QString exec = config_->readPathEntry("ExecPath");
  QFileInfo specInfo(specFile_), execInfo(exec);
  if(exec.isEmpty() || !execInfo.exists()) {
    exec = specInfo.dirPath(true) + QDir::separator() + specInfo.baseName(true); // remove ".spec"
  } else if(execInfo.isRelative()) {
    exec = specInfo.dirPath(true) + exec;
  } else if(!execInfo.isExecutable()) {
    kdWarning() << "Fetch::Manager::execPathForBundledScript() - not executable: " << specFile_ << endl;
    return false;
  }
  execInfo.setFile(exec);
  if(!execInfo.exists() || !execInfo.isExecutable()) {
    kdWarning() << "Fetch::Manager::execPathForBundledScript() - no exec file for " << specFile_ << endl;
    kdWarning() << "exec = " << exec << endl;
    return false; // we're not ok
  }

  config_->writePathEntry("ExecPath", exec);
  config_->sync(); // might be readonly, but that's ok
  return true;
}

#include "fetchmanager.moc"
