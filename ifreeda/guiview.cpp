/***************************************************************************
                               guiview.cpp
 ***************************************************************************/
#include <qinputdialog.h>
#include <qclipboard.h>
#include <qapplication.h>
#include <qmessagebox.h>
#include <q3popupmenu.h>
 //Added by qt3to4:
#include <Q3TextStream>
#include <QMouseEvent>
#include <QEvent>
#include <QWheelEvent>
#include <climits>
#include <cstdlib>
#include <iostream>

#include "gui.h"
#include "guimain.h"
#include "node.h"
#include "guiview.h"
//#include "components/spicedialog.h" QUCS-EDIT REMOVE SPICE
#include "components/componentdialog.h"
#include "diagrams/diagramdialog.h"
#include "diagrams/markerdialog.h"
#include "diagrams/tabdiagram.h"
#include "dialogs/labeldialog.h"

QucsView::QucsView(QWidget *parent) : Q3ScrollView(parent)
{
  setVScrollBarMode(Q3ScrollView::AlwaysOn);
  setHScrollBarMode(Q3ScrollView::AlwaysOn);
  viewport()->setPaletteBackgroundColor(QucsSettings.BGColor);
  viewport()->setMouseTracking(true);
  
  // Added by Shivam  To Paint outside Paint event
  setAttribute(Qt::WA_PaintOutsidePaintEvent, true);
  setAttribute(Qt::WA_PaintOnScreen,true);

  MouseMoveAction = &QucsView::MouseDoNothing;
  MousePressAction = &QucsView::MouseDoNothing;
  MouseReleaseAction = &QucsView::MouseDoNothing;
  MouseDoubleClickAction = &QucsView::MouseDoNothing;

  selComp  = 0;  // no component is selected
  selDiag  = 0;  // no diagram is selected
  selPaint = 0;  // no painting is selected
  isMoveEqual = false;  // mouse cursor move x and y the same way
  focusElement = 0;

  Docs.setAutoDelete(true);

  //connect(horizontalScrollBar(), SIGNAL(prevLine()),SLOT(slotScrollLeft()));
  //connect(horizontalScrollBar(), SIGNAL(nextLine()),SLOT(slotScrollRight()));
  //connect(verticalScrollBar(), SIGNAL(prevLine()), SLOT(slotScrollUp()));
  //connect(verticalScrollBar(), SIGNAL(nextLine()), SLOT(slotScrollDown()));
  
  // Added by Shivam. In Qt4 prevline() and nextLine() Signals donot exist
  // anymore. lineStep of Qt3 is Equivalent to singleStep of Qt4. See port qt 3
  // to 4 guide

  connect(horizontalScrollBar(), SIGNAL(actionTriggered(int)),SLOT(slotScrollLeft(int))); 
  connect(horizontalScrollBar(), SIGNAL(actionTriggered(int)),SLOT(slotScrollRight(int)));
  connect(verticalScrollBar(), SIGNAL(actionTriggered(int)), SLOT(slotScrollUp(int)));
  connect(verticalScrollBar(), SIGNAL(actionTriggered(int)), SLOT(slotScrollDown(int)));

  // .......................................................................
  // to repair some strange  scrolling artefacts
  connect(this, SIGNAL(horizontalSliderReleased()),
		viewport(), SLOT(repaint()));
  connect(this, SIGNAL(verticalSliderReleased()),
		viewport(), SLOT(repaint()));

  // to prevent user from editing something that he doesn't see
  connect(this, SIGNAL(horizontalSliderPressed()), SLOT(slotHideEdit()));
  connect(this, SIGNAL(verticalSliderPressed()), SLOT(slotHideEdit()));

  // .......................................................................
  // initialize menu appearing by right mouse button click on component
  ComponentMenu = new Q3PopupMenu(this);
  focusMEvent   = new QMouseEvent(QEvent::MouseButtonPress, QPoint(0,0),
				  Qt::NoButton, Qt::NoButton);

  // .......................................................................
  // valid expressions for component property editor
  Expression.setPattern("[^\"=]+");
  Validator = new QRegExpValidator(Expression, this);
  editText  = new QLineEdit(viewport());
  editText->setValidator(Validator);
  editText->setFrame(false);
  editText->setHidden(true);
  editText->setPaletteBackgroundColor(QucsSettings.BGColor);
  // Added by Shivam to paint outside paintEvent
  //editText->setAttribute(Qt::WA_PaintOutsidePaintEvent, true);
//  editText->setEraseColor(QucsSettings.BGColor);
  connect(editText, SIGNAL(returnPressed()), SLOT(slotApplyCompText()));
  connect(editText, SIGNAL(lostFocus()), SLOT(slotHideEdit()));
}


QucsView::~QucsView()
{
  delete ComponentMenu;
  delete focusMEvent;
  delete Validator;
}

// -----------------------------------------------------------
// Is called when the content (schematic or data display) has to be drawn.
void QucsView::drawContents(QPainter *p, int, int, int, int)
{
  QucsDoc *d = Docs.current();
  if (!d) return;

  Painter.init(p, d->Scale, -d->ViewX1, -d->ViewY1, contentsX(), contentsY());

  d->paintGrid(&Painter, contentsX(), contentsY(),
			visibleWidth(), visibleHeight());
  d->paint(&Painter);
//  drawn = false;
}

// -----------------------------------------------------------
float QucsView::Zoom(float s)
{
  QucsDoc *d = Docs.current();
  float Scale = d->Scale;
  Scale *= s;
  if(Scale > 10.0) Scale = 10.0f;
  if(Scale < 0.01) Scale = 0.01f;

  d->Scale = Scale;
  resizeContents(int(Scale*float(d->ViewX2 - d->ViewX1)),
		 int(Scale*float(d->ViewY2 - d->ViewY1)));
  return Scale;
}

// -----------------------------------------------------------
bool QucsView::pasteElements()
{
  QClipboard *cb = QApplication::clipboard();   // get system clipboard
  QString s = cb->text(QClipboard::Clipboard);
  Q3TextStream stream(&s, QIODevice::ReadOnly);
  movingElements.clear();
  if(!Docs.current()->paste(&stream, &movingElements)) return false;

//  viewport()->repaint();

  Element_Qucs *pe;
  int xmax, xmin, ymax, ymin;
  xmin = ymin = INT_MAX;
  xmax = ymax = INT_MIN;
  // First, get the max and min coordinates of all selected elements.
  for(pe = movingElements.first(); pe != 0; pe = movingElements.next()) {
    if(pe->Type == isWire) {
      if(pe->x1 < xmin) xmin = pe->x1;
      if(pe->x2 > xmax) xmax = pe->x2;
      if(pe->y1 < ymin) ymin = pe->y1;
      if(pe->y2 > ymax) ymax = pe->y2;
    }
    else {
      if(pe->cx < xmin) xmin = pe->cx;
      if(pe->cx > xmax) xmax = pe->cx;
      if(pe->cy < ymin) ymin = pe->cy;
      if(pe->cy > ymax) ymax = pe->cy;
    }
  }

  xmin = -((xmax+xmin) >> 1);   // calculate midpoint
  ymin = -((ymax+ymin) >> 1);
  Docs.current()->setOnGrid(xmin, ymin);

  // moving with mouse cursor in the midpoint
  for(pe = movingElements.first(); pe != 0; pe = movingElements.next())
    if(pe->Type & isLabel) {
      pe->cx += xmin;  pe->x1 += xmin;
      pe->cy += ymin;  pe->y1 += ymin;
    }
    else
      pe->setCenter(xmin, ymin, true);

  return true;
}


// -----------------------------------------------------------
// Enlarge the viewport area if the coordinates x1-x2/y1-y2 exceed the
// visible area.
void QucsView::enlargeView(int x1, int y1, int x2, int y2)
{
  int dx=0, dy=0;
  QucsDoc *d = Docs.current();

  if(x1 < d->UsedX1) d->UsedX1 = x1;
  if(y1 < d->UsedY1) d->UsedY1 = y1;
  if(x2 > d->UsedX2) d->UsedX2 = x2;
  if(y2 > d->UsedY2) d->UsedY2 = y2;

  if(x1 < d->ViewX1) {
    dx = int(d->Scale * float(d->ViewX1-x1+40));
    d->ViewX1 = x1-40;
  }
  if(y1 < d->ViewY1) {
    dy = int(d->Scale * float(d->ViewY1-y1+40));
    d->ViewY1 = y1-40;
  }
  if(x2 > d->ViewX2) d->ViewX2 = x2+40;
  if(y2 > d->ViewY2) d->ViewY2 = y2+40;

  resizeContents(int(d->Scale*float(d->ViewX2 - d->ViewX1)),
		 int(d->Scale*float(d->ViewY2 - d->ViewY1)));
//  resizeContents(d->ViewX2-d->ViewX1,d->ViewY2-d->ViewY1);
  scrollBy(dx,dy);
}

// -----------------------------------------------------------
void QucsView::setPainter(QPainter *p, QucsDoc *d)
{
  // contents to viewport transformation
  //cout << "Inside setPainter" << endl;
  p->translate(-contentsX(), -contentsY());
  p->scale(d->Scale, d->Scale);
  p->translate(-d->ViewX1, -d->ViewY1);
  p->setPen(Qt::DotLine);
  //p->setPen(Qt::SolidLine);
 // p->setPen(Qt::gray);
  //p->setRasterOp(Qt::NotROP);  // background should not be erased
  // Changed by Shivam
  p->setCompositionMode(QPainter::CompositionMode_SourceOut);
  //p->setCompositionMode(QPainter::CompositionMode_Clear);
}

// -----------------------------------------------------------
void QucsView::slotCursorLeft()
{
  if(!editText->isHidden()) return;  // for edit of component property ?

  QucsDoc *d = Docs.current();
  if(d->MarkerLeftRight(true)) {
    viewport()->repaint();
    drawn = false;
    return;   // if marker selected, do not move other elements
  }
  movingElements.clear();
  d->copySelectedElements(&movingElements);
  if(movingElements.isEmpty()) {
    if(ScrollLeft(horizontalScrollBar()->lineStep()))
      scrollBy(-horizontalScrollBar()->lineStep(), 0);
    viewport()->repaint();
    drawn = false;
    return;
  }

  MAx1 = -(d->GridX);  MAy1 = 0;   // move "GridX" to left
  MovingElements();
  MAx3 = 1;  // sign for moved elements
  endElementMoving();
}

void QucsView::slotCursorRight()
{
  if(!editText->isHidden()) return;  // for edit of component property ?

  QucsDoc *d = Docs.current();
  if(d->MarkerLeftRight(false)) {
    viewport()->repaint();
    drawn = false;
    return;   // if marker selected, do not move other elements
  }
  movingElements.clear();
  d->copySelectedElements(&movingElements);
  if(movingElements.isEmpty()) {
    if(ScrollRight(-horizontalScrollBar()->lineStep()))
      scrollBy(horizontalScrollBar()->lineStep(), 0);
    viewport()->repaint();
    drawn = false;
    return;
  }

  MAx1 = d->GridX;  MAy1 = 0;   // move "GridX" to right
  MovingElements();
  MAx3 = 1;  // sign for moved elements
  endElementMoving();
}

void QucsView::slotCursorUp()
{
  if(!editText->isHidden()) {  // for edit of component property ?
    if(MAx3 == 0) return;  // edit component namen ?
    Component *pc = (Component*)focusElement;
    Property *pp = pc->Props.at(MAx3-1);  // current property
    int Begin = pp->Description.find('[');
    if(Begin < 0) return;  // no selection list ?
    int End = pp->Description.find(editText->text(), Begin); // current
    if(End < 0) return;  // should never happen
    End = pp->Description.findRev(',', End);
    if(End < Begin) return;  // was first item ?
    End--;
    int Pos = pp->Description.findRev(',', End);
    if(Pos < Begin) Pos = Begin;   // is first item ?
    Pos++;
    if(pp->Description.at(Pos) == ' ') Pos++; // remove leading space
    editText->setText(pp->Description.mid(Pos, End-Pos+1));
    editText->selectAll();
    return;
  }

  QucsDoc *d = Docs.current();
  if(d->MarkerUpDown(true)) {
    viewport()->repaint();
    drawn = false;
    return;   // if marker selected, do not move other elements
  }
  movingElements.clear();
  d->copySelectedElements(&movingElements);
  if(movingElements.isEmpty()) {
    if(ScrollUp(verticalScrollBar()->lineStep()))
      scrollBy(0, -verticalScrollBar()->lineStep());
    viewport()->repaint();
    drawn = false;
    return;
  }

  MAx1 = 0;  MAy1 = -(d->GridY);   // move "GridY" up
  MovingElements();
  MAx3 = 1;  // sign for moved elements
  endElementMoving();
}

void QucsView::slotCursorDown()
{
  if(!editText->isHidden()) {  // for edit of component property ?
    if(MAx3 == 0) return;  // edit component namen ?
    Component *pc = (Component*)focusElement;
    Property *pp = pc->Props.at(MAx3-1);  // current property
    int Pos = pp->Description.find('[');
    if(Pos < 0) return;  // no selection list ?
    Pos = pp->Description.find(editText->text(), Pos); // current list item
    if(Pos < 0) return;  // should never happen
    Pos = pp->Description.find(',', Pos);
    if(Pos < 0) return;  // was last item ?
    Pos++;
    if(pp->Description.at(Pos) == ' ') Pos++; // remove leading space
    int End = pp->Description.find(',', Pos);
    if(End < 0) {  // is last item ?
      End = pp->Description.find(']', Pos);
      if(End < 0) return;  // should never happen
    }
    editText->setText(pp->Description.mid(Pos, End-Pos));
    editText->selectAll();
    return;
  }

  QucsDoc *d = Docs.current();
  if(d->MarkerUpDown(false)) {
    viewport()->repaint();
    drawn = false;
    return;   // if marker selected, do not move other elements
  }
  movingElements.clear();
  d->copySelectedElements(&movingElements);
  if(movingElements.isEmpty()) {
    if(ScrollDown(-verticalScrollBar()->lineStep()))
      scrollBy(0, verticalScrollBar()->lineStep());
    viewport()->repaint();
    drawn = false;
    return;
  }

  MAx1 = 0;  MAy1 = d->GridY;   // move "GridY" down
  MovingElements();
  MAx3 = 1;  // sign for moved elements
  endElementMoving();
}

