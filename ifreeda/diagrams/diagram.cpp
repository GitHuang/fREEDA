/***************************************************************************
                                diagram.cpp
                             -----------------
    begin                : Thu Oct 2 2003
    copyright            : (C) 2003, 2004, 2005 by Michael Margraf
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
#include <qabstractitemmodel.h>
#include <qvariant.h>
#include <qlocale.h>
#include <qhash.h>
#include <qicon.h>
#include <qfileinfo.h>
// Above lines are added by Shivam to remove header errors

#include <qdatetime.h>
#include <qregexp.h>
#include <qmessagebox.h>
#include <qmessagebox.h>

#include <q3textstream.h>                                                                                                                                                                            

#if HAVE_CONFIG_H                                                                                                                                                                                    
#include <config.h>
#endif

#if HAVE_IEEEFP_H
#include <ieeefp.h>
#endif

//#include <q3textstream.h>                                                                                                                                                                          //#include <qmessagebox.h>
//#include <qregexp.h>
//#include <qdatetime.h>

#include <stdlib.h>
#include <math.h>
#include <float.h>

#include "diagram.h"
#include "gui.h"                                                                                                                                                                                     
#include "guimain.h"

#ifdef __MINGW32__
#define finite(x) _finite(x)
#endif


using namespace std;

Diagram::Diagram(int _cx, int _cy)
{
  cx = _cx;  cy = _cy;

  xAxis.numGraphs = yAxis.numGraphs = zAxis.numGraphs = 0;
  xAxis.min = xAxis.low =
  yAxis.min = yAxis.low = zAxis.min = zAxis.low = 0.0;
  xAxis.max = xAxis.up =
  yAxis.max = yAxis.up = zAxis.max = zAxis.up = 1.0;
  xAxis.GridOn = yAxis.GridOn = true;
  zAxis.GridOn = false;
  xAxis.log = yAxis.log = zAxis.log = false;

  xAxis.limit_min = yAxis.limit_min = zAxis.limit_min = 0.0;
  xAxis.limit_max = yAxis.limit_max = zAxis.limit_max = 1.0;
  xAxis.step = yAxis.step = zAxis.step = 1.0;
  xAxis.autoScale = yAxis.autoScale = zAxis.autoScale = true;

  Type = isDiagram;
  isSelected = false;
  GridPen = QPen(Qt::lightGray,0);
  Graphs.setAutoDelete(true);
  Arcs.setAutoDelete(true);
  Lines.setAutoDelete(true);
  Texts.setAutoDelete(true);
}

Diagram::~Diagram()
{
}

// ------------------------------------------------------------
// Paint function for most diagrams (cartesian, smith, polar, ...)
void Diagram::paint(ViewPainter *p)
{
  // paint all arcs (1 pixel larger to compensate for strange Qt circles)
  for(struct Arc *pa = Arcs.first(); pa != 0; pa = Arcs.next()) {
    p->Painter->setPen(pa->style);
    p->drawArc(cx+pa->x, cy-pa->y, pa->w+1, pa->h+1, pa->angle, pa->arclen);
  }

  // paint all lines
  for(Line *pl = Lines.first(); pl != 0; pl = Lines.next()) {
    p->Painter->setPen(pl->style);
    p->drawLine(cx+pl->x1, cy-pl->y1, cx+pl->x2, cy-pl->y2);
  }

  Graph *pg;
  // draw all graphs
  for(pg = Graphs.first(); pg != 0; pg = Graphs.next())
    pg->paint(p, cx, cy);


  // write whole text (axis label inclusively)
  QMatrix wm = p->Painter->worldMatrix();
  for(Text *pt = Texts.first(); pt != 0; pt = Texts.next()) {
    p->Painter->setWorldMatrix(
        QMatrix(pt->mCos, -pt->mSin, pt->mSin, pt->mCos,
                 p->DX + float(cx+pt->x) * p->Scale,
                 p->DY + float(cy-pt->y) * p->Scale));

    p->Painter->setPen(pt->Color);
    p->Painter->drawText(0, 0, pt->s);
  }
  p->Painter->setWorldMatrix(wm);
  p->Painter->setWorldXForm(false);

  // draw markers last, so they are at the top of painting layers
  for(pg = Graphs.first(); pg != 0; pg = Graphs.next())
    for(Marker *pm = pg->Markers.first(); pm != 0; pm = pg->Markers.next())
      pm->paint(p, cx, cy);


  if(isSelected) {
    p->Painter->setPen(QPen(Qt::darkGray,3));
    p->drawRect(cx-5, cy-y2-5, x2+10, y2+10);
    p->Painter->setPen(QPen(Qt::darkRed,2));
    p->drawResizeRect(cx, cy-y2);  // markers for changing the size
    p->drawResizeRect(cx, cy);
    p->drawResizeRect(cx+x2, cy-y2);
    p->drawResizeRect(cx+x2, cy);
  }
}

// ------------------------------------------------------------
// Put axis labels into the text list.
void Diagram::createAxisLabels()
{
  QSize s;
  Graph *pg;
  int   x, y;
  QFont f = QucsSettings.font;
  f.setPointSizeFloat(10.0);
  QFontMetrics  metrics(f);
  int LineSpacing = metrics.lineSpacing();


  x = (x2>>1);
  y = -y1;
  if(xAxis.Label.isEmpty()) {
    // write all x labels ----------------------------------------
    for(pg = Graphs.first(); pg != 0; pg = Graphs.next()) {
	DataX *pD = pg->cPointsX.getFirst();
	if(!pD) continue;
	y -= LineSpacing;
	if(Name[0] != 'C') {   // location curve ?
          s = metrics.size(0, pD->Var);
          Texts.append(new Text(x-(s.width()>>1), y, pD->Var, pg->Color, 12.0));
	}
	else {
          s = metrics.size(0, "real("+pg->Var+")");
          Texts.append(new Text(x-(s.width()>>1), y, "real("+pg->Var+")",
                                pg->Color, 12.0));
	}
    }
  }
  else {
    s = metrics.size(0, xAxis.Label);
    Texts.append(new Text(x-(s.width()>>1), y-LineSpacing, xAxis.Label,
                          Qt::black, 12.0));
  }


  x = -x1;
  y = y2>>1;
  if(yAxis.Label.isEmpty()) {
    // draw left y-label for all graphs ------------------------------
    for(pg = Graphs.first(); pg != 0; pg = Graphs.next()) {
      if(pg->yAxisNo != 0)  continue;
      if(pg->cPointsY) {
	if(Name[0] != 'C') {   // location curve ?
          s = metrics.size(0, pg->Var);
          Texts.append(new Text(x, y-(s.width()>>1), pg->Var,
                                pg->Color, 12.0, 0.0, 1.0));
	}
	else {
          s = metrics.size(0, "imag("+pg->Var+")");
          Texts.append(new Text(x, y-(s.width()>>1), "imag("+pg->Var+")",
                                pg->Color, 12.0, 0.0, 1.0));
	}
      }
      else {     // if no data => <invalid>
        s = metrics.size(0, pg->Var+INVALID_STR);
        Texts.append(new Text(x, y-(s.width()>>1), pg->Var+INVALID_STR,
                              pg->Color, 12.0, 0.0, 1.0));
      }
      x -= LineSpacing;
    }
  }
  else {
    s = metrics.size(0, yAxis.Label);
    Texts.append(new Text(x, y-(s.width()>>1), yAxis.Label,
                          Qt::black, 12.0, 0.0, 1.0));
  }


  x = x3;
  y = y2>>1;
  if(zAxis.Label.isEmpty()) {
    // draw right y-label for all graphs ------------------------------
    for(pg = Graphs.first(); pg != 0; pg = Graphs.next()) {
      if(pg->yAxisNo != 1)  continue;
      if(pg->cPointsY) {
	if(Name[0] != 'C') {   // location curve ?
          s = metrics.size(0, pg->Var);
          Texts.append(new Text(x, y+(s.width()>>1), pg->Var,
                                pg->Color, 12.0, 0.0, -1.0));
	}
	else {
          s = metrics.size(0, "imag("+pg->Var+")");
          Texts.append(new Text(x, y+(s.width()>>1), "imag("+pg->Var+")",
                                pg->Color, 12.0, 0.0, -1.0));
	}
      }
      else {     // if no data => <invalid>
        s = metrics.size(0, pg->Var+INVALID_STR);
        Texts.append(new Text(x, y+(s.width()>>1), pg->Var+INVALID_STR,
                              pg->Color, 12.0, 0.0, -1.0));
      }
      x += LineSpacing;
    }
  }
  else {
    s = metrics.size(0, zAxis.Label);
    Texts.append(new Text(x, y+(s.width()>>1), zAxis.Label,
                          Qt::black, 12.0, 0.0, -1.0));
  }
}

// ------------------------------------------------------------
void Diagram::paintScheme(QPainter *p)
{
  p->drawRect(cx, cy-y2, x2, y2);
}

// ------------------------------------------------------------
int Diagram::regionCode(int x, int y)
{
  int code=0;   // code for clipping
  if(x < 0)  code |= 1;
  else if(x > x2)  code |= 2;
  if(y < 0)  code |= 4;
  else if(y > y2)  code |= 8;
  return code;
}

// ------------------------------------------------------------
// Is virtual. This one is for round diagrams only.
bool Diagram::insideDiagram(int x, int y)
{
  int R = (x2 >> 1) + 1;  // +1 seems better (graph sometimes little outside)
  x -= R;
  y -= R;
  return ((x*x + y*y) <= R*R);
}

// ------------------------------------------------------------
// Cohen-Sutherland clipping algorithm
void Diagram::rectClip(int* &p)
{
  int x=0, y=0, dx, dy, code, z=0;
  int x_1 = *(p-4), y_1 = *(p-3);
  int x_2 = *(p-2), y_2 = *(p-1);

  int code1 = regionCode(x_1, y_1);
  int code2 = regionCode(x_2, y_2);
  if((code1 | code2) == 0)  return;  // line completly inside ?

  if(code1 != 0) if(*(p-5) >= 0) { // is there already a line end flag ?
    p++;
    *(p-5) = -2;
  }
  if(code1 & code2) {   // line not visible at all ?
    *(p-4) = x_2;
    *(p-3) = y_2;
    p -= 2;
    return;
  }
  if(code2 != 0) {
    *p     = -2;
    *(p+1) = x_2;
    *(p+2) = y_2;
    z += 3;
  }


  for(;;) {
    if((code1 | code2) == 0) break;  // line completly inside ?

    if(code1)  code = code1;
    else  code = code2;

    dx = x_2 - x_1;
    dy = y_2 - y_1;
    if(code & 1) {
      y = y_1 - dy * x_1 / dx;
      x = 0;
    }
    else if(code & 2) {
      y = y_1 + dy * (x2-x_1) / dx;
      x = x2;
    }
    else if(code & 4) {
      x = x_1 - dx * y_1 / dy;
      y = 0;
    }
    else if(code & 8) {
      x = x_1 + dx * (y2-y_1) / dy;
      y = y2;
    }

    if(code == code1) {
      x_1 = x;
      y_1 = y;
      code1 = regionCode(x, y);
    }
    else {
      x_2 = x;
      y_2 = y;
      code2 = regionCode(x, y);
    }
  }

  *(p-4) = x_1;
  *(p-3) = y_1;
  *(p-2) = x_2;
  *(p-1) = y_2;
  p += z;
}

// ------------------------------------------------------------
// Clipping for round diagrams (smith, polar, ...)
void Diagram::clip(int* &p)
{
  int R = x2 >> 1;
  int x_1 = *(p-4) - R, y_1 = *(p-3) - R;
  int x_2 = *(p-2) - R, y_2 = *(p-1) - R;

  int dt1 = R*R;   // square of radius
  int dt2 = dt1 - x_2*x_2 - y_2*y_2;
  dt1 -= x_1*x_1 + y_1*y_1;

  if(dt1 >= 0) if(dt2 >= 0)  return;  // line completly inside ?

  if(dt1 < 0) if(*(p-5) >= 0) { // is there already a line end flag ?
    p++;
    *(p-5) = -2;
  }

  int x = x_1-x_2;
  int y = y_1-y_2;
  int C = x_1*x + y_1*y;
  int D = x*x + y*y;
  float F = float(C);
  F = F*F + float(dt1)*float(D); // use float because number is easily > 2^32

  x_1 += R;
  y_1 += R;
  x_2 += R;
  y_2 += R;
  if(F <= 0.0) {   // line not visible at all ?
    *(p-4) = x_2;
    *(p-3) = y_2;
    p -= 2;
    return;
  }

  int code = 0;
  R   = int(sqrt(F)+0.5);
  dt1 = C - R;
  if((dt1 > 0) && (dt1 < D)) { // intersection outside start/end point ?
    *(p-4) = x_1 - x*dt1 / D;
    *(p-3) = y_1 - y*dt1 / D;
    code |= 1;
  }
  else {
    *(p-4) = x_1;
    *(p-3) = y_1;
  }

  dt2 = C + R;
  if((dt2 > 0) && (dt2 < D)) { // intersection outside start/end point ?
    *(p-2) = x_1 - x*dt2 / D;
    *(p-1) = y_1 - y*dt2 / D;
    *p = -2;
    p += 3;
    code |= 2;
  }
  *(p-2) = x_2;
  *(p-1) = y_2;

  if(code == 0) {   // intersections both lie outside ?
    *(p-4) = x_2;
    *(p-3) = y_2;
    p -= 2;
  }

}


// ------------------------------------------------------------
// Enlarge memory block if neccessary.
#define  FIT_MEMORY_SIZE  \
  if(p >= p_end) {     \
    Size += 256;        \
    tmp = p - g->Points; \
    p = p_end = g->Points = (int*)realloc(g->Points, Size*sizeof(int)); \
    p += tmp; \
    p_end += Size - 9; \
  } \

// ------------------------------------------------------------
// g->Points must already be empty!!!
void Diagram::calcData(Graph *g)
{
  if(Name[0] == 'T')  return;   // no graph within tabulars

  double *px;
  double *pz = g->cPointsY;
  if(!pz)  return;
  if(g->cPointsX.count() < 1) return;

  int dx, dy, xtmp, ytmp, tmp, i, z, Counter=2;
  int Size = ((2*(g->cPointsX.getFirst()->count) + 1) * g->countY) + 8;
  int *p = (int*)malloc( Size*sizeof(int) );  // create memory for points
  int *p_end;
  g->Points = p_end = p;
  p_end += Size - 5;   // limit of buffer

  double Dummy = 0.0;  // number for 1-dimensional data in 3D cartesian
  double *py = &Dummy;
  if(Name == "Rect3D")
    if(g->countY > 1)  py = g->cPointsX.at(1)->Points;

  Axis *pa;
  if(g->yAxisNo == 0)  pa = &yAxis;
  else  pa = &zAxis;

  if(xAxis.autoScale)  if(yAxis.autoScale)  if(zAxis.autoScale)
    Counter = -50000;

  double Stroke=10.0, Space=10.0; // length of strokes and spaces in pixel
  switch(g->Style) {
    case 0:      // ***** solid line **********************************
      for(i=g->countY; i>0; i--) {  // every branch of curves
	px = g->cPointsX.getFirst()->Points;
	calcCoordinate(px, pz, py, p, p+1, pa);
	p += 2;
	for(z=g->cPointsX.getFirst()->count-1; z>0; z--) {  // every point
	  FIT_MEMORY_SIZE;  // need to enlarge memory block ?
	  calcCoordinate(px, pz, py, p, p+1, pa);
	  p += 2;
	  if(Counter >= 2)   // clipping only if an axis is manual
	    clip(p);
	}
	if(*(p-3) == -2)  p -= 3;  // no single point after "no stroke"
	*(p++) = -10;
	if(py != &Dummy) {   // more-dimensional Rect3D
	  py++;
	  if(py >= (g->cPointsX.at(1)->Points + g->cPointsX.at(1)->count))
	    py = g->cPointsX.at(1)->Points;
	}
      }
/*qDebug("\n****** p=%p", p);
for(int zz=60; zz>0; zz-=2)
  qDebug("c: %d/%d", *(p-zz), *(p-zz+1));*/
      if(Name == "Rect3D") if(g->countY > 1) {
	pz = g->cPointsY;
	for(int j=g->countY/g->cPointsX.at(1)->count; j>0; j--) {
	  DataX *pD = g->cPointsX.first();
	  px = pD->Points;
	  dx = pD->count;
	  pD = g->cPointsX.next();
	  dy = pD->count;
	  for(i=g->cPointsX.getFirst()->count; i>0; i--) {  // every branch
	    py = pD->Points;
	    calcCoordinate(px, pz, py, p, p+1, 0);
	    p += 2;
	    px--;  // back to the current x coordinate
	    py++;  // next y coordinate
	    pz += 2*(dx-1);  // next z coordinate
	    for(z=dy-1; z>0; z--) {  // every point
	      FIT_MEMORY_SIZE;  // need to enlarge memory block ?
	      calcCoordinate(px, pz, py, p, p+1, 0);
	      p += 2;
	      if(Counter >= 2)   // clipping only if an axis is manual
	        rectClip(p);
	      px--;  // back to the current x coordinate
	      py++;  // next y coordinate
	      pz += 2*(dx-1);  // next z coordinate
	    }
	    if(*(p-3) == -2)  p -= 3;  // no single point after "no stroke"
	    *(p++) = -10;

	    px++;   // next x coordinate
	    pz -= 2*dx*dy - 2;  // next z coordinate
	  }
	  pz += 2*dx*(dy-1);
	}
/*qDebug("\n------ p=%p", p);
for(int zz=120; zz>0; zz-=2)
  qDebug("c: %d/%d", *(p-zz), *(p-zz+1));*/
      }

      
      *p = -100;
      return;

    case 1: Stroke = 10.0; Space =  6.0;  break;   // dash line
    case 2: Stroke =  2.0; Space =  4.0;  break;   // dot line
    case 3: Stroke = 24.0; Space =  8.0;  break;   // long dash line

    default:
      if(g->Style == 4)  xtmp = -7;      // **** a star at each point ******
      else if(g->Style == 5)  xtmp = -6; // **** a circle at each point ****
      else if(g->Style == 6)  xtmp = -5; // **** an arrow at each point ****

      for(i=g->countY; i>0; i--) {  // every branch of curves
	px = g->cPointsX.getFirst()->Points;
	for(z=g->cPointsX.getFirst()->count; z>0; z--) {  // every point
	  FIT_MEMORY_SIZE;  // need to enlarge memory block ?
	  calcCoordinate(px, pz, py, p, p+1, pa);
	  if(insideDiagram(*p, *(p+1))) {    // within diagram ?
	    *(p+2) = xtmp;
	    p += 3;
	  }
	}
	*(p++) = -10;
	if(py != &Dummy) {   // more-dimensional Rect3D
	  py++;
	  if(py >= (g->cPointsX.at(1)->Points + g->cPointsX.at(1)->count))
	    py = g->cPointsX.at(1)->Points;
	}
      }
      *p = -100;
