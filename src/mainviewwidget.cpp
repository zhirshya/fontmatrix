/***************************************************************************
 *   Copyright (C) 2007 by Pierre Marchand   *
 *   pierre@oep-h.com   *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/
#include "mainviewwidget.h"
#include "fmactivate.h"
#include "fmbaseshaper.h"
#include "fmglyphhighlight.h"
#include "fmglyphsview.h"
#include "fmlayout.h"
#include "fmotf.h"
#include "fmpaths.h"
#include "fmpreviewlist.h"
#include "fontitem.h"
#include "listdockwidget.h"
#include "opentypetags.h"
#include "panosematch.h"
#include "systray.h"
#include "typotek.h"
#include "fmfontdb.h"
#include "fmfontstrings.h"
#include "tagswidget.h"

#include <cstdlib>

#include <QString>
#include <QCompleter>
#include <QDebug>
#include <QGraphicsItemAnimation>
#include <QGraphicsItem>
#include <QTransform>
#include <QDialog>
#include <QGridLayout>
#include <QGraphicsRectItem>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QScrollBar>
#include <QGraphicsRectItem>
#include <QProcess>
#include <QProgressDialog>
#include <QMenu>
#include <QMessageBox>
#include <QSettings>
#include <QStringListModel>
#include <QTime>
#include <QTimeLine>
#include <QClipboard>
#include <QMutex>
#include <QDesktopWidget>
#include <QButtonGroup>
#include <QInputDialog>

// #include <QTimeLine>
// #include <QGraphicsItemAnimation>


MainViewWidget::MainViewWidget ( QWidget *parent )
		: QWidget ( parent )
{
	setupUi ( this );
	
	uRangeIsNotEmpty = false;
	currentFonts.clear();

	QSettings settings;
	sampleFontSize = settings.value("Sample/FontSize",12.0).toDouble();
	sampleInterSize = settings.value("Sample/Interline",16.0).toDouble();
	sampleRatio = sampleInterSize / sampleFontSize  ;
	liveFontSizeSpin->setValue(sampleFontSize);

	iconPS1 =  QIcon(":/icon-PS1");
	iconTTF =  QIcon(":/icon-TTF");
	iconOTF =  QIcon(":/icon-OTF");

	textLayout = FMLayout::getLayout();
	
	radioRenderGroup = new QButtonGroup();
	radioRenderGroup->addButton(freetypeRadio);
	radioRenderGroup->addButton(nativeRadio);
	stackedTools->setCurrentIndex(VIEW_PAGE_SETTINGS);
	splitter_2->restoreState(settings.value("SplitterViewState").toByteArray());
	toolPanelWidth = splitter_2->sizes().at(1);
	if(toolPanelWidth == 0)
	{
		settingsButton->setChecked(false);
		stackedTools->hide();
		toolPanelWidth = splitter_2->width()/3;
	}
	radioFTHintingGroup = new QButtonGroup(freetypeRadio);
	radioFTHintingGroup->addButton(noHinting);
	radioFTHintingGroup->addButton(lightHinting);
	radioFTHintingGroup->addButton(normalHinting);
	
	theVeryFont = 0;
	typo = typotek::getInstance();
	m_lists = ListDockWidget::getInstance();
// 	currentFonts = typo->getAllFonts();
	currentFonts = FMFontDb::DB()->AllFonts();
	fontsetHasChanged = true;
	curGlyph = 0;
	fancyGlyphInUse = -1;


	fillUniPlanes();
	refillSampleList();
	uniLine->setVisible(false);

	fontInfoText->page()->setLinkDelegationPolicy(QWebPage::DelegateExternalLinks);

	abcScene = new QGraphicsScene;
	loremScene = new QGraphicsScene;
	ftScene =  new QGraphicsScene;
	playScene = new QGraphicsScene;
	QRectF pageRect ( 0,0,597.6,842.4 ); //TODO find means to smartly decide of page size (here, iso A4)
	
	loremScene->setSceneRect ( pageRect );
// 	QGraphicsRectItem *backp = loremScene->addRect ( pageRect,QPen(),Qt::white );
// 	backp->setEnabled ( false );
	
	ftScene->setSceneRect ( 0,0, 597.6 * ( double ) QApplication::desktop()->physicalDpiX() / 72.0, 842.4 * ( double ) QApplication::desktop()->physicalDpiX() / 72.0);
	

	abcView->setScene ( abcScene );
	abcView->setRenderHint ( QPainter::Antialiasing, true );

	loremView->setScene ( loremScene );
	loremView->locker = false;
	double horiScaleT (( double ) QApplication::desktop()->physicalDpiX() / 72.0);
	double vertScaleT ( ( double ) QApplication::desktop()->physicalDpiY() / 72.0);
	QTransform adjustAbsoluteViewT( horiScaleT , 0, 0,vertScaleT, 0, 0 );
	loremView->setTransform ( adjustAbsoluteViewT , false );

	loremView_FT->setScene ( ftScene );
	loremView_FT->locker = false;
	loremView_FT->fakePage();

	playScene->setSceneRect ( 0,0,10000,10000 );
	playView->setScene( playScene );

	sampleText= typo->namedSample (typo->defaultSampleName());
	
	
	QMap<QString, int> sTypes(FMShaperFactory::types());
	for(QMap<QString, int>::iterator sIt = sTypes.begin(); sIt != sTypes.end() ; ++sIt)
	{
		shaperTypeCombo->addItem(sIt.key(), sIt.value());
	}
	
	classSplitter->restoreState(settings.value("WState/ClassificationSplitter").toByteArray());
	slotShowClassification();
	currentOrdering = "family" ;
	
	QStringListModel* cslModel(new QStringListModel);
	QCompleter* cslCompleter(new QCompleter(charSearchLine));
	cslCompleter->setModel(cslModel);
	charSearchLine->setCompleter(cslCompleter);
	
	doConnect();
	


}


MainViewWidget::~MainViewWidget()
{
}


void MainViewWidget::doConnect()
{
	connect ( m_lists->fontTree,SIGNAL ( itemClicked ( QTreeWidgetItem*, int ) ),this,SLOT ( slotFontSelected ( QTreeWidgetItem*, int ) ) );
	connect ( m_lists->fontTree,SIGNAL ( currentChanged (QTreeWidgetItem*, int ) ), this,SLOT (slotFontSelected ( QTreeWidgetItem*, int ) ) );
	connect ( m_lists->searchString,SIGNAL ( returnPressed() ),this,SLOT ( slotSearch() ) );
	connect ( m_lists->viewAllButton,SIGNAL ( released() ),this,SLOT ( slotViewAll() ) );
	connect ( m_lists->fontTree,SIGNAL ( itemExpanded ( QTreeWidgetItem* ) ),this,SLOT ( slotItemOpened ( QTreeWidgetItem* ) ) );
	connect ( m_lists->tagsCombo,SIGNAL ( activated ( const QString& ) ),this,SLOT ( slotFilterTag ( QString ) ) );
	connect ( m_lists, SIGNAL(folderSelectFont(const QString&)), this, SLOT(slotSelectFromFolders(const QString&)));
	connect ( this, SIGNAL(listChanged()), m_lists, SLOT(slotPreviewUpdate()));
	
	connect ( this, SIGNAL(listChanged()), typo, SLOT(showToltalFilteredFonts()));
	
	connect( fontInfoText, SIGNAL(linkClicked ( const QUrl& )), this, SLOT(slotWebLink(const QUrl&)));
	connect( fontInfoText, SIGNAL(loadStarted () ),this,SLOT(slotWebStart()));
	connect( fontInfoText, SIGNAL(loadProgress ( int )  ),this, SLOT(slotWebLoad(int)));
	connect( fontInfoText, SIGNAL(loadFinished ( bool ) ),this,SLOT(slotWebFinished(bool)));

	connect (radioRenderGroup,SIGNAL(buttonClicked( QAbstractButton* )),this,SLOT(slotChangeViewPage(QAbstractButton*)));
	connect (radioFTHintingGroup, SIGNAL(buttonClicked(int)),this,SLOT(slotHintChanged(int)));
	
	connect (openTypeButton,SIGNAL(clicked( bool )),this,SLOT(slotChangeViewPageSetting( bool )));
	connect (settingsButton,SIGNAL(clicked( bool )),this,SLOT(slotChangeViewPageSetting( bool )));
	
	connect ( abcView,SIGNAL ( pleaseShowSelected() ),this,SLOT ( slotShowOneGlyph() ) );
	connect ( abcView,SIGNAL ( pleaseShowAll() ),this,SLOT ( slotShowAllGlyph() ) );
	connect ( abcView,SIGNAL ( refit ( int ) ),this,SLOT ( slotAdjustGlyphView ( int ) ) );
	connect ( abcView, SIGNAL(pleaseUpdateMe()), this, SLOT(slotUpdateGView()));
	connect ( abcView, SIGNAL(pleaseUpdateSingle()), this, SLOT(slotUpdateGViewSingle()));
	connect ( uniPlaneCombo,SIGNAL ( activated ( int ) ),this,SLOT ( slotPlaneSelected ( int ) ) );
	connect ( clipboardCheck, SIGNAL (toggled ( bool )),this,SLOT(slotShowULine(bool)));
	connect ( charSearchLine, SIGNAL(returnPressed()), this, SLOT(slotSearchCharName()));

	connect ( loremView, SIGNAL(pleaseUpdateMe()), this, SLOT(slotUpdateSView()));
	connect ( loremView, SIGNAL(pleaseZoom(int)),this,SLOT(slotZoom(int)));
	

	connect ( loremView_FT, SIGNAL(pleaseZoom(int)),this,SLOT(slotZoom(int)));
	connect ( loremView_FT, SIGNAL(pleaseUpdateMe()), this, SLOT(slotUpdateRView()));
	
	connect ( textLayout, SIGNAL(updateLayout()),this, SLOT(slotView()));
	connect ( this, SIGNAL(stopLayout()), textLayout,SLOT(stopLayout()));

	connect ( playView, SIGNAL(pleaseZoom(int)),this,SLOT(slotZoom(int)));

	
	connect ( sampleTextCombo,SIGNAL ( activated ( int ) ),this,SLOT ( slotSampleChanged() ) );
	connect ( sampleTextButton, SIGNAL(released()),this, SLOT(slotEditSampleText()));
	connect ( liveFontSizeSpin, SIGNAL( editingFinished() ),this,SLOT(slotLiveFontSize()));

	connect ( OpenTypeTree, SIGNAL ( itemClicked ( QTreeWidgetItem*, int ) ), this, SLOT ( slotFeatureChanged() ) );
	connect ( saveDefOTFBut, SIGNAL(released()),this,SLOT(slotDefaultOTF()));
	connect ( resetDefOTFBut, SIGNAL(released()),this,SLOT(slotResetOTF()));
	connect ( shaperTypeCombo,SIGNAL ( activated ( int ) ),this,SLOT ( slotChangeScript() ) );
	connect ( langCombo,SIGNAL ( activated ( int ) ),this,SLOT ( slotChangeScript() ) );
	
	connect ( textProgression, SIGNAL ( stateChanged (  ) ),this ,SLOT(slotProgressionChanged()));
	connect ( useShaperCheck,SIGNAL ( stateChanged ( int ) ),this,SLOT ( slotWantShape() ) );

	connect ( pushToPlayButton, SIGNAL(clicked ( bool ) ), this, SLOT(slotPushOnPlayground()) );
	
	connect ( classificationView, SIGNAL(selectedField(const QString&)), this, SLOT(slotUpdateClassDescription(const QString&)) );
	connect ( classificationView, SIGNAL(filterChanged()), this, SLOT(slotPanoseFilter()));
	connect ( classSplitter, SIGNAL(splitterMoved(int,int)), this, SLOT(slotSaveClassSplitter()));
}

void MainViewWidget::disConnect()
{
	disconnect ( m_lists->fontTree,SIGNAL ( itemClicked ( QTreeWidgetItem*, int ) ),this,SLOT ( slotFontSelected ( QTreeWidgetItem*, int ) ) );
	disconnect ( m_lists->fontTree,SIGNAL ( currentChanged (QTreeWidgetItem*, int ) ), this,SLOT (slotFontSelected ( QTreeWidgetItem*, int ) ) );
	disconnect ( m_lists->searchString,SIGNAL ( returnPressed() ),this,SLOT ( slotSearch() ) );
	disconnect ( m_lists->viewAllButton,SIGNAL ( released() ),this,SLOT ( slotViewAll() ) );
	disconnect ( m_lists->fontTree,SIGNAL ( itemExpanded ( QTreeWidgetItem* ) ),this,SLOT ( slotItemOpened ( QTreeWidgetItem* ) ) );
	disconnect ( m_lists->tagsCombo,SIGNAL ( activated ( const QString& ) ),this,SLOT ( slotFilterTag ( QString ) ) );
	disconnect ( m_lists, SIGNAL(folderSelectFont(const QString&)), this, SLOT(slotSelectFromFolders(const QString&)));
	disconnect ( this, SIGNAL(listChanged()), m_lists, SLOT(slotPreviewUpdate()));
	
	disconnect ( this, SIGNAL(listChanged()), typo, SLOT(showToltalFilteredFonts()));
	
	disconnect( fontInfoText, SIGNAL(linkClicked ( const QUrl& )), this, SLOT(slotWebLink(const QUrl&)));
	disconnect( fontInfoText, SIGNAL(loadStarted () ),this,SLOT(slotWebStart()));
	disconnect( fontInfoText, SIGNAL(loadProgress ( int )  ),this, SLOT(slotWebLoad(int)));
	disconnect( fontInfoText, SIGNAL(loadFinished ( bool ) ),this,SLOT(slotWebFinished(bool)));

	disconnect (radioRenderGroup,SIGNAL(buttonClicked( QAbstractButton* )),this,SLOT(slotChangeViewPage(QAbstractButton*)));
	disconnect (radioFTHintingGroup, SIGNAL(buttonClicked(int)),this,SLOT(slotHintChanged(int)));
	
	disconnect (openTypeButton,SIGNAL(clicked( bool )),this,SLOT(slotChangeViewPageSetting( bool )));
	disconnect (settingsButton,SIGNAL(clicked( bool )),this,SLOT(slotChangeViewPageSetting( bool )));
	
	disconnect ( abcView,SIGNAL ( pleaseShowSelected() ),this,SLOT ( slotShowOneGlyph() ) );
	disconnect ( abcView,SIGNAL ( pleaseShowAll() ),this,SLOT ( slotShowAllGlyph() ) );
	disconnect ( abcView,SIGNAL ( refit ( int ) ),this,SLOT ( slotAdjustGlyphView ( int ) ) );
	disconnect ( abcView, SIGNAL(pleaseUpdateMe()), this, SLOT(slotUpdateGView()));
	disconnect ( abcView, SIGNAL(pleaseUpdateSingle()), this, SLOT(slotUpdateGViewSingle()));
	disconnect ( uniPlaneCombo,SIGNAL ( activated ( int ) ),this,SLOT ( slotPlaneSelected ( int ) ) );
	disconnect ( clipboardCheck, SIGNAL (toggled ( bool )),this,SLOT(slotShowULine(bool)));
	disconnect ( charSearchLine, SIGNAL(returnPressed()), this, SLOT(slotSearchCharName()));

	disconnect ( loremView, SIGNAL(pleaseUpdateMe()), this, SLOT(slotUpdateSView()));
	disconnect ( loremView, SIGNAL(pleaseZoom(int)),this,SLOT(slotZoom(int)));
	

	disconnect ( loremView_FT, SIGNAL(pleaseZoom(int)),this,SLOT(slotZoom(int)));
	disconnect ( loremView_FT, SIGNAL(pleaseUpdateMe()), this, SLOT(slotUpdateRView()));
	
	disconnect ( textLayout, SIGNAL(updateLayout()),this, SLOT(slotView()));
	disconnect ( this, SIGNAL(stopLayout()), textLayout,SLOT(stopLayout()));

	disconnect ( playView, SIGNAL(pleaseZoom(int)),this,SLOT(slotZoom(int)));

	
	disconnect ( sampleTextCombo,SIGNAL ( activated ( int ) ),this,SLOT ( slotSampleChanged() ) );
	disconnect ( sampleTextButton, SIGNAL(released()),this, SLOT(slotEditSampleText()));
	disconnect ( liveFontSizeSpin, SIGNAL( editingFinished() ),this,SLOT(slotLiveFontSize()));

	disconnect ( OpenTypeTree, SIGNAL ( itemClicked ( QTreeWidgetItem*, int ) ), this, SLOT ( slotFeatureChanged() ) );
	disconnect ( saveDefOTFBut, SIGNAL(released()),this,SLOT(slotDefaultOTF()));
	disconnect ( resetDefOTFBut, SIGNAL(released()),this,SLOT(slotResetOTF()));
	disconnect ( shaperTypeCombo,SIGNAL ( activated ( int ) ),this,SLOT ( slotChangeScript() ) );
	disconnect ( langCombo,SIGNAL ( activated ( int ) ),this,SLOT ( slotChangeScript() ) );
	
	disconnect ( textProgression, SIGNAL ( stateChanged (  ) ),this ,SLOT(slotProgressionChanged()));
	disconnect ( useShaperCheck,SIGNAL ( stateChanged ( int ) ),this,SLOT ( slotWantShape() ) );

	disconnect ( pushToPlayButton, SIGNAL(clicked ( bool ) ), this, SLOT(slotPushOnPlayground()) );
	
	disconnect ( classificationView, SIGNAL(selectedField(const QString&)), this, SLOT(slotUpdateClassDescription(const QString&)) );
	disconnect ( classificationView, SIGNAL(filterChanged()), this, SLOT(slotPanoseFilter()));
	disconnect ( classSplitter, SIGNAL(splitterMoved(int,int)), this, SLOT(slotSaveClassSplitter()));
}

void MainViewWidget::fillTree()
{
// 	qDebug()<< "MainViewWidget::fillTree("<< curItemName <<")";
// 	QTime fillTime(0, 0, 0, 0);
// 	fillTime.start();
	m_lists->savePosition();
	QTreeWidgetItem *curItem = 0;

	// We log expanded families item
	openKeys.clear();
	for ( int i=0; i < m_lists->fontTree->topLevelItemCount();++i )
	{
		QTreeWidgetItem *topit = m_lists->fontTree->topLevelItem ( i );
		for ( int j=0;j < topit->childCount();++j )
			if ( topit->child ( j )->isExpanded() )
				openKeys << topit->child ( j )->text ( 0 );
	}
	m_lists->fontTree->clear();

// 	qDebug("LOGS Time elapsed: %d ms", fillTime.elapsed());
// 	fillTime.restart();

	// build the in-memory tree that hold the ordered current fonts set
	QMap< QChar,QMap< QString,QMap< QString,FontItem*> > > keyList;
	QMap< QString,QString > realFamilyName;
	orderedCurrentFonts.clear();
	for ( int i=0; i < currentFonts.count();++i )
	{
		QString family = currentFonts[i]->family();
		QString ordFamily = family.toUpper();
		QString variant = currentFonts[i]->variant();
		if( keyList.contains(ordFamily[0]) && keyList[ordFamily[0]].contains(ordFamily) && keyList[ordFamily[0]][ordFamily].contains(variant) )
		{
			int unique = 2;
			QString uniString(variant +" -%1");
			while(keyList[ordFamily[0]][ordFamily].contains(uniString.arg(unique,2)))
			{
				++unique;
			}
			variant = uniString.arg(unique,2);
		}
		keyList[ordFamily[0]][ordFamily][variant] = ( currentFonts[i] );
		realFamilyName[ordFamily] = family;
	}

// 	qDebug("MULTI Time elapsed: %d ms", fillTime.elapsed());
// 	fillTime.restart();

	// Rebuild the the tree
	QFont alphaFont ( "helvetica",14,QFont::Bold,false );

	QMap< QChar,QMap< QString,QMap< QString,FontItem*> > >::const_iterator kit;
	QMap< QString,QMap< QString,FontItem*> >::const_iterator oit;
	QMap< QString,FontItem*>::const_iterator fit;

// 	QTime tt(0,0);
// 	tt.start();
// 	int tttotal(0);
// 	int tcount(0);
	
	QMap<FontItem*,bool> act ;
	FMFontDb::DB()->TransactionBegin();
	for( kit = keyList.constBegin() ; kit != keyList.constEnd() ; ++kit )
	{
		for (oit = kit.value().constBegin(); oit !=  kit.value().constEnd(); ++ oit )
		{
			for(fit = oit.value().constBegin(); fit != oit.value().constEnd(); ++fit)
			{
				act[fit.value()] = fit.value()->isActivated();
			}
		}
	}
	FMFontDb::DB()->TransactionEnd();
	
	for( kit = keyList.constBegin() ; kit != keyList.constEnd() ; ++kit )
	{
		QChar firstChar ( kit.key() );
		QTreeWidgetItem *alpha = new QTreeWidgetItem ( m_lists->fontTree );
		alpha->setText ( 0, firstChar );
		alpha->setFont ( 0, alphaFont );
		alpha->setData ( 0,100,"alpha" );
		alpha->setBackgroundColor ( 0,Qt::lightGray );
		alpha->setBackgroundColor ( 1,Qt::lightGray );
		bool alphaIsUsed = false;
		for (oit = kit.value().constBegin(); oit !=  kit.value().constEnd(); ++ oit )
		{

			QString fam( realFamilyName[oit.key()] );
			bool isExpanded = false;
			QTreeWidgetItem *ord = new QTreeWidgetItem ( alpha );
			
			ord->setData ( 0,100,"family" );
			ord->setCheckState ( 0,Qt::Unchecked );
			bool chekno = false;
			bool checkyes = false;
			if ( openKeys.contains ( fam ) )
			{
				ord->setExpanded ( true );
				isExpanded = true;
			}

			for(fit = oit.value().constBegin(); fit != oit.value().constEnd(); ++fit)
			{
// 				++tcount;
// 				tt.restart();
				FontItem *fPointer = fit.value();
				orderedCurrentFonts << fPointer;
				QString var( fit.key() );
				if(typo->isSysFont(fit.value()))
					var+="*";
				QTreeWidgetItem *entry = new QTreeWidgetItem ( ord );
				entry->setText ( 0,  var );
// 				entry->setText ( 1, fPointer->path() );
				entry->setToolTip( 0, fPointer->path() );
				entry->setData ( 0, 100, "fontfile" );
				if(/*fPointer->isLocked() ||*/ fPointer->isRemote() )
				{
					entry->setFlags(Qt::ItemIsSelectable);
				}

				if(fPointer->type() == "CFF")
						entry->setIcon(0, iconOTF );
				else if(fPointer->type() == "TrueType")
						entry->setIcon(0, iconTTF);
				else if(fPointer->type() == "Type 1")
						entry->setIcon(0, iconPS1);

				
				if ( act[fPointer] )
				{
					checkyes = true;
				}
				else
				{
					chekno = true;
				}
				entry->setCheckState ( 0 , act[fPointer] ?  Qt::Checked : Qt::Unchecked );
				entry->setData ( 0,200, entry->checkState ( 0 ) );

				if ( entry->toolTip( 0 ) == curItemName )
					curItem = entry;