// -----------------------------------------------------------
void QucsView::editLabel(WireLabel *pl)
{
  LabelDialog *Dia = new LabelDialog(pl, this);
  int Result = Dia->exec();
  if(Result == 0) return;

  QString Name  = Dia->NodeName->text();
  QString Value = Dia->InitValue->text();
  delete Dia;
/*  bool OK;
  QString Name = QInputDialog::getText(tr("Insert Nodename"),
	tr("Enter the label:"), QLineEdit::Normal, pl->Name, &OK, this);
  if(!OK) return;*/

  if(Name.isEmpty() && Value.isEmpty()) { // if nothing entered, delete label
    if(pl->pWire) pl->pWire->setName("", "");   // delete name of wire
    else pl->pNode->setName("", "");
  }
  else {
/*    Name.replace(' ', '_');	// label must not contain spaces
    while(Name.at(0) == '_') Name.remove(0,1);  // must not start with '_'
    if(Name.isEmpty()) return;
    if(Name == pl->Name) return;*/
    if(Result == 1) return;  // nothing changed

    int old_x2 = pl->x2;
    pl->setName(Name);   // set new name
    pl->initValue = Value;
    if(pl->cx > (pl->x1+(pl->x2>>1)))
      pl->x1 -= pl->x2 - old_x2; // don't change position due to text width
  }

  QucsDoc *d = Docs.current();
  d->sizeOfAll(d->UsedX1, d->UsedY1, d->UsedX2, d->UsedY2);
  viewport()->repaint();
  drawn = false;
  Docs.current()->setChanged(true, true);
}

// -----------------------------------------------------------
// Reinserts all elements (moved by the user) back into the schematic.
void QucsView::endElementMoving()
{
  Element_Qucs *pe;
  QucsDoc *d = Docs.current();

  for(pe = movingElements.first(); pe!=0; pe = movingElements.next()) {
//    pe->isSelected = false;  // deselect first (maybe afterwards pe == NULL)
    switch(pe->Type) {
      case isWire:
	if(pe->x1 == pe->x2) if(pe->y1 == pe->y2) break;
	d->insertWire((Wire*)pe);
	break;
      case isDiagram:
	d->Diags->append((Diagram*)pe);
	break;
      case isPainting:
	d->Paints->append((Painting*)pe);
	break;
      case isComponent:
	d->insertRawComponent((Component*)pe);
	break;
      case isMovingLabel:
      case isHMovingLabel:
      case isVMovingLabel:
	d->insertNodeLabel((WireLabel*)pe);
	break;
      case isMarker:
	((Marker*)pe)->pGraph->Markers.append((Marker*)pe);
	break;
    }
  }

  movingElements.clear();
  if((MAx3 != 0) || (MAy3 != 0))  // moved or put at the same place ?
    d->setChanged(true, true);

  // enlarge viewarea if components lie outside the view
  d->sizeOfAll(d->UsedX1, d->UsedY1, d->UsedX2, d->UsedY2);
  enlargeView(d->UsedX1, d->UsedY1, d->UsedX2, d->UsedY2);
  viewport()->repaint();
  drawn = false;
}

// -----------------------------------------------------------
// Moves elements in "movingElements" by MAx1/MAy1
void QucsView::MovingElements()
{
  Wire *pw;
  Element_Qucs *pe;
  for(pe = movingElements.first(); pe != 0; pe = movingElements.next()) {
    if(pe->Type == isWire) {
      pw = (Wire*)pe;   // connecting wires are not moved completely

      if(((unsigned long)pw->Port1) > 3) {
	pw->x1 += MAx1;  pw->y1 += MAy1;
	if(pw->Label) { pw->Label->cx += MAx1;  pw->Label->cy += MAy1; }
      }
      else {  if(long(pw->Port1) & 1) { pw->x1 += MAx1; }
              if(long(pw->Port1) & 2) { pw->y1 += MAy1; } }

      if(long(pw->Port2) > 3) { pw->x2 += MAx1;  pw->y2 += MAy1; }
      else {  if(long(pw->Port2) & 1) pw->x2 += MAx1;
              if(long(pw->Port2) & 2) pw->y2 += MAy1; }

      if(pw->Label) {      // root of node label must lie on wire
        if(pw->Label->cx < pw->x1) pw->Label->cx = pw->x1;
        if(pw->Label->cy < pw->y1) pw->Label->cy = pw->y1;
        if(pw->Label->cx > pw->x2) pw->Label->cx = pw->x2;
        if(pw->Label->cy > pw->y2) pw->Label->cy = pw->y2;
      }

    }
    else pe->setCenter(MAx1, MAy1, true);
  }
}

// -----------------------------------------------------------
void QucsView::MouseDoNothing(QMouseEvent*)
{
}

// ***********************************************************************
// **********                                                   **********
// **********       Functions for serving mouse moving          **********
// **********                                                   **********
// ***********************************************************************
void QucsView::contentsMouseMoveEvent(QMouseEvent *Event)
{
  (this->*MouseMoveAction)(Event);
}

// -----------------------------------------------------------
void QucsView::MMovePainting(QMouseEvent *Event)
{
  //cout << "-----------Inside MMovePainting-------------------" << endl;
  QucsDoc *d = Docs.current();
   viewport()->repaint();
   viewport()->setAttribute(Qt::WA_PaintOutsidePaintEvent, true);
   //viewport()->setAttribute(Qt::WA_PaintOnScreen,true);
  QPainter painter(viewport()), paintScale(viewport());
  //painter.setRasterOp(Qt::NotROP);  // background should not be erased
  // Added by Shivam
  painter.setCompositionMode(QPainter::CompositionMode_SourceOut);
  
  setPainter(&paintScale, d);

  int x  = Event->pos().x();
  int y  = Event->pos().y();
  int fx = int(float(x)/d->Scale) + d->ViewX1;
  int fy = int(float(y)/d->Scale) + d->ViewY1;
  int gx = fx;
  int gy = fy;
  d->setOnGrid(gx, gy);

  x -= contentsX();
  y -= contentsY();
  selPaint->MouseMoving(&paintScale, fx, fy, gx, gy, &painter, x, y, drawn);
  drawn = true;
  //cout << "-----------Outside MMovePainting-------------------" << endl;
}

// -----------------------------------------------------------
void QucsView::MMoveWire2(QMouseEvent *Event)
{
  QucsDoc *d = Docs.current();
  viewport()->repaint();
  viewport()->setAttribute(Qt::WA_PaintOutsidePaintEvent, true);
  //viewport()->setAttribute(Qt::WA_PaintOnScreen,true);
  QPainter painter(viewport());
  //cout <<"---------------- Inside MMoveWire2------------------------" << endl;
  setPainter(&painter, d);

  if(drawn)
    if(MAx1 == 0) {
      //cout << "       Erasing Old      " << endl;
      painter.drawLine(MAx3, MAy3, MAx3, MAy2); // erase old
      painter.drawLine(MAx3, MAy2, MAx2, MAy2); // erase old
    }
    else {
      //cout << "       Erasing Old      " << endl;
      painter.drawLine(MAx3, MAy3, MAx2, MAy3); // erase old
      painter.drawLine(MAx2, MAy3, MAx2, MAy2); // erase old
    }
  else drawn = true;

  MAx2  = int(float(Event->pos().x())/d->Scale) + d->ViewX1;
  MAy2  = int(float(Event->pos().y())/d->Scale) + d->ViewY1;
  d->setOnGrid(MAx2, MAy2);

  if(MAx1 == 0) {
      //cout << "       Painting  New     " << endl;
    painter.drawLine(MAx3, MAy3, MAx3, MAy2); // paint
    painter.drawLine(MAx3, MAy2, MAx2, MAy2); // paint
  }
  else {
      //cout << "       Painting  New     " << endl;
    painter.drawLine(MAx3, MAy3, MAx2, MAy3); // paint
    painter.drawLine(MAx2, MAy3, MAx2, MAy2); // paint
  }
  //cout <<"---------------- Going out MMoveWire2------------------------" << endl;
}

// -----------------------------------------------------------
void QucsView::MMoveWire1(QMouseEvent *Event)
{
  QucsDoc *d = Docs.current();
  viewport()->repaint();
  viewport()->setAttribute(Qt::WA_PaintOutsidePaintEvent, true);
  //viewport()->setAttribute(Qt::WA_PaintOnScreen,true);
  QPainter painter(viewport());
  //cout <<"---------------- Inside MMoveWire1------------------------" << endl;
  setPainter(&painter, d);

  if(drawn) {
    //cout << "       Erasing Old      " << endl;
    painter.drawLine(MAx1, MAy3, MAx2, MAy3); // erase old
    //cout << MAx1 << " , " << MAy3 << " , " << MAx2 << " , " << MAy3 << endl;
    painter.drawLine(MAx3, MAy1, MAx3, MAy2);
    //cout << MAx3 << " , " << MAy1 << " , " << MAx3 << " , " << MAy2 << endl;
  }
  drawn = true;

  MAx3  = int(float(Event->pos().x())/d->Scale) + d->ViewX1;
  MAy3  = int(float(Event->pos().y())/d->Scale) + d->ViewY1;
  d->setOnGrid(MAx3, MAy3);

  MAx1  = contentsX()+d->ViewX1;
  MAy1  = contentsY()+d->ViewY1;
  MAx2  = MAx1 + visibleWidth();
  MAy2  = MAy1 + visibleHeight();

  painter.drawLine(MAx1, MAy3, MAx2, MAy3); // paint
  painter.drawLine(MAx3, MAy1, MAx3, MAy2);
  //cout <<"---------------- Going out MMoveWire1------------------------" << endl;
}

// -----------------------------------------------------------
void QucsView::MMoveComponent(QMouseEvent *Event)
{
  if(selComp == 0) return;

  //cout << "-------------Inside MMoveComponent----------------------" << endl;
  QucsDoc *d = Docs.current();
   viewport()->repaint();
   viewport()->setAttribute(Qt::WA_PaintOutsidePaintEvent, true);
   //viewport()->setAttribute(Qt::WA_PaintOnScreen,true);
  QPainter painter(viewport());
  setPainter(&painter, d);

  int x=int(float(Event->pos().x())/d->Scale) + d->ViewX1;
  int y=int(float(Event->pos().y())/d->Scale) + d->ViewY1;

  if(drawn) selComp->paintScheme(&painter); // erase old scheme
  drawn = true;
  d->setOnGrid(x, y);
  selComp->setCenter(x, y);
  selComp->paintScheme(&painter); // paint scheme at new position
  //cout << "------------------OutSide MMoveComponent----------------" << endl;
}

// -----------------------------------------------------------
void QucsView::MMoveDiagram(QMouseEvent *Event)
{
  //cout << "---------------Inside MMoveDiagram-----------------" << endl;
  if(selDiag == 0) return;

  QucsDoc *d = Docs.current();
   viewport()->repaint();
   viewport()->setAttribute(Qt::WA_PaintOutsidePaintEvent, true);
   //viewport()->setAttribute(Qt::WA_PaintOnScreen,true);
  QPainter painter(viewport());
  setPainter(&painter, d);

  int x=int(float(Event->pos().x())/d->Scale) + d->ViewX1;
  int y=int(float(Event->pos().y())/d->Scale) + d->ViewY1;

  if(drawn) selDiag->paintScheme(&painter); // erase old scheme
  drawn = true;
  d->setOnGrid(x, y);
  selDiag->setCenter(x, y);
  selDiag->paintScheme(&painter); // paint scheme at new position
  //cout << "---------------OutSide MMoveDiagram-----------------" << endl;
}

// -----------------------------------------------------------
// Paints a rectangle to select elements within it.
void QucsView::MMoveSelect(QMouseEvent *Event)
{
  //cout << "---------------Inside MMoveSelect-----------------" << endl;
  QucsDoc *d = Docs.current();
   viewport()->repaint();
   viewport()->setAttribute(Qt::WA_PaintOutsidePaintEvent, true);
   //viewport()->setAttribute(Qt::WA_PaintOnScreen,true);
  QPainter painter(viewport());
  setPainter(&painter, d);

  if(drawn) painter.drawRect(MAx1, MAy1, MAx2, MAy2); // erase old rectangle
  drawn = true;
  MAx2 = int(float(Event->pos().x())/d->Scale) + d->ViewX1 - MAx1;
  MAy2 = int(float(Event->pos().y())/d->Scale) + d->ViewY1 - MAy1;
  if(isMoveEqual)     // x and y size must be equal ?
  {
    if(abs(MAx2) > abs(MAy2)) 
    {
      if(MAx2<0) 
        MAx2 = -abs(MAy2); else MAx2 = abs(MAy2);
    }
    else 
    { 
      if(MAy2<0) 
        MAy2 = -abs(MAx2); else MAy2 = abs(MAx2); 
    }
  }
  painter.drawRect(MAx1, MAy1, MAx2, MAy2); // paint new rectangle
  //cout << "---------------Outside MMoveSelect-----------------" << endl;
}

