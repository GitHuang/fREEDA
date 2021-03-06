/***************************************************************************
                        viewpainter.h  -  description
                             -------------------
    begin                : Tue Oct 05 2004
    copyright            : (C) 2004 by Michael Margraf
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

#ifndef VIEWPAINTER_H
#define VIEWPAINTER_H

#include <qpainter.h>

/**
  *@author Michael Margraf
  */

/* This class is neccessary because Qt scaled Fonts looks very ungly.
   Avoiding this is unfortunately not easy.
   This class scales the font point size instead of the font glyphes.
   But this has the disadvantage, that the text size does not increase
   equally with the rest of the drawing. We therefore must compute
   some thing anew each time the paint function is called. The class
   "ViewPainter" supports this. */

class ViewPainter {
public:
  ViewPainter(QPainter *p=0);
  ~ViewPainter();

  void init(QPainter*, float, int, int, int, int);
  void map(int, int, int*, int*);
  void drawPoint(int, int);
  void drawLine (int, int, int, int);
  void drawRect (int, int, int, int);
  void drawRectD(int, int, int, int);
  void drawRoundRect(int, int, int, int);
  void drawEllipse(int, int, int, int);
  void drawArc(int, int, int, int, int, int);
  int  drawText(const QString&, int, int, int *Height=0);
  void fillRect(int, int, int, int, const QColor&);
  void eraseRect(int, int, int, int);
  void drawResizeRect(int, int);

  QPainter *Painter;
  float Scale, DX , DY;
  int LineSpacing;   // updated by init(), just for info
};

#endif