// 				tttotal += tt.elapsed();
			}

			if ( checkyes && chekno )
				ord->setCheckState ( 0,Qt::PartiallyChecked );
			else if ( checkyes )
				ord->setCheckState ( 0,Qt::Checked );
			// track checkState
			ord->setData ( 0,200,ord->checkState ( 0 ) );
			ord->setText ( 0, fam + "  ["+ QString::number ( ord->childCount() ) +"]");
// 			ord->setText ( 1, QString::number ( ord->childCount() ) );
			alphaIsUsed = true;

		}
		if ( alphaIsUsed )
		{
			m_lists->fontTree->addTopLevelItem ( alpha );
			alpha->setExpanded ( true );
		}
		else
		{
			delete alpha;
		}
	}
// 	qDebug()<<"SUB TREE Time Total: "<<tttotal<<" ms; Iterations : "  << tcount<< "; Average" << (double)tttotal / (double)tcount <<" ms/It";
// 	qDebug("TREE Time elapsed: %d ms", fillTime.elapsed());
// 	fillTime.restart();

// 	m_lists->previewList->slotRefill ( currentFonts, fontsetHasChanged );
	if ( curItem )
	{
// 		qDebug() << "get curitem : " << curItem->text ( 0 ) << curItem->text ( 1 );
		m_lists->restorePosition();
		if( !m_lists->nameItemIsVisible(curItem) )
		{
			m_lists->fontTree->scrollToItem ( curItem, QAbstractItemView::PositionAtCenter );
		}

// // 		QColor scol (255,240,221,255);
// // 		QColor pcol (255,211,155,255);
// 		QColor scol (QApplication::palette().highlight().color());
// 		QColor pcol (scol);
// 		QFont selFont;
// 		selFont.setBold(true);
// 		curItem->parent()->setBackgroundColor ( 0,pcol );
// 		curItem->parent()->setBackgroundColor ( 1,pcol );
// 		curItem->parent()->setFont(0, selFont);
// 		curItem->setBackgroundColor ( 0,scol );
// // 		curItem->setBackgroundColor ( 1,scol );
// 		curItem->setFont(0,selFont);
		QFont selFont;
		selFont.setBold ( true );
		curItem->parent()->setFont ( 0, selFont );
		curItem->setSelected(true);
	}
	else
	{
		qDebug() << "NO CURITEM";
	}
	m_lists->fontTree->resizeColumnToContents ( 0 )  ;