// -----------------------------------------------------------
void QucsView::MMoveResizePainting(QMouseEvent *Event)
{
  //cout << "---------------Inside MMoveResizePainting-----------------" << endl;
  QucsDoc *d = Docs.current();
   viewport()->repaint(); 
   viewport()->setAttribute(Qt::WA_PaintOutsidePaintEvent, true);
   //viewport()->setAttribute(Qt::WA_PaintOnScreen,true);
  QPainter painter(viewport());
  setPainter(&painter, d);

  MAx1 = int(float(Event->pos().x())/d->Scale) + d->ViewX1;
  MAy1 = int(float(Event->pos().y())/d->Scale) + d->ViewY1;
  Docs.current()->setOnGrid(MAx1, MAy1);
  ((Painting*)focusElement)->MouseResizeMoving(MAx1, MAy1, &painter);
  //cout << "---------------Outside MMoveResizePainting-----------------" << endl;
}

// -----------------------------------------------------------
// Moves components by keeping the mouse button pressed.
void QucsView::MMoveMoving(QMouseEvent *Event)
{
  //cout << "---------------Inside MMoveMoving-----------------" << endl;
    QucsDoc *d = Docs.current();
   // Commented by Shivam
   //viewport()->setAttribute(Qt::WA_PaintOutsidePaintEvent, true);
   ////viewport()->setAttribute(Qt::WA_PaintOnScreen,true);                    
  //QPainter painter(viewport());
  //setPainter(&painter, d);

  MAx2 = int(Event->pos().x()/d->Scale) + d->ViewX1;
  MAy2 = int(Event->pos().y()/d->Scale) + d->ViewY1;

  d->setOnGrid(MAx2, MAy2);
  MAx3 = MAx1 = MAx2 - MAx1;
  MAy3 = MAy1 = MAy2 - MAy1;

  movingElements.clear();
  d->copySelectedElements(&movingElements);
  viewport()->repaint();
     // Painter Initialized after repaint to avoid the warning: A paint device can only be painted by one painter at a time
    viewport()->setAttribute(Qt::WA_PaintOutsidePaintEvent, true);
    //viewport()->setAttribute(Qt::WA_PaintOnScreen,true);                    
    QPainter painter(viewport());
    setPainter(&painter, d);
  
  Wire *pw;
  // Changes the position of all moving elements by dx/dy
  for(Element_Qucs *pe=movingElements.first(); pe!=0; pe=movingElements.next()) {
    if(pe->Type == isWire) {
      pw = (Wire*)pe;   // connecting wires are not moved completely

      if(((unsigned long)pw->Port1) > 3) { pw->x1 += MAx1;  pw->y1 += MAy1; }
      else {  if(long(pw->Port1) & 1) { pw->x1 += MAx1; }
              if(long(pw->Port1) & 2) { pw->y1 += MAy1; } }

      if(long(pw->Port2) > 3) { pw->x2 += MAx1;  pw->y2 += MAy1; }
      else {  if(long(pw->Port2) & 1) pw->x2 += MAx1;
              if(long(pw->Port2) & 2) pw->y2 += MAy1; }

      if(pw->Label) {      // root of node label must lie on wire
        if(pw->Label->cx < pw->x1) pw->Label->cx = pw->x1;
        if(pw->Label->cy < pw->y1) pw->Label->cy = pw->y1;
        if(pw->Label->cx > pw->x2) pw->Label->cx = pw->x2;
        if(pw->Label->cy > pw->y2) pw->Label->cy = pw->y2;
      }

    }
    else pe->setCenter(MAx1, MAy1, true);

    pe->paintScheme(&painter);
  }

  drawn = true;
  MAx1 = MAx2;
  MAy1 = MAy2;
  MouseMoveAction = &QucsView::MMoveMoving2;
  MouseReleaseAction = &QucsView::MReleaseMoving;
 //cout << "---------------Outside MMoveMoving-----------------" << endl;
}

// -----------------------------------------------------------
// Moves components by keeping the mouse button pressed.
void QucsView::MMoveMoving2(QMouseEvent *Event)
{
  //cout << "---------------Inside MMoveMoving2-----------------" << endl;
  QucsDoc *d = Docs.current();
  viewport()->repaint(); 
  viewport()->setAttribute(Qt::WA_PaintOutsidePaintEvent, true);                                                                                                                                   
  //viewport()->setAttribute(Qt::WA_PaintOnScreen,true);         
  QPainter painter(viewport());
  setPainter(&painter, d);

  MAx2 = int(float(Event->pos().x())/d->Scale) + d->ViewX1;
  MAy2 = int(float(Event->pos().y())/d->Scale) + d->ViewY1;

  Element_Qucs *pe;
  if(drawn) // erase old scheme
    for(pe = movingElements.first(); pe != 0; pe = movingElements.next())
      pe->paintScheme(&painter);
//      if(pe->Type == isWire)  if(((Wire*)pe)->Label)
//        if(!((Wire*)pe)->Label->isSelected)
//          ((Wire*)pe)->Label->paintScheme(&painter);

  drawn = true;
  d->setOnGrid(MAx2, MAy2);
  MAx1 = MAx2 - MAx1;
  MAy1 = MAy2 - MAy1;
  MAx3 += MAx1;  MAy3 += MAy1;   // keep track of the complete movement

  MovingElements();   // moves elements by MAx1/MAy1

  // paint afterwards to avoid conflict between wire and label painting
  for(pe = movingElements.first(); pe != 0; pe = movingElements.next())
    pe->paintScheme(&painter);
//    if(pe->Type == isWire)  if(((Wire*)pe)->Label)
//      if(!((Wire*)pe)->Label->isSelected)
//        ((Wire*)pe)->Label->paintScheme(&painter);

  MAx1 = MAx2;
  MAy1 = MAy2;
  //cout << "---------------Outside MMoveMoving2-----------------" << endl;
}

// -----------------------------------------------------------
// Moves components after paste from clipboard.
void QucsView::MMovePaste(QMouseEvent *Event)
{
  //cout << "--------------Inside MMovePaste-----------------" << endl;
  QucsDoc *d = Docs.current();
  viewport()->repaint(); 
  viewport()->setAttribute(Qt::WA_PaintOutsidePaintEvent, true);                                                                                                                                   
  //viewport()->setAttribute(Qt::WA_PaintOnScreen,true);         
  QPainter painter(viewport());
  setPainter(&painter, d);

  MAx1 = int(float(Event->pos().x())/d->Scale) + d->ViewX1;
  MAy1 = int(float(Event->pos().y())/d->Scale) + d->ViewY1;
  d->setOnGrid(MAx1, MAy1);

  for(Element_Qucs *pe=movingElements.first(); pe!=0; pe=movingElements.next()) {
    if(pe->Type & isLabel) {
      pe->cx += MAx1;  pe->x1 += MAx1;
      pe->cy += MAy1;  pe->y1 += MAy1;
    }
    else
      pe->setCenter(MAx1, MAy1, true);
    pe->paintScheme(&painter);
  }

  drawn = true;
  MouseMoveAction = &QucsView::MMoveMoving2;
  MouseReleaseAction = &QucsView::MReleasePaste;
  //cout << "--------------Outside MMovePaste-----------------" << endl;
}

// -----------------------------------------------------------
// Paints a cross under the mouse cursor to show the delete modus.
void QucsView::MMoveDelete(QMouseEvent *Event)
{
  //cout << "--------------Inside MMoveDelete-----------------" << endl;
  viewport()->repaint(); 
  viewport()->setAttribute(Qt::WA_PaintOutsidePaintEvent, true);                                                                                                                                   
  //viewport()->setAttribute(Qt::WA_PaintOnScreen,true);         
  QPainter painter(viewport());
  //painter.setRasterOp(Qt::NotROP);  // background should not be erased
  // Added by Shivam
  painter.setCompositionMode(QPainter::CompositionMode_SourceOut);
  if(drawn) {
    painter.drawLine(MAx3-15, MAy3-15, MAx3+15, MAy3+15); // erase old
    painter.drawLine(MAx3-15, MAy3+15, MAx3+15, MAy3-15);
  }
  drawn = true;

  MAx3  = Event->pos().x() - contentsX();
  MAy3  = Event->pos().y() - contentsY();

  painter.drawLine(MAx3-15, MAy3-15, MAx3+15, MAy3+15); // paint
  painter.drawLine(MAx3-15, MAy3+15, MAx3+15, MAy3-15);
  //cout << "--------------Outside MMoveDelete-----------------" << endl;
}

// -----------------------------------------------------------
// Paints a label above the mouse cursor to show the set wire label modus.
void QucsView::MMoveLabel(QMouseEvent *Event)
{
  //cout << "---------------------Inside MMoveLabel------------" << endl;
  viewport()->repaint(); 
  viewport()->setAttribute(Qt::WA_PaintOutsidePaintEvent, true);                                                                                                                                   
  //viewport()->setAttribute(Qt::WA_PaintOnScreen,true);         
  QPainter painter(viewport());
  //painter.setRasterOp(Qt::NotROP);  // background should not be erased
  // Added by Shivam
  painter.setCompositionMode(QPainter::CompositionMode_SourceOut);

  if(drawn) {
    painter.drawLine(MAx3, MAy3, MAx3+10, MAy3-10); // erase old
    painter.drawLine(MAx3+10, MAy3-10, MAx3+20, MAy3-10);
    painter.drawLine(MAx3+10, MAy3-10, MAx3+10, MAy3-17);

    painter.drawLine(MAx3+12, MAy3-12, MAx3+15, MAy3-23);   // "A"
    painter.drawLine(MAx3+14, MAy3-17, MAx3+17, MAy3-17);
    painter.drawLine(MAx3+19, MAy3-12, MAx3+16, MAy3-23);
  }
  drawn = true;

  MAx3  = Event->pos().x() - contentsX();
  MAy3  = Event->pos().y() - contentsY();

  painter.drawLine(MAx3, MAy3, MAx3+10, MAy3-10); // paint new
  painter.drawLine(MAx3+10, MAy3-10, MAx3+20, MAy3-10);
  painter.drawLine(MAx3+10, MAy3-10, MAx3+10, MAy3-17);

  painter.drawLine(MAx3+12, MAy3-12, MAx3+15, MAy3-23);   // "A"
  painter.drawLine(MAx3+14, MAy3-17, MAx3+17, MAy3-17);
  painter.drawLine(MAx3+19, MAy3-12, MAx3+16, MAy3-23);
  //cout << "--------------Outside MMoveLabel-----------------" << endl;
}

// -----------------------------------------------------------
// Paints a triangle above the mouse cursor to show the set marker modus.
void QucsView::MMoveMarker(QMouseEvent *Event)
{
   //cout << "--------------Inside MMoveMarker-----------------" << endl;
  viewport()->repaint(); 
  viewport()->setAttribute(Qt::WA_PaintOutsidePaintEvent, true);                                                                                                                                   
  //viewport()->setAttribute(Qt::WA_PaintOnScreen,true);         
  QPainter painter(viewport());
  //painter.setRasterOp(Qt::NotROP);  // background should not be erased
  // Added by Shivam
    painter.setCompositionMode(QPainter::CompositionMode_SourceOut);
  if(drawn) {
    painter.drawLine(MAx3, MAy3-2, MAx3-8, MAy3-10); // erase old
    painter.drawLine(MAx3+1, MAy3-3, MAx3+8, MAy3-10);
    painter.drawLine(MAx3-7, MAy3-10, MAx3+7, MAy3-10);
  }
  drawn = true;

  MAx3  = Event->pos().x() - contentsX();
  MAy3  = Event->pos().y() - contentsY();

  painter.drawLine(MAx3, MAy3-2, MAx3-8, MAy3-10); // paint new
  painter.drawLine(MAx3+1, MAy3-3, MAx3+8, MAy3-10);
  painter.drawLine(MAx3-7, MAy3-10, MAx3+7, MAy3-10);
  //cout << "--------------Outside MMoveMarker-----------------" << endl;
}

// -----------------------------------------------------------
// Paints rounded arrows above the mouse cursor to show the
// "mirror about y axis" modus.
void QucsView::MMoveMirrorY(QMouseEvent *Event)
{
  //cout << "--------------------Inside MMoveMirrorY---------------" << endl;
  viewport()->repaint(); 
  viewport()->setAttribute(Qt::WA_PaintOutsidePaintEvent, true);                                                                                                                                   
  //viewport()->setAttribute(Qt::WA_PaintOnScreen,true);         
  QPainter painter(viewport());
  //painter.setRasterOp(Qt::NotROP);  // background should not be erased
  // Added by Shivam
  painter.setCompositionMode(QPainter::CompositionMode_SourceOut);
  if(drawn) {
    painter.drawLine(MAx3-11, MAy3-4, MAx3-9, MAy3-9); // erase old
    painter.drawLine(MAx3-11, MAy3-3, MAx3-6, MAy3-3);
    painter.drawLine(MAx3+11, MAy3-4, MAx3+9, MAy3-9);
    painter.drawLine(MAx3+11, MAy3-3, MAx3+6, MAy3-3);
    painter.drawArc(MAx3-10, MAy3-8, 21, 10, 16*20, 16*140);
  }
  drawn = true;

  MAx3  = Event->pos().x() - contentsX();
  MAy3  = Event->pos().y() - contentsY();

  painter.drawLine(MAx3-11, MAy3-4, MAx3-9, MAy3-9); // paint new
  painter.drawLine(MAx3-11, MAy3-3, MAx3-6, MAy3-3);
  painter.drawLine(MAx3+11, MAy3-4, MAx3+9, MAy3-9);
  painter.drawLine(MAx3+11, MAy3-3, MAx3+6, MAy3-3);
  painter.drawArc(MAx3-10, MAy3-8, 21, 10, 16*20, 16*140);
  //cout << "------------------------OutSide MMoveMirrorY---------------" << endl;
}

