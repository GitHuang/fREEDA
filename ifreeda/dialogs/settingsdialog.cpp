/***************************************************************************
                          settingsdialog.cpp  -  description
                             -------------------
    begin                : Mon Oct 20 2003
    copyright            : (C) 2003, 2004 by Michael Margraf
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

#include <qwidget.h>
#include <qlabel.h>
#include <q3hbox.h>
#include <qpushbutton.h>
#include <qtabwidget.h>
#include <qlayout.h>
#include <qvalidator.h>
#include <qregexp.h>
#include <qlineedit.h>
#include <qcheckbox.h>
//Added by qt3to4:
#include <Q3GridLayout>
#include <Q3VBoxLayout>

#include "settingsdialog.h"

#include "gui.h"
#include "guidoc.h"
#include "guiview.h"



SettingsDialog::SettingsDialog(QucsDoc *d, QWidget *parent, const char *name)
                         : QDialog(parent, name, TRUE, Qt::WDestructiveClose)
{
  Doc = d;
  setCaption(tr("Edit File Properties"));

  all = new Q3VBoxLayout(this); // to provide the neccessary size
  QTabWidget *t = new QTabWidget(this);
  all->addWidget(t);

  // ...........................................................
  QWidget *Tab1 = new QWidget(t);
  Q3GridLayout *gp = new Q3GridLayout(Tab1,4,2,5,5);

  QLabel *l1 = new QLabel(tr("Data Display:"), Tab1);
  gp->addWidget(l1,0,0);
  Input_DataDisplay = new QLineEdit(Tab1);
  gp->addWidget(Input_DataDisplay,0,1);

  QLabel *l2 = new QLabel(tr("Data Set:"), Tab1);
  gp->addWidget(l2,1,0);
  Input_DataSet = new QLineEdit(Tab1);
  gp->addWidget(Input_DataSet,1,1);

  Check_OpenDpl = new QCheckBox(tr("open data display after simulation"),
				Tab1);
  gp->addMultiCellWidget(Check_OpenDpl,2,2,0,1);

  t->addTab(Tab1, tr("Simulation"));

  // ...........................................................
  QWidget *Tab2 = new QWidget(t);
  Q3GridLayout *gp2 = new Q3GridLayout(Tab2,4,2,5,5);
  Check_GridOn = new QCheckBox(tr("show Grid"),Tab2);
  gp2->addMultiCellWidget(Check_GridOn,0,0,0,1);

  valExpr = new QRegExpValidator(QRegExp("[1-9]\\d{0,2}"), this);

  QLabel *l3 = new QLabel(tr("horizontal Grid:"), Tab2);
  gp2->addWidget(l3,1,0);
  Input_GridX = new QLineEdit(Tab2);
  Input_GridX->setValidator(valExpr);
//  Input_GridX->setInputMask("000");   // for Qt 3.2
  gp2->addWidget(Input_GridX,1,1);

  QLabel *l4 = new QLabel(tr("vertical Grid:"), Tab2);
  gp2->addWidget(l4,2,0);
  Input_GridY = new QLineEdit(Tab2);
  Input_GridY->setValidator(valExpr);
//  Input_GridY->setInputMask("000");   // for Qt 3.2
  gp2->addWidget(Input_GridY,2,1);

  t->addTab(Tab2, tr("Grid"));

  // ...........................................................
  // buttons on the bottom of the dialog (independent of the TabWidget)
  Q3HBox *Butts = new Q3HBox(this);
  Butts->setSpacing(5);
  Butts->setMargin(5);
  all->addWidget(Butts);

  QPushButton *OkButt = new QPushButton(tr("OK"), Butts);
  connect(OkButt, SIGNAL(clicked()), SLOT(slotOK()));
  QPushButton *ApplyButt = new QPushButton(tr("Apply"), Butts);
  connect(ApplyButt, SIGNAL(clicked()), SLOT(slotApply()));
  QPushButton *CancelButt = new QPushButton(tr("Cancel"), Butts);
  connect(CancelButt, SIGNAL(clicked()), SLOT(reject()));

  OkButt->setDefault(true);

  // ...........................................................
  // fill the fields with the QucsDoc-Properties

  Input_DataSet->setText(Doc->DataSet);
  Input_DataDisplay->setText(Doc->DataDisplay);
  Check_OpenDpl->setChecked(Doc->SimOpenDpl);
  Check_GridOn->setChecked(Doc->GridOn);
  Input_GridX->setText(QString::number(Doc->GridX));
  Input_GridY->setText(QString::number(Doc->GridY));
}

SettingsDialog::~SettingsDialog()
{
  delete all;
  delete valExpr;
}

// -----------------------------------------------------------
void SettingsDialog::slotOK()
{
  slotApply();
  accept();
}

// -----------------------------------------------------------
void SettingsDialog::slotApply()
{
  bool changed = false;
  
  if(Doc->DataSet != Input_DataSet->text()) {
    Doc->DataSet = Input_DataSet->text();
    changed = true;
  }

  if(Doc->DataDisplay != Input_DataDisplay->text()) {
    Doc->DataDisplay = Input_DataDisplay->text();
    changed = true;
  }

  if(Doc->SimOpenDpl != Check_OpenDpl->isChecked()) {
    Doc->SimOpenDpl = Check_OpenDpl->isChecked();
    changed = true;
  }

  if(Doc->GridOn != Check_GridOn->isChecked()) {
    Doc->GridOn = Check_GridOn->isChecked();
    changed = true;
  }
   
  //if(Doc->GridX != Input_GridX->text()) {
    //Changed by Shivam     text() function of QLineedit class returns QString type where as GridX
    //parameter is of interger type. So before comparison it is necessary to
    //convert in integer.
   if(Doc->GridX != Input_GridX->text().toInt()) {
    Doc->GridX = Input_GridX->text().toInt();
    changed = true;
  }

  //if(Doc->GridY != Input_GridY->text()) {
  // Changed by Shivam
   if(Doc->GridY != Input_GridY->text().toInt()) {
    Doc->GridY = Input_GridY->text().toInt();
    changed = true;
  }

  if(changed) {
    Doc->setChanged(true);
    ((QucsApp*)parent())->view->viewport()->repaint();
  }
}
