/***************************************************************************
    copyright            : (C) 2003-2008 by Robby Stephenson
    email                : robby@periapsis.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of version 2 of the GNU General Public License as  *
 *   published by the Free Software Foundation;                            *
 *                                                                         *
 ***************************************************************************/

#include "fetchdialog.h"
#include "fetch/fetchmanager.h"
#include "fetch/fetcher.h"
#include "entryview.h"
#include "isbnvalidator.h"
#include "upcvalidator.h"
#include "tellico_kernel.h"
#include "filehandler.h"
#include "collection.h"
#include "entry.h"
#include "document.h"
#include "tellico_debug.h"
#include "gui/combobox.h"
#include "gui/listview.h"
#include "tellico_utils.h"
#include "stringset.h"

#include <klocale.h>
#include <klineedit.h>
#include <kpushbutton.h>
#include <kstatusbar.h>
#include <khtmlview.h>
#include <kprogress.h>
#include <kconfig.h>
#include <kdialogbase.h>
#include <kfiledialog.h>
#include <kiconloader.h>
#include <kaccelmanager.h>
#include <ktextedit.h>

#include <qlayout.h>
#include <qhbox.h>
#include <qvgroupbox.h>
#include <qsplitter.h>
#include <qtimer.h>
#include <qwhatsthis.h>
#include <qcheckbox.h>
#include <qvbox.h>
#include <qtimer.h>
#include <qimage.h>

#include <config.h>
#ifdef ENABLE_WEBCAM
#include "barcode/barcode.h"
#endif

namespace {
  static const int FETCH_STATUS_ID = 0;
  static const int FETCH_PROGRESS_ID = 0;
  static const int FETCH_MIN_WIDTH = 600;

  static const char* FETCH_STRING_SEARCH = I18N_NOOP("&Search");
  static const char* FETCH_STRING_STOP   = I18N_NOOP("&Stop");
}

using Tellico::FetchDialog;
using barcodeRecognition::barcodeRecognitionThread;

class FetchDialog::SearchResultItem : public Tellico::GUI::ListViewItem {
  friend class FetchDialog;
  // always add to end
  SearchResultItem(GUI::ListView* lv, Fetch::SearchResult* r)
      : GUI::ListViewItem(lv, lv->lastItem()), m_result(r) {
    setText(1, r->title);
    setText(2, r->desc);
    setPixmap(3, Fetch::Manager::self()->fetcherIcon(r->fetcher.data()));
    setText(3, r->fetcher->source());
  }
  Fetch::SearchResult* m_result;
};