// -----------------------------------------------------------
// Paints rounded arrows beside the mouse cursor to show the
// "mirror about x axis" modus.
void QucsView::MMoveMirrorX(QMouseEvent *Event)
{
  //cout << "------------------------InSide MMoveMirrorX---------------" << endl;
  viewport()->repaint(); 
  viewport()->setAttribute(Qt::WA_PaintOutsidePaintEvent, true);                                                                                                                                   
  //viewport()->setAttribute(Qt::WA_PaintOnScreen,true);         
  QPainter painter(viewport());
  //painter.setRasterOp(Qt::NotROP);  // background should not be erased
  // Added by Shivam
  painter.setCompositionMode(QPainter::CompositionMode_SourceOut);

  if(drawn) {
    painter.drawLine(MAx3-4, MAy3-11, MAx3-9, MAy3-9); // erase old
    painter.drawLine(MAx3-3, MAy3-11, MAx3-3, MAy3-6);
    painter.drawLine(MAx3-4, MAy3+11, MAx3-9, MAy3+9);
    painter.drawLine(MAx3-3, MAy3+11, MAx3-3, MAy3+6);
    painter.drawArc(MAx3-8, MAy3-10, 10, 21, 16*110, 16*140);
  }
  drawn = true;

  MAx3  = Event->pos().x() - contentsX();
  MAy3  = Event->pos().y() - contentsY();

  painter.drawLine(MAx3-4, MAy3-11, MAx3-9, MAy3-9); // paint new
  painter.drawLine(MAx3-3, MAy3-11, MAx3-3, MAy3-6);
  painter.drawLine(MAx3-4, MAy3+11, MAx3-9, MAy3+9);
  painter.drawLine(MAx3-3, MAy3+11, MAx3-3, MAy3+6);
  painter.drawArc(MAx3-8, MAy3-10, 10, 21, 16*110, 16*140);
  //cout << "------------------------OutSide MMoveMirrorX---------------" << endl;
}

// -----------------------------------------------------------
// Paints a rounded arrow above the mouse cursor to show the "rotate" modus.
void QucsView::MMoveRotate(QMouseEvent *Event)
{
  //cout << "------------------------InSide MMoveRotate---------------" << endl;
  viewport()->repaint(); 
  viewport()->setAttribute(Qt::WA_PaintOutsidePaintEvent, true);                                                                                                                                   
  //viewport()->setAttribute(Qt::WA_PaintOnScreen,true);         
  QPainter painter(viewport());
  //painter.setRasterOp(Qt::NotROP);  // background should not be erased
  // Added by Shivam
  painter.setCompositionMode(QPainter::CompositionMode_SourceOut);
  if(drawn) {
    painter.drawLine(MAx3-6, MAy3+8, MAx3-6, MAy3+1); // erase old
    painter.drawLine(MAx3-7, MAy3+8, MAx3-12, MAy3+8);
    painter.drawArc(MAx3-10, MAy3-10, 21, 21, -16*20, 16*240);
  }
  drawn = true;

  MAx3  = Event->pos().x() - contentsX();
  MAy3  = Event->pos().y() - contentsY();

  painter.drawLine(MAx3-6, MAy3+8, MAx3-6, MAy3+1); // paint new
  painter.drawLine(MAx3-7, MAy3+8, MAx3-12, MAy3+8);
  painter.drawArc(MAx3-10, MAy3-10, 21, 21, -16*20, 16*240);
  //cout << "------------------------OutSide MMoveRotate---------------" << endl;
}

// -----------------------------------------------------------
// Paints a rectangle beside the mouse cursor to show the "activate" modus.
void QucsView::MMoveActivate(QMouseEvent *Event)
{
  //cout << "------------------------OutSide MMoveActivate---------------" << endl;
  viewport()->repaint(); 
  viewport()->setAttribute(Qt::WA_PaintOutsidePaintEvent, true);                                                                                                                                   
  //viewport()->setAttribute(Qt::WA_PaintOnScreen,true);         
  QPainter painter(viewport());
  //painter.setRasterOp(Qt::NotROP);  // background should not be erased
  // Added by Shivam
  painter.setCompositionMode(QPainter::CompositionMode_SourceOut);

  if(drawn) {
    painter.drawLine(MAx3, MAy3-9, MAx3+13, MAy3-9); // erase old
    painter.drawLine(MAx3, MAy3, MAx3+13, MAy3);
    painter.drawLine(MAx3, MAy3-9, MAx3, MAy3);
    painter.drawLine(MAx3+13, MAy3-9, MAx3+13, MAy3);
    painter.drawLine(MAx3, MAy3-9, MAx3+13, MAy3);
    painter.drawLine(MAx3, MAy3, MAx3+13, MAy3-9);
  }
  drawn = true;

  MAx3  = Event->pos().x() - contentsX();
  MAy3  = Event->pos().y() - contentsY();

  painter.drawLine(MAx3, MAy3-9, MAx3+13, MAy3-9); // paint new
  painter.drawLine(MAx3, MAy3, MAx3+13, MAy3);
  painter.drawLine(MAx3, MAy3-9, MAx3, MAy3);
  painter.drawLine(MAx3+13, MAy3-9, MAx3+13, MAy3);
  painter.drawLine(MAx3, MAy3-9, MAx3+13, MAy3);
  painter.drawLine(MAx3, MAy3, MAx3+13, MAy3-9);
  //cout << "------------------------OutSide MMoveActivate---------------" << endl;
}

// -----------------------------------------------------------
// Paints a grid beside the mouse cursor to show the "on grid" modus.
void QucsView::MMoveOnGrid(QMouseEvent *Event)
{
  //cout << "------------------------InSide MMoveOnGrid---------------" << endl;
  viewport()->repaint(); 
  viewport()->setAttribute(Qt::WA_PaintOutsidePaintEvent, true);                                                                                                                                   
  //viewport()->setAttribute(Qt::WA_PaintOnScreen,true);         
  QPainter painter(viewport());
  //painter.setRasterOp(Qt::NotROP);  // background should not be erased
  // Added by Shivam
  painter.setCompositionMode(QPainter::CompositionMode_SourceOut);

  if(drawn) {
    painter.drawLine(MAx3+10, MAy3+ 3, MAx3+25, MAy3+3); // erase old
    painter.drawLine(MAx3+10, MAy3+ 7, MAx3+25, MAy3+7);
    painter.drawLine(MAx3+10, MAy3+11, MAx3+25, MAy3+11);
    painter.drawLine(MAx3+13, MAy3, MAx3+13, MAy3+15);
    painter.drawLine(MAx3+17, MAy3, MAx3+17, MAy3+15);
    painter.drawLine(MAx3+21, MAy3, MAx3+21, MAy3+15);
  }
  drawn = true;

  MAx3  = Event->pos().x() - contentsX();
  MAy3  = Event->pos().y() - contentsY();

  painter.drawLine(MAx3+10, MAy3+ 3, MAx3+25, MAy3+3); // paint new
  painter.drawLine(MAx3+10, MAy3+ 7, MAx3+25, MAy3+7);
  painter.drawLine(MAx3+10, MAy3+11, MAx3+25, MAy3+11);
  painter.drawLine(MAx3+13, MAy3, MAx3+13, MAy3+15);
  painter.drawLine(MAx3+17, MAy3, MAx3+17, MAy3+15);
  painter.drawLine(MAx3+21, MAy3, MAx3+21, MAy3+15);
  //cout << "------------------------OutSide MMoveOnGrid---------------" << endl;
}

// -----------------------------------------------------------
// Paints symbol beside the mouse to show the "move component text" modus.
void QucsView::MMoveMoveTextB(QMouseEvent *Event)
{
  //cout << "--------------------------Inside MMoveMoveTextB-----------------" << endl;
  viewport()->repaint(); 
  viewport()->setAttribute(Qt::WA_PaintOutsidePaintEvent, true);                                                                                                                                   
  //viewport()->setAttribute(Qt::WA_PaintOnScreen,true);         
  QPainter painter(viewport());
  //painter.setRasterOp(Qt::NotROP);  // background should not be erased
  // Added by Shivam
  painter.setCompositionMode(QPainter::CompositionMode_SourceOut);

  if(drawn) {
    painter.drawLine(MAx3+14, MAy3   , MAx3+16, MAy3); // erase old
    painter.drawLine(MAx3+23, MAy3   , MAx3+25, MAy3);
    painter.drawLine(MAx3+13, MAy3   , MAx3+13, MAy3+ 3);
    painter.drawLine(MAx3+13, MAy3+ 7, MAx3+13, MAy3+10);
    painter.drawLine(MAx3+14, MAy3+10, MAx3+16, MAy3+10);
    painter.drawLine(MAx3+23, MAy3+10, MAx3+25, MAy3+10);
    painter.drawLine(MAx3+26, MAy3   , MAx3+26, MAy3+ 3);
    painter.drawLine(MAx3+26, MAy3+ 7, MAx3+26, MAy3+10);
  }
  drawn = true;

  MAx3 = Event->pos().x() - contentsX();
  MAy3 = Event->pos().y() - contentsY();

  painter.drawLine(MAx3+14, MAy3   , MAx3+16, MAy3); // paint new
  painter.drawLine(MAx3+23, MAy3   , MAx3+25, MAy3);
  painter.drawLine(MAx3+13, MAy3   , MAx3+13, MAy3+ 3);
  painter.drawLine(MAx3+13, MAy3+ 7, MAx3+13, MAy3+10);
  painter.drawLine(MAx3+14, MAy3+10, MAx3+16, MAy3+10);
  painter.drawLine(MAx3+23, MAy3+10, MAx3+25, MAy3+10);
  painter.drawLine(MAx3+26, MAy3   , MAx3+26, MAy3+ 3);
  painter.drawLine(MAx3+26, MAy3+ 7, MAx3+26, MAy3+10);
  //cout << "--------------------------Outside MMoveMoveTextB-----------------" << endl;
}

// -----------------------------------------------------------
void QucsView::MMoveMoveText(QMouseEvent *Event)
{
  //cout << "--------------------------Inside MMoveMoveText-----------------" << endl;
  QucsDoc *d = Docs.current();
  viewport()->repaint(); 
  viewport()->setAttribute(Qt::WA_PaintOutsidePaintEvent, true);                                                                                                                                   
  //viewport()->setAttribute(Qt::WA_PaintOnScreen,true);         
  QPainter painter(viewport());
  setPainter(&painter, d);

  if(drawn)
    painter.drawRect(MAx1, MAy1, MAx2, MAy2); // erase old
  drawn = true;

  int newX = int(float(Event->pos().x())/d->Scale) + d->ViewX1;
  int newY = int(float(Event->pos().y())/d->Scale) + d->ViewY1;
  MAx1 += newX - MAx3;
  MAy1 += newY - MAy3;
  MAx3  = newX;
  MAy3  = newY;

  painter.drawRect(MAx1, MAy1, MAx2, MAy2); // paint new
  //cout << "--------------------------Outside MMoveMoveText-----------------" << endl;
}

// -----------------------------------------------------------
// Paints symbol beside the mouse to show the "Zoom in" modus.
void QucsView::MMoveZoomIn(QMouseEvent *Event)
{
  //cout << "--------------------------Inside MMoveZoomIn----------------" << endl; 
  viewport()->repaint(); 
  viewport()->setAttribute(Qt::WA_PaintOutsidePaintEvent, true);                                                                                                                                   
  //viewport()->setAttribute(Qt::WA_PaintOnScreen,true);         
  QPainter painter(viewport());
  //painter.setRasterOp(Qt::NotROP);  // background should not be erased
  // Added by Shivam
  painter.setCompositionMode(QPainter::CompositionMode_SourceOut);

  if(drawn) {
    painter.drawLine(MAx3+14, MAy3   , MAx3+22, MAy3); // erase old
    painter.drawLine(MAx3+18, MAy3-4 , MAx3+18, MAy3+4);
    painter.drawEllipse(MAx3+12, MAy3-6, 13, 13);
  }
  drawn = true;

  MAx3 = Event->pos().x() - contentsX();
  MAy3 = Event->pos().y() - contentsY();

  painter.drawLine(MAx3+14, MAy3   , MAx3+22, MAy3);  // paint new
  painter.drawLine(MAx3+18, MAy3-4 , MAx3+18, MAy3+4);
  painter.drawEllipse(MAx3+12, MAy3-6, 13, 13);
  //cout << "--------------------------Outside MMoveZoomIn----------------" << endl;
}


// ************************************************************************
// **********                                                    **********
// **********    Functions for serving mouse button clicking     **********
// **********                                                    **********
// ************************************************************************
void QucsView::contentsMousePressEvent(QMouseEvent *Event)
{
  (this->*MousePressAction)(Event);
}

// -----------------------------------------------------------
// Is called from several MousePress functions to show right button menu.
void QucsView::rightPressMenu(QMouseEvent *Event)
{
  //cout << "------------------ Inside rightPressMenu------------------------" << endl;
  QucsDoc *d = Docs.current();
  MAx1 = int(float(Event->pos().x())/d->Scale) + d->ViewX1;
  MAy1 = int(float(Event->pos().y())/d->Scale) + d->ViewY1;
  focusElement = d->selectElement(MAx1, MAy1, false);

  if(focusElement)  // remove special function (4 least significant bits)
    focusElement->Type &= isSpecialMask;


  // define menu
  ComponentMenu->clear();
  while(true) {
    if(focusElement) {
      focusElement->isSelected = true;
      ComponentMenu->insertItem(tr("Edit Properties"), this, SLOT(slotEditElement()));

      if(focusElement->Type != isComponent) break;
    }
    QucsMain->Acts.moveText->addTo(ComponentMenu);
    break;
  }
  QucsMain->Acts.onGrid->addTo(ComponentMenu);
  QucsMain->editCopy->addTo(ComponentMenu);
  QucsMain->Acts.editPaste->addTo(ComponentMenu);
  QucsMain->Acts.editDelete->addTo(ComponentMenu);
  while(true) {
    if(focusElement) if(focusElement->Type == isDiagram) break;
    ComponentMenu->insertSeparator();
    if(focusElement) if(focusElement->Type == isComponent)
      QucsMain->Acts.editActivate->addTo(ComponentMenu);
    QucsMain->Acts.editRotate->addTo(ComponentMenu);
    QucsMain->Acts.editMirror->addTo(ComponentMenu);
    QucsMain->Acts.editMirrorY->addTo(ComponentMenu);
    break;
  }


  *focusMEvent = *Event;  // remember event for "edit component" action
  ComponentMenu->popup(Event->globalPos());
  viewport()->repaint();
  drawn = false;
  //cout << "------------------ Outside rightPressMenu------------------------" << endl;
}

