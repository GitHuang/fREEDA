/***************************************************************************
                          ellipse.cpp  -  description
                             -------------------
    begin                : Sun Nov 23 2003
    copyright            : (C) 2003 by Michael Margraf
    email                : michael.margraf@alumni.tu-berlin.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <qpushbutton.h>
#include <qlineedit.h>
#include <qcombobox.h>                                                                                                                                                                               
#include <qcheckbox.h>

#include "ellipse.h"
#include "filldialog.h"


Ellipse::Ellipse(bool _filled)
{
  Name = "Ellipse ";
  isSelected = false;
  Pen = QPen(QColor());
  Brush = QBrush(Qt::lightGray);
  filled = _filled;
  cx = cy = 0;
  x1 = x2 = 0;
  y1 = y2 = 0;
}

Ellipse::~Ellipse()
{
}

// --------------------------------------------------------------------------
void Ellipse::paint(ViewPainter *p)
{
  if(isSelected) {
    p->Painter->setPen(QPen(Qt::darkGray,Pen.width()+5));
    if(filled)  p->Painter->setBrush(Brush);
    p->drawEllipse(cx, cy, x2, y2);
    p->Painter->setPen(QPen(Qt::white, Pen.width(), Pen.style()));
    p->Painter->setBrush(Qt::NoBrush);
    p->drawEllipse(cx, cy, x2, y2);

    p->Painter->setPen(QPen(Qt::darkRed,2));
    p->drawResizeRect(cx, cy+y2);  // markers for changing the size
    p->drawResizeRect(cx, cy);
    p->drawResizeRect(cx+x2, cy+y2);
    p->drawResizeRect(cx+x2, cy);
    return;
  }
  p->Painter->setPen(Pen);
  if(filled)  p->Painter->setBrush(Brush);
  p->drawEllipse(cx, cy, x2, y2);
  p->Painter->setBrush(Qt::NoBrush); // no filling for the next paintings
}

// --------------------------------------------------------------------------
void Ellipse::paintScheme(QPainter *p)
{
  p->drawEllipse(cx, cy, x2, y2);
}

// --------------------------------------------------------------------------
void Ellipse::getCenter(int& x, int &y)
{
  x = cx+(x2>>1);
  y = cy+(y2>>1);
}

// --------------------------------------------------------------------------
// Sets the center of the painting to x/y.
void Ellipse::setCenter(int x, int y, bool relative)
{
  if(relative) { cx += x;  cy += y; }
  else { cx = x-(x2>>1);  cy = y-(y2>>1); }
}

// --------------------------------------------------------------------------
Painting* Ellipse::newOne()
{
  return new Ellipse();
}

// --------------------------------------------------------------------------
bool Ellipse::load(const QString& s)
{
  bool ok;

  QString n;
  n  = s.section(' ',1,1);    // cx
  cx = n.toInt(&ok);
  if(!ok) return false;

  n  = s.section(' ',2,2);    // cy
  cy = n.toInt(&ok);
  if(!ok) return false;

  n  = s.section(' ',3,3);    // x2
  x2 = n.toInt(&ok);
  if(!ok) return false;

  n  = s.section(' ',4,4);    // y2
  y2 = n.toInt(&ok);
  if(!ok) return false;

  n  = s.section(' ',5,5);    // color
  QColor co;
  co.setNamedColor(n);
  Pen.setColor(co);
  if(!Pen.color().isValid()) return false;

  n  = s.section(' ',6,6);    // thickness
  Pen.setWidth(n.toInt(&ok));
  if(!ok) return false;

  n  = s.section(' ',7,7);    // line style
  Pen.setStyle((Qt::PenStyle)n.toInt(&ok));
  if(!ok) return false;

  n  = s.section(' ',8,8);    // fill color
  co.setNamedColor(n);
  Brush.setColor(co);
  if(!Brush.color().isValid()) return false;

  n  = s.section(' ',9,9);    // fill style
  Brush.setStyle((Qt::BrushStyle)n.toInt(&ok));
  if(!ok) return false;

  n  = s.section(' ',10,10);    // filled
  if(n.toInt(&ok) == 0) filled = false;
  else filled = true;
  if(!ok) return false;

  return true;
}

// --------------------------------------------------------------------------
QString Ellipse::save()
{
  QString s = Name +
	QString::number(cx) + " " + QString::number(cy) + " " +
	QString::number(x2) + " " + QString::number(y2) + " " +
	Pen.color().name()  + " " + QString::number(Pen.width()) + " " +
	QString::number(Pen.style()) + " " +
	Brush.color().name() + " " + QString::number(Brush.style());
  if(filled) s += " 1";
  else s += " 0";
  return s;
}

// --------------------------------------------------------------------------
// Checks if the resize area was clicked.
bool Ellipse::ResizeTouched(int x, int y)
{
  State = -1;
  if(x < cx-5) return false;
  if(y < cy-5) return false;
  if(x > cx+x2+5) return false;
  if(y > cy+y2+5) return false;

  State = 0;
  if(x < cx+5) State = 1;
  else if(x <= cx+x2-5) { State = -1; return false; }
  if(y < cy+5)  State |= 2;
  else if(y <= cy+y2-5) { State = -1; return false; }

  return true;
}

// --------------------------------------------------------------------------
// Mouse move action during resize.
void Ellipse::MouseResizeMoving(int x, int y, QPainter *p)
{
  paintScheme(p);  // erase old painting
  switch(State) {
    case 0: x2 = x-cx; y2 = y-cy; // lower right corner
	    break;
    case 1: x2 -= x-cx; cx = x; y2 = y-cy; // lower left corner
	    break;
    case 2: x2 = x-cx; y2 -= y-cy; cy = y; // upper right corner
	    break;
    case 3: x2 -= x-cx; cx = x; y2 -= y-cy; cy = y; // upper left corner
	    break;
  }
  if(x2 < 0) { State ^= 1; x2 *= -1; cx -= x2; }
  if(y2 < 0) { State ^= 2; y2 *= -1; cy -= y2; }

  paintScheme(p);  // paint new painting
}

// --------------------------------------------------------------------------
// fx/fy are the precise coordinates, gx/gy are the coordinates set on grid.
// x/y are coordinates without scaling.
void Ellipse::MouseMoving(
	QPainter *paintScale, int, int, int gx, int gy,
	QPainter *p, int x, int y, bool drawn)
{
  if(State > 0) {
    if(State > 1)
      paintScale->drawEllipse(x1, y1, x2-x1, y2-y1); // erase old painting
    State++;
    x2 = gx;
    y2 = gy;
    paintScale->drawEllipse(x1, y1, x2-x1, y2-y1);  // paint new painting
  }
  else { x2 = gx; y2 = gy; }


  p->setPen(Qt::SolidLine);
  if(drawn) {
    p->drawEllipse(cx+13, cy, 18, 12);  // erase old cursor symbol
    if(filled) {
      p->drawLine(cx+14, cy+7, cx+20, cy+1);
      p->drawLine(cx+25, cy+2, cx+18, cy+9);
      p->drawLine(cx+29, cy+4, cx+23, cy+10);
    }
  }
  cx = x;
  cy = y;
  p->drawEllipse(cx+13, cy, 18, 12);  // paint new cursor symbol
  if(filled) {
    p->drawLine(cx+14, cy+7, cx+20, cy+1);
    p->drawLine(cx+25, cy+2, cx+18, cy+9);
    p->drawLine(cx+29, cy+4, cx+23, cy+10);
  }
}

// --------------------------------------------------------------------------
bool Ellipse::MousePressing()
{
  State++;
  if(State == 1) {
    x1 = x2;
    y1 = y2;    // first corner is determined
  }
  else {
    if(x1 < x2) { cx = x1; x2 = x2-x1; } // cx/cy to upper left corner
    else { cx = x2; x2 = x1-x2; }
    if(y1 < y2) { cy = y1; y2 = y2-y1; }
    else { cy = y2; y2 = y1-y2; }
    x1 = y1 = 0;
    State = 0;
    return true;    // painting is ready
  }
  return false;
}

// --------------------------------------------------------------------------
// Checks if the coordinates x/y point to the painting.
bool Ellipse::getSelected(int x, int y)
{
  if(filled) {
    x  = (x-cx-(x2>>1));  x *= x;
    y  = (y-cy-(y2>>1));  y *= y;

    int a = x2 >> 1;  a *= a;
    int b = y2 >> 1;  b *= b;

    if((double(x)/double(a) + double(y)/double(b)) > 1.0) return false;
    return true;
  }

  x  = (x-cx-(x2>>1));  x *= x;
  y  = (y-cy-(y2>>1));  y *= y;

  int a1 = (x2-5)>>1;  a1 *= a1;
  int a2 = (x2+5)>>1;  a2 *= a2;
  int b1 = (y2-5)>>1;  b1 *= b1;
  int b2 = (y2+5)>>1;  b2 *= b2;

  double x_double = double(x);
  double y_double = double(y);

  if((x_double/double(a1) + y_double/double(b1)) < 1.0) return false;
  if((x_double/double(a2) + y_double/double(b2)) > 1.0) return false;
  return true;
}

// --------------------------------------------------------------------------
void Ellipse::Bounding(int& _x1, int& _y1, int& _x2, int& _y2)
{
  _x1 = cx;     _y1 = cy;
  _x2 = cx+x2;  _y2 = cy+y2;
}

// --------------------------------------------------------------------------
// Rotates around the center.
void Ellipse::rotate()
{
  cy += (y2-x2) >> 1;
  cx += (x2-y2) >> 1;
  int tmp = x2;
  x2 = y2;
  y2 = tmp;
}

// --------------------------------------------------------------------------
// Mirrors about center line.
void Ellipse::mirrorX()
{
  // nothing to do
}

// --------------------------------------------------------------------------
// Mirrors about center line.
void Ellipse::mirrorY()
{
  // nothing to do
}

// --------------------------------------------------------------------------
// Calls the property dialog for the painting and changes them accordingly.
// If there were changes, it returns 'true'.
bool Ellipse::Dialog()
{
  bool changed = false;

  FillDialog *d = new FillDialog(QObject::tr("Edit Ellipse Properties"));
  d->ColorButt->setPaletteBackgroundColor(Pen.color());
  d->LineWidth->setText(QString::number(Pen.width()));
  d->StyleBox->setCurrentItem(Pen.style()-1);
  d->FillColorButt->setPaletteBackgroundColor(Brush.color());
  d->FillStyleBox->setCurrentItem(Brush.style());
  d->CheckFilled->setChecked(filled);
  d->slotCheckFilled(filled);

  if(d->exec() == QDialog::Rejected) {
    delete d;
    return false;
  }

  if(Pen.color() != d->ColorButt->paletteBackgroundColor()) {
    Pen.setColor(d->ColorButt->paletteBackgroundColor());
    changed = true;
  }
  if(Pen.width()  != d->LineWidth->text().toUInt()) {
    Pen.setWidth(d->LineWidth->text().toUInt());
    changed = true;
  }
  if(Pen.style()  != (Qt::PenStyle)(d->StyleBox->currentItem()+1)) {
    Pen.setStyle((Qt::PenStyle)(d->StyleBox->currentItem()+1));
    changed = true;
  }
  if(filled != d->CheckFilled->isChecked()) {
    filled = d->CheckFilled->isChecked();
    changed = true;
  }
  if(Brush.color() != d->FillColorButt->paletteBackgroundColor()) {
    Brush.setColor(d->FillColorButt->paletteBackgroundColor());
    changed = true;
  }
  if(Brush.style()  != d->FillStyleBox->currentItem()) {
    Brush.setStyle((Qt::BrushStyle)d->FillStyleBox->currentItem());
    changed = true;
  }

  delete d;
  return changed;
}