FetchDialog::FetchDialog(QWidget* parent_, const char* name_)
    : KDialogBase(parent_, name_, false, i18n("Internet Search"), 0),
      m_timer(new QTimer(this)), m_started(false) {
  m_collType = Kernel::self()->collectionType();

  QWidget* mainWidget = new QWidget(this, "FetchDialog mainWidget");
  setMainWidget(mainWidget);
  QVBoxLayout* topLayout = new QVBoxLayout(mainWidget, 0, KDialog::spacingHint());

  QVGroupBox* queryBox = new QVGroupBox(i18n("Search Query"), mainWidget, "FetchDialog queryBox");
  topLayout->addWidget(queryBox);

  QHBox* box1 = new QHBox(queryBox, "FetchDialog box1");
  box1->setSpacing(KDialog::spacingHint());

  QLabel* label = new QLabel(i18n("Start the search", "S&earch:"), box1);

  m_valueLineEdit = new KLineEdit(box1);
  label->setBuddy(m_valueLineEdit);
  QWhatsThis::add(m_valueLineEdit, i18n("Enter a search value. An ISBN search must include the full ISBN."));
  m_keyCombo = new GUI::ComboBox(box1);
  Fetch::KeyMap map = Fetch::Manager::self()->keyMap();
  for(Fetch::KeyMap::ConstIterator it = map.begin(); it != map.end(); ++it) {
    m_keyCombo->insertItem(it.data(), it.key());
  }
  connect(m_keyCombo, SIGNAL(activated(int)), SLOT(slotKeyChanged(int)));
  QWhatsThis::add(m_keyCombo, i18n("Choose the type of search"));

  m_searchButton = new KPushButton(box1);
  m_searchButton->setGuiItem(KGuiItem(i18n(FETCH_STRING_STOP),
                                      SmallIconSet(QString::fromLatin1("cancel"))));
  connect(m_searchButton, SIGNAL(clicked()), SLOT(slotSearchClicked()));
  QWhatsThis::add(m_searchButton, i18n("Click to start or stop the search"));

  // the search button's text changes from search to stop
  // I don't want it resizing, so figure out the maximum size and set that
  m_searchButton->polish();
  int maxWidth = m_searchButton->sizeHint().width();
  int maxHeight = m_searchButton->sizeHint().height();
  m_searchButton->setGuiItem(KGuiItem(i18n(FETCH_STRING_SEARCH),
                                      SmallIconSet(QString::fromLatin1("find"))));
  maxWidth = QMAX(maxWidth, m_searchButton->sizeHint().width());
  maxHeight = QMAX(maxHeight, m_searchButton->sizeHint().height());
  m_searchButton->setMinimumWidth(maxWidth);
  m_searchButton->setMinimumHeight(maxHeight);

  QHBox* box2 = new QHBox(queryBox);
  box2->setSpacing(KDialog::spacingHint());

  m_multipleISBN = new QCheckBox(i18n("&Multiple ISBN/UPC search"), box2);
  QWhatsThis::add(m_multipleISBN, i18n("Check this box to search for multiple ISBN or UPC values."));
  connect(m_multipleISBN, SIGNAL(toggled(bool)), SLOT(slotMultipleISBN(bool)));

  m_editISBN = new KPushButton(KGuiItem(i18n("Edit List..."), QString::fromLatin1("text_block")), box2);
  m_editISBN->setEnabled(false);
  QWhatsThis::add(m_editISBN, i18n("Click to open a text edit box for entering or editing multiple ISBN values."));
  connect(m_editISBN, SIGNAL(clicked()), SLOT(slotEditMultipleISBN()));

  // add for spacing
  box2->setStretchFactor(new QWidget(box2), 10);

  label = new QLabel(i18n("Search s&ource:"), box2);
  m_sourceCombo = new KComboBox(box2);
  label->setBuddy(m_sourceCombo);
  Fetch::FetcherVec sources = Fetch::Manager::self()->fetchers(m_collType);
  for(Fetch::FetcherVec::Iterator it = sources.begin(); it != sources.end(); ++it) {
    m_sourceCombo->insertItem(Fetch::Manager::self()->fetcherIcon(it.data()), (*it).source());
  }
  connect(m_sourceCombo, SIGNAL(activated(const QString&)), SLOT(slotSourceChanged(const QString&)));
  QWhatsThis::add(m_sourceCombo, i18n("Select the database to search"));

  QSplitter* split = new QSplitter(QSplitter::Vertical, mainWidget);
  topLayout->addWidget(split);

  m_listView = new GUI::ListView(split);
//  topLayout->addWidget(m_listView);
//  topLayout->setStretchFactor(m_listView, 1);
  m_listView->setSorting(10); // greater than number of columns, so not sorting until user clicks column header
  m_listView->setShowSortIndicator(true);
  m_listView->setAllColumnsShowFocus(true);
  m_listView->setSelectionMode(QListView::Extended);
  m_listView->addColumn(QString::null, 20); // will show a check mark when added
  m_listView->setColumnAlignment(0, Qt::AlignHCenter); // align checkmark in middle
//  m_listView->setColumnWidthMode(0, QListView::Manual);
  m_listView->addColumn(i18n("Title"));
  m_listView->addColumn(i18n("Description"));
  m_listView->addColumn(i18n("Source"));
  m_listView->viewport()->installEventFilter(this);

  connect(m_listView, SIGNAL(selectionChanged()), SLOT(slotShowEntry()));
  // double clicking should add the entry
  connect(m_listView, SIGNAL(doubleClicked(QListViewItem*)), SLOT(slotAddEntry()));
  QWhatsThis::add(m_listView, i18n("As results are found, they are added to this list. Selecting one "
                                   "will fetch the complete entry and show it in the view below."));

  m_entryView = new EntryView(split, "entry_view");
  // don't bother creating funky gradient images for compact view
  m_entryView->setUseGradientImages(false);
  // set the xslt file AFTER setting the gradient image option
  m_entryView->setXSLTFile(QString::fromLatin1("Compact.xsl"));
  QWhatsThis::add(m_entryView->view(), i18n("An entry may be shown here before adding it to the "
                                            "current collection by selecting it in the list above"));

  QHBox* box3 = new QHBox(mainWidget);
  topLayout->addWidget(box3);
  box3->setSpacing(KDialog::spacingHint());

  m_addButton = new KPushButton(i18n("&Add Entry"), box3);
  m_addButton->setEnabled(false);
  m_addButton->setIconSet(UserIconSet(Kernel::self()->collectionTypeName()));
  connect(m_addButton, SIGNAL(clicked()), SLOT(slotAddEntry()));
  QWhatsThis::add(m_addButton, i18n("Add the selected entry to the current collection"));

  m_moreButton = new KPushButton(KGuiItem(i18n("Get More Results"), SmallIconSet(QString::fromLatin1("find"))), box3);
  m_moreButton->setEnabled(false);
  connect(m_moreButton, SIGNAL(clicked()), SLOT(slotMoreClicked()));
  QWhatsThis::add(m_moreButton, i18n("Fetch more results from the current data source"));

  KPushButton* clearButton = new KPushButton(KStdGuiItem::clear(), box3);
  connect(clearButton, SIGNAL(clicked()), SLOT(slotClearClicked()));
  QWhatsThis::add(clearButton, i18n("Clear all search fields and results"));

  QHBox* bottombox = new QHBox(mainWidget, "box");
  topLayout->addWidget(bottombox);
  bottombox->setSpacing(KDialog::spacingHint());

  m_statusBar = new KStatusBar(bottombox, "statusbar");
  m_statusBar->insertItem(QString::null, FETCH_STATUS_ID, 1, false);
  m_statusBar->setItemAlignment(FETCH_STATUS_ID, AlignLeft | AlignVCenter);
  m_progress = new QProgressBar(m_statusBar, "progress");
  m_progress->setTotalSteps(0);
  m_progress->setFixedHeight(fontMetrics().height()+2);
  m_progress->hide();
  m_statusBar->addWidget(m_progress, 0, true);

  KPushButton* closeButton = new KPushButton(KStdGuiItem::close(), bottombox);
  connect(closeButton, SIGNAL(clicked()), SLOT(slotClose()));

  connect(m_timer, SIGNAL(timeout()), SLOT(slotMoveProgress()));

  setMinimumWidth(QMAX(minimumWidth(), FETCH_MIN_WIDTH));
  setStatus(i18n("Ready."));

  resize(configDialogSize(QString::fromLatin1("Fetch Dialog Options")));

  KConfigGroup config(kapp->config(), "Fetch Dialog Options");
  QValueList<int> splitList = config.readIntListEntry("Splitter Sizes");
  if(!splitList.empty()) {
    split->setSizes(splitList);
  }

  connect(Fetch::Manager::self(), SIGNAL(signalResultFound(Tellico::Fetch::SearchResult*)),
                                  SLOT(slotResultFound(Tellico::Fetch::SearchResult*)));
  connect(Fetch::Manager::self(), SIGNAL(signalStatus(const QString&)),
                                  SLOT(slotStatus(const QString&)));
  connect(Fetch::Manager::self(), SIGNAL(signalDone()),
                                  SLOT(slotFetchDone()));

  // make sure to delete results afterwards
  m_results.setAutoDelete(true);

  KAcceleratorManager::manage(this);
  // initialize combos
  QTimer::singleShot(0, this, SLOT(slotInit()));

#ifdef ENABLE_WEBCAM
  // barcode recognition
  m_barcodeRecognitionThread = new barcodeRecognitionThread();
  if (m_barcodeRecognitionThread->isWebcamAvailable()) {
    m_barcodePreview = new QLabel(0);
    m_barcodePreview->move( QApplication::desktop()->width() - 350, 30 );
    m_barcodePreview->setAutoResize( true );
    m_barcodePreview->show();
  } else {
    m_barcodePreview = 0;
  }
  connect( m_barcodeRecognitionThread, SIGNAL(recognized(QString)), this, SLOT(slotBarcodeRecognized(QString)) );
  connect( m_barcodeRecognitionThread, SIGNAL(gotImage(QImage&)), this, SLOT(slotBarcodeGotImage(QImage&)) );
  m_barcodeRecognitionThread->start();
/*  //DEBUG
  QImage img( "/home/sebastian/white.png", "PNG" );
  m_barcodeRecognitionThread->recognizeBarcode( img );*/
#endif
}