/*qDebug("\n******");
for(int zz=0; zz<60; zz+=2)
  qDebug("c: %d/%d", *(g->Points+zz), *(g->Points+zz+1));*/
      return;
  }

  double alpha, dist;
  int Flag;    // current state: 1=stroke, 0=space
  for(i=g->countY; i>0; i--) {  // every branch of curves
    Flag = 1;
    dist = -Stroke;
    px = g->cPointsX.getFirst()->Points;
    calcCoordinate(px, pz, py, &xtmp, &ytmp, pa);
    *(p++) = xtmp;
    *(p++) = ytmp;
    Counter = 1;
    for(z=g->cPointsX.getFirst()->count-1; z>0; z--) {
      dx = xtmp;
      dy = ytmp;
      calcCoordinate(px, pz, py, &xtmp, &ytmp, pa);
      dx = xtmp - dx;
      dy = ytmp - dy;
      dist += sqrt(double(dx*dx + dy*dy)); // distance between points
      if(Flag == 1) if(dist <= 0.0) {
	FIT_MEMORY_SIZE;  // need to enlarge memory block ?

	*(p++) = xtmp;    // if stroke then save points
	*(p++) = ytmp;
	if((++Counter) >= 2)  clip(p);
	continue;
      }
      alpha = atan2(double(dy), double(dx));   // slope for interpolation
      while(dist > 0) {   // stroke or space finished ?
	FIT_MEMORY_SIZE;  // need to enlarge memory block ?

	*(p++) = xtmp - int(dist*cos(alpha) + 0.5); // linearly interpolate
	*(p++) = ytmp - int(dist*sin(alpha) + 0.5);
	if((++Counter) >= 2)  clip(p);

        if(Flag == 0) {
          dist -= Stroke;
          if(dist <= 0) {
	    *(p++) = xtmp;  // don't forget point after ...
	    *(p++) = ytmp;  // ... interpolated point
	    if((++Counter) >= 2)  clip(p);
          }
        }
        else {
	  dist -= Space;
	  if(*(p-3) < 0)  p -= 2;
	  else *(p++) = -2;  // value for interrupt stroke
	  if(Counter < 0)  Counter = -50000;   // if auto-scale
	  else  Counter = 0;
        }
        Flag ^= 1; // toggle between stroke and space
      }

    } // of x loop
    if(*(p-3) == -2)
      p -= 3;  // no single point after "no stroke"
    *(p++) = -10;
    if(py != &Dummy) {   // more-dimensional Rect3D
      py++;
      if(py >= (g->cPointsX.at(1)->Points + g->cPointsX.at(1)->count))
        py = g->cPointsX.at(1)->Points;
    }
  } // of y loop



 if(Name == "Rect3D") if(g->countY > 1) {
  pz = g->cPointsY;
  for(int j=g->countY/g->cPointsX.at(1)->count; j>0; j--) {
    DataX *pD = g->cPointsX.first();
    px = pD->Points;
    int xlen = pD->count;
    pD = g->cPointsX.next();
    for(i=g->cPointsX.getFirst()->count; i>0; i--) {  // every branch
      Flag = 1;
      dist = -Stroke;
      py = pD->Points;
      calcCoordinate(px, pz, py, &xtmp, &ytmp, 0);
      *(p++) = xtmp;
      *(p++) = ytmp;
      Counter = 1;
      px--;  // back to the current x coordinate
      py++;  // next y coordinate
      pz += 2*(xlen-1);  // next z coordinate
      for(z=pD->count-1; z>0; z--) {  // every point
        dx = xtmp;
        dy = ytmp;
        calcCoordinate(px, pz, py, &xtmp, &ytmp, 0);
        px--;  // back to the current x coordinate
        py++;  // next y coordinate
        pz += 2*(xlen-1);  // next z coordinate

        dx = xtmp - dx;
        dy = ytmp - dy;
        dist += sqrt(double(dx*dx + dy*dy)); // distance between points
        if(Flag == 1) if(dist <= 0.0) {
	  FIT_MEMORY_SIZE;  // need to enlarge memory block ?

	  *(p++) = xtmp;    // if stroke then save points
	  *(p++) = ytmp;
	  if((++Counter) >= 2)  clip(p);
	  continue;
        }
        alpha = atan2(double(dy), double(dx));   // slope for interpolation
        while(dist > 0) {   // stroke or space finished ?
	  FIT_MEMORY_SIZE;  // need to enlarge memory block ?

	  *(p++) = xtmp - int(dist*cos(alpha) + 0.5); // linearly interpolate
	  *(p++) = ytmp - int(dist*sin(alpha) + 0.5);
	  if((++Counter) >= 2)  clip(p);

          if(Flag == 0) {
            dist -= Stroke;
            if(dist <= 0) {
	      *(p++) = xtmp;  // don't forget point after ...
	      *(p++) = ytmp;  // ... interpolated point
	      if((++Counter) >= 2)  clip(p);
            }
          }
          else {
	    dist -= Space;
	    if(*(p-3) < 0)  p -= 2;
	    else *(p++) = -2;  // value for interrupt stroke
	    if(Counter < 0)  Counter = -50000;   // if auto-scale
	    else  Counter = 0;
          }
          Flag ^= 1; // toggle between stroke and space
        }
      }
      if(*(p-3) == -2)  p -= 3;  // no single point after "no stroke"
      *(p++) = -10;

      px++;   // next x coordinate
      pz -= 2*xlen*pD->count - 2;  // next z coordinate
    }
    pz += 2*xlen*(pD->count-1);
  }
 }

  *p = -100;
}