// 	m_lists->fontTree->resizeColumnToContents ( 1 ) ;
// 	m_lists->fontTree->setColumnWidth(0,200);

	fontsetHasChanged = false;
	listChanged();
// 	m_lists->slotPreviewUpdate();
// 	qDebug("END Time elapsed: %d ms", fillTime.elapsed());
}

void MainViewWidget::updateTree ( bool checkFontActive )
{
	QTreeWidgetItem *curItem = 0;
	QFont deselect;
	int topCount ( m_lists->fontTree->topLevelItemCount() );
	for ( int topIdx ( 0 ) ; topIdx < topCount; ++topIdx )
	{
		QTreeWidgetItem *topItem ( m_lists->fontTree->topLevelItem ( topIdx ) );
		int famCount ( topItem->childCount() );
		for ( int famIdx ( 0 ); famIdx < famCount; ++ famIdx )
		{
			QTreeWidgetItem *famItem ( topItem->child ( famIdx ) );
			int varCount ( famItem->childCount() );
			for ( int varIdx ( 0 ); varIdx < varCount; ++ varIdx )
			{
				QTreeWidgetItem *varItem ( famItem->child ( varIdx ) );
				if ( varItem->toolTip ( 0 ) == lastIndex )
				{
					varItem->setFont ( 0, deselect );
					varItem->setBackgroundColor ( 0, Qt::transparent );
					varItem->setBackgroundColor ( 1, Qt::transparent );
					varItem->parent()->setFont ( 0, deselect );
					varItem->parent()->setBackgroundColor ( 0, Qt::transparent );
					varItem->parent()->setBackgroundColor ( 1, Qt::transparent );
					varItem->setSelected(false);
				}
				else if ( varItem->toolTip ( 0 ) == curItemName )
				{
					curItem = varItem;
				}
			}
		}
	}
	
	// Check if active
	if ( checkFontActive )
	{
		for ( int topIdx ( 0 ) ; topIdx < topCount; ++topIdx )
		{
			QTreeWidgetItem *topItem ( m_lists->fontTree->topLevelItem ( topIdx ) );
			int famCount ( topItem->childCount() );
			for ( int famIdx ( 0 ); famIdx < famCount; ++ famIdx )
			{
				QTreeWidgetItem *famItem ( topItem->child ( famIdx ) );
				int varCount ( famItem->childCount() );
				if ( famItem->isExpanded() )
				{
					for ( int varIdx ( 0 ); varIdx < varCount; ++ varIdx )
					{
						QTreeWidgetItem *varItem ( famItem->child ( varIdx ) );
						// Check if active

						QString s ( varItem->toolTip ( 0 ) );
						FontItem* f ( FMFontDb::DB()->Font ( s ) );
						bool isActive ( f->isActivated() );
						if ( isActive )
						{
							varItem->setCheckState ( 0,Qt::Checked );
						}
						else
						{
							varItem->setCheckState ( 0,Qt::Unchecked );
						}
					}
				}
			}
		}
	}
// 	m_lists->previewList->slotRefill ( currentFonts, false );
	if ( curItem )
	{
		QFont selFont;
		selFont.setBold ( true );
		curItem->parent()->setFont ( 0, selFont );
		curItem->setSelected(true);
	}
	else
	{
		qDebug() << "NO CURITEM";
	}
	if ( !m_lists->nameItemIsVisible ( curItem ) )
	{
		m_lists->fontTree->scrollToItem ( curItem, QAbstractItemView::PositionAtCenter );
	}
	fontsetHasChanged = false;
}

void MainViewWidget::slotItemOpened ( QTreeWidgetItem * item )
{
// 	if(item->data(0,100).toString() == "family")
// 	{
// 		slotFontSelected (item, 0);
// 	}
//
}


void MainViewWidget::slotOrderingChanged ( QString s )
{
	//Update "m_lists->fontTree"


// 	currentFonts = typo->getAllFonts();
	currentOrdering = s;
	fillTree();

}

/// Should be renamed in slotNameItemSelected
void MainViewWidget::slotFontSelected ( QTreeWidgetItem * item, int column )
{
// 	qDebug() << "font select";
	if ( item->data ( 0,100 ).toString() == "alpha" )
	{
// 		qDebug() << "Item is an alpha";
		return;
// 		fillTree();
	}

	if ( item->data ( 0,100 ).toString() == "family" )
	{
// 		qDebug() << "Item is a family";
		item->setExpanded(true);
		bool wantView = true;
		bool hasChild = false;
		QStringList names;
		QMap<QString, QString> variantMap;
		for ( int i=0; i < item->childCount(); ++i )
		{
			hasChild = true;
			if ( item->child( i )->toolTip(0) == curItemName )
				wantView = false;
			names << item->child( i )->toolTip( 0 ) ;
			variantMap[item->child ( i )->text ( 0 ) ] = item->child ( i )->toolTip(0) ;
		}
		slotFontActionByNames ( names );
		int oldc = item->data ( 0,200 ).toInt();
		if ( oldc == item->checkState ( 0 ) ) // filters when checkbox has not been hit
		{
			// TODO keep an eye on it
// 			fillTree();

		}
		else if ( item->checkState ( 0 ) != Qt::PartiallyChecked )
		{

			bool cs = item->checkState ( 0 ) == Qt::Checked ? true : false;
			item->setData( 0,200, item->checkState ( 0 ) );
			QList<FontItem*> todo;
			for ( int i=0; i<item->childCount(); ++i )
			{
				todo << FMFontDb::DB()->Font( item->child ( i )->toolTip(0) );
			}
// 			for (int fIndex(0);fIndex < todo.count(); ++fIndex)
// 			{
// 				FontItem* afont = todo[fIndex];
// 				if(fIndex == todo.count() - 1)
// 					activation ( afont, cs , true);
// 				else
// 					activation ( afont, cs , false);
// 			}
			activation ( todo, cs );

		}
		else
		{
			qDebug() << "Something wrong, Qt::PartiallyChecked should not be reached" ;

		}
		if ( wantView && hasChild )
		{
			QString select;

			if ( variantMap.contains ( "Regular" ) )
				select =  variantMap["Regular"];
			else if ( variantMap.contains ( "Roman" ) )
				select =  variantMap["Roman"];
			else if ( variantMap.contains ( "Medium" ) )
				select =  variantMap["Medium"];
			else if ( variantMap.contains ( "Book" ) )
				select =  variantMap["Book"];
			else
				select =  * ( variantMap.begin() );

// 			m_lists->previewList->slotSelect(select);
			slotFontSelectedByName(select);

		}
		return;
	}

	if ( item->data ( 0,100 ).toString() == "fontfile" )
	{
		QString fontname(item->toolTip(0));
		bool wantActivate = (item->checkState(0) == Qt::Checked) ? true : false;
// 		m_lists->previewList->slotSelect(fontname);
		slotFontSelectedByName(fontname);
// 		if ( !theVeryFont->isLocked() )
		{
			if(theVeryFont->isActivated())
			{
				if(!wantActivate)
				{
					activation(theVeryFont,false,true);
				}
			}
			else
			{
				if(wantActivate)
				{
					activation(theVeryFont,true,true);
				}
			}
		}
	}
	return;

}

void MainViewWidget::slotFontSelectedByName ( QString fname )
{
// 	qDebug() << "MainViewWidget::slotFontSelectedByName("<<fname<<")";
	if ( fname.isEmpty() || fname ==  faceIndex )
		return;
	lastIndex = faceIndex;
	faceIndex = fname;
	curItemName = faceIndex;

	if ( faceIndex.count() && faceIndex != lastIndex )
	{
// 		qDebug() << "Font has changed \n\tOLD : "<<lastIndex<<"\n\tNEW : " << faceIndex ;
		if(abcView->state() == FMGlyphsView::SingleView)
			slotShowAllGlyph();
		theVeryFont = FMFontDb::DB()->Font( faceIndex );
// 		theVeryFont->updateItem();
		slotFontActionByName ( fname );
		if(theVeryFont->isRemote())
		{
			qDebug() << faceIndex <<" is remote";
			if(!theVeryFont->isCached())
			{
				connect(theVeryFont,SIGNAL(dowloadFinished()), this, SLOT(slotRemoteFinished()));
				theVeryFont->getFromNetwork();
				currentDownload = faceIndex ;
				faceIndex = lastIndex;
				return;
			}
			else
			{
				currentDownload = "";
			}
		}
		fillOTTree();
		fillUniPlanesCombo ( theVeryFont );
		QStringListModel *m = reinterpret_cast<QStringListModel*>(charSearchLine->completer()->model());
		if(m) 
			m->setStringList(theVeryFont->getNames());
		slotView ( true );
		typo->setWindowTitle ( theVeryFont->fancyName() + " - Fontmatrix" );
		m_lists->fontTree->headerItem()->setText(0, tr("Names")+" ("+theVeryFont->family()+")");
		typo->presentFontName ( theVeryFont->fancyName() );
// 		fillTree();
		updateTree();
		m_lists->listPreview->setCurrentFont(theVeryFont->path());
		abcView->verticalScrollBar()->setValue ( 0 );
		
		// Update panose widget
		QStringList plist(FMFontDb::DB()->getValue(theVeryFont->path(), FMFontDb::Panose).toString().split(":"));
		if(plist.count() == 10)
		{
			QMap<QString,QStringList> filter;
			FontStrings::PanoseKey pk(FontStrings::firstPanoseKey());
			do
			{
				QString v(FontStrings::Panose().value(pk).value(plist[pk].toInt()));
				filter[FontStrings::PanoseKeyName(pk)] << v;
// 				qDebug()<<"F"<<FontStrings::PanoseKeyName(pk)<<v;
				pk = FontStrings::nextPanoseKey(pk);
			}
			while(pk != FontStrings::InvalidPK);
			disconnect ( classificationView, SIGNAL(filterChanged()), this, SLOT(slotPanoseFilter()));
			classificationView->setFilter(filter);
			connect ( classificationView, SIGNAL(filterChanged()), this, SLOT(slotPanoseFilter()));
		}
	}


}