FetchDialog::~FetchDialog() {
#ifdef ENABLE_WEBCAM
  m_barcodeRecognitionThread->stop();
  if (!m_barcodeRecognitionThread->wait( 1000 ))
    m_barcodeRecognitionThread->terminate();
  delete m_barcodeRecognitionThread;
  if (m_barcodePreview)
    delete m_barcodePreview;
#endif

  // we might have downloaded a lot of images we don't need to keep
  Data::EntryVec entriesToCheck;
  for(QMap<int, Data::EntryPtr>::Iterator it = m_entries.begin(); it != m_entries.end(); ++it) {
    entriesToCheck.append(it.data());
  }
  // no additional entries to check images to keep though
  Data::Document::self()->removeImagesNotInCollection(entriesToCheck, Data::EntryVec());

  saveDialogSize(QString::fromLatin1("Fetch Dialog Options"));

  KConfigGroup config(kapp->config(), "Fetch Dialog Options");
  config.writeEntry("Splitter Sizes", static_cast<QSplitter*>(m_listView->parentWidget())->sizes());
  config.writeEntry("Search Key", m_keyCombo->currentData().toInt());
  config.writeEntry("Search Source", m_sourceCombo->currentText());
}

void FetchDialog::slotSearchClicked() {
  m_valueLineEdit->selectAll();
  if(m_started) {
    setStatus(i18n("Cancelling the search..."));
    Fetch::Manager::self()->stop();
    slotFetchDone();
  } else {
    QString value = m_valueLineEdit->text().simplifyWhiteSpace();
    if(value != m_oldSearch) {
      m_listView->clear();
      m_entryView->clear();
    }
    m_resultCount = 0;
    m_oldSearch = value;
    m_started = true;
    m_searchButton->setGuiItem(KGuiItem(i18n(FETCH_STRING_STOP),
                                        SmallIconSet(QString::fromLatin1("cancel"))));
    startProgress();
    setStatus(i18n("Searching..."));
    kapp->processEvents();
    Fetch::Manager::self()->startSearch(m_sourceCombo->currentText(),
                                        static_cast<Fetch::FetchKey>(m_keyCombo->currentData().toInt()),
                                        value);
  }
}

