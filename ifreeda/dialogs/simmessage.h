/***************************************************************************
                          simmessage.h  -  description
                             -------------------
    begin                : Sat Sep 6 2003
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

#ifndef SIMMESSAGE_H
#define SIMMESSAGE_H

#include <qdialog.h>
#include <q3process.h>
#include <qstringlist.h>
#include <qfile.h>
//Added by qt3to4:
#include <Q3TextStream>
#include <Q3VBoxLayout>

class Q3TextEdit;
class Q3VBoxLayout;
class QPushButton;
class Q3ProgressBar;
class QucsDoc;

/**
  *@author Michael Margraf
  */

class SimMessage : public QDialog  {
   Q_OBJECT
public:
  SimMessage(QucsDoc*, QWidget *parent=0);
  ~SimMessage();

  bool startProcess();

signals:
  void SimulationEnded(int, SimMessage*);
  void displayDataPage(QString);

public slots:
  void slotClose();

private slots:
  void slotDisplayMsg();
  void slotDisplayErr();
  void slotSimEnded();
  void slotDisplayButton();

  void slotReadSpiceNetlist();
  void slotFinishSpiceNetlist();

private:
  void FinishSimulation(int);
  void nextSPICE();
  void startSimulator();

public:
  QucsDoc *Doc;

  Q3Process      SimProcess;
  Q3TextEdit    *ProgText, *ErrText;
  bool          wasLF;   // linefeed for "ProgText"
  QPushButton  *Display, *Abort;
  Q3ProgressBar *SimProgress;
  QString       ProgressText;

  bool          makeSubcircuit, insertSim;
  QStringList   Collect;
  QFile         NetlistFile;
  Q3TextStream   Stream;

  Q3VBoxLayout  *all;
};

#endif