// -------------------------------------------------------
void Diagram::Bounding(int& _x1, int& _y1, int& _x2, int& _y2)
{
  _x1 = cx-x1;
  _y1 = cy-y2;
  _x2 = cx+x3;
  _y2 = cy+y1;

  if(Name == "Tab") return;

  bool xLabelHide=true, yLabelHide=true, zLabelHide=true;
  QFontMetrics  metrics(QucsSettings.font);
  if(!xAxis.Label.isEmpty()) {
    xLabelHide = false;
    _x1 -= metrics.lineSpacing();
  }
  if(!yAxis.Label.isEmpty()) {
    yLabelHide = false;
    _y2 += metrics.lineSpacing();
  }
  if(!zAxis.Label.isEmpty()) {
    zLabelHide = false;
    _x2 += metrics.lineSpacing();
  }

  for(Graph *pg = Graphs.first(); pg != 0; pg = Graphs.next()) {

    if(pg->yAxisNo == 0) {   // used with left axis ?
      if(xLabelHide)
        _x1 -= metrics.lineSpacing();   // expand bounding with text size
    }
    else {
      if(zLabelHide)
        _x2 += metrics.lineSpacing();
    }
    if(yLabelHide) _y2 += metrics.lineSpacing();

  }
}

// -------------------------------------------------------
bool Diagram::getSelected(int x_, int y_)
{
  if(x_ >= cx-x1) if(x_ <= cx+x3) if(y_ >= cy-y2) if(y_ <= cy+y1)
    return true;

  return false;
}