void FetchDialog::slotClearClicked() {
  slotFetchDone(false);
  m_listView->clear();
  m_entryView->clear();
  Fetch::Manager::self()->stop();
  m_multipleISBN->setChecked(false);
  m_valueLineEdit->clear();
  m_valueLineEdit->setFocus();
  m_addButton->setEnabled(false);
  m_moreButton->setEnabled(false);
  m_isbnList.clear();
  m_statusMessages.clear();
  setStatus(i18n("Ready.")); // because slotFetchDone() writes text
}

void FetchDialog::slotStatus(const QString& status_) {
  m_statusMessages.push_back(status_);
  // if the queue was empty, start the timer
  if(m_statusMessages.count() == 1) {
    // wait 2 seconds
    QTimer::singleShot(2000, this, SLOT(slotUpdateStatus()));
  }
}

void FetchDialog::slotUpdateStatus() {
  if(m_statusMessages.isEmpty()) {
    return;
  }
  setStatus(m_statusMessages.front());
  m_statusMessages.pop_front();
  if(!m_statusMessages.isEmpty()) {
    // wait 2 seconds
    QTimer::singleShot(2000, this, SLOT(slotUpdateStatus()));
  }
}

void FetchDialog::setStatus(const QString& text_) {
  m_statusBar->changeItem(QChar(' ') + text_, FETCH_STATUS_ID);
}

