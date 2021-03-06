/***************************************************************************
                          simmessage.cpp  -  description
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
#include <qabstractitemmodel.h>
#include <qlocale.h>
#include <qvariant.h>
// Above line added by Shivam to remove header file errors
#include <q3deepcopy.h>
#include <qlabel.h>
#include <qlayout.h>
#include <q3vgroupbox.h>
#include <q3hgroupbox.h>
#include <q3hbox.h>
#include <qtimer.h>
#include <qpushbutton.h>
#include <q3progressbar.h>
#include <q3textedit.h>
#include <qdatetime.h>
#include <qregexp.h>
//Added by qt3to4:
#include <Q3TextStream>
#include <Q3VBoxLayout>

#include "simmessage.h"
#include "guimain.h"
#include "gui.h"
#include "guidoc.h"


SimMessage::SimMessage(QucsDoc *Doc_, QWidget *parent)
		: QDialog(parent, 0, FALSE, Qt::WDestructiveClose)
{
  Doc = Doc_;
  setCaption(tr("fREEDA Simulation Messages"));

  all = new Q3VBoxLayout(this);
  all->setSpacing(5);
  all->setMargin(5);
  Q3VGroupBox *Group1 = new Q3VGroupBox(tr("Progress:"),this);
  all->addWidget(Group1);

  ProgText = new Q3TextEdit(Group1);
  ProgText->setTextFormat(Qt::PlainText);
  ProgText->setReadOnly(true);
  ProgText->setWordWrap(Q3TextEdit::NoWrap);
  ProgText->setMinimumSize(400,80);
  wasLF = false;

  Q3HGroupBox *HGroup = new Q3HGroupBox(this);
  HGroup->setInsideMargin(5);
  HGroup->setInsideSpacing(5);
  all->addWidget(HGroup);
  new QLabel(tr("Progress:"), HGroup);
  SimProgress = new Q3ProgressBar(HGroup);

  Q3VGroupBox *Group2 = new Q3VGroupBox(tr("Errors and Warnings:"),this);
  all->addWidget(Group2);

  ErrText = new Q3TextEdit(Group2);
  ErrText->setTextFormat(Qt::PlainText);
  ErrText->setReadOnly(true);
  ErrText->setWordWrap(Q3TextEdit::NoWrap);
  ErrText->setMinimumSize(400,80);

  Q3HBox *Butts = new Q3HBox(this);
  all->addWidget(Butts);

  Display = new QPushButton(tr("Goto display page"), Butts);
  Display->setDisabled(true);
  connect(Display,SIGNAL(clicked()),SLOT(slotDisplayButton()));

  Abort = new QPushButton(tr("Abort simulation"), Butts);
  connect(Abort,SIGNAL(clicked()),SLOT(reject()));
}

SimMessage::~SimMessage()
{
  if(SimProcess.isRunning())  SimProcess.kill();
  delete all;
}

// ------------------------------------------------------------------------
bool SimMessage::startProcess()
{
  Abort->setText(tr("Abort simulation"));
  Display->setDisabled(true);

  if(Doc->GenNetlist)
  {
     ProgText->insert(tr("Creating netlist on ")+
                   QDate::currentDate().toString("ddd dd. MMM yyyy"));
     ProgText->insert(tr(" at ")+
                   QTime::currentTime().toString("hh:mm:ss")+"\n\n");
  }
  else
  {
    ProgText->insert(tr("Starting new simulation on ")+
                   QDate::currentDate().toString("ddd dd. MMM yyyy"));
    ProgText->insert(tr(" at ")+
                   QTime::currentTime().toString("hh:mm:ss")+"\n\n");
  }
  SimProcess.blockSignals(false);
  if(SimProcess.isRunning()) {
    ErrText->insert(tr("ERROR: Simulator is still running!"));
    FinishSimulation(-1);
    return false;
  }

  ProgText->insert(tr("creating netlist ...."));
  QString netlist = Doc->DocName;
  int index = 0;
  index = netlist.find(".sch"); //find .sch so .net can be added
  netlist.truncate(index);  //now remove .sch
  //the schematic name + .net is the netlist sent to freeda
  NetlistFile.setName(QucsWorkDir.filePath(netlist+".net"));
  if(!NetlistFile.open(QIODevice::WriteOnly)) {
    ErrText->insert(tr("ERROR: Cannot write netlist file!"));
    FinishSimulation(-1);
    return false;
  }

  Collect.clear();  // clear list for NodeSets, SPICE components etc.
  Stream.setDevice(&NetlistFile);
  if(!Doc->File.prepareNetlist(Stream, Collect, ErrText)) {
    NetlistFile.close();
    FinishSimulation(-1);
    return false;
  }
  Collect.append("*");   // mark the end


  disconnect(&SimProcess, 0, 0, 0);
  connect(&SimProcess, SIGNAL(readyReadStderr()), SLOT(slotDisplayErr()));
  connect(&SimProcess, SIGNAL(readyReadStdout()),
                       SLOT(slotReadSpiceNetlist()));
  connect(&SimProcess, SIGNAL(processExited()),
                       SLOT(slotFinishSpiceNetlist()));
  nextSPICE();
  return true;
}
  
// ---------------------------------------------------
// Converts a spice netlist into Qucs format and outputs it.
void SimMessage::nextSPICE()
{
  QString Line;
  for(;;) {  // search for next SPICE component
    Line = *(Collect.begin());
    Collect.remove(Collect.begin());
    if(Line == "*") {  // worked on all components ?
      startSimulator();
      return;
    }
    if(Line.left(5) == "SPICE") {
      if(Line.at(5) != 'o') insertSim = true;
      else insertSim = false;
      break;
    }
    Collect.append(Line);
  }


  QString FileName = Line.section('"', 1,1);
  Line = Line.section('"', 2);  // port nodes
  if(Line.isEmpty())  makeSubcircuit = false;
  else  makeSubcircuit = true;

  QStringList com;
  com << (QucsSettings.BinDir + "qucsconv");
  if(makeSubcircuit)
    com << "-g" << "_ref";
  com << "-if" << "spice" << "-of" << "qucs" << "-i" << FileName;
  SimProcess.setArguments(com);


  if(makeSubcircuit) {
    if(FileName.at(0) <= '9') if(FileName.at(0) >= '0')
      FileName = '_' + FileName;
    FileName.replace(QRegExp("\\W"), "_"); // none [a-zA-Z0-9] into "_"
    Stream << "\n.Def:" << FileName << " ";
  
    Line.replace(',', ' ');
    Stream << Line;
    if(!Line.isEmpty()) Stream << " _ref";
  }
  Stream << "\n";


  ProgressText = "";
  if(!SimProcess.start()) {
    ErrText->insert(tr("ERROR: Cannot start QucsConv!"));
    FinishSimulation(-1);
    return;
  }
  SimProcess.closeStdin();
}

// ------------------------------------------------------------------------
void SimMessage::slotReadSpiceNetlist()
{
  int i;
  QString s;
  ProgressText += QString(SimProcess.readStdout());

  while((i = ProgressText.find('\n')) >= 0) {

    s = ProgressText.left(i);
    ProgressText.remove(0, i+1);


    s = s.stripWhiteSpace();
    if(s.isEmpty()) continue;
    if(s.at(0) == '#') continue;
    if(s.at(0) == '.') if(s.left(5) != ".Def:") { // insert simulations later
      if(insertSim) Collect.append(s);
      continue;
    }
    Stream << "  " << s << '\n';
  }
}


// ------------------------------------------------------------------------
void SimMessage::slotFinishSpiceNetlist()
{
  if(makeSubcircuit)
    Stream << ".Def:End\n\n";

  nextSPICE();
}

// ------------------------------------------------------------------------
void SimMessage::startSimulator()
{
  // output NodeSets, SPICE simulations etc.
  Stream << Collect.join("\n") << '\n';
  Doc->File.createNetlist(Stream);
  NetlistFile.close();
  ProgText->insert(tr("done.\n"));

  QStringList com;
  

  /*com << QucsSettings.BinDir + "qucsator" << "-b" << "-i"
      << QucsWorkDir.filePath("netlist.net")
      << "-o" << QucsWorkDir.filePath(Doc->DataSet);*/     
       
  //If we are only generating a netlist dont call the simulator.
  if(!Doc->GenNetlist)
  {
     com << "./freeda" << NetlistFile.name(); 
      //<< QucsWorkDir.filePath(NetlistFile.name());
      //<< "-o" << QucsWorkDir.filePath(Doc->DataSet);
  }
  SimProcess.setArguments(com);


  disconnect(&SimProcess, 0, 0, 0);
  connect(&SimProcess, SIGNAL(readyReadStderr()), SLOT(slotDisplayErr()));
  connect(&SimProcess, SIGNAL(readyReadStdout()), SLOT(slotDisplayMsg()));
  connect(&SimProcess, SIGNAL(processExited()), SLOT(slotSimEnded()));
  ProgressText = "";
  if(!SimProcess.start()) {
    ErrText->insert(tr("ERROR: Cannot start simulator!"));
    FinishSimulation(-1);
    return;
  }
}

