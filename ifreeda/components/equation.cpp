/***************************************************************************
                          equation.cpp  -  description
                             -------------------
    begin                : Sat Aug 23 2003
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

#include "equation.h"
#include "guimain.h"


Equation::Equation()
{
  Description = QObject::tr("equation");

  QFont f = QucsSettings.font;
  f.setWeight(QFont::Light);
  f.setPointSizeFloat(12.0);
  QFontMetrics  metrics(f);
  QSize r = metrics.size(0, QObject::tr("Equation"));
  int xb = r.width()  >> 1;
  int yb = r.height() >> 1;

  Lines.append(new Line(-xb, -yb, -xb,  yb,QPen(Qt::darkGreen,2)));
  Lines.append(new Line(-xb,  yb,  xb+3,yb,QPen(Qt::darkBlue,2)));
  Texts.append(new Text(-xb+4,  -yb-3, QObject::tr("Equation"),
			QColor(0,0,0), 12.0));

  x1 = -xb-3;  y1 = -yb-5;
  x2 =  xb+9; y2 =  yb+3;

  tx = x1+4;
  ty = y2+4;
  Model = "Eqn";
  Name  = "Eqn";

  Props.append(new Property("y", "1", true));
  Props.append(new Property("Export", "yes", false,
  		QObject::tr("put result into dataset")+" [yes, no]"));
}

Equation::~Equation()
{
}

Component* Equation::newOne()
{
  return new Equation();
}

Component* Equation::info(QString& Name, char* &BitmapFile, bool getNewOne)
{
  Name = QObject::tr("Equation");
  BitmapFile = "";

  if(getNewOne)  return new Equation();
  return 0;
}