void MainViewWidget::slotInfoFont()
{
	if(theVeryFont)
	{
// 		qDebug()<<"MainViewWidget::slotInfoFont"<<theVeryFont->name();
                QString fIT;
                fIT += "<html xmlns=\"http://www.w3.org/1999/xhtml\">";
		fIT += "<head>";
		fIT += "<title>" + theVeryFont->fancyName() + "</title>";
		fIT += "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\" />";
		fIT += "<link rel='stylesheet' href='file://" + FMPaths::ResourcesDir() + "info.css' type='text/css' />";
                fIT += "</head ><body>" +  theVeryFont->infoText(false) + "</body> </html>";
//		fontInfoText->setHtml (fIT);
                fontInfoText->setContent(fIT.toUtf8(), "application/xhtml+xml");
//                qDebug()<<"=========================================================";
//                qDebug()<<fIT;
//                qDebug()<<"=========================================================";
//                 QFile df("fontmatrix.xhtml" );
//                 if(df.open(QIODevice::WriteOnly | QIODevice::Truncate))
//                 {
//                     df.write(fIT.toUtf8());
//                 }
	}
	

}

void MainViewWidget::slotView ( bool needDeRendering )
{
	QTime t;
	t.start();
	FontItem *l = FMFontDb::DB()->Font( lastIndex );
	FontItem *f = FMFontDb::DB()->Font( faceIndex );
	if ( !f )
		return;
	if ( needDeRendering )
	{
		if ( l )
		{
			l->deRenderAll();
		}
		f->deRenderAll();

		curGlyph = 0;
	}

	bool wantDeviceDependant = loremView_FT->isVisible();
	unsigned int storedHinting(theVeryFont->getFTHintMode());
	if(wantDeviceDependant)
	{
		theVeryFont->setFTHintMode(hinting());
	}

	if(textProgression->inLine() == TextProgression::INLINE_LTR )
		theVeryFont->setProgression(PROGRESSION_LTR );
	else if(textProgression->inLine() == TextProgression::INLINE_RTL )
		theVeryFont->setProgression(PROGRESSION_RTL);
	else if(textProgression->inLine() == TextProgression::INLINE_TTB )
		theVeryFont->setProgression(PROGRESSION_TTB );
	else if(textProgression->inLine() == TextProgression::INLINE_BTT )
		theVeryFont->setProgression(PROGRESSION_BTT);

	theVeryFont->setFTRaster ( wantDeviceDependant );
	theVeryFont->setShaperType(shaperTypeCombo->itemData(  shaperTypeCombo->currentIndex() ).toInt() );

	QString pkey = uniPlaneCombo->itemData ( uniPlaneCombo->currentIndex() ).toString();
	QPair<int,int> uniPair ( uniPlanes[pkey + uniPlaneCombo->currentText() ] );
	int coverage = theVeryFont->countCoverage ( uniPair.first, uniPair.second );
	int interval = uniPair.second - uniPair.first;
	coverage = coverage * 100 / ( interval + 1 );// against /0 exception
	unicodeCoverageStat->setText ( QString::number ( coverage ) + "\%" );

	if ( abcView->isVisible() )
	{
		if(uRangeIsNotEmpty)
			f->renderAll ( abcScene , uniPair.first, uniPair.second );
		else
		{
			// TODO something useful, if possible
		}
	}

	if ( loremView->isVisible() || loremView_FT->isVisible() )
	{
//		qDebug()<<"lv(ft) is visible";
		if(textLayout->isRunning())
		{
//			qDebug()<<"tl is running";
			textLayout->stopLayout();
		}
		else
		{
//			qDebug()<<"tl is NOT running";
			QGraphicsScene *targetScene;
			loremView_FT->unSheduleUpdate();
			loremView->unSheduleUpdate();
			if(loremView->isVisible())
			{
				targetScene = loremScene;
			}
			else if(loremView_FT->isVisible())
			{
				targetScene = ftScene;
			}
			
			bool processFeatures = f->isOpenType() &&  !deFillOTTree().isEmpty();
			QString script = langCombo->currentText();
                        bool processScript =  f->isOpenType() && ( useShaperCheck->checkState() == Qt::Checked ) && ( !script.isEmpty() );

			textLayout->setTheFont(theVeryFont);
			textLayout->setDeviceIndy(!wantDeviceDependant);
			textLayout->setTheScene(targetScene);
			textLayout->setAdjustedSampleInter( sampleInterSize );
			
			double fSize(sampleFontSize);
			
			QList<GlyphList> list;
			QStringList stl( typo->namedSample(sampleTextCombo->currentText() ).split("\n"));
			if ( processScript )
			{
				for(int p(0);p<stl.count();++p)
					list << theVeryFont->glyphs( stl[p] , fSize, script );
			}
			else if(processFeatures)
			{
				for(int p(0);p<stl.count();++p)	
					list << theVeryFont->glyphs( stl[p] , fSize, deFillOTTree());
			}
			else
			{
				for(int p(0);p<stl.count();++p)
					list << theVeryFont->glyphs( stl[p] , fSize  );
			}
			textLayout->doLayout(list, fSize);
// 			if (loremView->isVisible() /*&& fitViewCheck->isChecked()*/ )
// 			{
// 				loremView->fitInView ( textLayout->getRect(), Qt::KeepAspectRatio );
// 			}
			textLayout->start(QThread::LowestPriority);
		}
	}
	else if(!loremView->isVisible() && !loremView_FT->isVisible())
	{
		loremView->sheduleUpdate();
		loremView_FT->sheduleUpdate();
	}
// 	qDebug("VIEW Time elapsed: %d ms", t.elapsed());
// 	t.restart();
	slotInfoFont();
// 	qDebug("INFO Time elapsed: %d ms", t.elapsed());
// 	renderingLock = false;

}


void MainViewWidget::slotSearch()
{
// 	qDebug()<<"slotSearch";
	m_lists->fontTree->clear();
// 	m_lists->previewList->slotRefill(QList<FontItem*>(), true);
	fontsetHasChanged = true;

	QApplication::setOverrideCursor ( Qt::WaitCursor );
	QString fs ( m_lists->searchString->text() );

	int field(  m_lists->getCurrentFieldAction()->data().toInt() );
	QList<FontItem*> tmpList;
	
	if(field == FILTER_FIELD_SPECIAL_UNICODE)  //Unicode
	{
		QList<FontItem*> allList;

		int startC(0xFFFFFFFF);
		int endC(0);
		int patCount(fs.count());
		for(int a(0); a < patCount; ++a)
		{
			unsigned int ca(fs[a].unicode());
			if( ca < startC)
				startC = ca;
			if(ca > endC)
				endC = ca;
		}
		
		// FontItem->countCoverage is very costly, so we take some code from operateFilter
		// to avoid calling it too much, if possible.
		bool queue(m_lists->getOperation().contains("AND"));
		allList = queue ? currentFonts : FMFontDb::DB()->AllFonts();
		int superSetCount(allList.count()); 
		for ( int i =0; i < superSetCount; ++i )
		{
			int cc(allList[i]->countCoverage ( startC, endC ) );
			if ( cc >= patCount )
			{
				tmpList.append ( allList[i]);
			}
		}
		
		operateFilter( tmpList, QString("U://") + QString(fs)  );
	}
	else if(field == FMFontDb::AllInfo)
	{
		FMFontDb::InfoItem k;

		tmpList.clear();
		for(int gIdx(0); gIdx < FontStrings::Names().keys().count() ; ++gIdx)
		{
			k = FontStrings::Names().keys()[gIdx];
			if(k !=  FMFontDb::AllInfo)
			{
				tmpList +=  FMFontDb::DB()->Fonts(fs,k);
			}
		}
		
		operateFilter(tmpList, fs);
			
	}
	else
	{
		tmpList =  FMFontDb::DB()->Fonts(fs, FMFontDb::InfoItem(field ) );
		operateFilter(tmpList, fs);
	}
	
	currentOrdering = "family";
	fillTree();
	m_lists->searchString->clear();
	
	QApplication::restoreOverrideCursor();
}


void MainViewWidget::slotFilterTag ( QString tag )
{
	int tIdx(m_lists->tagsCombo->currentIndex());
	if(tIdx < 0)
		return;

	QString key(m_lists->tagsCombo->itemData(tIdx).toString());
	
	if(key == "TAG") // regular tag
	{
		m_lists->fontTree->clear();
		fontsetHasChanged = true;
		operateFilter( FMFontDb::DB()->Fonts(tag, FMFontDb::Tags ), tag);
		currentOrdering = "family";
		fillTree();
	}
	else if(key == "ALL_ACTIVATED")
	{
		m_lists->fontTree->clear();
		fontsetHasChanged = true;
		operateFilter( FMFontDb::DB()->Fonts(1, FMFontDb::Activation ), tr("Activated"));
		currentOrdering = "family";
		fillTree();
	}
	else if(key == "SIMILAR")
	{
		if(theVeryFont)
		{
			m_lists->fontTree->clear();
			fontsetHasChanged = true;
			operateFilter( PanoseMatchFont::similar(theVeryFont, typo->getPanoseMatchTreshold() ), "S://"+ theVeryFont->family());
			fillTree();
		}
	}
}

void MainViewWidget::operateFilter(QList< FontItem * > allFiltered, const QString filterName)
{
	QList<FontItem*> tmpList = allFiltered;
	QList<FontItem*> negList;
	QList<FontItem*> queList;	
	
	QStringList ops(m_lists->getOperation());
	bool negate(ops.contains("NOT"));
	bool queue(ops.contains("AND"));
	m_lists->clearOperation();
	
	if(queue)
	{
		addFilterToCrumb((negate?"!":"") +filterName);
		queList = currentFonts;
	}
	else
	{
		setCrumb();
		addFilterToCrumb((negate?"!":"") +filterName);
	}
	if(negate)
		negList = FMFontDb::DB()->AllFonts();
		
	currentFonts.clear();
	
	if(negate)
	{
		if(queue)
		{
			foreach(FontItem* f, negList)
			{
				if(!currentFonts.contains(f) && !tmpList.contains(f) && queList.contains(f))
					currentFonts.append(f);
			}
		}
		else // not queue
		{
			foreach(FontItem* f, negList)
			{
				if(!currentFonts.contains(f) && !tmpList.contains(f))
					currentFonts.append(f);
			}
		}
	}
	else // not negate
	{
		if(queue)
		{
			foreach(FontItem* f, tmpList)
			{
				if(!currentFonts.contains(f) && queList.contains(f))
					currentFonts.append(f);
			}
		}
		else // not queue
		{
			foreach(FontItem* f, tmpList)
			{
				if(!currentFonts.contains(f))
					currentFonts.append(f);
			}
		}
	}	
}


void MainViewWidget::slotFontAction ( QTreeWidgetItem * item, int column )
{
// 	qDebug()<<"MainViewWidget::slotFontAction";
	if ( column >2 ) return;

	FontItem * FoIt = FMFontDb::DB()->Font( item->text ( 1 ) );
	if ( FoIt/* && (!FoIt->isLocked())*/ )
	{
		QList<FontItem*> fl;
		fl.append ( FoIt );
		TagsWidget::getInstance()->prepare ( fl );
	}
}

void MainViewWidget::slotFontActionByName (const QString &fname )
{
// 	qDebug()<<"MainViewWidget::slotFontActionByName ("<< fname <<")";
	FontItem * FoIt = FMFontDb::DB()->Font( fname );
	if ( FoIt/* && (!FoIt->isLocked())*/ )
	{
		QList<FontItem*> fl;
		fl.append ( FoIt );
		TagsWidget::getInstance()->prepare ( fl );
	}
}

void MainViewWidget::slotFontActionByNames ( QStringList fnames )
{
// 	qDebug()<<"MainViewWidget::slotFontActionByNames ("<< fnames.join(";") <<")";
	QList<FontItem*> FoIt;
	for ( int i= 0; i < fnames.count() ; ++i )
	{
		FoIt.append ( FMFontDb::DB()->Font( fnames[i] ) );
	}
	if ( FoIt.count() )
		TagsWidget::getInstance()->prepare ( FoIt );
}