// ------------------------------------------------------------------------
// Is called when the process sends an output to stdout.
void SimMessage::slotDisplayMsg()
{
  int i, Para;
  QString s = QString(SimProcess.readStdout());
//qDebug(s);
  while((i = s.find('\r')) >= 0) {
    if (s.find('\n') == i + 1) break; // necessary on Win32 platforms
    if(wasLF)
      ProgressText += s.left(i-1);
    else {
      int k = s.findRev('\n',i-s.length());
      if (k > 0) {
	ProgText->insert(s.left(k));
	s = s.mid(k+1);
	i = s.find('\r');
      }
      Para = ProgText->paragraphs()-1;
      ProgressText = ProgText->text(Para) + s.left(i-1);
      ProgText->removeParagraph(Para);  // remove last text line
    }
    s = s.mid(i+1);
    Para = ProgressText.length()-11;
    Para = 10*int(ProgressText.at(Para).latin1()-'0') +
	      int(ProgressText.at(Para+1).latin1()-'0');
    if(Para < 0)  Para += 160;
    SimProgress->setProgress(Para, 100);
    ProgressText = "";
    wasLF = true;
  }
  if(s.length() < 1)  return;

  if(wasLF) {
    if(s.find('\n') >= 0) {
      ProgText->insert("\n"+s);
      wasLF = false;
    }
  }
  else  ProgText->insert(s);
}