// -----------------------------------------------------------
void QucsView::MPressPainting(QMouseEvent *)
{
  //cout << "---------------------Inside MPressPainting------------------" << endl;
  if(selPaint->MousePressing()) {
    Docs.current()->Paints->append(selPaint);
    selPaint = selPaint->newOne();

    drawn = false;
    viewport()->repaint();
    Docs.current()->setChanged(true, true);
  }
  //cout << "---------------------Outside MPressPainting------------------" << endl;
}

// -----------------------------------------------------------
void QucsView::PressLabel(QMouseEvent *Event)
{
  //cout << "---------------Inside PressLabel-----------" << endl;
  QucsDoc *d = Docs.current();
  int x = int(float(Event->pos().x())/d->Scale) + d->ViewX1;
  int y = int(float(Event->pos().y())/d->Scale) + d->ViewY1;
  Wire *pw = 0;
  WireLabel *pl=0;
  Node *pn = d->selectedNode(x, y);
  if(!pn) {
    pw = d->selectedWire(x, y);
    if(!pw) return;
  }

  QString Name, Value;
  Element_Qucs *pe=0;
  // is wire line already labeled ?
  if(pw) pe = d->getWireLabel(pw->Port1);
  else pe = d->getWireLabel(pn);
  if(pe) {
    if(pe->Type == isComponent) {
      QMessageBox::information(0, tr("Info"),
			tr("The ground potential cannot be labeled!"));
      return;
    }
    if(pe->Type == isNode) pl = ((Node*)pe)->Label;
    else pl = ((Wire*)pe)->Label;
//    Name  = pl->Name;
//    Value = pl->initValue;
  }

  LabelDialog *Dia = new LabelDialog(pl, this);
  if(Dia->exec() == 0) return;

  Name  = Dia->NodeName->text();
  Value = Dia->InitValue->text();
  delete Dia;
//  bool OK;
//  Name = QInputDialog::getText(tr("Insert Nodename"),
//		tr("Enter the label:"), QLineEdit::Normal, Name, &OK, this);
//  if(!OK) return;

  if(Name.isEmpty() && Value.isEmpty() ) { // if nothing entered, delete name
    if(pe) {
      if(pe->Type == isWire) ((Wire*)pe)->setName("", ""); // delete old name
      else ((Node*)pe)->setName("", "");
    }
    else {
      if(pw) pw->setName("", "");   // delete name of wire
      else pn->setName("", "");
    }
  }
  else {
/*    Name.replace(' ', '_');	// label must not contain spaces
    while(Name.at(0) == '_') Name.remove(0,1);  // must not start with '_'
    if(Name.isEmpty()) return;
*/
    if(pe) {
      if(pe->Type == isWire) ((Wire*)pe)->setName("", "");  // delete old name
      else ((Node*)pe)->setName("", "");
    }

    int xl = x+30;
    int yl = y-30;
    d->setOnGrid(xl, yl);
    // set new name
    if(pw) pw->setName(Name, Value, x-pw->x1 + y-pw->y1, xl, yl);
    else pn->setName(Name, Value, xl, yl);
  }

  d->sizeOfAll(d->UsedX1, d->UsedY1, d->UsedX2, d->UsedY2);
  viewport()->repaint();
  drawn = false;
  Docs.current()->setChanged(true, true);
  //cout << "---------------Outside PressLabel-----------" << endl;
}

// -----------------------------------------------------------
void QucsView::MPressLabel(QMouseEvent *Event)
{
  //cout << "-------------Inside MPressLabel: Menu on Right mouse button-----------" << endl;
  if(Event->button() != Qt::LeftButton) {
    rightPressMenu(Event);   // show menu on right mouse button
    return;
  }

  PressLabel(Event);
  //cout << "-------------Outside MPressLabel: Menu on Right mouse button-----------" << endl;
}

// -----------------------------------------------------------
void QucsView::MPressSelect(QMouseEvent *Event)
{
  //cout << "------------------------Inside MPressSelect---------------------" << endl;
  editText->setHidden(true); // disable text edit of component property
  
  if(Event->button() != Qt::LeftButton) {
    rightPressMenu(Event);
    return;
  }

  bool Ctrl;
  if(Event->state() & Qt::ControlButton) Ctrl = true;
  else Ctrl = false;

  int No=0;
  QucsDoc *d = Docs.current();
  MAx1 = int(float(Event->pos().x())/d->Scale) + d->ViewX1;
  MAy1 = int(float(Event->pos().y())/d->Scale) + d->ViewY1;
  focusElement = d->selectElement(MAx1, MAy1, Ctrl, &No);
  isMoveEqual = false;   // moving not neccessarily square


  if(focusElement)
  switch(focusElement->Type) {
    case isPaintingResize:  // resize painting ?
	focusElement->Type = isPainting;
	MouseReleaseAction = &QucsView::MReleaseResizePainting;
	MouseMoveAction = &QucsView::MMoveResizePainting;
	MousePressAction = &QucsView::MouseDoNothing;
	MouseDoubleClickAction = &QucsView::MouseDoNothing;
	this->grabKeyboard();  // no keyboard inputs during move actions
	return;

    case isDiagramResize:  // resize diagram ?
	if(((Diagram*)focusElement)->Name.left(4) != "Rect")
	  if(((Diagram*)focusElement)->Name != "Tab")
	    if(((Diagram*)focusElement)->Name != "Curve")
	      isMoveEqual = true;  // diagram must be square

	focusElement->Type = isDiagram;
	MAx1 = focusElement->cx;
	MAx2 = focusElement->x2;
	if(((Diagram*)focusElement)->State & 1) {
	  MAx1 += MAx2;
	  MAx2 *= -1;
	}
	MAy1 =  focusElement->cy;
	MAy2 = -focusElement->y2;
	if(((Diagram*)focusElement)->State & 2) {
	  MAy1 += MAy2;
	  MAy2 *= -1;
	}

//	Docs.current()->setOnGrid(MAx1, MAy1);
	MouseReleaseAction = &QucsView::MReleaseResizeDiagram;
	MouseMoveAction = &QucsView::MMoveSelect;
	MousePressAction = &QucsView::MouseDoNothing;
	MouseDoubleClickAction = &QucsView::MouseDoNothing;
	this->grabKeyboard(); // no keyboard inputs during move actions
	return;

    case isDiagramScroll:  // scroll in tabular ?
	focusElement->Type = isDiagram;

	if(((TabDiagram*)focusElement)->scroll(MAy1))
	  d->setChanged(true, true, 'm'); // 'm' = only the first time
	viewport()->repaint();
	drawn = false;
//	focusElement = 0;  // avoid double-click on diagram scrollbar
	return;

    case isComponentText:  // property text of component ?
	focusElement->Type = isComponent;

	MAx3 = No;
	slotApplyCompText();
	return;
  }



  MousePressAction = &QucsView::MouseDoNothing;
  MouseDoubleClickAction = &QucsView::MouseDoNothing;
  this->grabKeyboard();  // no keyboard inputs during move actions
  viewport()->repaint();
  drawn = false;

  if(focusElement == 0) {
    MAx2 = 0;  // if not clicking on an element => open a rectangle
    MAy2 = 0;
    MouseReleaseAction = &QucsView::MReleaseSelect2;
    MouseMoveAction = &QucsView::MMoveSelect;
  }
  else {  // element could be moved
    if(!Ctrl) {
      if(!focusElement->isSelected)// Don't move selected elements if clicked
        d->deselectElements(focusElement); // element was not selected.
      focusElement->isSelected = true;
    }
    d->setOnGrid(MAx1, MAy1);
    MouseMoveAction = &QucsView::MMoveMoving;
  }
  //cout << "------------------------Inside MPressSelect---------------------" << endl;
}

// -----------------------------------------------------------
void QucsView::MPressDelete(QMouseEvent *Event)
{
  //cout << "------------------------Inside MPressDelete-------------------" << endl;
  if(Event->button() != Qt::LeftButton) {
    rightPressMenu(Event);   // show menu on right mouse button
    return;
  }

  QucsDoc *d = Docs.current();

  Element_Qucs *pe = d->selectElement(
		int(float(Event->pos().x())/d->Scale)+d->ViewX1,
		int(float(Event->pos().y())/d->Scale)+d->ViewY1, false);
  if(pe) {
    pe->isSelected = true;
    d->deleteElements();

    d->sizeOfAll(d->UsedX1, d->UsedY1, d->UsedX2, d->UsedY2);
    viewport()->repaint();
    drawn = false;
  }
  //cout << "------------------------Outside MPressDelete-------------------" << endl;
}

// -----------------------------------------------------------
void QucsView::MPressActivate(QMouseEvent *Event)
{
  //cout << "-----------------------Inside MPressActivate---------------------" << endl;
  if(Event->button() != Qt::LeftButton) {
    rightPressMenu(Event);   // show menu on right mouse button
    return;
  }

  QucsDoc *d = Docs.current();
  MAx1 = int(float(Event->pos().x())/d->Scale) + d->ViewX1;
  MAy1 = int(float(Event->pos().y())/d->Scale) + d->ViewY1;
  if(!d->activateComponent(MAx1, MAy1)) {
//    if(Event->button() != Qt::LeftButton) return;
    MAx2 = 0;  // if not clicking on a component => open a rectangle
    MAy2 = 0;
    MousePressAction = &QucsView::MouseDoNothing;
    MouseReleaseAction = &QucsView::MReleaseActivate;
    MouseMoveAction = &QucsView::MMoveSelect;
  }
  viewport()->repaint();
  drawn = false;
  //cout << "-----------------------Outside MPressActivate---------------------" << endl;
}

// -----------------------------------------------------------
void QucsView::MPressMirrorX(QMouseEvent *Event)
{
  //cout << "--------------------------Inside MPressMirrorX-----------------------" << endl;
  if(Event->button() != Qt::LeftButton) {
    rightPressMenu(Event);   // show menu on right mouse button
    return;
  }

  QucsDoc *d = Docs.current();
  int x = int(float(Event->pos().x())/d->Scale) + d->ViewX1;
  int y = int(float(Event->pos().y())/d->Scale) + d->ViewY1;

  // no use in mirroring wires or diagrams
  Component *c = d->selectedComponent(x, y);
  if(c) {
    if(c->Ports.count() < 1) return;  // only mirror components with ports
    c->mirrorX();
    d->setCompPorts(c);
  }
  else {
    Painting *p = d->selectedPainting(x, y);
    if(p == 0) return;
    p->mirrorX();
  }

  viewport()->repaint();
  drawn = false;
  d->setChanged(true, true);
  //cout << "--------------------------Outside MPressMirrorX-----------------------" << endl;
}

// -----------------------------------------------------------
void QucsView::MPressMirrorY(QMouseEvent *Event)
{
  //cout << "--------------------------Inside MPressMirrorY-----------------------" << endl;
  if(Event->button() != Qt::LeftButton) {
    rightPressMenu(Event);   // show menu on right mouse button
    return;
  }

  QucsDoc *d = Docs.current();
  int x = int(float(Event->pos().x())/d->Scale) + d->ViewX1;
  int y = int(float(Event->pos().y())/d->Scale) + d->ViewY1;

  // no use in mirroring wires or diagrams
  Component *c = d->selectedComponent(x, y);
  if(c) {
    if(c->Ports.count() < 1) return;  // only mirror components with ports
    c->mirrorY();
    d->setCompPorts(c);
  }
  else {
    Painting *p = d->selectedPainting(x, y);
    if(p == 0) return;
    p->mirrorY();
  }

  viewport()->repaint();
  drawn = false;
  d->setChanged(true, true);
  //cout << "--------------------------Outside MPressMirrorY-----------------------" << endl;
}

// -----------------------------------------------------------
void QucsView::MPressRotate(QMouseEvent *Event)
{
  //cout << "-------------------Inside MPressRotate--------------------" << endl;
  if(Event->button() != Qt::LeftButton) {
    rightPressMenu(Event);   // show menu on right mouse button
    return;
  }

  QucsDoc *d = Docs.current();
  Element_Qucs *e = d->selectElement(
	int(float(Event->pos().x())/d->Scale)+d->ViewX1,
	int(float(Event->pos().y())/d->Scale)+d->ViewY1, false);
  if(e == 0) return;
  e->Type &= isSpecialMask;  // remove special functions

  
  WireLabel *pl;
  int x1, y1, x2, y2;
//  e->isSelected = false;
  switch(e->Type) {
    case isComponent: if(((Component*)e)->Ports.count() < 1)
                        break;  // do not rotate components without ports
                      ((Component*)e)->rotate();
                      d->setCompPorts((Component*)e);
                      // enlarge viewarea if component lies outside the view
                      ((Component*)e)->entireBounds(x1,y1,x2,y2);
                      enlargeView(x1, y1, x2, y2);
                      break;
    case isWire:      pl = ((Wire*)e)->Label;
                      ((Wire*)e)->Label = 0;    // prevent label to be deleted
                      d->Wires->setAutoDelete(false);
                      d->deleteWire((Wire*)e);
                      ((Wire*)e)->Label = pl;
                      ((Wire*)e)->rotate();
                      d->setOnGrid(e->x1, e->y1);
                      d->setOnGrid(e->x2, e->y2);
                      if(pl) d->setOnGrid(pl->cx, pl->cy);
                      d->insertWire((Wire*)e);
                      d->Wires->setAutoDelete(true);
                      if (d->Wires->containsRef ((Wire*)e))
			enlargeView(e->x1, e->y1, e->x2, e->y2);
                      break;
    case isPainting:  ((Painting*)e)->rotate();
                      // enlarge viewarea if component lies outside the view
                      ((Painting*)e)->Bounding(x1,y1,x2,y2);
                      enlargeView(x1, y1, x2, y2);
                      break;
    default:          return;
  }
  viewport()->repaint();
  drawn = false;
  Docs.current()->setChanged(true, true);
  //cout << "-------------------Outside MPressRotate--------------------" << endl;
}

