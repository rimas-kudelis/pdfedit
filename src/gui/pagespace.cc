#include "pagespace.h"
#include "settings.h"
#include <stdlib.h>
#include <qlabel.h>
#include <qstring.h>
#include <qpixmap.h>
#include "cpage.h"
#include "static.h"
#include "QOutputDevPixmap.h"

// using namespace std;

namespace gui {

typedef struct { int labelWidth, labelHeight; } initStruct;
// TODO asi prepracovat
void Init( initStruct * is, const QString & s ) {
	QLabel pageNumber( s ,0);
	pageNumber.show();
	is->labelWidth = pageNumber.width();
	is->labelHeight = pageNumber.height();
}

QString PAGESPC = "gui/PageSpace/";
QString ICON = "icon/";
QString format = "x:%'.2f y:%'.2f";

PageSpace::PageSpace(QWidget *parent /*=0*/, const char *name /*=0*/) : QWidget(parent,name) {
	initStruct is;

	vBox = new QVBoxLayout(this);
	
	scrollPageSpace = new QScrollView(this);
	vBox->addWidget(scrollPageSpace);
	
	hBox = new QHBoxLayout(vBox);

	actualPdf = NULL;
	actualPage = NULL;
	actualPagePixmap = NULL;
	actualSelectedObjects = NULL;
	pageImage = NULL;
	newPageView();
//	scrollPageSpace->setBackGroundRole(QPalette::Dark); // TODO configurovatelna farba
	
	hBox->addStretch();
	bFirstPage = new QPushButton(this,"bFirstPage");
	bFirstPage->setPixmap( QPixmap( globalSettings->getFullPathName("icon", globalSettings->read( ICON +"FirstPage") ) ));
	hBox->addWidget(bFirstPage);
	bPrevPage = new QPushButton(this,"bPrevPage");
	bPrevPage->setPixmap( QPixmap( globalSettings->getFullPathName("icon", globalSettings->read( ICON +"PrevPage") ) ));
	hBox->addWidget(bPrevPage);
	connect( bFirstPage, SIGNAL(clicked()), this, SLOT(firstPage()));
	connect( bPrevPage, SIGNAL(clicked()), this, SLOT(prevPage()));
//TODO: pevna minimalna velikost (bez init)
	Init( &is , "00000" );
	pageNumber = new QLabel( "0", this, "cisStr" );
	pageNumber->setMinimumWidth( is.labelWidth );
	pageNumber->setAlignment( AlignCenter | pageNumber->alignment() );
	//pageNumber->setNum( 0 );
	hBox->addWidget( pageNumber );

	bNextPage = new QPushButton(this,"bNextPage");
	bNextPage->setPixmap( QPixmap( globalSettings->getFullPathName("icon", globalSettings->read( ICON +"NextPage") ) ));
	hBox->addWidget(bNextPage);
	bLastPage = new QPushButton(this,"bLastPage");
	bLastPage->setPixmap( QPixmap( globalSettings->getFullPathName("icon", globalSettings->read( ICON +"LastPage") ) ));
	hBox->addWidget(bLastPage);
	hBox->addStretch();
	connect( bNextPage, SIGNAL(clicked()), this, SLOT(nextPage()));
	connect( bLastPage, SIGNAL(clicked()), this, SLOT(lastPage()));

	hBox->setResizeMode(QLayout::Minimum);

	QString pom;
	Init( &is , format + "0000" );
	mousePositionOnPage = new QLabel( pom.sprintf( format, 0,0 ), this );
	mousePositionOnPage->setMinimumWidth( is.labelWidth );
	mousePositionOnPage->setAlignment( AlignRight | mousePositionOnPage->alignment() );
	hBox->addWidget( mousePositionOnPage, 0, AlignRight);

	hBox->insertSpacing( 0, is.labelWidth );	// for center pageNambuer
}

PageSpace::~PageSpace() {
	delete actualPdf;
	delete actualPage;
	delete actualPagePixmap;
}

void PageSpace::hideButtonsAndPageNumber ( ) {
	bFirstPage->hide();
	bPrevPage->hide();
	bNextPage->hide();
	bLastPage->hide();
	pageNumber->hide();
}
void PageSpace::showButtonsAndPageNumber ( ) {
	bFirstPage->show();
	bPrevPage->show();
	bNextPage->show();
	bLastPage->show();
	pageNumber->show();
}

void PageSpace::newPageView() {
	if (pageImage != NULL) {
		scrollPageSpace->removeChild( pageImage );
		delete pageImage;
	}
	pageImage = new PageView(scrollPageSpace->viewport());
	scrollPageSpace->addChild(pageImage);

	pageImage->setResizingZone( globalSettings->readNum( PAGESPC + "ResizingZone" ) );

	connect( pageImage, SIGNAL( leftClicked(const QRect &) ), this, SLOT( newSelection(const QRect &) ) );
	connect( pageImage, SIGNAL( rightClicked(const QPoint &, const QRect &) ),
		this, SLOT( requirementPopupMenu(const QPoint &, const QRect &) ) );
	connect( pageImage, SIGNAL( selectionMovedTo(const QPoint &) ), this, SLOT( moveSelection(const QPoint &) ) );
	connect( pageImage, SIGNAL( selectionResized(const QRect &, const QRect &) ),
		this, SLOT( resizeSelection(const QRect &, const QRect &) ) );
	connect( pageImage, SIGNAL( changeMousePosition(const QPoint &) ), this, SLOT( showMousePosition(const QPoint &) ) );
}

void PageSpace::newPageView( QPixmap &qp ) {
	newPageView();
	pageImage->setPixmap( qp );
	pageImage->show();
	centerPageView();
}
void PageSpace::centerPageView( ) {
	bool reposition;
	int posX, posY;

	// When page is refreshing, pageImage->width() is not correct the page's width (same height)
	if (pageImage->pixmap() == NULL) {
		posX = pageImage->width();
		posY = pageImage->height();
	} else {
		posX = pageImage->pixmap()->width();
		posY = pageImage->pixmap()->height();
	}

	// Calculation topLeft position of page on scrollPageSpace
	if (reposition = (posX = (scrollPageSpace->visibleWidth() - posX) / 2) < 0 )
		posX = 0;
	if ((posY = (scrollPageSpace->visibleHeight() - posY) / 2) < 0 )
		posY = 0;
	else
		reposition = false;
	// If scrollPageSpace is smaller then page and page's position on scrollPageSpace is not set to (0,0), must be changed
	reposition = reposition && (0 == (scrollPageSpace->childX(pageImage) + scrollPageSpace->childY(pageImage)));

	if (! reposition) {
		// move page to center of scrollPageSpace
		scrollPageSpace->moveChild( pageImage, posX, posY );
	}
}

void PageSpace::resizeEvent ( QResizeEvent * re) {
	this->QWidget::resizeEvent( re );

	centerPageView();
}

void PageSpace::refresh ( int pageToView, /*QSPdf * */ QObject * pdf ) {	// same as above
	QSPdf * p = dynamic_cast<QSPdf *>(pdf);
	if (p)
		refresh( pageToView, p );
}
void PageSpace::refresh ( QSPage * pageToView, /*QSPdf * */ QObject * pdf ) {
	QSPdf * p = dynamic_cast<QSPdf *>(pdf);
	if (p)
		refresh( pageToView, p );
}

void PageSpace::refresh ( int pageToView, QSPdf * pdf ) {			// if pdf is NULL refresh page from current pdf
	int pageCount;

	if (pdf == NULL) {
		if (actualPdf == NULL)
			return ;
		pdf = actualPdf;
	}

	if (pageToView < 1)
			pageToView = 1;
		
	pageCount = pdf->get()->getPageCount();
	if (pageToView > pageCount)
			pageToView = pageCount;
		
	QSPage p( pdf->get()->getPage( pageToView ) );
	refresh( &p, pdf );
}

#define splashMakeRGB8(to, r, g, b) \
	  (to[3]=0, to[2]=((r) & 0xff) , to[1]=((g) & 0xff) , to[0]=((b) & 0xff) )

void PageSpace::refresh ( QSPage * pageToView, QSPdf * pdf ) {		// if pageToView is NULL, refresh actual page
									// if pageToView == actualPage  refresh is not need
	if ((pageToView != NULL) && (actualPage != pageToView) && (pdf != NULL)) {
		if (pdf != actualPdf) {
			delete actualPdf;
			actualPdf = new QSPdf( pdf->get() );
		}
		delete actualPage;
		actualPage = new QSPage( pageToView->get() );
//		pageNumber->setNum( 0/*(int) pageToView->getPageNumber()*/ );//MP: po zmene kernelu neslo zkompilovat (TODO)
//		actualSelectedObject = NULL;
//-		if (actualPage != NULL) {
//-printf("3");
//-			delete actualPage;
//-		}
//-printf("4");
//-		actualPage = new QSPage( pageToView->get() );
		actualSelectedObjects = NULL;
		pageNumber->setNum( actualPdf->getPagePosition( actualPage ) );

		SplashColor paperColor;
		splashMakeRGB8(paperColor, 0xff, 0xff, 0xff);
		QOutputDevPixmap output ( paperColor );

		actualPage->get()->displayPage( output );
		delete r2;
		QImage img = output.getImage();
		if (img.isNull())
			r2 = new QPixmap();
		else {
			r2 = new QPixmap( img );
		}
		newPageView( *r2 );
	} else {
		if ((actualPage == NULL) || (pageToView != NULL))
			return ;		// no page to refresh or refresh actual page is not need
		
		/* TODO zmazat */
	}
	/* TODO zobrazenie aktualnej stranky*/
	/* newPageView( *actualPagePixmap ); */
}
#undef splashMakeRGB8

void PageSpace::keyPressEvent ( QKeyEvent * e ) {
	switch ( e->key() ) {
		case Qt::Key_Escape :
			if ( ! pageImage->escapeSelection() )
				e->ignore();
			break;
		default:
			e->ignore();
	}
}

void PageSpace::newSelection ( const QRect & r) {
	// TODO napr. zoom
	std::vector<boost::shared_ptr<PdfOperator> > ops;

	if (actualPage != NULL) {
		if ( r.topLeft() == r.bottomRight() ) {
			Point p;
			convertPixmapPosToPdfPos( r.topLeft(), p );
			actualPage->get()->getObjectsAtPosition( ops, Point( p.x, p.y ) );
		} else {
			Point p1, p2;
			convertPixmapPosToPdfPos( r.topLeft(), p1 );
			convertPixmapPosToPdfPos( r.bottomRight(), p2 );
			actualPage->get()->getObjectsAtPosition( ops, Rectangle( p1.x, p1.y, p2.x, p2.y ) );
		}
	}
}
void PageSpace::requirementPopupMenu ( const QPoint & globalPos, const QRect & r) {
	// TODO
	printf("requirementPopupMenu\n");
	if (r.isEmpty())
		printf("empty\n");
	else
		printf("non empty\n");
}
void PageSpace::moveSelection ( const QPoint & relativeMove ) {
	// TODO
}
void PageSpace::resizeSelection ( const QRect &, const QRect & ) {
	// TODO
}

void PageSpace::convertPixmapPosToPdfPos( const QPoint & pos, Point & pdfPos ) {
	if (actualPage == NULL) {
		pdfPos.x = 0;
		pdfPos.y = 0;
		return ;
	}
	pdfPos.x = pos.x();
	pdfPos.y = r2->height() - pos.y();
}

void PageSpace::showMousePosition ( const QPoint & pos ) {
	QString pom;
	Point pdfPagePos;
	convertPixmapPosToPdfPos( pos, pdfPagePos );
	pom = pom.sprintf( format, pdfPagePos.x, pdfPagePos.y );
	mousePositionOnPage->setText( pom );
}

void PageSpace::firstPage ( ) {
	if (!actualPdf)
		return;
	QSPage p (actualPdf->get()->getFirstPage());
	refresh( &p, actualPdf );
}
void PageSpace::prevPage ( ) {
	if (!actualPdf)
		return;
	if (!actualPage) {
		firstPage ();
		return;
	}

	if ( (actualPdf->get()->hasPrevPage( actualPage->get() )) == true) {
		QSPage p (actualPdf->get()->getPrevPage( actualPage->get() ));
		refresh( &p, actualPdf );
	}
}
void PageSpace::nextPage ( ) {
	if (!actualPdf)
		return;
	if (!actualPage) {
		firstPage ();
		return;
	}

	if ( (actualPdf->get()->hasNextPage( actualPage->get() )) == true) {
		QSPage p (actualPdf->get()->getNextPage( actualPage->get() ));
		refresh( &p, actualPdf );
	}
}
void PageSpace::lastPage ( ) {
	if (!actualPdf)
		return;
	QSPage p (actualPdf->get()->getLastPage());
	refresh( &p, actualPdf );
}
} // namespace gui