void MainViewWidget::slotEditAll()
{
	QList<FontItem*> fl;
	for ( int i =0; i< currentFonts.count(); ++i )
	{
		fl.append ( currentFonts[i] );
	}
	if ( fl.isEmpty() )
		return;

	TagsWidget::getInstance()->prepare ( fl );
}


void MainViewWidget::slotZoom ( int z )
{
	double delta =  1.0 + ( z/1000.0 ) ;
	QTransform trans;
	trans.scale ( delta,delta );

	QGraphicsView * concernedView;
	if ( loremView_FT->isVisible() )
		concernedView = loremView_FT;
	else if ( loremView->isVisible() )
	{
		concernedView = loremView;
		if ( delta == 1.0 )
		{
			double horiScaleT (( double ) QApplication::desktop()->physicalDpiX() / 72.0);
			double vertScaleT ( ( double ) QApplication::desktop()->physicalDpiY() / 72.0);
			QTransform adjustAbsoluteViewT( horiScaleT , 0, 0,vertScaleT, 0, 0 );
			trans =  adjustAbsoluteViewT;
		}
	}
	else if ( playView->isVisible() )
		concernedView = playView;

	concernedView->setTransform ( trans, ( z == 0 ) ? false : true );

}

void MainViewWidget::slotAppendTag ( QString tag )
{
	emit newTag ( tag );
	m_lists->reloadTagsCombo();
}

void MainViewWidget::activation ( FontItem* fit , bool act , bool andUpdate )
{
	if(fit->isActivated() == act)
		return;
	FMActivate::getInstance()->activate(fit, act);

	if ( andUpdate )
		updateTree(true);
}

void MainViewWidget::activation(QList< FontItem * > fit, bool act)
{
	// First check if one of the font is in a different state than required
	QList< FontItem * > actualF;
	for(int i(0); i < fit.count(); ++i)
	{
		if(fit[i]->isActivated() != act)
			actualF.append(fit[i]);
	}
	if(actualF.count() == 0)
		return;
	FMActivate::getInstance()->activate(actualF, act);
	updateTree(true);
}


void MainViewWidget::allActivation ( bool act )
{

	QProgressDialog progress(tr("Activation event"),tr("Cancel"),1,currentFonts.count(),this);
	progress.setWindowModality ( Qt::WindowModal );
	progress.setAutoReset(false);
	QString activString = act ? tr("Activation of :") : tr("Deactivation of :");
	int i =1;
	foreach ( FontItem* fit, currentFonts )
	{
		activation ( fit,act, false );// false here prevents to refill TreeView eachtime.
		progress.setLabelText ( activString + " " +( fit->name() ) );
		progress.setValue ( ++i );
		if ( progress.wasCanceled() )
			break;
	}
	fillTree();
}

void MainViewWidget::slotDesactivateAll()
{
	allActivation ( false );
}

void MainViewWidget::slotActivateAll()
{
	allActivation ( true );
}

void MainViewWidget::slotSetSampleText ( QString s )
{
	sampleText = s ;
	slotView ( true );

}

void MainViewWidget::slotActivate ( bool act, QTreeWidgetItem * item, int column )
{
	if ( column >2 ) return;
	FontItem * FoIt = FMFontDb::DB()->Font( item->text ( 1 ) );
	if ( FoIt )
	{
		activation ( FoIt, act );
	}
}

void MainViewWidget::slotReloadFontList()
{
	currentFonts.clear();
	currentFonts = FMFontDb::DB()->AllFonts();
	fontsetHasChanged = true;
	fillTree();
}

void MainViewWidget::slotSwitchAntiAlias ( bool aa )
{
	loremView->setRenderHint ( QPainter::Antialiasing, aa );
}


void MainViewWidget::slotViewAll()
{
	fontsetHasChanged = true;
	currentFonts = FMFontDb::DB()->AllFonts();
	fillTree();
	setCrumb();
}

void MainViewWidget::slotViewActivated()
{
// 	slotFilterTag ( "Activated_On" );
}