void FetchDialog::slotFetchDone(bool checkISBN /* = true */) {
//  myDebug() << "FetchDialog::slotFetchDone()" << endl;
  m_started = false;
  m_searchButton->setGuiItem(KGuiItem(i18n(FETCH_STRING_SEARCH),
                                      SmallIconSet(QString::fromLatin1("find"))));
  stopProgress();
  if(m_resultCount == 0) {
    slotStatus(i18n("The search returned no items."));
  } else {
    /* TRANSLATORS: This is a plural form, you need to translate both lines (except "_n: ") */
    slotStatus(i18n("The search returned 1 item.",
                    "The search returned %n items.",
                    m_resultCount));
  }
  m_moreButton->setEnabled(Fetch::Manager::self()->hasMoreResults());

  // if we're not checking isbn values, then, ok to return
  if(!checkISBN) {
    return;
  }

  const Fetch::FetchKey key = static_cast<Fetch::FetchKey>(m_keyCombo->currentData().toInt());
  // no way to currently check EAN/UPC values for non-book items
  if(m_collType & (Data::Collection::Book | Data::Collection::Bibtex) &&
     m_multipleISBN->isChecked() &&
     (key == Fetch::ISBN || key == Fetch::UPC)) {
    QStringList values = QStringList::split(QString::fromLatin1("; "),
                                            m_oldSearch.simplifyWhiteSpace());
    for(QStringList::Iterator it = values.begin(); it != values.end(); ++it) {
      *it = ISBNValidator::cleanValue(*it);
    }
    for(QListViewItemIterator it(m_listView); it.current(); ++it) {
      QString i = ISBNValidator::cleanValue(static_cast<SearchResultItem*>(it.current())->m_result->isbn);
      values.remove(i);
      if(i.length() > 10 && i.startsWith(QString::fromLatin1("978"))) {
        values.remove(ISBNValidator::cleanValue(ISBNValidator::isbn10(i)));
      }
    }
    if(!values.isEmpty()) {
      for(QStringList::Iterator it = values.begin(); it != values.end(); ++it) {
        ISBNValidator::staticFixup(*it);
      }
      // TODO dialog caption
      KDialogBase* dlg = new KDialogBase(this, "isbn not found dialog", false, QString::null, KDialogBase::Ok);
      QWidget* box = new QWidget(dlg);
      QVBoxLayout* lay = new QVBoxLayout(box, KDialog::marginHint(), KDialog::spacingHint()*2);
      QHBoxLayout* lay2 = new QHBoxLayout(lay);
      QLabel* lab = new QLabel(box);
      lab->setPixmap(KGlobal::iconLoader()->loadIcon(QString::fromLatin1("messagebox_info"),
                                                     KIcon::NoGroup,
                                                     KIcon::SizeMedium));
      lay2->addWidget(lab);
      QString s = i18n("No results were found for the following ISBN values:");
      lay2->addWidget(new QLabel(s, box), 10);
      KTextEdit* edit = new KTextEdit(box, "isbn list edit");
      lay->addWidget(edit);
      edit->setText(values.join(QChar('\n')));
      QWhatsThis::add(edit, s);
      connect(dlg, SIGNAL(okClicked()), dlg, SLOT(deleteLater()));
      dlg->setMainWidget(box);
      dlg->setMinimumWidth(QMAX(dlg->minimumWidth(), FETCH_MIN_WIDTH*2/3));
      dlg->show();
    }
  }
}

void FetchDialog::slotResultFound(Tellico::Fetch::SearchResult* result_) {
  m_results.append(result_);
  (void) new SearchResultItem(m_listView, result_);
  ++m_resultCount;
  adjustColumnWidth();
  kapp->processEvents();
}