// ------------------------------------------------------------
// Checks if the resize area was clicked. If so return "true" and sets
// x1/y1 and x2/y2 to the border coordinates to draw a rectangle.
bool Diagram::ResizeTouched(int x, int y)
{
  if(x < cx-5) return false;
  if(x > cx+x2+5) return false;
  if(y < cy-y2-5) return false;
  if(y > cy+5) return false;

  State = 0;
  if(x < cx+5) State = 1;
  else  if(x <= cx+x2-5) return false;
  if(y > cy-5) State |= 2;
  else  if(y >= cy-y2+5) return false;

  return true;
}

// --------------------------------------------------------------------------
void Diagram::loadGraphData(const QString& defaultDataSet)
{
  yAxis.numGraphs = zAxis.numGraphs = 0;
  yAxis.min = zAxis.min = xAxis.min =  DBL_MAX;
  yAxis.max = zAxis.max = xAxis.max = -DBL_MAX;

  for(Graph *pg = Graphs.first(); pg != 0; pg = Graphs.next())
    loadVarData(defaultDataSet, pg);  // load data, determine max/min values

  if(xAxis.min > xAxis.max) {
    xAxis.min = 0.0;
    xAxis.max = 1.0;
  }
  if(yAxis.min > yAxis.max) {
    yAxis.min = 0.0;
    yAxis.max = 1.0;
  }
  if(zAxis.min > zAxis.max) {
    zAxis.min = 0.0;
    zAxis.max = 1.0;
  }
/*  if((Name == "Polar") || (Name == "Smith")) {  // one axis only
    if(yAxis.min > zAxis.min)  yAxis.min = zAxis.min;
    if(yAxis.max < zAxis.max)  yAxis.max = zAxis.max;
  }*/
/*qDebug("x:  %g, %g", xAxis.min, xAxis.max);
qDebug("y:  %g, %g", yAxis.min, yAxis.max);
qDebug("z:  %g, %g", zAxis.min, zAxis.max);*/
  updateGraphData();
}

// ------------------------------------------------------------------------
// Calculate diagram again without reading dataset from file.
void Diagram::recalcGraphData()
{
  yAxis.min = zAxis.min = xAxis.min =  DBL_MAX;
  yAxis.max = zAxis.max = xAxis.max = -DBL_MAX;

  int z;
  double x, y, *p;
  // get maximum and minimum values
  for(Graph *pg = Graphs.first(); pg != 0; pg = Graphs.next()) {
    DataX *pD = pg->cPointsX.first();
    if(pD == 0) continue;
    if(Name[0] != 'C') {   // not for location curves
      p = pD->Points;
      for(z=pD->count; z>0; z--) { // check x coordinates (1. dimension)
        x = *(p++);
        if(x > xAxis.max) xAxis.max = x;
        if(x < xAxis.min) xAxis.min = x;
      }
    }

    if(Name == "Rect3D") {
      pD = pg->cPointsX.next();
      if(pD) {
        p = pD->Points;
        for(z=pD->count; z>0; z--) { // check y coordinates (2. dimension)
          y = *(p++);
          if(y > yAxis.max) yAxis.max = y;
          if(y < yAxis.min) yAxis.min = y;
        }
      }
    }

    Axis *pa;
    if(pg->yAxisNo == 0)  pa = &yAxis;
    else  pa = &zAxis;
    p = pg->cPointsY;
    for(z=pg->countY*pD->count; z>0; z--) {  // check every y coordinate
      x = *(p++);
      y = *(p++);
    
      if(Name[0] != 'C') {
        if(fabs(y) >= 1e-250) x = sqrt(x*x+y*y);
        if(finite(x)) {
          if(x > pa->max) pa->max = x;
          if(x < pa->min) pa->min = x;
        }
      }
      else {   // location curve needs different treatment
        if(finite(x)) {
          if(x > xAxis.max) xAxis.max = x;
          if(x < xAxis.min) xAxis.min = x;
        }
        if(finite(y)) {
          if(y > pa->max) pa->max = y;
          if(y < pa->min) pa->min = y;
        }
      }
    }
  }

  if(xAxis.min > xAxis.max) {
    xAxis.min = 0.0;
    xAxis.max = 1.0;
  }
  if(yAxis.min > yAxis.max) {
    yAxis.min = 0.0;
    yAxis.max = 1.0;
  }
  if(zAxis.min > zAxis.max) {
    zAxis.min = 0.0;
    zAxis.max = 1.0;
  }
  if((Name == "Polar") || (Name == "Smith")) {  // one axis only
    if(yAxis.min > zAxis.min)  yAxis.min = zAxis.min;
    if(yAxis.max < zAxis.max)  yAxis.max = zAxis.max;
  }

  updateGraphData();
}

// ------------------------------------------------------------------------
void Diagram::updateGraphData()
{
  int valid = calcDiagram();   // do not calculate graph data if invalid

  for(Graph *pg = Graphs.first(); pg != 0; pg = Graphs.next()) {
    if(pg->Points != 0) { free(pg->Points);  pg->Points = 0; }
    if((valid & (pg->yAxisNo+1)) != 0)
      calcData(pg);   // calculate screen coordinates
    else if(pg->cPointsY) { delete[] pg->cPointsY;  pg->cPointsY = 0; }

    for(Marker *pm = pg->Markers.first(); pm != 0; pm = pg->Markers.next())
      pm->createText();
  }

  createAxisLabels();  // virtual function
}