void MainViewWidget::fillUniPlanes()
{
// 	uniPlanes[ "Basic Multilingual Plane (BMP)"] = qMakePair(0x0000,0xFFFF) ;
	// BMP is huge, we split it into langs
	uniPlanes[ tr( "000Basic Latin" ) ] = qMakePair ( 0x0020,0x007F );
	uniPlanes[ tr( "001Latin-1 Supplement" ) ] = qMakePair ( 0x0080,0x00FF );
	uniPlanes[ tr( "002Latin Extended-A" ) ] = qMakePair ( 0x0100,0x017F );
	uniPlanes[ tr( "003Latin Extended-B" ) ] = qMakePair ( 0x0180,0x024F );
	uniPlanes[ tr( "004IPA Extensions" ) ] = qMakePair ( 0x0250,0x02AF );
	uniPlanes[ tr( "005Spacing Modifier Letters" ) ] = qMakePair ( 0x02B0,0x02FF );
	uniPlanes[ tr( "006Combining Diacritical Marks" ) ] = qMakePair ( 0x0300,0x036F );
	uniPlanes[ tr( "007Greek and Coptic" ) ] = qMakePair ( 0x0370,0x03FF );
	uniPlanes[ tr( "008Cyrillic" ) ] = qMakePair ( 0x0400,0x04FF );
	uniPlanes[ tr( "009Cyrillic Supplement" ) ] = qMakePair ( 0x0500,0x052F );
	uniPlanes[ tr( "010Armenian" ) ] = qMakePair ( 0x0530,0x058F );
	uniPlanes[ tr( "011Hebrew" ) ] = qMakePair ( 0x0590,0x05FF );
	uniPlanes[ tr( "012Arabic" ) ] = qMakePair ( 0x0600,0x06FF );
	uniPlanes[ tr( "013Syriac" ) ] = qMakePair ( 0x0700,0x074F );
	uniPlanes[ tr( "014Arabic Supplement" ) ] = qMakePair ( 0x0750,0x077F );
	uniPlanes[ tr( "015Thaana" ) ] = qMakePair ( 0x0780,0x07BF );
	uniPlanes[ tr( "016N'Ko" ) ] = qMakePair ( 0x07C0,0x07FF );
	uniPlanes[ tr( "017Devanagari" ) ] = qMakePair ( 0x0900,0x097F );
	uniPlanes[ tr( "018Bengali" ) ] = qMakePair ( 0x0980,0x09FF );
	uniPlanes[ tr( "019Gurmukhi" ) ] = qMakePair ( 0x0A00,0x0A7F );
	uniPlanes[ tr( "020Gujarati" ) ] = qMakePair ( 0x0A80,0x0AFF );
	uniPlanes[ tr( "021Oriya" ) ] = qMakePair ( 0x0B00,0x0B7F );
	uniPlanes[ tr( "022Tamil" ) ] = qMakePair ( 0x0B80,0x0BFF );
	uniPlanes[ tr( "023Telugu" ) ] = qMakePair ( 0x0C00,0x0C7F );
	uniPlanes[ tr( "024Kannada" ) ] = qMakePair ( 0x0C80,0x0CFF );
	uniPlanes[ tr( "025Malayalam" ) ] = qMakePair ( 0x0D00,0x0D7F );
	uniPlanes[ tr( "026Sinhala" ) ] = qMakePair ( 0x0D80,0x0DFF );
	uniPlanes[ tr( "027Thai" ) ] = qMakePair ( 0x0E00,0x0E7F );
	uniPlanes[ tr( "028Lao" ) ] = qMakePair ( 0x0E80,0x0EFF );
	uniPlanes[ tr( "029Tibetan" ) ] = qMakePair ( 0x0F00,0x0FFF );
	uniPlanes[ tr( "030Burmese" ) ] = qMakePair ( 0x1000,0x109F );
	uniPlanes[ tr( "031Georgian" ) ] = qMakePair ( 0x10A0,0x10FF );
	uniPlanes[ tr( "032Hangul Jamo" ) ] = qMakePair ( 0x1100,0x11FF );
	uniPlanes[ tr( "033Ethiopic" ) ] = qMakePair ( 0x1200,0x137F );
	uniPlanes[ tr( "034Ethiopic Supplement" ) ] = qMakePair ( 0x1380,0x139F );
	uniPlanes[ tr( "035Cherokee" ) ] = qMakePair ( 0x13A0,0x13FF );
	uniPlanes[ tr( "036Unified Canadian Aboriginal Syllabics" ) ] = qMakePair ( 0x1400,0x167F );
	uniPlanes[ tr( "037Ogham" ) ] = qMakePair ( 0x1680,0x169F );
	uniPlanes[ tr( "038Runic" ) ] = qMakePair ( 0x16A0,0x16FF );
	uniPlanes[ tr( "039Tagalog" ) ] = qMakePair ( 0x1700,0x171F );
	uniPlanes[ tr( "040Hanunóo" ) ] = qMakePair ( 0x1720,0x173F );
	uniPlanes[ tr( "041Buhid" ) ] = qMakePair ( 0x1740,0x175F );
	uniPlanes[ tr( "042Tagbanwa" ) ] = qMakePair ( 0x1760,0x177F );
	uniPlanes[ tr( "043Khmer" ) ] = qMakePair ( 0x1780,0x17FF );
	uniPlanes[ tr( "044Mongolian" ) ] = qMakePair ( 0x1800,0x18AF );
	uniPlanes[ tr( "045Limbu" ) ] = qMakePair ( 0x1900,0x194F );
	uniPlanes[ tr( "046Tai Le" ) ] = qMakePair ( 0x1950,0x197F );
	uniPlanes[ tr( "047New Tai Lue" ) ] = qMakePair ( 0x1980,0x19DF );
	uniPlanes[ tr( "048Khmer Symbols" ) ] = qMakePair ( 0x19E0,0x19FF );
	uniPlanes[ tr( "049Buginese" ) ] = qMakePair ( 0x1A00,0x1A1F );
	uniPlanes[ tr( "050Balinese" ) ] = qMakePair ( 0x1B00,0x1B7F );
	uniPlanes[ tr( "051Lepcha" ) ] =  qMakePair ( 0x1C00,0x1C4F );
	uniPlanes[ tr( "052Phonetic Extensions" ) ] = qMakePair ( 0x1D00,0x1D7F );
	uniPlanes[ tr( "053Phonetic Extensions Supplement" ) ] = qMakePair ( 0x1D80,0x1DBF );
	uniPlanes[ tr( "054Combining Diacritical Marks Supplement" ) ] = qMakePair ( 0x1DC0,0x1DFF );
	uniPlanes[ tr( "055Latin Extended Additional" ) ] = qMakePair ( 0x1E00,0x1EFF );
	uniPlanes[ tr( "056Greek Extended" ) ] = qMakePair ( 0x1F00,0x1FFF );
	uniPlanes[ tr( "057General Punctuation" ) ] = qMakePair ( 0x2000,0x206F );
	uniPlanes[ tr( "058Superscripts and Subscripts" ) ] = qMakePair ( 0x2070,0x209F );
	uniPlanes[ tr( "059Currency Symbols" ) ] = qMakePair ( 0x20A0,0x20CF );
	uniPlanes[ tr( "060Combining Diacritical Marks for Symbols" ) ] = qMakePair ( 0x20D0,0x20FF );
	uniPlanes[ tr( "061Letterlike Symbols" ) ] = qMakePair ( 0x2100,0x214F );
	uniPlanes[ tr( "062Number Forms" ) ] = qMakePair ( 0x2150,0x218F );
	uniPlanes[ tr( "063Arrows" ) ] = qMakePair ( 0x2190,0x21FF );
	uniPlanes[ tr( "064Mathematical Operators" ) ] = qMakePair ( 0x2200,0x22FF );
	uniPlanes[ tr( "065Miscellaneous Technical" ) ] = qMakePair ( 0x2300,0x23FF );
	uniPlanes[ tr( "066Control Pictures" ) ] = qMakePair ( 0x2400,0x243F );
	uniPlanes[ tr( "067Optical Character Recognition" ) ] = qMakePair ( 0x2440,0x245F );
	uniPlanes[ tr( "068Enclosed Alphanumerics" ) ] = qMakePair ( 0x2460,0x24FF );
	uniPlanes[ tr( "069Box Drawing" ) ] = qMakePair ( 0x2500,0x257F );
	uniPlanes[ tr( "070Block Elements" ) ] = qMakePair ( 0x2580,0x259F );
	uniPlanes[ tr( "071Geometric Shapes" ) ] = qMakePair ( 0x25A0,0x25FF );
	uniPlanes[ tr( "072Miscellaneous Symbols" ) ] = qMakePair ( 0x2600,0x26FF );
	uniPlanes[ tr( "073Dingbats" ) ] = qMakePair ( 0x2700,0x27BF );
	uniPlanes[ tr( "074Miscellaneous Mathematical Symbols-A" ) ] = qMakePair ( 0x27C0,0x27EF );
	uniPlanes[ tr( "075Supplemental Arrows-A" ) ] = qMakePair ( 0x27F0,0x27FF );
	uniPlanes[ tr( "076Braille Patterns" ) ] = qMakePair ( 0x2800,0x28FF );
	uniPlanes[ tr( "077Supplemental Arrows-B" ) ] = qMakePair ( 0x2900,0x297F );
	uniPlanes[ tr( "078Miscellaneous Mathematical Symbols-B" ) ] = qMakePair ( 0x2980,0x29FF );
	uniPlanes[ tr( "079Supplemental Mathematical Operators" ) ] = qMakePair ( 0x2A00,0x2AFF );
	uniPlanes[ tr( "080Miscellaneous Symbols and Arrows" ) ] = qMakePair ( 0x2B00,0x2BFF );
	uniPlanes[ tr( "081Glagolitic" ) ] = qMakePair ( 0x2C00,0x2C5F );
	uniPlanes[ tr( "082Latin Extended-C" ) ] = qMakePair ( 0x2C60,0x2C7F );
	uniPlanes[ tr( "083Coptic" ) ] = qMakePair ( 0x2C80,0x2CFF );
	uniPlanes[ tr( "084Georgian Supplement" ) ] = qMakePair ( 0x2D00,0x2D2F );
	uniPlanes[ tr( "085Tifinagh" ) ] = qMakePair ( 0x2D30,0x2D7F );
	uniPlanes[ tr( "086Ethiopic Extended" ) ] = qMakePair ( 0x2D80,0x2DDF );
	uniPlanes[ tr( "087Supplemental Punctuation" ) ] = qMakePair ( 0x2E00,0x2E7F );
	uniPlanes[ tr( "088CJK Radicals Supplement" ) ] = qMakePair ( 0x2E80,0x2EFF );
	uniPlanes[ tr( "089Kangxi Radicals" ) ] = qMakePair ( 0x2F00,0x2FDF );
	uniPlanes[ tr( "090Ideographic Description Characters" ) ] = qMakePair ( 0x2FF0,0x2FFF );
	uniPlanes[ tr( "091CJK Symbols and Punctuation" ) ] = qMakePair ( 0x3000,0x303F );
	uniPlanes[ tr( "092Hiragana" ) ] = qMakePair ( 0x3040,0x309F );
	uniPlanes[ tr( "093Katakana" ) ] = qMakePair ( 0x30A0,0x30FF );
	uniPlanes[ tr( "094Bopomofo" ) ] = qMakePair ( 0x3100,0x312F );
	uniPlanes[ tr( "095Hangul Compatibility Jamo" ) ] = qMakePair ( 0x3130,0x318F );
	uniPlanes[ tr( "096Kanbun" ) ] = qMakePair ( 0x3190,0x319F );
	uniPlanes[ tr( "097Bopomofo Extended" ) ] = qMakePair ( 0x31A0,0x31BF );
	uniPlanes[ tr( "098CJK Strokes" ) ] = qMakePair ( 0x31C0,0x31EF );
	uniPlanes[ tr( "099Katakana Phonetic Extensions" ) ] = qMakePair ( 0x31F0,0x31FF );
	uniPlanes[ tr( "100Enclosed CJK Letters and Months" ) ] = qMakePair ( 0x3200,0x32FF );
	uniPlanes[ tr( "101CJK Compatibility" ) ] = qMakePair ( 0x3300,0x33FF );
	uniPlanes[ tr( "102CJK Unified Ideographs Extension A" ) ] = qMakePair ( 0x3400,0x4DBF );
	uniPlanes[ tr( "103Yijing Hexagram Symbols" ) ] = qMakePair ( 0x4DC0,0x4DFF );
	uniPlanes[ tr( "104CJK Unified Ideographs" ) ] = qMakePair ( 0x4E00,0x9FFF );
	uniPlanes[ tr( "105Yi Syllables" ) ] = qMakePair ( 0xA000,0xA48F );
	uniPlanes[ tr( "106Yi Radicals" ) ] = qMakePair ( 0xA490,0xA4CF );
	uniPlanes[ tr( "107Modifier Tone Letters" ) ] = qMakePair ( 0xA700,0xA71F );
	uniPlanes[ tr( "108Latin Extended-D" ) ] = qMakePair ( 0xA720,0xA7FF );
	uniPlanes[ tr( "109Syloti Nagri" ) ] = qMakePair ( 0xA800,0xA82F );
	uniPlanes[ tr( "110Phags-pa" ) ] = qMakePair ( 0xA840,0xA87F );
	uniPlanes[ tr( "111Hangul Syllables" ) ] = qMakePair ( 0xAC00,0xD7AF );
	uniPlanes[ tr( "112High Surrogates" ) ] = qMakePair ( 0xD800,0xDB7F );
	uniPlanes[ tr( "113High Private Use Surrogates" ) ] = qMakePair ( 0xDB80,0xDBFF );
	uniPlanes[ tr( "114Low Surrogates" ) ] = qMakePair ( 0xDC00,0xDFFF );
	uniPlanes[ tr( "115Private Use Area" ) ] = qMakePair ( 0xE000,0xF8FF );
	uniPlanes[ tr( "116CJK Compatibility Ideographs" ) ] = qMakePair ( 0xF900,0xFAFF );
	uniPlanes[ tr( "117Alphabetic Presentation Forms" ) ] = qMakePair ( 0xFB00,0xFB4F );
	uniPlanes[ tr( "118Arabic Presentation Forms-A" ) ] = qMakePair ( 0xFB50,0xFDFF );
	uniPlanes[ tr( "119Variation Selectors" ) ] = qMakePair ( 0xFE00,0xFE0F );
	uniPlanes[ tr( "120Vertical Forms" ) ] = qMakePair ( 0xFE10,0xFE1F );
	uniPlanes[ tr( "121Combining Half Marks" ) ] = qMakePair ( 0xFE20,0xFE2F );
	uniPlanes[ tr( "122CJK Compatibility Forms" ) ] = qMakePair ( 0xFE30,0xFE4F );
	uniPlanes[ tr( "123Small Form Variants" ) ] = qMakePair ( 0xFE50,0xFE6F );
	uniPlanes[ tr( "124Arabic Presentation Forms-B" ) ] = qMakePair ( 0xFE70,0xFEFF );
	uniPlanes[ tr( "125Halfwidth and Fullwidth Forms" ) ] = qMakePair ( 0xFF00,0xFFEF );
	uniPlanes[ tr( "126Specials" ) ] = qMakePair ( 0xFFF0,0xFFFF );
	// TODO split planes into at least scripts
	uniPlanes[ tr( "127Supplementary Multilingual Plane (SMP)" ) ] = qMakePair ( 0x10000,0x1FFFF ) ;
	uniPlanes[ tr( "128Supplementary Ideographic Plane (SIP)" ) ] = qMakePair ( 0x20000,0x2FFFF ) ;
	uniPlanes[ tr( "129unassigned" ) ] = qMakePair ( 0x30000,0xDFFFF ) ;
	uniPlanes[ tr( "130Supplementary Special-purpose Plane (SSP)" ) ] = qMakePair ( 0xE0000,0xEFFFF ) ;
	uniPlanes[ tr( "131Private Use Area 1 (PUA)" ) ] = qMakePair ( 0xF0000,0xFFFFF ) ;
	uniPlanes[ tr( "132Private Use Area 2 (PUA)" ) ] = qMakePair ( 0x100000,0x10FFFF ) ;
	uniPlanes[ tr( "133Un-Mapped Glyphs" ) ] = qMakePair ( -1,100 ) ;
	uniPlanes[ tr( "134View all mapped glyphs" ) ] = qMakePair ( 0, 0x10FFFF ) ;

}

void MainViewWidget::fillUniPlanesCombo ( FontItem* item )
{
	QString stickyRange(uniPlaneCombo->currentText());
// 	qDebug()<<"STiCKyRaNGe :: "<<stickyRange;
	int stickyIndex(0);

	uniPlaneCombo->clear();
	QStringList plist= uniPlanes.keys();
	for ( int i= 0;i<plist.count();++i )
	{
		QString p=plist.at ( i );
		if ( p.isEmpty() )
			continue;
		int begin = uniPlanes[p].first;
		int end = uniPlanes[p].second;
		int codecount = item->countCoverage ( begin,end );
		if ( codecount > 0 )
		{
// 			qDebug() << p << codecount;
			uniPlaneCombo->addItem ( p.mid ( 3 ), p.mid ( 0,3 ) );
			if(p.mid ( 3 ) == stickyRange)
			{
				stickyIndex = uniPlaneCombo->count() - 1;
				uRangeIsNotEmpty = true;
			}
		}
		else
		{
			if(p.mid ( 3 ) == stickyRange)
			{
				// Here we are, there is not
// 				uniPlaneCombo->addItem ( p.mid ( 3 ), p.mid ( 0,3 ) );
				stickyIndex = uniPlaneCombo->count() - 1;
				uRangeIsNotEmpty = true;
			}
		}
	}
	uniPlaneCombo->setCurrentIndex ( stickyIndex );

}

void MainViewWidget::keyPressEvent ( QKeyEvent * event )
{
// 	qDebug() << " MainViewWidget::keyPressEvent(QKeyEvent * "<<event<<")";
// 	if ( event->key() == Qt::Key_Space &&  event->modifiers().testFlag ( Qt::ControlModifier ) )
// 	{
// 		// Switch list view
// 		if ( m_lists->fontlistTab->currentIndex() == 0 )
// 			m_lists->fontlistTab->setCurrentIndex ( 1 );
// 		else
// 			m_lists->fontlistTab->setCurrentIndex ( 0 );
// 	}
}

void MainViewWidget::slotAdjustGlyphView ( int width )
{
	if ( !theVeryFont )
		return;

// 	theVeryFont->adjustGlyphsPerRow ( width );
	slotView ( true );
}

void MainViewWidget::fillOTTree()
{
	OpenTypeTree->clear();
	langCombo->clear();
	langCombo->setEnabled ( false );
	useShaperCheck->setCheckState ( Qt::Unchecked );
	useShaperCheck->setEnabled ( false );
	QStringList scripts;
	if ( theVeryFont && theVeryFont->isOpenType() )
	{
		FMOtf * otf = theVeryFont->takeOTFInstance();
		foreach ( QString table, otf->get_tables() )
		{
			otf->set_table ( table );
			QTreeWidgetItem *tab_item = new QTreeWidgetItem ( OpenTypeTree,QStringList ( table ) );
			tab_item->setExpanded ( true );
			foreach ( QString script, otf->get_scripts() )
			{
				scripts << script;
				otf->set_script ( script );
				QTreeWidgetItem *script_item = new QTreeWidgetItem ( tab_item, QStringList ( script ) );
				script_item->setExpanded ( true );
				foreach ( QString lang, otf->get_langs() )
				{
					otf->set_lang ( lang );
					QTreeWidgetItem *lang_item = new QTreeWidgetItem ( script_item, QStringList ( lang ) );
					lang_item->setExpanded ( true );
					foreach ( QString feature, otf->get_features() )
					{
						QStringList f ( feature );
						f << OTTagMeans ( feature );
						QTreeWidgetItem *feature_item = new QTreeWidgetItem ( lang_item, f );
						feature_item->setCheckState ( 0, Qt::Unchecked );
						if(table == "GPOS")
						{
							if(typo->getDefaultOTFScript() == script && typo->getDefaultOTFLang() == lang && typo->getDefaultOTFGPOS().contains(feature) )
							{
								feature_item->setCheckState ( 0, Qt::Checked );
							}
						}
						else if(table == "GSUB")
						{
							if(typo->getDefaultOTFScript() == script && typo->getDefaultOTFLang() == lang && typo->getDefaultOTFGSUB().contains(feature) )
							{
								feature_item->setCheckState ( 0, Qt::Checked );
							}
						}
					}
				}
			}
		}
		OpenTypeTree->resizeColumnToContents ( 0 ) ;
		theVeryFont->releaseOTFInstance ( otf );
	}
	scripts = scripts.toSet().toList();
// 	scripts.removeAll ( "latn" );
	if ( !scripts.isEmpty() )
	{
		langCombo->setEnabled ( true );
		useShaperCheck->setEnabled ( true );
		langCombo->addItems ( scripts );
	}
}