void FetchDialog::slotAddEntry() {
  GUI::CursorSaver cs;
  Data::EntryVec vec;
  for(QListViewItemIterator it(m_listView, QListViewItemIterator::Selected); it.current(); ++it) {
    SearchResultItem* item = static_cast<SearchResultItem*>(it.current());

    Fetch::SearchResult* r = item->m_result;
    Data::EntryPtr entry = m_entries[r->uid];
    if(!entry) {
      setStatus(i18n("Fetching %1...").arg(r->title));
      startProgress();
      entry = r->fetchEntry();
      if(!entry) {
        continue;
      }
      m_entries.insert(r->uid, entry);
      stopProgress();
      setStatus(i18n("Ready."));
    }
    // add a copy, intentionally allowing multiple copies to be added
    vec.append(new Data::Entry(*entry));
    item->setPixmap(0, UserIcon(QString::fromLatin1("checkmark")));
  }
  if(!vec.isEmpty()) {
    Kernel::self()->addEntries(vec, true);
  }
}

void FetchDialog::slotMoreClicked() {
  if(m_started) {
    myDebug() << "FetchDialog::slotMoreClicked() - can't continue while running" << endl;
    return;
  }

  m_started = true;
  m_searchButton->setGuiItem(KGuiItem(i18n(FETCH_STRING_STOP),
                                      SmallIconSet(QString::fromLatin1("cancel"))));
  startProgress();
  setStatus(i18n("Searching..."));
  kapp->processEvents();
  Fetch::Manager::self()->continueSearch();
}

void FetchDialog::slotShowEntry() {
  // just in case
  m_statusMessages.clear();

  const GUI::ListViewItemList& items = m_listView->selectedItems();
  if(items.isEmpty()) {
    m_addButton->setEnabled(false);
    return;
  }

  m_addButton->setEnabled(true);
  if(items.count() > 1) {
    m_entryView->clear();
    return;
  }

  SearchResultItem* item = static_cast<SearchResultItem*>(items.getFirst());
  Fetch::SearchResult* r = item->m_result;
  setStatus(i18n("Fetching %1...").arg(r->title));
  Data::EntryPtr entry = m_entries[r->uid];
  if(!entry) {
    GUI::CursorSaver cs;
    startProgress();
    entry = r->fetchEntry();
    if(entry) { // might conceivably be null
      m_entries.insert(r->uid, entry);
    }
    stopProgress();
  }
  setStatus(i18n("Ready."));

  m_entryView->showEntry(entry);
}

void FetchDialog::startProgress() {
  m_progress->show();
  m_timer->start(100);
}

void FetchDialog::slotMoveProgress() {
  m_progress->setProgress(m_progress->progress()+5);
}

void FetchDialog::stopProgress() {
  m_timer->stop();
  m_progress->hide();
}

void FetchDialog::slotInit() {
  if(!Fetch::Manager::self()->canFetch()) {
    m_searchButton->setEnabled(false);
    Kernel::self()->sorry(i18n("No Internet sources are available for your current collection type."), this);
  }

  KConfigGroup config(kapp->config(), "Fetch Dialog Options");
  int key = config.readNumEntry("Search Key", Fetch::FetchFirst);
  // only change key if valid
  if(key > Fetch::FetchFirst) {
    m_keyCombo->setCurrentData(key);
  }
  slotKeyChanged(m_keyCombo->currentItem());

  QString source = config.readEntry("Search Source");
  if(!source.isEmpty()) {
    m_sourceCombo->setCurrentItem(source);
  }
  slotSourceChanged(m_sourceCombo->currentText());

  m_valueLineEdit->setFocus();
  m_searchButton->setDefault(true);
}

void FetchDialog::slotKeyChanged(int idx_) {
  int key = m_keyCombo->data(idx_).toInt();
  if(key == Fetch::ISBN || key == Fetch::UPC || key == Fetch::LCCN) {
    m_multipleISBN->setEnabled(true);
    if(key == Fetch::ISBN) {
      m_valueLineEdit->setValidator(new ISBNValidator(this));
    } else {
      UPCValidator* upc = new UPCValidator(this);
      connect(upc, SIGNAL(signalISBN()), SLOT(slotUPC2ISBN()));
      m_valueLineEdit->setValidator(upc);
      // only want to convert to ISBN if ISBN is accepted by the fetcher
      Fetch::KeyMap map = Fetch::Manager::self()->keyMap(m_sourceCombo->currentText());
      upc->setCheckISBN(map.contains(Fetch::ISBN));
    }
  } else {
    m_multipleISBN->setChecked(false);
    m_multipleISBN->setEnabled(false);
//    slotMultipleISBN(false);
    m_valueLineEdit->setValidator(0);
  }
}