// --------------------------------------------------------------------------
bool Diagram::loadVarData(const QString& fileName, Graph *g)
{
  g->countY = 0;
  g->cPointsX.clear();
  if(g->cPointsY) { delete[] g->cPointsY;  g->cPointsY = 0; }
  if(g->Var.isEmpty()) return false;

  QFile file;
  QString Variable;
  int pos = g->Var.find(':');
  if(pos <= 0) {
    file.setName(fileName);
    Variable = g->Var;
  }
  else {
    file.setName(g->Var.left(pos)+".dat");
    Variable = g->Var.mid(pos+1);
  }

  file.setName(QucsWorkDir.filePath(file.name()));
  if(!file.open(QIODevice::ReadOnly)) {
//    QMessageBox::critical(0, QObject::tr("Error"),
//                 QObject::tr("Cannot load dataset: ")+file.name());
    return false;
  }

  // *****************************************************************
  // To strongly speed up the file read operation the whole file is
  // read into the memory in one piece.
  Q3TextStream ReadWhole(&file);
  QString FileString = ReadWhole.read();
  file.close();


  QString Line, tmp, Var;
  // *****************************************************************
  // look for variable name in data file  ****************************
  int i=0, j=0, k=0;   // i and j must not be used temporarily !!!
  i = FileString.find('<')+1;
  while(i > 0) {
    j = FileString.find('>', i);
    Line = FileString.mid(i, j-i);
    i = FileString.find('<', j)+1;
    if(Line.left(3) == "dep") {
      tmp = Line.section(' ', 1, 1);
      if(Variable != tmp) continue; // found variable with name sought for ?

      k = 2;
      tmp = Line.section(' ',k,k);
      while(!tmp.isEmpty()) {
        g->cPointsX.append(new DataX(tmp));  // name of independet variable
        k++;
        tmp = Line.section(' ',k,k);
      }
      break;
    }
    if(Line.left(5) == "indep") {
      tmp = Line.section(' ', 1, 1);
      if(Variable != tmp) continue;  // found variable with name sought for ?
      break;
    }
  }

  if(i <= 0) {
//    QMessageBox::critical(0, QObject::tr("Error"),
//                 QObject::tr("Cannot find variable: ")+Variable);
    return false;   // return if data name was not found
  }

  Axis *pa;
  // *****************************************************************
  // get independent variable ****************************************
  bool ok=true, ok2=true;
  double *p;
  int counting = 0;
  if(g->cPointsX.isEmpty()) {    // create independent variable by myself ?
    tmp = Line.section(' ', 2, 2);  // get number of points
    counting = tmp.toInt(&ok);
    g->cPointsX.append(new DataX("number", 0, counting));
    if(!ok) {
//      QMessageBox::critical(0, QObject::tr("Error"),
//                   QObject::tr("Cannot get size of independent data \"")+
//		   Variable+"\"");
      // do not clear cPointX, but show it as invalid
//      g->cPointsX.clear();
      return false;
    }

    p = new double[counting];  // memory of new independent variable
    g->countY = 1;
    g->cPointsX.current()->Points = p;
    for(int z=1; z<=counting; z++)  *(p++) = double(z);
    if(xAxis.min > 1.0)  xAxis.min = 1.0;
    if(xAxis.max < double(counting))  xAxis.max = double(counting);
  }
  else {  // ...................................
    // get independent variables from data file
    g->countY = 1;
    DataX *bLast = 0;
    if(Name == "Rect3D")  bLast = g->cPointsX.at(1);  // y axis for Rect3D

    double min_tmp = xAxis.min, max_tmp = xAxis.max;
    for(DataX *pD = g->cPointsX.last(); pD!=0; pD = g->cPointsX.prev()) {
      pa = &xAxis;
      if(pD == g->cPointsX.getFirst()) {
	xAxis.min = min_tmp;    // only count first independent variable
	xAxis.max = max_tmp;
      }
      else if(pD == bLast)  pa = &yAxis;   // y axis for Rect3D
      counting = loadIndepVarData(pD->Var, FileString, pa, g);
      if(counting <= 0) {     // failed to load independent variable ?
        // do not clear cPointX, but show it as invalid
//	g->cPointsX.clear();
        return false;  // error message was already created
      }
      g->countY *= counting;
    }
    g->countY /= counting;
  }


  // *****************************************************************
  // get dependent variables *****************************************
  counting  *= g->countY;
  p = new double[2*counting]; // memory for dependent variables
  g->cPointsY = p;
  if(g->yAxisNo == 0)  pa = &yAxis;   // for which axis
  else  pa = &zAxis;
  (pa->numGraphs)++;    // count graphs

  double x, y;
  QRegExp WhiteSpace("\\s");
  QRegExp noWhiteSpace("\\S");
  i = FileString.find(noWhiteSpace, j+1);
  j = FileString.find(WhiteSpace, i);
  Line = FileString.mid(i, j-i);
  for(int z=counting; z>0; z--) {
    k = Line.find('j');
    if(k < 0) {
      x = Line.toDouble(&ok);
      y = 0.0;
    }
    else {
      tmp = Line.mid(k);  // imaginary part
      //tmp.at(0) = Line.at(k-1);   // copy sign over "j"
      // Changed by Shivam
       tmp[0] = Line[k-1];
      y = tmp.toDouble(&ok);
      Line = Line.left(k-1);  // real part
      x = Line.toDouble(&ok2);
    }
    if((!ok) || (!ok2)) {
//      QMessageBox::critical(0, QObject::tr("Error"),
//		QObject::tr("Too few dependent data \"") + Variable+"\"");
      // do not clear cPointX, but show it as invalid
//      g->cPointsX.clear();
      delete[] g->cPointsY;  g->cPointsY = 0;
      return false;
    }
    *(p++) = x;
    *(p++) = y;
    if(Name[0] != 'C') {
      if(fabs(y) >= 1e-250) x = sqrt(x*x+y*y);
      if(finite(x)) {
        if(x > pa->max) pa->max = x;
        if(x < pa->min) pa->min = x;
      }
    }
    else {   // location curve needs different treatment
      if(finite(x)) {
        if(x > xAxis.max) xAxis.max = x;
        if(x < xAxis.min) xAxis.min = x;
      }
      if(finite(y)) {
        if(y > pa->max) pa->max = y;
        if(y < pa->min) pa->min = y;
      }
    }

    i = FileString.find(noWhiteSpace, j);
    j = FileString.find(WhiteSpace, i);
    Line = FileString.mid(i, j-i);
  }

  return true;
}

// --------------------------------------------------------------------------
// Reads the data of an independent variable. Returns the number of points.
int Diagram::loadIndepVarData(const QString& var,
			      const QString& FileString, Axis *pa, Graph *pg)
{
  QString Line, tmp;

  int i=0, j=0;
  i = FileString.find('<')+1;
  if(i > 0)
  do {    // look for variable name in data file
    j = FileString.find('>', i);
    Line = FileString.mid(i, j-i);
    i = FileString.find('<', j)+1;
    if(Line.left(5) == "indep") {
      tmp = Line.section(' ', 1, 1);
      if(var == tmp) break;     // found variable with name sought for ?
    }
    else if(Line.left(3) == "dep")   // dependent variable can also be used...
      if(Line.section(' ', 3, 3).isEmpty()) {   // ...if only one dependency
	tmp = Line.section(' ', 1, 1);
	if(var == tmp) {     // found variable with name sought for ?
	  int z = FileString.find("<indep "+Line.section(' ', 2, 2)+" ");
	  if(z <= 0) { i = -1; break; }
	  tmp = FileString.mid(z, FileString.find('>', z)-z);
	  Line = Line.section(' ', 0, 1)+" "+tmp.section(' ', 2, 2);
	  break;
	}
      }

  } while(i > 0);

  if(i <= 0) {
//    QMessageBox::critical(0, QObject::tr("Error"),
//	QObject::tr("Independent data \"")+var+QObject::tr("\" not found"));
    return -1;
  }

  bool ok;
  tmp = Line.section(' ', 2, 2);  // get number of points
  int n = tmp.toInt(&ok);
  if(!ok) {
//    QMessageBox::critical(0, QObject::tr("Error"),
//	QObject::tr("Cannot get size of independent data \"")+var+"\"");
    return -1;
  }

  double *p = new double[n];     // memory for new independent variable
  DataX *pD = pg->cPointsX.current();
  pD->Points = p;
  pD->count  = n;

  double x;
  QRegExp WhiteSpace("\\s");
  QRegExp noWhiteSpace("\\S");
  i = FileString.find(noWhiteSpace, j+1);
  j = FileString.find(WhiteSpace, i);
  Line = FileString.mid(i, j-i);
  for(int z=0; z<n; z++) {
    x = Line.toDouble(&ok);  // get number
    if(!ok) {
//      QMessageBox::critical(0, QObject::tr("Error"),
//		 QObject::tr("Too few independent data \"") + var + "\"");
      delete[] pD->Points;  pD->Points = 0;
      return -1;
    }
    *(p++) = x;
    if(Name[0] != 'C')   // not for location curves
      if(finite(x)) {
        if(x > pa->max) pa->max = x;
        if(x < pa->min) pa->min = x;
      }

    i = FileString.find(noWhiteSpace, j);
    j = FileString.find(WhiteSpace, i);
    Line = FileString.mid(i, j-i);
  }

  return n;   // return number of independent data
}

// ------------------------------------------------------------
void Diagram::setCenter(int x, int y, bool relative)
{
  if(relative) {
    cx += x;  cy += y;
  }
  else {
    cx = x;  cy = y;
  }
}

// -------------------------------------------------------
void Diagram::getCenter(int& x, int& y)
{
  x = cx + (x2 >> 1);
  y = cy - (y2 >> 1);
}

// ------------------------------------------------------------
Diagram* Diagram::newOne()
{
  return new Diagram();
}

// ------------------------------------------------------------
QString Diagram::save()
{
  QString s = "<"+Name+" "+QString::number(cx)+" "+QString::number(cy)+" ";
  s += QString::number(x2)+" "+QString::number(y2)+" ";
  if(xAxis.GridOn) s+= "1 ";
  else s += "0 ";
  s += GridPen.color().name() + " " + QString::number(GridPen.style());

  if(xAxis.log) s+= " 1";  else s += " 0";
  char c = '0';
  if(yAxis.log)  c |= 1;
  if(zAxis.log)  c |= 2;
  s += c;

  if(xAxis.autoScale)  s+= " 1 ";
  else  s+= " 0 ";
  s += QString::number(xAxis.limit_min) + " ";
  s += QString::number(xAxis.step) + " ";
  s += QString::number(xAxis.limit_max);
  if(yAxis.autoScale)  s+= " 1 ";
  else  s+= " 0 ";
  s += QString::number(yAxis.limit_min) + " ";
  s += QString::number(yAxis.step) + " ";
  s += QString::number(yAxis.limit_max);
  if(zAxis.autoScale)  s+= " 1 ";
  else  s+= " 0 ";
  s += QString::number(zAxis.limit_min) + " ";
  s += QString::number(zAxis.step) + " ";
  s += QString::number(zAxis.limit_max) + " ";

  s += QString::number(rotX)+" "+QString::number(rotY)+" "+
       QString::number(rotZ);

  // labels can contain spaces -> must be last items in the line
  s += " \""+xAxis.Label+"\" \""+yAxis.Label+"\" \""+zAxis.Label+"\">\n";

  for(Graph *pg=Graphs.first(); pg != 0; pg=Graphs.next())
    s += pg->save()+"\n";

  s += "  </"+Name+">";
  return s;
}