OTFSet MainViewWidget::deFillOTTree()
{
// 	qDebug() << "MainViewWidget::deFillOTTree()";
	OTFSet ret;
// 	qDebug() << OpenTypeTree->topLevelItemCount();
	for ( int table_index = 0; table_index < OpenTypeTree->topLevelItemCount(); ++table_index ) //tables
	{
// 		qDebug() << "table_index = " << table_index;
		QTreeWidgetItem * table_item = OpenTypeTree->topLevelItem ( table_index ) ;
// 		qDebug() <<  table_item->text(0);
		for ( int script_index = 0; script_index < table_item->childCount();++script_index ) //scripts
		{
			QTreeWidgetItem * script_item = table_item->child ( script_index );
// 			qDebug() << "\tscript_index = " <<  script_index << script_item->text(0);
			for ( int lang_index = 0; lang_index < script_item->childCount(); ++lang_index ) //langs
			{
				QTreeWidgetItem * lang_item = script_item->child ( lang_index );
// 				qDebug() << "\t\tlang_index = "<< lang_index << lang_item->text(0);
				for ( int feature_index = 0; feature_index < lang_item->childCount(); ++feature_index ) //features
				{
// 					qDebug() << lang_item->childCount() <<" / "<<  feature_index;
					QTreeWidgetItem * feature_item = lang_item->child ( feature_index );
// 					qDebug() << "\t\t\tfeature_item -> "<< feature_item->text(0);
					if ( feature_item->checkState ( 0 ) == Qt::Checked )
					{
						if ( table_item->text ( 0 ) == "GPOS" )
						{
							ret.script = script_item->text ( 0 );
							ret.lang = lang_item->text ( 0 );
							ret.gpos_features.append ( feature_item->text ( 0 ) );
						}
						if ( table_item->text ( 0 ) == "GSUB" )
						{
							ret.script = script_item->text ( 0 );
							ret.lang = lang_item->text ( 0 );
							ret.gsub_features.append ( feature_item->text ( 0 ) );
						}
					}
				}
			}
		}
	}
// 	qDebug() << "endOf";
	return ret;

}


void MainViewWidget::slotDefaultOTF()
{
	OTFSet ots(deFillOTTree());
	
	typo->setDefaultOTFScript(ots.script);
	typo->setDefaultOTFLang(ots.lang);
	typo->setDefaultOTFGPOS(ots.gpos_features);
	typo->setDefaultOTFGSUB(ots.gsub_features);
}

void MainViewWidget::slotResetOTF()
{
	typo->setDefaultOTFScript(QString());
	typo->setDefaultOTFLang(QString());
	typo->setDefaultOTFGPOS(QStringList());
	typo->setDefaultOTFGSUB(QStringList());
}

void MainViewWidget::slotFeatureChanged()
{
// 	OTFSet ret = deFillOTTree();
	slotView ( true );
}

void MainViewWidget::slotSampleChanged()
{
	slotView ( true );
}

#define MAX_PALYSTRING_LEN 30

void MainViewWidget::refillSampleList()
{
	sampleTextCombo->clear();
	playString->clear();

	QStringList pList( typo->namedSample(typo->defaultSampleName()).split("\n") );
	foreach(QString pString, pList)
	{
		playString->addItem(pString.left( MAX_PALYSTRING_LEN ));
	}

	QStringList sl = typo->namedSamplesNames();
	for ( int i = 0;i < sl.count(); ++i )
	{
		if ( sl[i] == typo->defaultSampleName() )
		{
			continue;
		}
		else
		{
			sampleTextCombo->addItem ( sl[i] );
			QStringList nl(typo->namedSample(sl[i]).split("\n")) ;
			foreach(QString pString, nl)
			{
				playString->addItem(pString.left( MAX_PALYSTRING_LEN ));
			}

		}
	}
	sampleTextCombo-> insertItem ( 0,typo->defaultSampleName() );
	sampleTextCombo->setCurrentIndex ( 0 );
}

void MainViewWidget::slotFTRasterChanged()
{
// 	fitViewCheck->setChecked(false);
	slotView ( true );
}

void MainViewWidget::slotWantShape()
{
	slotView ( true );
}

void MainViewWidget::slotChangeScript()
{
	if ( useShaperCheck->checkState() == Qt::Checked )
	{
		slotView ( true );
	}
}


void MainViewWidget::slotPlaneSelected ( int i )
{
	bool stickState = uRangeIsNotEmpty;
	uRangeIsNotEmpty = true;
	slotShowAllGlyph();
	slotView ( true );
	if( stickState == false && theVeryFont)
	{
		fillUniPlanesCombo(theVeryFont);
	}
	abcView->verticalScrollBar()->setValue ( 0 );
}

void MainViewWidget::slotSearchCharName()
{	
	if(!theVeryFont)
		return;
	QString name(charSearchLine->text());
	unsigned short cc(theVeryFont->getNamedChar(name));
	qDebug()<<"CS"<<name<<cc;
	if(!cc)
	{
		// TODO display a usefull message
// 		charSearchLine->clear();
		return;
	}
	
	foreach(const QString& key, uniPlanes.keys())
	{
		if((cc >= uniPlanes[key].first)
		  && (cc < uniPlanes[key].second))
		{
			QString ks(key.mid(3));
			qDebug()<<"ZONZ"<<cc<<ks<<uniPlanes[key].first<<uniPlanes[key].second;
			int idx(uniPlaneCombo->findText(ks));
			uniPlaneCombo->setCurrentIndex(idx);
			slotPlaneSelected(idx);
// 			QString dbgS;
			bool inFrame(false);
			while(!inFrame)
			{
				foreach(QGraphicsItem* sit, abcScene->items())
				{
					if((sit->data(1).toString() == "select")
					&& (sit->data(3).toInt() == cc))
					{
						inFrame = true;
						QGraphicsRectItem* ms(reinterpret_cast<QGraphicsRectItem*> (sit));
						if(ms)
						{
							QRectF rf(ms->rect());
							new FMGlyphHighlight(abcScene, rf, 2000, 160);
						}
						else
						{
							qDebug()<<"ERROR: An select item not being a QRect?";
						}
// 						charSearchLine->clear();
						return;
						
					}
				}
				int sv(abcView->verticalScrollBar()->value());
				abcView->verticalScrollBar()->setValue(sv + abcView->height());
			}
		}
	}
	
}

void MainViewWidget::slotShowOneGlyph()
{
	qDebug() <<"slotShowOneGlyph()"<<abcScene->selectedItems().count();
	if ( abcScene->selectedItems().isEmpty() )
		return;
	if ( abcView->lock() )
	{
		curGlyph = reinterpret_cast<QGraphicsRectItem*> ( abcScene->selectedItems().first() );
		curGlyph->setSelected ( false );
		if ( fancyGlyphInUse < 0 )
		{
			if ( curGlyph->data ( 3 ).toInt() > 0 ) // Is a codepoint
			{
				fancyGlyphData = curGlyph->data ( 3 ).toInt();
				if(clipboardCheck->isChecked())
				{
					new FMGlyphHighlight(abcScene, curGlyph->rect());
					QString simpleC;
					simpleC += QChar(fancyGlyphData);
					QApplication::clipboard()->setText(simpleC, QClipboard::Clipboard);
					uniLine->setText(uniLine->text() + simpleC);
				}
				else
					fancyGlyphInUse = theVeryFont->showFancyGlyph ( abcView, fancyGlyphData );
			}
			else // Is a glyph index
			{
				fancyGlyphData = curGlyph->data ( 2 ).toInt();
				fancyGlyphInUse = theVeryFont->showFancyGlyph ( abcView, fancyGlyphData , true );
			}
			if ( fancyGlyphInUse < 0 )
			{
				abcView->unlock();
				return;
			}
			abcView->setState ( FMGlyphsView::SingleView );
		}
		abcView->unlock();
	}
	else
		qDebug()<<"cannot lock ABCview";
}

void MainViewWidget::slotShowAllGlyph()
{
// 	qDebug() <<"slotShowAllGlyph()";
	if ( fancyGlyphInUse < 0 )
		return;
	if ( abcView->lock() )
	{
// 		qDebug()<<"View Locked";
		theVeryFont->hideFancyGlyph ( fancyGlyphInUse );
		fancyGlyphInUse = -1;
		abcView->setState ( FMGlyphsView::AllView );

		abcView->unlock();
	}
// 	qDebug() <<"ENDOF slotShowAllGlyph()";
}

void MainViewWidget::slotUpdateGView()
{
// 	qDebug()<<"slotUpdateGView()";
	// If all is how I think it must be, we don’t need to check anything here :)
	if(theVeryFont && abcView->lock())
	{
		theVeryFont->deRenderAll();
		QString pkey = uniPlaneCombo->itemData ( uniPlaneCombo->currentIndex() ).toString();
		QPair<int,int> uniPair ( uniPlanes[pkey + uniPlaneCombo->currentText() ] );
		theVeryFont->renderAll ( abcScene , uniPair.first, uniPair.second );
		abcView->unlock();
	}
}


void MainViewWidget::slotUpdateGViewSingle()
{
// 	qDebug()<<"slotUpdateGViewSingle";
	if ( theVeryFont && abcView->lock())
	{
// 			qDebug() <<"1.FGI"<<fancyGlyphInUse;
			theVeryFont->hideFancyGlyph ( fancyGlyphInUse );
			if ( fancyGlyphData > 0 ) // Is a codepoint
			{
				fancyGlyphInUse = theVeryFont->showFancyGlyph ( abcView, fancyGlyphData );
// 				qDebug() <<"2.FGI"<<fancyGlyphInUse;
			}
			else // Is a glyph index
			{
				fancyGlyphInUse = theVeryFont->showFancyGlyph ( abcView, fancyGlyphData , true );
// 				qDebug() <<"3.FGI"<<fancyGlyphInUse;
			}
			abcView->unlock();

	}

}



void MainViewWidget::slotUpdateSView()
{
	if(loremView->isVisible())
		slotView(true);
}

void MainViewWidget::slotUpdateRView()
{
	if(loremView_FT->isVisible())
		slotView(true);
}


void MainViewWidget::slotUpdateTree()
{
	updateTree(true);
}


void MainViewWidget::slotEditSampleText()
{
	typo->slotPrefsPanel(PrefsPanelDialog::PAGE_SAMPLETEXT);
}