// -----------------------------------------------------------
void QucsView::MPressComponent(QMouseEvent *Event)
{
  //cout << "---------------Inside MPressComponent---------------------------" << endl;
  QucsDoc *d = Docs.current();
  // Commented by Shivam
  //viewport()->setAttribute(Qt::WA_PaintOutsidePaintEvent, true);                                                                                                                                   
  ////viewport()->setAttribute(Qt::WA_PaintOnScreen,true);         
  //QPainter painter(viewport());
  //setPainter(&painter, d);

  int x1, y1, x2, y2;
  switch(Event->button()) {
  case Qt::LeftButton :{
    // left mouse button inserts component into the schematic
    if(selComp == 0) break;
    selComp->TextSize(x1, y1);
    d->insertComponent(selComp);
    selComp->TextSize(x2, y2);
    if(selComp->tx < selComp->x1) selComp->tx -= x2 - x1;

    // enlarge viewarea if component lies outside the view
    selComp->entireBounds(x1,y1,x2,y2);
    enlargeView(x1, y1, x2, y2);

    viewport()->repaint();
    d->setChanged(true, true);
    selComp = selComp->newOne(); // component is used, so create a new one
   // Following Lines are Added after repaint by Shivam for avoiding Warning : A paint device
   // can only be painted by one painter at a time.
    viewport()->setAttribute(Qt::WA_PaintOutsidePaintEvent, true);                                                                                                                                       //viewport()->setAttribute(Qt::WA_PaintOnScreen,true);         
    QPainter painter(viewport());
    setPainter(&painter, d);
   
   selComp->paintScheme(&painter);
    break;}

  case Qt::RightButton : {  // right mouse button rotates the component
    if(selComp == 0) break;
    if(selComp->Ports.count() == 0)
      break;  // do not rotate components without ports
     // Added by Shivam
     viewport()->setAttribute(Qt::WA_PaintOutsidePaintEvent, true);                                                                                                                                       //viewport()->setAttribute(Qt::WA_PaintOnScreen,true);         
     QPainter painter(viewport());
     setPainter(&painter, d);

     selComp->paintScheme(&painter); // erase old component scheme
     selComp->rotate();
     selComp->paintScheme(&painter); // paint new component scheme
    break; }

  default: ;   // avoids compiler warnings
  }
  //cout << "---------------Outside MPressComponent---------------------------" << endl;
}

// -----------------------------------------------------------
void QucsView::MPressDiagram(QMouseEvent *Event)
{
  //cout << "------------------Inside MPressDiagram----------------" << endl;
  QucsDoc *d = Docs.current();
  //viewport()->setAttribute(Qt::WA_PaintOutsidePaintEvent, true);                                                                                                                                   
  ////viewport()->setAttribute(Qt::WA_PaintOnScreen,true);         
  //QPainter painter(viewport());
  //setPainter(&painter, d);

  if(selDiag == 0) return;
  if(Event->button() != Qt::LeftButton) return;

  // dialog is Qt::WDestructiveClose !!!
  DiagramDialog *dia = new DiagramDialog(selDiag, d->DataSet, this);
  if(dia->exec() == QDialog::Rejected) {  // don't insert if dialog canceled
    viewport()->repaint();
    drawn = false;
    return;
  }

  d->Diags->append(selDiag);
  enlargeView(selDiag->cx, selDiag->cy-selDiag->y2,
	      selDiag->cx+selDiag->x2, selDiag->cy);
  d->setChanged(true, true);   // document has been changed

  viewport()->repaint();
  selDiag = selDiag->newOne(); // the component is used, so create a new one
   // Added by Shivam after Repaint
  
   viewport()->setAttribute(Qt::WA_PaintOutsidePaintEvent, true);
   //viewport()->setAttribute(Qt::WA_PaintOnScreen,true);         
   QPainter painter(viewport());
   setPainter(&painter, d);     
  
  selDiag->paintScheme(&painter);
  //cout << "--------------Outside MPressDiagram---------------------" << endl;
}

// -----------------------------------------------------------
// Is called if starting point of wire is pressed
void QucsView::MPressWire1(QMouseEvent *Event)
{
  if(Event->button() != Qt::LeftButton) {
    rightPressMenu(Event);   // show menu on right mouse button
    return;
  }
  //cout << "--------------------Inside MPressWire1--------------------------" << endl;
  viewport()->setAttribute(Qt::WA_PaintOutsidePaintEvent, true);
  //viewport()->setAttribute(Qt::WA_PaintOnScreen,true);
  QPainter painter(viewport());
  setPainter(&painter, Docs.current());

  if(drawn) {
    //cout << "Erasing Old one of MPressWire" << endl;
    painter.drawLine(MAx1, MAy3, MAx2, MAy3); // erase old mouse cross
    painter.drawLine(MAx3, MAy1, MAx3, MAy2);
  }
  drawn = false;

  MAx1 = 0;   // paint wire corner first up, then left/right
  MAx2 = MAx3;
  MAy2 = MAy3;
  MouseMoveAction = &QucsView::MMoveWire2;
  MousePressAction = &QucsView::MPressWire2;
  MouseDoubleClickAction = &QucsView::MDoubleClickWire2;
  //cout <<"-------------Outside MPressWire1------------------" << endl;
}

// -----------------------------------------------------------
// Is called if ending point of wire is pressed
void QucsView::MPressWire2(QMouseEvent *Event)
{
  //cout << "--------------------- Inside MPressWire2--------------------" << endl;
  QucsDoc *d = Docs.current();
  // Commented By Shivam
  //viewport()->setAttribute(Qt::WA_PaintOutsidePaintEvent, true);
   //QPainter painter(viewport());
  //setPainter(&painter, d);

  int set = 0;
  switch(Event->button()) {
  case Qt::LeftButton :
    if(MAx1 == 0) { // which wire direction first ?
      if(MAx2 != MAx3) {
	set = d->insertWire(new Wire(MAx3, MAy2, MAx2, MAy2));
	if(set & 2) {
	  // if last port is connected, then start a new wire
	  MouseMoveAction = &QucsView::MMoveWire1;
	  MousePressAction = &QucsView::MPressWire1;
	  MouseDoubleClickAction = &QucsView::MouseDoNothing;
	}
	if(MAy2 != MAy3)
	  set |= d->insertWire(new Wire(MAx3, MAy3, MAx3, MAy2));
      }
      else if(MAy2 != MAy3) {
	set = d->insertWire(new Wire(MAx3, MAy3, MAx3, MAy2));
	if(set & 2) {
	  // if last port is connected, then start a new wire
	  MouseMoveAction = &QucsView::MMoveWire1;
	  MousePressAction = &QucsView::MPressWire1;
	  MouseDoubleClickAction = &QucsView::MouseDoNothing;
	} }
    }
    else {
      if(MAy2 != MAy3) {
	set = d->insertWire(new Wire(MAx2, MAy3, MAx2, MAy2));
	if(set & 2) {
	  // if last port is connected, then start a new wire
	  MouseMoveAction = &QucsView::MMoveWire1;
	  MousePressAction = &QucsView::MPressWire1;
	  MouseDoubleClickAction = &QucsView::MouseDoNothing;
	}
	if(MAx2 != MAx3)
	  set |= d->insertWire(new Wire(MAx3, MAy3, MAx2, MAy3));
      }
      else if(MAx2 != MAx3) {
	set = d->insertWire(new Wire(MAx3, MAy3, MAx2, MAy3));
	if(set & 2) {
	  // if last port is connected, then start a new wire
	  MouseMoveAction = &QucsView::MMoveWire1;
	  MousePressAction = &QucsView::MPressWire1;
	  MouseDoubleClickAction = &QucsView::MouseDoNothing;
	} }
    }
    viewport()->repaint();
    drawn = false;
    if(set) d->setChanged(true, true);
    MAx3 = MAx2;
    MAy3 = MAy2;
    break;

  case Qt::RightButton :  // right mouse button changes the wire corner
     // Added by Shivam as it is required in this Branch only as here only
     // painter is used
     {  QPainter painter(viewport());
     viewport()->setAttribute(Qt::WA_PaintOutsidePaintEvent, true);
     setPainter(&painter, d);
    
    if(MAx1 == 0) {
      painter.drawLine(MAx3, MAy3, MAx3, MAy2); // erase old
      painter.drawLine(MAx3, MAy2, MAx2, MAy2); // erase old
    }
    else {
      painter.drawLine(MAx3, MAy3, MAx2, MAy3); // erase old
      painter.drawLine(MAx2, MAy3, MAx2, MAy2); // erase old
    }

    MAx2  = int(float(Event->pos().x())/d->Scale) + d->ViewX1;
    MAy2  = int(float(Event->pos().y())/d->Scale) + d->ViewY1;
    d->setOnGrid(MAx2, MAy2);

    MAx1 ^= 1;    // change the painting direction of wire corner
    if(MAx1 == 0) {
      painter.drawLine(MAx3, MAy3, MAx3, MAy2); // paint
      painter.drawLine(MAx3, MAy2, MAx2, MAy2); // paint
    }
    else {
      painter.drawLine(MAx3, MAy3, MAx2, MAy3); // paint
      painter.drawLine(MAx2, MAy3, MAx2, MAy2); // paint
    }
    break; } 

  default: ;    // avoids compiler warnings
  }
}

// -----------------------------------------------------------
// Is called for setting a marker on a diagram's graph
void QucsView::MPressMarker(QMouseEvent *Event)
{
  //cout << "---------Inside MPressMarker-----------------" << endl;
  if(Event->button() != Qt::LeftButton) {
    rightPressMenu(Event);   // show menu on right mouse button
    return;
  }

  MAx1 = int(float(Event->pos().x())/Docs.current()->Scale)
		+Docs.current()->ViewX1;
  MAy1 = int(float(Event->pos().y())/Docs.current()->Scale)
		+Docs.current()->ViewY1;
  Marker *pm = Docs.current()->setMarker(MAx1, MAy1);

  if(pm) {
    int x0 = pm->Diag->cx;
    int y0 = pm->Diag->cy;
    enlargeView(x0+pm->x1, y0-pm->y1-pm->y2, x0+pm->x1+pm->x2, y0-pm->y1);
  }
  viewport()->repaint();
  drawn = false;
  //cout << "----------------Outside MPressMarker-----------------" << endl;
}

// -----------------------------------------------------------
void QucsView::MPressOnGrid(QMouseEvent *Event)
{
  //cout << "--------------------Inside MPressOnGrid--------------------------" << endl;
  if(Event->button() != Qt::LeftButton) {
    rightPressMenu(Event);   // show menu on right mouse button
    return;
  }

  QucsDoc *d = Docs.current();
  Element_Qucs *pe = d->selectElement(
		int(float(Event->pos().x())/d->Scale)+d->ViewX1,
		int(float(Event->pos().y())/d->Scale)+d->ViewY1, false);
  if(pe) {
    pe->Type &= isSpecialMask;  // remove special functions (4 lowest bits)
    pe->isSelected = true;
    Docs.current()->elementsOnGrid();

    d->sizeOfAll(d->UsedX1, d->UsedY1, d->UsedX2, d->UsedY2);
    viewport()->repaint();
    drawn = false;
  }
  //cout << "--------------------Outside MPressOnGrid--------------------------" << endl;
}

// -----------------------------------------------------------
void QucsView::MPressMoveText(QMouseEvent *Event)
{
  //cout << "---------------Inside MPressMoveText-------------------" << endl;
  if(Event->button() != Qt::LeftButton) {
    rightPressMenu(Event);   // show menu on right mouse button
    return;
  }

  QucsDoc *d = Docs.current();
  MAx1 = int(float(Event->pos().x())/d->Scale)+d->ViewX1;
  MAy1 = int(float(Event->pos().y())/d->Scale)+d->ViewY1;
  focusElement = d->selectCompText(MAx1, MAy1, MAx2, MAy2);

  if(focusElement) {
    MAx3 = MAx1;
    MAy3 = MAy1;
    MAx1 = ((Component*)focusElement)->cx + ((Component*)focusElement)->tx;
    MAy1 = ((Component*)focusElement)->cy + ((Component*)focusElement)->ty;
    viewport()->repaint();
    drawn = false;
    MouseMoveAction = &QucsView::MMoveMoveText;
    MouseReleaseAction = &QucsView::MReleaseMoveText;
    this->grabKeyboard();  // no keyboard inputs during move actions
  }
  //cout << "---------------Outside MPressMoveText-------------------" << endl;
}

// -----------------------------------------------------------
void QucsView::MPressZoomIn(QMouseEvent *Event)
{
  //cout << "---------------Inside MPressZoomIn -------------------" << endl;
  if(Event->button() != Qt::LeftButton) {
    rightPressMenu(Event);   // show menu on right mouse button
    return;
  }

  QucsDoc *d = Docs.current();
  MAx1 = int(float(Event->pos().x())/d->Scale) + d->ViewX1;
  MAy1 = int(float(Event->pos().y())/d->Scale) + d->ViewY1;
  MAx2 = 0;  // rectangle size
  MAy2 = 0;

  MouseMoveAction = &QucsView::MMoveSelect;
  MouseReleaseAction = &QucsView::MReleaseZoomIn;
  this->grabKeyboard();  // no keyboard inputs during move actions
  viewport()->repaint();
  drawn = false;
  //cout << "---------------Outside MPressZoomIn -------------------" << endl;
}