// ------------------------------------------------------------------------
// Is called when the process sends an output to stderr.
void SimMessage::slotDisplayErr()
{
  ErrText->append(QString(SimProcess.readStderr()));
}

// ------------------------------------------------------------------------
// Is called when the simulation process terminates.
void SimMessage::slotSimEnded()
{
  int stat = (!SimProcess.normalExit()) ? -1 : SimProcess.exitStatus();
  FinishSimulation(stat);
}

// ------------------------------------------------------------------------
// Is called when the simulation ended with errors before starting simulator
// process.
void SimMessage::FinishSimulation(int Status)
{
  Abort->setText(tr("Close window"));
  Display->setDisabled(false);
  SimProgress->setProgress(100, 100);   // progress bar to 100%

  QDate d = QDate::currentDate();   // get date of today
  QTime t = QTime::currentTime();   // get time

  if(Doc->GenNetlist)
  {
    Status = 0;
  }
  if(Status == 0) {
    if(Doc->GenNetlist){
       ProgText->insert(tr("\nCreating netlist ended on ")+
                     d.toString("ddd dd. MMM yyyy"));
       ProgText->insert(tr(" at ")+t.toString("hh:mm:ss")+"\n");
       ProgText->insert(tr("Ready.\n"));
    }
    else
    {
       ProgText->insert(tr("\nSimulation ended on ")+
                     d.toString("ddd dd. MMM yyyy"));
       ProgText->insert(tr(" at ")+t.toString("hh:mm:ss")+"\n");
       ProgText->insert(tr("Ready.\n"));    
    }
  }
  else {
    ProgText->insert(tr("\nErrors occured during simulation on ")+
                     d.toString("ddd dd. MMM yyyy"));
    ProgText->insert(tr(" at ")+t.toString("hh:mm:ss")+"\n");
    ProgText->insert(tr("Aborted.\n"));
  }

  QFile file(QucsWorkDir.filePath("log.txt"));  // save simulator messages
  if(file.open(QIODevice::WriteOnly)) {
    int z;
    Q3TextStream stream(&file);
    stream << tr("Output:\n----------\n\n");
    for(z=0; z<=ProgText->paragraphs(); z++)
      stream << ProgText->text(z) << "\n";
    stream << tr("\n\n\nErrors:\n--------\n\n");
    for(z=0; z<ErrText->paragraphs(); z++)
      stream << ErrText->text(z) << "\n";
    file.close();
  }

  emit SimulationEnded(Status, this);
}

// ------------------------------------------------------------------------
// To call accept(), which is protected, from the outside.
void SimMessage::slotClose()
{
  accept();
}

// ------------------------------------------------------------------------
void SimMessage::slotDisplayButton()
{
  emit displayDataPage(Doc->DataDisplay);
  accept();
}