// ------------------------------------------------------------
bool Diagram::load(const QString& Line, Q3TextStream *stream)
{
  bool ok;
  QString s = Line;

  if(s.at(0) != '<') return false;
  if(s.at(s.length()-1) != '>') return false;
  s = s.mid(1, s.length()-2);   // cut off start and end character

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

  n = s.section(' ',5,5);    // GridOn
  xAxis.GridOn = yAxis.GridOn = n.at(0) != '0';

  n = s.section(' ',6,6);    // color for GridPen
  QColor co;
  co.setNamedColor(n);
  GridPen.setColor(co);
  if(!GridPen.color().isValid()) return false;

  n = s.section(' ',7,7);    // line style
  GridPen.setStyle((Qt::PenStyle)n.toInt(&ok));
  if(!ok) return false;

  char c;
  n = s.section(' ',8,8);    // xlog, ylog
  xAxis.log = n.at(0) != '0';
  c = n.at(1).latin1();
  yAxis.log = ((c - '0') & 1) == 1;
  zAxis.log = ((c - '0') & 2) == 2;

  n = s.section(' ',9,9);   // xAxis.autoScale
  if(n.at(0) != '"') {      // backward compatible
    if(n == "1")  xAxis.autoScale = true;
    else  xAxis.autoScale = false;

    n = s.section(' ',10,10);    // xAxis.limit_min
    xAxis.limit_min = n.toDouble(&ok);
    if(!ok) return false;

    n = s.section(' ',11,11);  // xAxis.step
    xAxis.step = n.toDouble(&ok);
    if(!ok) return false;

    n = s.section(' ',12,12);  // xAxis.limit_max
    xAxis.limit_max = n.toDouble(&ok);
    if(!ok) return false;

    n = s.section(' ',13,13);    // yAxis.autoScale
    if(n == "1")  yAxis.autoScale = true;
    else  yAxis.autoScale = false;

    n = s.section(' ',14,14);    // yAxis.limit_min
    yAxis.limit_min = n.toDouble(&ok);
    if(!ok) return false;

    n = s.section(' ',15,15);    // yAxis.step
    yAxis.step = n.toDouble(&ok);
    if(!ok) return false;

    n = s.section(' ',16,16);    // yAxis.limit_max
    yAxis.limit_max = n.toDouble(&ok);
    if(!ok) return false;

    n = s.section(' ',17,17);    // zAxis.autoScale
    if(n == "1")  zAxis.autoScale = true;
    else  zAxis.autoScale = false;

    n = s.section(' ',18,18);    // zAxis.limit_min
    zAxis.limit_min = n.toDouble(&ok);
    if(!ok) return false;

    n = s.section(' ',19,19);    // zAxis.step
    zAxis.step = n.toDouble(&ok);
    if(!ok) return false;

    n = s.section(' ',20,20);    // zAxis.limit_max
    zAxis.limit_max = n.toDouble(&ok);
    if(!ok) return false;

    n = s.section(' ',21,21); // rotX
    if(n.at(0) != '"') {      // backward compatible
      rotX = n.toInt(&ok);
      if(!ok) return false;

      n = s.section(' ',22,22); // rotY
      rotY = n.toInt(&ok);
      if(!ok) return false;

      n = s.section(' ',23,23); // rotZ
      rotZ = n.toInt(&ok);
      if(!ok) return false;
    }
  }

  xAxis.Label = s.section('"',1,1);   // xLabel
  yAxis.Label = s.section('"',3,3);   // yLabel left
  zAxis.Label = s.section('"',5,5);   // yLabel right

  Graph *pg;
  // .......................................................
  // load graphs of the diagram
  while(!stream->atEnd()) {
    s = stream->readLine();
    s = s.stripWhiteSpace();
    if(s.isEmpty()) continue;

    if(s == ("</"+Name+">")) return true;  // found end tag ?
    if(s.section(' ', 0,0) == "<Mkr") {

      // .......................................................
      // load markers of the diagram
      pg = Graphs.current();
      if(!pg)  return false;
      Marker *pm = new Marker(this, pg);
      if(!pm->load(s)) {
	delete pm;
	return false;
      }
      pg->Markers.append(pm);
      continue;
    }

    pg = new Graph();
    if(!pg->load(s)) {
      delete pg;
      return false;
    }
    Graphs.append(pg);
  }

  return false;   // end tag missing
}

// --------------------------------------------------------------
void Diagram::calcSmithAxisScale(Axis *Axis, int& GridX, int& GridY)
{
  xAxis.low = xAxis.min;
  xAxis.up  = xAxis.max;

  Axis->low = 0.0;
  if(fabs(Axis->min) > Axis->max)
    Axis->max = fabs(Axis->min);  // also fit negative values
  if(Axis->autoScale) {
    if(Axis->max > 1.01)  Axis->up = 1.05*Axis->max;
    else  Axis->up = 1.0;
    GridX = GridY = 4;
  }
  else {
    Axis->up = Axis->limit_max = fabs(Axis->limit_max);
    GridX = GridY = int(Axis->step);
  }
}

// ------------------------------------------------------------
void Diagram::createSmithChart(Axis *Axis, int Mode)
{
  int GridX;    // number of arcs with re(z)=const
  int GridY;    // number of arcs with im(z)=const
  calcSmithAxisScale(Axis, GridX, GridY);


  if(!xAxis.GridOn)  return;

  bool Zplane = ((Mode & 1) == 1);   // impedance or admittance chart ?
  bool Above  = ((Mode & 2) == 2);   // paint upper half ?
  bool Below  = ((Mode & 4) == 4);   // paint lower half ?

  int dx2 = x2>>1;

  double im, n_cos, n_sin, real, real1, real2, root;
  double rMAXq = Axis->up*Axis->up;
  int    theta, beta, phi, len, m, x, y;

  // ....................................................
  // draw arcs with im(z)=const

  for(m=1; m<GridY; m++) {
    n_sin = M_PI*double(m)/double(GridY);
    n_cos = cos(n_sin);
    n_sin = sin(n_sin);
    im = (1-n_cos)/n_sin * pow(Axis->up,0.7); // up^0.7 is beauty correction
    x  = int((1-im)/Axis->up*dx2);
    y  = int(im/Axis->up*x2);

    if(Axis->up <= 1.0) {       // Smith chart with |r|=1
      beta  = int(16.0*180.0*atan2(n_sin-im,n_cos-1)/M_PI - 0.5);
      if(beta<0) beta += 16*360;
      theta = 16*270-beta;
    }
    else {         // Smith chart with |r|>1
      im = 1/im;
      real = (rMAXq+1)/(rMAXq-1);
      root =  real*real - im*im-1;
      if(root<0) {       // circle lies completely within the Smith chart ?
        beta = 0;        // yes, ...
        theta = 16*360;  // ... draw whole circle
      }
      else {
	// calculate both intersections with most outer circle
	real1 =  sqrt(root)-real;
	real2 = -sqrt(root)-real;

	root  = (real1+1)*(real1+1) + im*im;
	n_cos = (real1*real1 + im*im - 1) / root;
	n_sin = 2*im / root;
	beta  = int(16.0*180.0*atan2(n_sin-1/im,n_cos-1)/M_PI);
	if(beta<0) beta += 16*360;

	root  = (real2+1)*(real2+1) + im*im;
	n_cos = (real2*real2 + im*im - 1) / root;
	n_sin = 2*im / root;
	theta  = int(16.0*180.0*atan2(n_sin-1/im,n_cos-1)/M_PI);
	if(theta<0) theta += 16*360;
	theta = theta - beta;   // arc length
	if(theta < 0) theta = 16*360+theta;
      }
    }

    if(Zplane)
      x += dx2;
    else {
      x -= dx2;
      beta = 16*180 - beta - theta;  // mirror
      if(beta < 0) beta += 16*360;   // angle has to be > 0
    }

    if(Above)
      Arcs.append(new struct Arc(x, dx2+y, y, y, beta, theta, GridPen));
    if(Below)
      Arcs.append(new struct Arc(x, dx2, y, y, 16*360-beta-theta, theta, GridPen));
  }

  // ....................................................
  // draw  arcs with Re(z)=const
  theta = 0;       // arc length
  beta  = 16*180;  // start angle
  if(Above)  { beta = 0;  theta = 16*180; }
  if(Below)  theta += 16*180;

  for(m=1; m<GridX; m++) {
    im = m*(Axis->up+1.0)/GridX - Axis->up;
    x  = int(im/Axis->up*double(dx2) + 0.5);  // center
    y  = int((1.0-im)/Axis->up*double(dx2));  // diameter

    if(Zplane)  x += dx2;
    else  x = 0;
    if(fabs(fabs(im)-1.0) > 0.2)   // if too near to |r|=1, it looks ugly
      Arcs.append(new struct Arc(x, dx2+(y>>1), y, y, beta, theta, GridPen));

    if(Axis->up > 1.0) {  // draw arcs on the rigth-handed side ?
      im = 1.0-im;
      im = (rMAXq-1.0)/(im*(im/2.0+1.0)) - 1.0;
      if(Zplane)  x += y;
      else  x -= y;
      if(im >= 1.0)
	Arcs.append(new struct Arc(x, dx2+(y>>1), y, y, beta, theta, GridPen));
      else {
	phi = int(16.0*180.0/M_PI*acos(im));
	len = 16*180-phi;
	if(Above && Below)  len += len;
	else if(Below)  phi = 16*180;
	if(!Zplane)  phi += 16*180;
	Arcs.append(new struct Arc(x, dx2+(y>>1), y, y, phi, len, GridPen));
      }
    }
  }


  // ....................................................
  if(Axis->up > 1.0) {  // draw circle with |r|=1 ?
    x = int(x2/Axis->up + 0.5);
    Arcs.append(new struct Arc(dx2-(x>>1), dx2+(x>>1), x, x, beta, theta,
			QPen(Qt::black,0)));

    // vertical line Re(r)=1 (visible only if |r|>1)
    x = int(x2/Axis->up + 0.5) >> 1;
    y = int(sqrt(rMAXq-1)/Axis->up*dx2 + 0.5);
    if(Zplane)  x += dx2;
    else  x = dx2 - x;
    if(Above)  m = y;
    else  m = 0;
    if(!Below)  y = 0;
    Lines.append(new Line(x, dx2+m, x, dx2-y, GridPen));

    if(Below)
      Texts.append(new Text(0, 4, StringNum(Axis->up)));
    else
      Texts.append(
        new Text(0, y2-4-QucsSettings.font.pointSize(), StringNum(Axis->up)));
  }

}