// ***********************************************************************
// **********                                                   **********
// **********    Functions for serving mouse button releasing   **********
// **********                                                   **********
// ***********************************************************************
void QucsView::contentsMouseReleaseEvent(QMouseEvent *Event)
{
  (this->*MouseReleaseAction)(Event);
}

// -----------------------------------------------------------
void QucsView::MReleaseSelect(QMouseEvent *Event)
{
  //cout << "--------------Inside MReleaseSelect-------------------------" << endl;
  bool ctrl;
  if(Event->state() & Qt::ControlButton) ctrl = true;
  else ctrl = false;

  if(!ctrl) Docs.current()->deselectElements(focusElement);

  if(focusElement)
    if(Event->button() == Qt::LeftButton)
      if(focusElement->Type == isWire) {
        Docs.current()->selectWireLine(focusElement,
				((Wire*)focusElement)->Port1, ctrl);
        Docs.current()->selectWireLine(focusElement,
				((Wire*)focusElement)->Port2, ctrl);
      }

  this->releaseKeyboard();  // allow keyboard inputs again
  MousePressAction = &QucsView::MPressSelect;
  MouseReleaseAction = &QucsView::MReleaseSelect;
  MouseDoubleClickAction = &QucsView::MDoubleClickSelect;
  MouseMoveAction = &QucsView::MouseDoNothing;   // no element moving
  viewport()->repaint();
  drawn = false;
  //cout << "----------------------Outside MReleaseSelect-------------------------" << endl;
}

// -----------------------------------------------------------
// Is called after the rectangle for selection is released.
void QucsView::MReleaseSelect2(QMouseEvent *Event)
{
  //cout << "----------------------Inside MReleaseSelect2-------------------------" << endl;
  if(Event->button() != Qt::LeftButton) return;

  QucsDoc *d = Docs.current();
//  QPainter painter(viewport());
//  setPainter(&painter, d);

  bool Ctrl;
  if(Event->state() & Qt::ControlButton) Ctrl = true;
  else Ctrl = false;

  // selects all elements within the rectangle
  d->selectElements(MAx1, MAy1, MAx1+MAx2, MAy1+MAy2, Ctrl);

  this->releaseKeyboard();  // allow keyboard inputs again
  MouseMoveAction = &QucsView::MouseDoNothing;
  MousePressAction = &QucsView::MPressSelect;
  MouseReleaseAction = &QucsView::MReleaseSelect;
  MouseDoubleClickAction = &QucsView::MDoubleClickSelect;
  viewport()->repaint();
  drawn = false;
  //cout << "----------------------Outside MReleaseSelect2-------------------------" << endl;
}

// -----------------------------------------------------------
void QucsView::MReleaseActivate(QMouseEvent *Event)
{
  //cout << "----------------------Inside MReleaseActivate -------------------------" << endl;
  if(Event->button() != Qt::LeftButton) return;

  QucsDoc *d = Docs.current();
//  QPainter painter(viewport());
//  setPainter(&painter, d);

//  if(drawn) painter.drawRect(MAx1, MAy1, MAx2, MAy2);  // erase old rectangle
//  drawn = false;

  // activates all components within the rectangle
  d->activateComps(MAx1, MAy1, MAx1+MAx2, MAy1+MAy2);

  MouseMoveAction = &QucsView::MMoveActivate;
  MousePressAction = &QucsView::MPressActivate;
  MouseReleaseAction = &QucsView::MouseDoNothing;
  MouseDoubleClickAction = &QucsView::MouseDoNothing;
  viewport()->repaint();
  drawn = false;
  //cout << "----------------------Outside MReleaseActivate -------------------------" << endl;
}

// -----------------------------------------------------------
// Is called after moving selected elements.
void QucsView::MReleaseMoving(QMouseEvent *Event)
{
  //cout << "-------------Inside MReleaseMoving-------------------------" << endl;
  if(Event->button() != Qt::LeftButton) return;
  endElementMoving();
  this->releaseKeyboard();  // allow keyboard inputs again

  MouseMoveAction = &QucsView::MouseDoNothing;
  MousePressAction = &QucsView::MPressSelect;
  MouseReleaseAction = &QucsView::MReleaseSelect;
  MouseDoubleClickAction = &QucsView::MDoubleClickSelect;
  //cout << "---------------Outside MReleaseMoving-------------------------" << endl;
}

// -----------------------------------------------------------
void QucsView::MReleaseResizeDiagram(QMouseEvent *Event)
{
   //cout << "-------------Inside MReleaseResizeDiagram-----------------------" << endl; 
  if(Event->button() != Qt::LeftButton) return;

  MAx3  = focusElement->cx;
  MAy3  = focusElement->cy;
  if(MAx2 < 0) {    // resize diagram
    if(MAx2 > -10) MAx2 = -10;   // not smaller than 10 pixels
    focusElement->x2 = -MAx2;
    focusElement->cx = MAx1+MAx2;
  }
  else {
    if(MAx2 < 10) MAx2 = 10;
    focusElement->x2 = MAx2;
    focusElement->cx = MAx1;
  }
  if(MAy2 < 0) {
    if(MAy2 > -10) MAy2 = -10;
    focusElement->y2 = -MAy2;
    focusElement->cy = MAy1;
  }
  else {
    if(MAy2 < 10) MAy2 = 10;
    focusElement->y2 = MAy2;
    focusElement->cy = MAy1+MAy2;
  }
  MAx3 -= focusElement->cx;
  MAy3 -= focusElement->cy;

  Diagram *pd = (Diagram*)focusElement;
  pd->updateGraphData();
  for(Graph *pg = pd->Graphs.first(); pg != 0; pg = pd->Graphs.next())
    for(Marker *pm = pg->Markers.first(); pm!=0; pm = pg->Markers.next()) {
      pm->x1 += MAx3;      // correct changes due to move of diagram corner
      pm->y1 += MAy3;
    }

  MouseMoveAction = &QucsView::MouseDoNothing;
  MousePressAction = &QucsView::MPressSelect;
  MouseReleaseAction = &QucsView::MReleaseSelect;
  MouseDoubleClickAction = &QucsView::MDoubleClickSelect;
  this->releaseKeyboard();  // allow keyboard inputs again

  viewport()->repaint();
  drawn = false;
  Docs.current()->setChanged(true, true);
  //cout << "-------------Outside MReleaseResizeDiagram-----------------------" << endl;
}

// -----------------------------------------------------------
void QucsView::MReleaseResizePainting(QMouseEvent *Event)
{
  //cout << "----------Inside MReleaseResizePainting-----------------------" << endl;
  if(Event->button() != Qt::LeftButton) return;


  MouseMoveAction = &QucsView::MouseDoNothing;
  MousePressAction = &QucsView::MPressSelect;
  MouseReleaseAction = &QucsView::MReleaseSelect;
  MouseDoubleClickAction = &QucsView::MDoubleClickSelect;
  this->releaseKeyboard();  // allow keyboard inputs again

  viewport()->repaint();
  drawn = false;
  Docs.current()->setChanged(true, true);
  //cout << "-------------------Outside MReleaseResizePainting-----------------------" << endl;
}

// -----------------------------------------------------------
void QucsView::MReleasePaste(QMouseEvent *Event)
{
  //cout << "----------------------Inside MReleasePaste-------------------------" << endl;
  int x1, y1, x2, y2, tmp;
  QucsDoc *d = Docs.current();
 // QPainter painter(viewport());

  Element_Qucs *pe;
  switch(Event->button()) {
  case Qt::LeftButton :
    // insert all moved elements into document
    for(pe = movingElements.first(); pe!=0; pe = movingElements.next()) {
      pe->isSelected = false;
      switch(pe->Type) {
	case isWire:
	  if(pe->x1 == pe->x2) if(pe->y1 == pe->y2)  break;
	  d->insertWire((Wire*)pe);
	  if (d->Wires->containsRef ((Wire*)pe))
	    enlargeView(pe->x1, pe->y1, pe->x2, pe->y2);
	  else pe = NULL;
	  break;
	case isDiagram:
	  d->Diags->append((Diagram*)pe);
	  ((Diagram*)pe)->loadGraphData(d->DataSet);
	  enlargeView(pe->cx, pe->cy-pe->y2, pe->cx+pe->x2, pe->cy);
	  break;
	case isPainting:
	  d->Paints->append((Painting*)pe);
	  ((Painting*)pe)->Bounding(x1,y1,x2,y2);
	  enlargeView(x1, y1, x2, y2);
	  break;
	case isMovingLabel:
	  pe->Type = isNodeLabel;
	  d->placeNodeLabel((WireLabel*)pe);
	  break;
	case isComponent:
	  d->insertComponent((Component*)pe);
	  ((Component*)pe)->entireBounds(x1,y1,x2,y2);
	  enlargeView(x1, y1, x2, y2);
	  break;
      }
    }

    pasteElements();
    MouseMoveAction = &QucsView::MMovePaste;
    MousePressAction = &QucsView::MouseDoNothing;
    MouseReleaseAction = &QucsView::MouseDoNothing;
    MouseDoubleClickAction = &QucsView::MouseDoNothing;

    drawn = false;
    viewport()->repaint();
    d->setChanged(true, true);
    break;

  // ............................................................
  case Qt::RightButton :  // right button rotates the elements
       { 
       viewport()->setAttribute(Qt::WA_PaintOutsidePaintEvent, true);                                                                                                                          
       //viewport()->setAttribute(Qt::WA_PaintOnScreen,true);         
       QPainter painter(viewport());
       setPainter(&painter, d);

    if(drawn) // erase old scheme
      for(pe = movingElements.first(); pe != 0; pe = movingElements.next())
        pe->paintScheme(&painter);
    drawn = true;

    x1  = int(float(Event->pos().x())/d->Scale) + d->ViewX1;
    y1  = int(float(Event->pos().y())/d->Scale) + d->ViewY1;
    d->setOnGrid(x1, y1);

    for(pe = movingElements.first(); pe != 0; pe = movingElements.next()) {
      switch(pe->Type) {
        case isComponent:
	  ((Component*)pe)->rotate(); // rotate !before! rotating the center
          x2 = x1 - pe->cx;
          pe->setCenter(pe->cy - y1 + x1, x2 + y1);
          break;
        case isWire:
	  x2    = pe->x1;
          pe->x1 = pe->y1 - y1 + x1;
          pe->y1 = x1 - x2 + y1;
          x2    = pe->x2;
          pe->x2 = pe->y2 - y1 + x1;
          pe->y2 = x1 - x2 + y1;
          break;
        case isPainting:
	  ((Painting*)pe)->rotate();  // rotate !before! rotating the center
          ((Painting*)pe)->getCenter(x2, y2);
          tmp = x1 - x2;
          pe->setCenter(y2 - y1 + x1, tmp + y1);
          break;
        default:
	  x2 = x1 - pe->cx;   // if diagram -> only rotate cx/cy
          pe->setCenter(pe->cy - y1 + x1, x2 + y1);
          break;
      }
      pe->paintScheme(&painter);
    }
    break; }

  default: ;    // avoids compiler warnings
  }
  //cout << "----------------------Outside MReleasePaste-------------------------" << endl;
}

// -----------------------------------------------------------
void QucsView::MReleaseMoveText(QMouseEvent *Event)
{
  //cout << "------------------Inside MReleaseMoveText-----------------" << endl;
  if(Event->button() != Qt::LeftButton) return;

  MouseMoveAction = &QucsView::MMoveMoveTextB;
  MouseReleaseAction = &QucsView::MouseDoNothing;
  this->releaseKeyboard();  // allow keyboard inputs again

  ((Component*)focusElement)->tx = MAx1 - ((Component*)focusElement)->cx;
  ((Component*)focusElement)->ty = MAy1 - ((Component*)focusElement)->cy;
  viewport()->repaint();
  drawn = false;

  Docs.current()->setChanged(true, true);
  //cout << "---------------------OutSide MReleaseMoveText-----------------" << endl;
}

// -----------------------------------------------------------
void QucsView::MReleaseZoomIn(QMouseEvent *Event)
{
  //cout << "--------------Inside MReleaseZoomIn---------------" << endl;
  if(Event->button() != Qt::LeftButton) return;

  QucsDoc *d = Docs.current();
  MAx1 = Event->pos().x();
  MAy1 = Event->pos().y();
  float DX = float(abs(MAx2));
  float DY = float(abs(MAy2));
  if((d->Scale * DX) < 6.0) {
    MAx1  = (MAx1<<1) - (visibleWidth()>>1)  - contentsX();
    MAy1  = (MAy1<<1) - (visibleHeight()>>1) - contentsY();
    Zoom(2.0);    // a simple click zooms by factor 2
  }
  else {
    float xScale = float(visibleWidth())  / DX;
    float yScale = float(visibleHeight()) / DY;
    if(xScale > yScale) xScale = yScale;
    if(xScale > 10.0) xScale = 10.0f;
    if(xScale < 0.01) xScale = 0.01f;

    if(MAx2 > 0)  MAx1 -= int(float(MAx2)*d->Scale);
    if(MAy2 > 0)  MAy1 -= int(float(MAy2)*d->Scale);
    MAx1 = int(float(MAx1) * xScale / d->Scale) - contentsX();
    MAy1 = int(float(MAy1) * xScale / d->Scale) - contentsY();

    d->Scale = xScale;
    resizeContents(int(xScale*float(d->ViewX2 - d->ViewX1)),
		   int(xScale*float(d->ViewY2 - d->ViewY1)));
  }
  scrollBy(MAx1, MAy1);

  MouseMoveAction = &QucsView::MMoveZoomIn;
  MouseReleaseAction = &QucsView::MouseDoNothing;
  this->releaseKeyboard();  // allow keyboard inputs again

  viewport()->repaint();
  drawn = false;
  //cout << "--------------Outside MReleaseZoomIn---------------" << endl;
}