void FetchDialog::slotSourceChanged(const QString& source_) {
  int curr = m_keyCombo->currentData().toInt();
  m_keyCombo->clear();
  Fetch::KeyMap map = Fetch::Manager::self()->keyMap(source_);
  for(Fetch::KeyMap::ConstIterator it = map.begin(); it != map.end(); ++it) {
    m_keyCombo->insertItem(it.data(), it.key());
  }
  m_keyCombo->setCurrentData(curr);
  slotKeyChanged(m_keyCombo->currentItem());
}

void FetchDialog::slotMultipleISBN(bool toggle_) {
  bool wasEnabled = m_valueLineEdit->isEnabled();
  m_valueLineEdit->setEnabled(!toggle_);
  if(!wasEnabled && m_valueLineEdit->isEnabled()) {
    // if we enable it, it probably had multiple isbn values
    // the validator doesn't like that, so only keep the first value
    QString val = m_valueLineEdit->text().section(';', 0, 0);
    m_valueLineEdit->setText(val);
  }
  m_editISBN->setEnabled(toggle_);
}

void FetchDialog::slotEditMultipleISBN() {
  KDialogBase dlg(this, "isbn edit dialog", true, i18n("Edit ISBN/UPC Values"),
                  KDialogBase::Ok|KDialogBase::Cancel);
  QVBox* box = new QVBox(&dlg);
  box->setSpacing(10);
  QString s = i18n("<qt>Enter the ISBN or UPC values, one per line.</qt>");
  (void) new QLabel(s, box);
  m_isbnTextEdit = new KTextEdit(box, "isbn text edit");
  m_isbnTextEdit->setText(m_isbnList.join(QChar('\n')));
  QWhatsThis::add(m_isbnTextEdit, s);
  KPushButton* fromFileBtn = new KPushButton(SmallIconSet(QString::fromLatin1("fileopen")),
                                             i18n("&Load From File..."), box);
  QWhatsThis::add(fromFileBtn, i18n("<qt>Load the list from a text file.</qt>"));
  connect(fromFileBtn, SIGNAL(clicked()), SLOT(slotLoadISBNList()));
  dlg.setMainWidget(box);
  dlg.setMinimumWidth(QMAX(dlg.minimumWidth(), FETCH_MIN_WIDTH*2/3));

  if(dlg.exec() == QDialog::Accepted) {
    m_isbnList = QStringList::split('\n', m_isbnTextEdit->text());
    const QValidator* val = m_valueLineEdit->validator();
    if(val) {
      for(QStringList::Iterator it = m_isbnList.begin(); it != m_isbnList.end(); ++it) {
        val->fixup(*it);
        if((*it).isEmpty()) {
          it = m_isbnList.remove(it);
          // this is next item, shift backward
          --it;
        }
      }
    }
    if(m_isbnList.count() > 100) {
      Kernel::self()->sorry(i18n("<qt>An ISBN search can contain a maximum of 100 ISBN values. Only the "
                                 "first 100 values in your list will be used.</qt>"), this);
    }
    while(m_isbnList.count() > 100) {
      m_isbnList.pop_back();
    }
    m_valueLineEdit->setText(m_isbnList.join(QString::fromLatin1("; ")));
  }
  m_isbnTextEdit = 0; // gets auto-deleted
}

void FetchDialog::slotLoadISBNList() {
  if(!m_isbnTextEdit) {
    return;
  }
  KURL u = KFileDialog::getOpenURL(QString::null, QString::null, this);
  if(u.isValid()) {
    m_isbnTextEdit->setText(m_isbnTextEdit->text() + FileHandler::readTextFile(u));
    m_isbnTextEdit->moveCursor(QTextEdit::MoveEnd, false);
    m_isbnTextEdit->scrollToBottom();
  }
}