// --------------------------------------------------------------
void Diagram::calcPolarAxisScale(Axis *Axis, double& numGrids,
				 double& GridStep, double& zD)
{
  if(Axis->autoScale) {  // auto-scale or user defined limits ?
    double Expo, Base;
    numGrids = floor(double(x2)/80.0); // minimal grid is 40 pixel
    Expo = floor(log10(Axis->max/numGrids));
    Base = Axis->max/numGrids/pow(10.0,Expo);// get first significant digit
    if(Base < 3.5) {       // use only 1, 2 and 5, which ever is best fitted
      if(Base < 1.5) Base = 1.0;
      else Base = 2.0;
    }
    else {
      if(Base < 7.5) Base = 5.0;
      else { Base = 1.0; Expo++; }
    }
    GridStep = Base * pow(10.0,Expo); // grid distance in real values
    numGrids -= floor(numGrids - Axis->max/GridStep); // correct num errors
    Axis->up = GridStep*numGrids;
  }
  else {   // no auto-scale
    Axis->up = Axis->limit_max = fabs(Axis->limit_max);
    GridStep = Axis->step;
    numGrids = Axis->limit_max / Axis->step;
  }
  zD = double(x2) / numGrids;   // grid distance in pixel
}

// ------------------------------------------------------------
void Diagram::createPolarDiagram(Axis *Axis, int Mode)
{
  xAxis.low  = xAxis.min;
  xAxis.up   = xAxis.max;
  Axis->low = 0.0;
  if(fabs(Axis->min) > Axis->max)
    Axis->max = fabs(Axis->min);  // also fit negative values


  bool Above  = ((Mode & 1) == 1);  // paint upper half ?
  bool Below  = ((Mode & 2) == 2);  // paint lower half ?

  int i, z, tmp;
  if(Above)  i = y2;  else  i = y2>>1;
  if(Below)  z = 0;   else  z = y2>>1;
  // y line
  Lines.append(new Line(x2>>1, i, x2>>1, z, GridPen));

  int len  = 0;       // arc length
  int beta = 16*180;  // start angle
  if(Above)  { beta = 0;  len = 16*180; }
  if(Below)  len += 16*180;

  int phi, tPos;
  int tHeight = QucsSettings.font.pointSize() + 5;
  if(!Below)  tPos = (y2>>1) + 3;
  else  tPos = (y2>>1) - tHeight + 3;

  int Prec;
  char form;
  double Expo, Base, numGrids, GridStep, zD;
  if(xAxis.GridOn) {
    calcPolarAxisScale(Axis, numGrids, GridStep, zD);

    if(fabs(log10(Axis->up)) < 3.0)  { form = 'g';  Prec = 3; }
    else  { form = 'e';  Prec = 0; }


    double zDstep = zD;
    double GridNum  = 0.0;
    for(i=int(numGrids); i>1; i--) {    // create all grid circles
      z = int(zD);
      GridNum += GridStep;
      Texts.append(new Text(((x2+z)>>1)-10, tPos,
			    StringNum(GridNum, form, Prec)));

      phi = int(16.0*180.0/M_PI*atan(double(2*tHeight)/zD));
      if(!Below)  tmp = beta + phi;
      else  tmp = beta;
      Arcs.append(new struct Arc((x2-z)>>1, (y2+z)>>1, z, z, tmp, len-phi,
			  GridPen));
      zD += zDstep;
    }
  }
  else {  // of  "if(GridOn)"
    Expo = floor(log10(Axis->max));
    Base = ceil(Axis->max/pow(10.0,Expo) - 0.01);
    Axis->up = Base * pow(10.0,Expo);  // separate Base * 10^Expo
  }

  // create outer circle
  if(fabs(log10(Axis->up)) < 3.0)
    Texts.append(new Text(x2-8, tPos, StringNum(Axis->up)));
  else
    Texts.append(new Text(x2-8, tPos, StringNum(Axis->up, 'e', 0)));
  phi = int(16.0*180.0/M_PI*atan(double(2*tHeight)/double(x2)));
  if(!Below)  tmp = phi;
  else  tmp = 0;
  Arcs.append(new struct Arc(0, y2, x2, y2, tmp, 16*360-phi, QPen(Qt::black,0)));

  QFontMetrics  metrics(QucsSettings.font);
  QSize r = metrics.size(0, Texts.current()->s);  // width of text
  len = x2+r.width()-4;   // more space at the right
  if(len > x3)  x3 = len;
}

// --------------------------------------------------------------
// Calculations for Cartesian diagrams (RectDiagram and Rect3DDiagram).
// parameters:   Axis - pointer to the axis to scale
//               Dist - length of axis in pixel on the screen
// return value: "true" if axis runs from largest to smallest value
//               (only used for logarithmical axes)
//
//               GridNum  - number where the first numbered grid is placed
//               GridStep - distance from one grid to the next
//               zD     - screen coordinate where the first grid is placed
//               zDstep - distance on screen from one grid to the next
bool Diagram::calcAxisScale(Axis *Axis, double& GridNum, double& zD,
				double& zDstep, double& GridStep, double Dist)
{
  bool back=false;
  if(fabs(Axis->max-Axis->min) < 1e-200) { // if max = min, double difference
    Axis->max += fabs(Axis->max);
    Axis->min -= fabs(Axis->min);
  }
  if(Axis->max == 0) if(Axis->min == 0) {
    Axis->max = 1;
    Axis->min = -1;
  }
  Axis->low = Axis->min; Axis->up = Axis->max;

  double numGrids, Base, Expo, corr;
if(Axis->autoScale) {
  numGrids = floor(Dist/60.0);   // minimal grid is 60 pixel
  if(numGrids < 1.0) Base = Axis->max-Axis->min;
  else Base = (Axis->max-Axis->min)/numGrids;
  Expo = floor(log10(Base));
  Base = Base/pow(10.0,Expo);        // separate first significant digit
  if(Base < 3.5) {     // use only 1, 2 and 5, which ever is best fitted
    if(Base < 1.5) Base = 1.0;
    else Base = 2.0;
  }
  else {
    if(Base < 7.5) Base = 5.0;
    else { Base = 1.0; Expo++; }
  }
  GridStep = Base * pow(10.0,Expo);   // grid distance in real coordinates
  corr = floor((Axis->max-Axis->min)/GridStep - numGrids);
  if(corr < 0.0) corr++;
  numGrids += corr;     // correct rounding faults


  // upper y boundery ...........................
  zD = fabs(fmod(Axis->max, GridStep));// expand grid to upper diagram edge ?
  GridNum = zD/GridStep;
  if((1.0-GridNum) < 1e-10) GridNum = 0.0;  // fix rounding errors
  if(Axis->max <= 0.0) {
    if(GridNum < 0.3) { Axis->up += zD;  zD = 0.0; }
  }
  else  if(GridNum > 0.7)  Axis->up += GridStep-zD;
        else if(GridNum < 0.1)
	       if(GridNum*Dist >= 1.0)// more than 1 pixel above ?
		 Axis->up += 0.3*GridStep;  // beauty correction


  // lower y boundery ...........................
  zD = fabs(fmod(Axis->min, GridStep));// expand grid to lower diagram edge ?
  GridNum = zD/GridStep;
  if((1.0-GridNum) < 1e-10) zD = GridNum = 0.0;  // fix rounding errors
  if(Axis->min <= 0.0) {
    if(GridNum > 0.7) { Axis->low -= GridStep-zD;  zD = 0.0; }
    else if(GridNum < 0.1)
	   if(GridNum*Dist >= 1.0) { // more than 1 pixel above ?
	     Axis->low -= 0.3*GridStep;   // beauty correction
	     zD += 0.3*GridStep;
	   }
  }
  else {
    if(GridNum > 0.3) {
      zD = GridStep-zD;
      if(GridNum > 0.9) {
	if((1.0-GridNum)*Dist >= 1.0) { // more than 1 pixel above ?
	  Axis->low -= 0.3*GridStep;    // beauty correction
	  zD += 0.3*GridStep;
	}
      }
    }
    else { Axis->low -= zD; zD = 0.0; }
  }

  GridNum = Axis->low + zD;
  zD /= (Axis->up-Axis->low)/Dist;
}
else {   // user defined limits
  zD = 0.0;
  Axis->low = GridNum = Axis->limit_min;
  Axis->up  = Axis->limit_max;
  if(Axis->limit_max < Axis->limit_min)
    back = true;
  GridStep  = Axis->step;
}

  zDstep = GridStep/(Axis->up-Axis->low)*Dist; // grid in pixel
  return back;
}