void MainViewWidget::slotRemoveCurrentItem()
{
	if(curItemName.isEmpty())
		return;
	if(theVeryFont->isActivated())
	{
		QMessageBox::information(this, tr("Fontmatrix takes care of you"), curItemName + tr(" is activated.\nIf you want to remove it from Fontmatrix database, please deactivate it first."), QMessageBox::Yes );
		return;
	}
	if( QMessageBox::question ( this, tr("Fontmatrix safe"), tr("You are about to remove a font from Fontmatrix database") +"\n"+curItemName+"\n" + tr("Do you want to continue?"),QMessageBox::Yes |  QMessageBox::No, QMessageBox::No) == QMessageBox::Yes )
	{
		theVeryFont->deRenderAll();
		currentFonts.removeAll(theVeryFont);
		TagsWidget::getInstance()->removeFromTagged(theVeryFont);
		theVeryFont  = 0 ;
		typo->removeFontItem(curItemName);
		curItemName = lastIndex = faceIndex = "";
		fontsetHasChanged = true;
		fillTree();
	}
}

void MainViewWidget::slotLiveFontSize()
{
	double fs( liveFontSizeSpin->value() );
	reSize(fs, fs * sampleRatio);
	slotView(true);
}

void MainViewWidget::slotRemoteFinished()
{
	qDebug()<<"slotRemoteFinished : "<<currentDownload;
	slotFontSelectedByName(currentDownload);
	slotInfoFont();
	slotUpdateGView();
	slotUpdateRView();
	slotUpdateSView();

}

void MainViewWidget::slotProgressionChanged()
{
	slotView(true);
}

void MainViewWidget::slotPushOnPlayground()
{
	QString spec(playString->currentText());
	if(spec.isEmpty())
		return;

	if(!theVeryFont)
		return;

	double fSize(playFontSize->value());

	playView->displayGlyphs(spec, theVeryFont, fSize);
// 	playString->clearEditText();

}

QString MainViewWidget::sampleName()
{
	QString ret( sampleTextCombo->currentText() );
	if (ret.isEmpty())
		ret = typo->defaultSampleName();
	return ret;
}

void MainViewWidget::displayWelcomeMessage()
{
	if(!typotek::getInstance()->welcomeURL().isEmpty())
	{
		fontInfoText->load(QUrl(typotek::getInstance()->welcomeURL()));
		return;
	}
	
	QString welcomeFontName;
	QString welcomeSVG;
	if(FMFontDb::DB()->FontCount() > 0)
	{
			QList<FontItem*> fl(FMFontDb::DB()->AllFonts());
			QString welcomeString(tr("Welcome to Fontmatrix") );
			if(fl.count() > 0)
			{
				int flcount(fl.count());
				int rIdx( std::rand() % flcount );
				QList<int> triedFont;
				while(triedFont.count() < flcount)
				{
					while(triedFont.contains(rIdx))
						rIdx = std::rand() %  flcount;
					triedFont << rIdx;
					FontItem * f(fl[rIdx]);
					if(f->hasChars(welcomeString))
					{
						QStringList wList(welcomeString.split(" "));
						foreach(const QString& wPart, wList)
						{
							welcomeSVG += "<div>";
							welcomeSVG += f->renderSVG( wPart , QString(QSettings().value("General/WelcomeSize", tr("122.0", "Size of the welcome message" )).toString()).toDouble());
							welcomeSVG += "</div>";
						}
						welcomeFontName = f->fancyName();
						break;
					}	
				}
			}
	}
// 	QString ResPat(FMPaths::ResourcesDir());
// 	QFile wFile( ResPat + "welcome_"+ FMPaths::sysLoc() + ".html");
// 	wFile.open(QIODevice::ReadOnly | QIODevice::Text);
// 	QByteArray wArray(wFile.readAll());
// 	QString wString(QString::fromUtf8(wArray.data(),wArray.length()));
// 	wString.replace("##RECOURCES_DIR##", QUrl::fromLocalFile(ResPat).toString() );
// 	wString.replace("##WELCOME_MESSAGE##", welcomeSVG);
// 	wString.replace("##WELCOME_FONT##", welcomeFontName);
	QString wString;
	wString += "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n" ;
	wString += "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.1//EN\" \"http://www.w3.org/TR/xhtml11/DTD/xhtml11.dtd\">" ;
	wString += "<html xmlns=\"http://www.w3.org/1999/xhtml\"><body style=\"background-color:#eee;\">" ;
	wString += welcomeSVG ;
	wString +="<div style=\"font-family:sans-serif;text-align:right;\">"+welcomeFontName+"</div>";
	wString += "</body></html>" ;
	fontInfoText->setContent(wString.toUtf8(), "application/xhtml+xml");
}

// QTextDocument * MainViewWidget::infoDocument()
// {
// 	return fontInfoText->document();
// }

QGraphicsScene * MainViewWidget::currentSampleScene()
{
	if(!loremView->isVisible())
	{
		tabWidget->setCurrentIndex(1);
		stackedViews->setCurrentIndex(VIEW_PAGE_ABSOLUTE);
		slotView(true);
	}
	return loremScene;
}

FMPlayGround * MainViewWidget::getPlayground()
{
	return playView;
}

void MainViewWidget::addFilterToCrumb(QString filter)
{
	QString t(m_lists->filtersCrumb->text());
	t += "[" + filter.trimmed() + "]";
	m_lists->filtersCrumb->setText(t);
}

void MainViewWidget::setCrumb(QString text)
{
	m_lists->filtersCrumb->setText(text);
}

// Don’t know if it’s really useful
// It will be used for track down problems at least
void MainViewWidget::slotSelectFromFolders(const QString &f)
{
// 	qDebug()<<"or even this other";
	slotFontSelectedByName(f);
}

QWebView * MainViewWidget::info()
{
	return fontInfoText;
}

void MainViewWidget::slotChangeViewPageSetting(bool ch)
{
// 	qDebug()<<"MainViewWidget::slotChangeViewPageSetting("<<ch<<")";
	QString butName( sender()->objectName() );
	if(!ch)
	{
		toolPanelWidth = splitter_2->sizes().at(1) ;
		stackedTools->hide();
	}
	else 
	{
		stackedTools->show();
		if(splitter_2->sizes().at(1) == 0)
		{
			QList<int> li;
			li << splitter_2->width() - toolPanelWidth << toolPanelWidth;
			splitter_2->setSizes(li);
		}
	}
	if(butName == "openTypeButton")
	{
		if(settingsButton->isChecked())
			settingsButton->setChecked(false);
		stackedTools->setCurrentIndex(VIEW_PAGE_OPENTYPE);
	}
	else if(butName == "settingsButton")
	{
		if(openTypeButton->isChecked())
			openTypeButton->setChecked(false);
		stackedTools->setCurrentIndex(VIEW_PAGE_SETTINGS);
	}
}

void MainViewWidget::slotChangeViewPage(QAbstractButton* but)
{
	QString radioName( but->objectName() );
	
	if(radioName == "freetypeRadio" )
	{
		stackedViews->setCurrentIndex(VIEW_PAGE_FREETYPE);
		hintingSelect->setEnabled(true);
	}
	else if(radioName == "nativeRadio" )
	{
		stackedViews->setCurrentIndex(VIEW_PAGE_ABSOLUTE);
		hintingSelect->setEnabled(false);
	}
	
	slotView(true);
}

QByteArray MainViewWidget::splitterState(int spl)
{
	if(spl == SPLITTER_VIEW_1)
		return splitter_2->saveState();
	
	return QByteArray();
}


unsigned int MainViewWidget::hinting()
{
	if(lightHinting->isChecked())
		return FT_LOAD_TARGET_LIGHT;
	else if(normalHinting->isChecked())
		return FT_LOAD_TARGET_NORMAL;
		
	return FT_LOAD_NO_HINTING ;
}

void MainViewWidget::slotHintChanged(int )
{
	slotView(true);
}

void MainViewWidget::slotWebStart()
{
// 	qDebug()<<"slotWebStart";
	typo->startProgressJob(100);
}

void MainViewWidget::slotWebFinished(bool status)
{
// 	qDebug()<<"slotWebinished"<<status;
	typo->endProgressJob();
}

void MainViewWidget::slotWebLoad(int i)
{
// 	qDebug()<<"slotWebLoad("<<i<<")";
	typo->runProgressJob(i);
}

void MainViewWidget::slotWebLink(const QUrl & url)
{
	if(typo->getWebBrowser() == QString( "Fontmatrix" ))
	{
		typo->showStatusMessage(tr("Load") + " " + url.toString());
		fontInfoText->load(url);
	}
	else
	{
		QStringList arguments(typo->getWebBrowserOptions().split(" ", QString::SkipEmptyParts));
		arguments << url.toString();

		if(!QProcess::startDetached(typo->getWebBrowser(), arguments))
		{
			arguments.removeLast();
			QMessageBox::warning(this,"Fontmatrix", QString(tr("An error occured when tried to load %1\nwith command: %2", "%1 is an url and %2 a program")).arg(url.toString()).arg( typo->getWebBrowser() + " " + arguments.join(" ")));
		}
	}
}

QList<FontItem*> MainViewWidget::curFonts()
{
// 	qDebug()<<"curFonts"<<currentFonts.count();
// 	return currentFonts;
	// #12231 
	return orderedCurrentFonts;
}

void MainViewWidget::slotShowULine(bool checked)
{
	if(checked)
	{
		uniLine->setText("");
		uniLine->setVisible(true);
	}
	else
	{
		uniLine->setVisible(false);
	}
}

void MainViewWidget::slotShowClassification()
{
// 	qDebug()<<"MainViewWidget::slotShowClassification";
	// Rather testing for now
	classVariableDescription->setHtml(FontStrings::PanoseKeyInfo(FontStrings::firstPanoseKey()));
	ParallelCoorDataType pcdata;
	QList<FontDBResult> dbresult( FMFontDb::DB()->getValues(currentFonts, FMFontDb::Panose) );
	for(int i(0); i < dbresult.count() ; ++i)
	{
		QList<int> list;
		QString p(dbresult[i].second);
		QStringList pl(p.split(":"));
		if(pl.count() == 10)
		{
			for(int a(0);a < 10 ;++a)
			{
				list << pl[a].toInt();
			}
			pcdata << list;
		}
	}
	
	ParallelCoorDataSet* pcs(new ParallelCoorDataSet);
	QMap< FontStrings::PanoseKey, QMap<int, QString> > pan(FontStrings::Panose());
	FontStrings::PanoseKey k(FontStrings::firstPanoseKey());
	while(k != FontStrings::InvalidPK)
	{
		QString key( FontStrings::PanoseKeyName(k) );
// 		qDebug()<<key;
		QList<QString> list;
		for(QMap<int, QString>::iterator i( pan[k].begin() ); i != pan[k].end(); ++i)
		{
// 			qDebug()<<i.value() ;
			list << i.value() ;
		}
		pcs->append( qMakePair(key, list) );
		k = FontStrings::nextPanoseKey(k);
	}
	
	
	pcs->setData(pcdata);
	classificationView->setDataSet(pcs);
// 	classificationView->updateGraphic();
	
}

void MainViewWidget::slotUpdateClassDescription(const QString & ks)
{
	FontStrings::PanoseKey pk(FontStrings::firstPanoseKey());
	do
	{
		if(FontStrings::PanoseKeyName(pk) == ks)
			break;
		pk = FontStrings::nextPanoseKey(pk);
	}
	while(pk != FontStrings::InvalidPK);
	
	classVariableDescription->setHtml(FontStrings::PanoseKeyInfo(pk));
}

void MainViewWidget::slotPanoseFilter()
{
	QList<FontDBResult> dbresult( FMFontDb::DB()->getValues(currentFonts, FMFontDb::Panose) );
	QList<FontItem*> fil;
	for(int i(0); i < dbresult.count() ; ++i)
	{
		QList<int> list;
		QString p(dbresult[i].second);
		QStringList pl(p.split(":"));
		if(pl.count() == 10)
		{
			for(int a(0);a < 10 ;++a)
			{
				list << pl[a].toInt();
			}
			if(classificationView->matchFilter(list))
				fil << dbresult[i].first;
		}
	}
	currentFonts = fil;
	setCrumb(classificationView->filterAsString());
	fillTree();
}

void MainViewWidget::slotSaveClassSplitter()
{
	QSettings settings;
	settings.setValue("WState/ClassificationSplitter", classSplitter->saveState());
}