void FetchDialog::slotUPC2ISBN() {
  int key = m_keyCombo->currentData().toInt();
  if(key == Fetch::UPC) {
    m_keyCombo->setCurrentData(Fetch::ISBN);
    slotKeyChanged(m_keyCombo->currentItem());
  }
}

bool FetchDialog::eventFilter(QObject* obj_, QEvent* ev_) {
  if(obj_ == m_listView->viewport() && ev_->type() == QEvent::Resize) {
    adjustColumnWidth();
  }
  return false;
}

void FetchDialog::adjustColumnWidth() {
  if(!m_listView || m_listView->childCount() == 0) {
    return;
  }

  int sum1 = 0;
  int sum2 = 0;
  int w3 = 0;
  for(QListViewItemIterator it(m_listView); it.current(); ++it) {
    sum1 += it.current()->width(m_listView->fontMetrics(), m_listView, 1);
    sum2 += it.current()->width(m_listView->fontMetrics(), m_listView, 2);
    w3 = QMAX(w3, it.current()->width(m_listView->fontMetrics(), m_listView, 3));
  }
  // try to be smart about column width
  // column 0 is fixed, the checkmark icon, give it 20
  const int w0 = 20;
  // column 3 is the source, say max is 25% of viewport
  const int vw = m_listView->visibleWidth();
  w3 = static_cast<int>(QMIN(1.1 * w3, 0.25 * vw));
  // scale averages of col 1 and col 2
  const int avg1 = sum1 / m_listView->childCount();
  const int avg2 = sum2 / m_listView->childCount();
  const int w2 = (vw - w0 - w3) * avg2 / (avg1 + avg2);
  const int w1 = vw - w0 - w3 - w2;
  m_listView->setColumnWidth(1, w1);
  m_listView->setColumnWidth(2, w2);
  m_listView->setColumnWidth(3, w3);
}

void FetchDialog::slotResetCollection() {
  if(m_collType == Kernel::self()->collectionType()) {
    return;
  }
  m_collType = Kernel::self()->collectionType();
  m_sourceCombo->clear();
  Fetch::FetcherVec sources = Fetch::Manager::self()->fetchers(m_collType);
  for(Fetch::FetcherVec::Iterator it = sources.begin(); it != sources.end(); ++it) {
    m_sourceCombo->insertItem(Fetch::Manager::self()->fetcherIcon(it.data()), (*it).source());
  }

  m_addButton->setIconSet(UserIconSet(Kernel::self()->collectionTypeName()));

  if(Fetch::Manager::self()->canFetch()) {
    m_searchButton->setEnabled(true);
  } else {
    m_searchButton->setEnabled(false);
    Kernel::self()->sorry(i18n("No Internet sources are available for your current collection type."), this);
  }
}

void FetchDialog::slotBarcodeRecognized( QString string )
{
  // attention: this slot is called in the context of another thread => do not use GUI-functions!
  QCustomEvent *e = new QCustomEvent( QEvent::User );
  QString *data = new QString( string );
  e->setData( data );
  qApp->postEvent( this, e ); // the event loop will call FetchDialog::customEvent() in the context of the GUI thread
}

void FetchDialog::slotBarcodeGotImage( QImage &img )
{
  // attention: this slot is called in the context of another thread => do not use GUI-functions!
  QCustomEvent *e = new QCustomEvent( QEvent::User+1 );
  QImage *data = new QImage( img.copy() );
  e->setData( data );
  qApp->postEvent( this, e ); // the event loop will call FetchDialog::customEvent() in the context of the GUI thread
}

void FetchDialog::customEvent( QCustomEvent *e )
{
  if (!e)
    return;
  if ((e->type() == QEvent::User) && e->data()) {
    // slotBarcodeRecognized() queued call
    QString temp = *(QString*)(e->data());
    delete (QString*)(e->data());
    qApp->beep();
    m_valueLineEdit->setText( temp );
    m_searchButton->animateClick();
  }
  if ((e->type() == QEvent::User+1) && e->data()) {
    // slotBarcodegotImage() queued call
    QImage temp = *(QImage*)(e->data());
    delete (QImage*)(e->data());
    m_barcodePreview->setPixmap( temp );
  }
}

#include "fetchdialog.moc"