// --------------------------------------------------------------
// Calculations for Cartesian diagrams (RectDiagram and Rect3DDiagram).
bool Diagram::calcAxisLogScale(Axis *Axis, int& z, double& zD,
				   double& zDstep, double& corr, int len)
{
  if(fabs(Axis->max-Axis->min) < 1e-200) { // if max = min, double difference
    Axis->max += fabs(Axis->max);
    Axis->min -= fabs(Axis->min);
  }
  if(Axis->max == 0) if(Axis->min == 0) {
    Axis->max = 1;
    Axis->min = -1;
  }
  Axis->low = Axis->min; Axis->up = Axis->max;

  if(!Axis->autoScale) {
    Axis->low = Axis->limit_min;
    Axis->up  = Axis->limit_max;
  }


  bool mirror=false, mirror2=false;
  double tmp;
  if(Axis->up < 0.0) {   // for negative values
    tmp = Axis->low;
    Axis->low = -Axis->up;
    Axis->up  = -tmp;
    mirror = true;
  }

  double Base, Expo;
  if(Axis->autoScale) {
    if(mirror) {   // set back values ?
      tmp = Axis->min;
      Axis->min = -Axis->max;
      Axis->max = -tmp;
    }

    Expo = floor(log10(Axis->max));
    Base = Axis->max/pow(10.0,Expo);
    if(Base > 3.0001) Axis->up = pow(10.0,Expo+1.0);
    else  if(Base < 1.0001) Axis->up = pow(10.0,Expo);
	  else Axis->up = 3.0 * pow(10.0,Expo);

    Expo = floor(log10(Axis->min));
    Base = Axis->min/pow(10.0,Expo);
    if(Base < 2.999) Axis->low = pow(10.0,Expo);
    else  if(Base > 9.999) Axis->low = pow(10.0,Expo+1.0);
	  else Axis->low = 3.0 * pow(10.0,Expo);

    corr = double(len) / log10(Axis->up / Axis->low);

    z = 0;
    zD = Axis->low;
    zDstep = pow(10.0,Expo);

    if(mirror) {   // set back values ?
      tmp = Axis->min;
      Axis->min = -Axis->max;
      Axis->max = -tmp;
    }
  }
  else {   // user defined limits
    if(Axis->up < Axis->low) {
      tmp = Axis->low;
      Axis->low = Axis->up;
      Axis->up  = tmp;
      mirror2 = true;
    }

    Expo = floor(log10(Axis->low));
    Base = ceil(Axis->low/pow(10.0,Expo));
    zD = Base * pow(10.0, Expo);
    zDstep = pow(10.0,Expo);
    if(zD > 9.5*zDstep)  zDstep *= 10.0;

    corr = double(len) / log10(Axis->up / Axis->low);
    z = int(corr*log10(zD / Axis->low) + 0.5); // int(..) implies floor(..)

    if(mirror2) {   // set back values ?
      tmp = Axis->low;
      Axis->low = Axis->up;
      Axis->up  = tmp;
    }
  }

  if(mirror) {   // set back values ?
    tmp = Axis->low;
    Axis->low = -Axis->up;
    Axis->up  = -tmp;
  }

  if(mirror == mirror2)  return false;
  else  return true;
}

// --------------------------------------------------------------
bool Diagram::calcYAxis(Axis *Axis, int x0)
{
  int z;
  double GridStep, corr, zD, zDstep, GridNum;

  QSize r;
  QString tmp;
  QFontMetrics  metrics(QucsSettings.font);
  int maxWidth = 0;
  int Prec;
  char form;

  bool back = false;
if(Axis->log) {
  if(Axis->autoScale) {
    if(Axis->max*Axis->min <= 0.0)  return false;  // invalid
  }
  else  if(Axis->limit_min*Axis->limit_max <= 0.0)  return false;  // invalid

  back = calcAxisLogScale(Axis, z, zD, zDstep, corr, y2);

  if(back) z = y2;
  while((z <= y2) && (z >= 0)) {    // create all grid lines
    if(Axis->GridOn)  if(z < y2)  if(z > 0)
      Lines.prepend(new Line(0, z, x2, z, GridPen));  // y grid

    if((zD < 1.5*zDstep) || (z == 0)) {
      if(fabs(log10(zD)) < 3.0)  tmp = StringNum(zD);
      else  tmp = StringNum(zD, 'e', 1);
      if(Axis->up < 0.0)  tmp = '-'+tmp;

      r = metrics.size(0, tmp);  // width of text
      if(maxWidth < r.width()) maxWidth = r.width();
      if(x0 > 0)
        Texts.append(new Text(x0+7, z-6, tmp)); // text aligned left
      else
        Texts.append(new Text(-r.width()-7, z-6, tmp)); // text aligned right

      // y marks
      Lines.append(new Line(x0-5, z, x0+5, z, QPen(Qt::black,0)));
    }

    zD += zDstep;
    if(zD > 9.5*zDstep)  zDstep *= 10.0;
    if(back) {
      z = int(corr*log10(zD / fabs(Axis->up)) + 0.5); // int() implies floor()
      z = y2 - z;
    }
    else
      z = int(corr*log10(zD / fabs(Axis->low)) + 0.5);// int() implies floor()
  }
}
else {  // not logarithmical
  back = calcAxisScale(Axis, GridNum, zD, zDstep, GridStep, double(y2));

  double Expo;
  if(Axis->up == 0.0)  Expo = log10(fabs(Axis->up-Axis->low));
  else  Expo = log10(fabs(Axis->up));
  if(fabs(Expo) < 3.0)  { form = 'g';  Prec = 3; }
  else  { form = 'e';  Prec = 0; }

  zD += 0.5;     // perform rounding
  z = int(zD);   //  "int(...)" implies "floor(...)"
  while((z <= y2) && (z >= 0)) {  // create all grid lines
    if(fabs(GridNum) < 0.01*pow(10.0, Expo)) GridNum = 0.0;// make 0 really 0
    tmp = StringNum(GridNum, form, Prec);

    r = metrics.size(0, tmp);  // width of text
    if(maxWidth < r.width()) maxWidth = r.width();
    if(x0 > 0)
      Texts.append(new Text(x0+8, z-6, tmp));  // text aligned left
    else
      Texts.append(new Text(-r.width()-7, z-6, tmp));  // text aligned right
    GridNum += GridStep;

    if(Axis->GridOn)  if(z < y2)  if(z > 0)
      Lines.prepend(new Line(0, z, x2, z, GridPen));  // y grid
    Lines.append(new Line(x0-5, z, x0+5, z, QPen(Qt::black,0))); // y marks
    zD += zDstep;
    z = int(zD);
  }
} // of "if(ylog) ... else ..."
  if(x0 == 0)  x1 = maxWidth+14;
  else  x3 = x2+maxWidth+14;
  return true;
}