// ***********************************************************************
// **********                                                   **********
// **********    Functions for mouse button double clicking     **********
// **********                                                   **********
// ***********************************************************************
void QucsView::contentsMouseDoubleClickEvent(QMouseEvent *Event)
{
  (this->*MouseDoubleClickAction)(Event);
}

// -----------------------------------------------------------
void QucsView::editElement(QMouseEvent *Event)
{
  //cout << "---------------Inside editElement-------------------" << endl;
  if(focusElement == 0) return;
  QucsDoc *d = Docs.current();

  Component *c;
  Diagram *dia;
  DiagramDialog *ddia;
  MarkerDialog *mdia;
  int x1, y1, x2, y2;

  switch(focusElement->Type) {
    case isComponent:
         c = (Component*)focusElement;
         if(c->Model == "GND") return;
 
         /*  QUCS-EDIT REMOVE SPICE
	 if(c->Model == "SPICE") {
	   SpiceDialog *sd = new SpiceDialog((SpiceFile*)c, d, this);
           if(sd->exec() != 1) break;   // dialog is WDestructiveClose
	 }
	 */
	 else {
	   ComponentDialog * cd = new ComponentDialog(c, d, this);
	   if(cd->exec() != 1) break;   // dialog is WDestructiveClose

           d->Comps->findRef(c);
           d->Comps->take();
           d->setComponentNumber(c); // for ports/power sources
           d->Comps->append(c);
	 }

         d->setChanged(true, true);
         c->entireBounds(x1,y1,x2,y2);
         enlargeView(x1,y1,x2,y2);
         break;

    case isDiagram :
         dia = (Diagram*)focusElement;
         if(dia->Name == "Tab") { // don't open dialog on scrollbar of tabular
           int x_ = int(Event->pos().x()/d->Scale) + d->ViewX1;
           if(dia->cx > x_) {
	     if(((TabDiagram*)focusElement)->scroll(MAy1))
	       d->setChanged(true, true, 'm'); // 'm' = only the first time
	     break;
           }
	 }
         ddia = new DiagramDialog(dia, d->DataSet, this);
         if(ddia->exec() != QDialog::Rejected)   // is WDestructiveClose
           d->setChanged(true, true);
         break;

    case isWire:
         PressLabel(Event);
         break;

    case isNodeLabel:
    case isHWireLabel:
    case isVWireLabel:
	 editLabel((WireLabel*)focusElement);
	 break;

    case isPainting:
         if( ((Painting*)focusElement)->Dialog() )
           d->setChanged(true, true);
         break;

    case isMarker:
         mdia = new MarkerDialog((Marker*)focusElement, this);
         if(mdia->exec() > 1) d->setChanged(true, true);
         break;
  }

  viewport()->repaint();
  drawn = false;
  //cout << "---------------Outside editElement-------------------" << endl;
}

// -----------------------------------------------------------
void QucsView::MDoubleClickSelect(QMouseEvent *Event)
{
  this->releaseKeyboard();  // allow keyboard inputs again
  editElement(Event);
}

// -----------------------------------------------------------
// Is called if the "edit" action is clicked on right mouse button menu.
void QucsView::slotEditElement()
{
  if(focusMEvent)
    editElement(focusMEvent);
}

// -----------------------------------------------------------
void QucsView::MDoubleClickWire2(QMouseEvent *Event)
{
  MPressWire2(Event);

  MouseMoveAction = &QucsView::MMoveWire1;
  MousePressAction = &QucsView::MPressWire1;
//  MouseReleaseAction = &QucsView::MouseDoNothing;
  MouseDoubleClickAction = &QucsView::MouseDoNothing;
}

// *************************************************************************
// **********                                                     **********
// **********        Function for serving mouse wheel moving      **********
// **********                                                     **********
// *************************************************************************
void QucsView::contentsWheelEvent(QWheelEvent *Event)
{
  editText->setHidden(true);  // disable edit of component property
  int delta = Event->delta() >> 1;     // use smaller steps

  // .....................................................................
  if((Event->state() & Qt::ShiftButton) ||
     (Event->orientation() == Qt::Horizontal)) { // scroll horizontally ?
      if(delta > 0) { if(ScrollLeft(delta)) scrollBy(-delta, 0); }
      else { if(ScrollRight(delta)) scrollBy(-delta, 0); }
      viewport()->repaint(); // because QScrollView thinks nothing has changed
      drawn = false;
  }
  // .....................................................................
  else if(Event->state() & Qt::ControlButton) {  // use mouse wheel to zoom ?
      float Scaling;
      if(delta < 0) Scaling = float(-delta)/60.0/1.1;
      else Scaling = 1.1*60.0/float(delta);
      Scaling = Zoom(Scaling);
//      center(Event->x(), Event->y());
      viewport()->repaint();
      drawn = false;
  }
  // .....................................................................
  else {     // scroll vertically !
      if(delta > 0) { if(ScrollUp(delta)) scrollBy(0, -delta); }
      else { if(ScrollDown(delta)) scrollBy(0, -delta); }
      viewport()->repaint(); // because QScrollView thinks nothing has changed
      drawn = false;
  }

  Event->accept();   // QScrollView must not handle this event
}

// -----------------------------------------------------------
// Scrolls the visible area upwards and enlarges or reduces the view
// area accordingly.
bool QucsView::ScrollUp(int step)
{
  int diff;
  QucsDoc *d = Docs.current();

  diff = contentsY() - step;
  if(diff < 0) {     // scroll outside the active area ?  (upwards)
    resizeContents(contentsWidth(), contentsHeight()-diff);
    d->ViewY1 += diff;
    scrollBy(0, diff);
    return false;
  }

  diff = d->ViewY2 - d->UsedY2 - 20;    // keep border of 20
  if(diff > 0) {      // make active area smaller ?
    if(step < diff) diff = step;
    resizeContents(contentsWidth(), contentsHeight()-diff);
    d->ViewY2 -= diff;
  }

  return true;
}

// -----------------------------------------------------------
// Scrolls the visible area downwards and enlarges or reduces the view
// area accordingly. ("step" must be negative!)
bool QucsView::ScrollDown(int step)
{
  int diff;
  QucsDoc *d = Docs.current();

  diff = contentsHeight() - contentsY()-visibleHeight() + step;
  if(diff < 0) {     // scroll outside the active area ?  (downwards)
    resizeContents(contentsWidth(), contentsHeight()-diff);
    d->ViewY2 -= diff;
    scrollBy(0, -step);
    return false;
  }

  diff = d->ViewY1 - d->UsedY1 + 20;    // keep border of 20
  if(diff < 0) {      // make active area smaller ?
    if(step > diff) diff = step;
    resizeContents(contentsWidth(), contentsHeight()+diff);
    d->ViewY1 -= diff;
    return false;
  }

  return true;
}

// -----------------------------------------------------------
// Scrolls the visible area to the left and enlarges or reduces the view
// area accordingly.
bool QucsView::ScrollLeft(int step)
{
  int diff;
  QucsDoc *d = Docs.current();

  diff = contentsX() - step;
  if(diff < 0) {     // scroll outside the active area ?  (to the left)
    resizeContents(contentsWidth()-diff, contentsHeight());
    d->ViewX1 += diff;
    scrollBy(diff, 0);
    return false;
  }

  diff = d->ViewX2 - d->UsedX2 - 20;    // keep border of 20
  if(diff > 0) {      // make active area smaller ?
    if(step < diff) diff = step;
    resizeContents(contentsWidth()-diff, contentsHeight());
    d->ViewX2 -= diff;
  }

  return true;
}

// -----------------------------------------------------------
// Scrolls the visible area to the right and enlarges or reduces the
// view area accordingly. ("step" must be negative!)
bool QucsView::ScrollRight(int step)
{
  int diff;
  QucsDoc *d = Docs.current();

  diff = contentsWidth() - contentsX()-visibleWidth() + step;
  if(diff < 0) {     // scroll outside the active area ?  (to the right)
    resizeContents(contentsWidth()-diff, contentsHeight());
    d->ViewX2 -= diff;
    scrollBy(-step, 0);
    return false;
  }

  diff = d->ViewX1 - d->UsedX1 + 20;    // keep border of 20
  if(diff < 0) {      // make active area smaller ?
    if(step > diff) diff = step;
    resizeContents(contentsWidth()+diff, contentsHeight());
    d->ViewX1 -= diff;
    return false;
  }

  return true;
}

// -----------------------------------------------------------
// Is called if the scroll arrow of the ScrollBar is pressed.
//void QucsView::slotScrollUp()
// Changed by Shivam
void QucsView::slotScrollUp(int action)
{
  if (action == 2)    // QAbstractSlider::SliderSingleStepSub value = 2  means  Previous Line
  {
  editText->setHidden(true);  // disable edit of component property
  ScrollUp(verticalScrollBar()->singleStep());
  viewport()->repaint();   // because QScrollView thinks nothing has changed
  drawn = false;
  }
}

// -----------------------------------------------------------
// Is called if the scroll arrow of the ScrollBar is pressed.
// changed by Shivam
void QucsView::slotScrollDown(int action)
{
  if (action == 1)   // QAbstractSlider::SliderSingleStepAdd value = 1  means next line lineStep of Qt3 = singleStep of Qt4
  {
  editText->setHidden(true);  // disable edit of component property
  //ScrollDown(-verticalScrollBar()->lineStep());
   ScrollDown(-verticalScrollBar()->singleStep());
  viewport()->repaint();   // because QScrollView thinks nothing has changed
  drawn = false;
  }
}

// -----------------------------------------------------------
// Is called if the scroll arrow of the ScrollBar is pressed.
// changed by Shivam
void QucsView::slotScrollLeft(int action)
{
  if (action == 2)    // QAbstractSlider::SliderSingleStepSub value = 2  means  Previous Line
  {
  editText->setHidden(true);  // disable edit of component property
 // ScrollLeft(horizontalScrollBar()->lineStep());
  ScrollLeft(horizontalScrollBar()->singleStep());
  viewport()->repaint();   // because QScrollView thinks nothing has changed
  drawn = false;
}
}

// -----------------------------------------------------------
// Is called if the scroll arrow of the ScrollBar is pressed.
// Changed by Shivam
void QucsView::slotScrollRight(int action)
{
  if (action == 1)   // QAbstractSlider::SliderSingleStepAdd value = 1  means next line lineStep of Qt3 = singleStep of Qt4
  {
  editText->setHidden(true);  // disable edit of component property
  //ScrollRight(-horizontalScrollBar()->lineStep());
  ScrollRight(-horizontalScrollBar()->singleStep());
  viewport()->repaint();   // because QScrollView thinks nothing has changed
  drawn = false;
}
}

// -----------------------------------------------------------
// Is called if return is pressed in the component text QLineEdit.
void QucsView::slotApplyCompText()
{
  QString s;
  QucsDoc *d = Docs.current();
  QFont f = QucsSettings.font;
  f.setPointSizeFloat( d->Scale * float(f.pointSize()) );
  editText->setFont(f);

  Property  *pp = 0;
  Component *pc = (Component*)focusElement;
  if(!pc) return;  // should never happen
  MAx1 = pc->cx + pc->tx;
  MAy1 = pc->cy + pc->ty;

  int z, n=0;  // "n" is number of property on screen
  pp = pc->Props.first();
  for(z=MAx3; z>0; z--) {  // calculate "n"
    if(!pp) {  // should never happen
      editText->setHidden(true);
      return;
    }
    if(pp->display) n++;   // is visible ?
    pp = pc->Props.next();
  }
  
  pp = 0;
  if(MAx3 > 0)  pp = pc->Props.at(MAx3-1);   // current property
  else s = pc->Name;

  if(!editText->isHidden()) {   // is called the first time ?
    // no -> apply value to current property
    if(MAx3 == 0) {   // component name ?
      Component *pc2;
      if(!editText->text().isEmpty())
        if(pc->Name != editText->text()) {
          for(pc2 = d->Comps->first(); pc2!=0; pc2 = d->Comps->next())
            if(pc2->Name == editText->text())
              break;  // found component with the same name ?
          if(!pc2) {
            pc->Name = editText->text();
            d->setChanged(true, true, 'p');  // only one undo state
          }
        }
    }
    else if(pp) {  // property was applied
      if(pp->Value != editText->text()) {
        pp->Value = editText->text();
        if(MAx3 == 1)  d->setComponentNumber(pc); // number for sources, ports
        d->setChanged(true, true, 'p');  // only one undo state
      }
    }

    n++;     // next row on screen
    MAx3++;  // next property
    pp = pc->Props.at(MAx3-1);  // search for next property

    if(!pp) {     // was already last property ?
      editText->setHidden(true);
      return;
    }


    while(!pp->display) {  // search for next visible property
      MAx3++;  // next property
      pp = pc->Props.next();
      if(!pp) {     // was already last property ?
        editText->setHidden(true);
        return;
      }
    }
  }


  contentsToViewport(int(d->Scale * double(MAx1 - d->ViewX1)),
			 int(d->Scale * double(MAy1 - d->ViewY1)),
			 MAx2, MAy2);
  editText->setReadOnly(false);
  if(pp) {
    s = pp->Value;
    MAx2 += editText->fontMetrics().width(pp->Name+"=");
    if(pp->Description.find('[') >= 0)  // is selection list ?
      editText->setReadOnly(true);
  }
  z = editText->fontMetrics().lineSpacing();
  MAy2 += n*z;
  editText->move(QPoint(MAx2, MAy2));
  editText->setText(s);
  editText->resize(editText->fontMetrics().width(s)+3*z, z);
  editText->setFocus();
  editText->selectAll();
  editText->setHidden(false);
}

// -----------------------------------------------------------
// Hides the edit for component property. Called e.g. if QLineEdit lost focus.
void QucsView::slotHideEdit()
{
  editText->setHidden(true);
}
