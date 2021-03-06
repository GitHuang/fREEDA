/***************************************************************************
                          guiactions.cpp  -  description
 ***************************************************************************/
#include <q3process.h>
#include <qmessagebox.h>

#include "guiactions.h"

#include "gui.h"
#include "guiview.h"
#include "components/ground.h"
//#include "components/subcirport.h" QUCS-EDIT REMOVE SUBCIRCUIT
#include "components/equation.h"
#include "components/plot.h"
#include "components/dot_model.h"
#include "components/dot_top.h"
#include "components/dot_bottom.h"

#include <climits>


QucsActions::QucsActions()
{
}

QucsActions::~QucsActions()
{
}

// -----------------------------------------------------------------------
void QucsActions::init(QucsApp *p_)
{
  App  = p_;
  view = App->view;
}

// -----------------------------------------------------------------------
// This function is called from all toggle actions.
bool QucsActions::performToggleAction(bool on, QAction *Action,
	pToggleFunc Function, pMouseFunc MouseMove, pMouseFunc MousePress)
{
  view->editText->setHidden(true); // disable text edit of component property

  if(!on) {
    view->MouseMoveAction = &QucsView::MouseDoNothing;
    view->MousePressAction = &QucsView::MouseDoNothing;
    view->MouseReleaseAction = &QucsView::MouseDoNothing;
    view->MouseDoubleClickAction = &QucsView::MouseDoNothing;
    App->activeAction = 0;   // no action active
    return false;
  }

  do {
    if(Function) if((view->Docs.current()->*Function)()) {
      Action->blockSignals(true);
      Action->setOn(false);  // release toolbar button
      Action->blockSignals(false);
      break;
    }

    if(App->activeAction) {
      App->activeAction->blockSignals(true); // do not call toggle slot
      App->activeAction->setOn(false);       // set last toolbar button off
      App->activeAction->blockSignals(false);
    }
    App->activeAction = Action;

    view->MouseMoveAction = MouseMove;
    view->MousePressAction = MousePress;
    view->MouseReleaseAction = &QucsView::MouseDoNothing;
    view->MouseDoubleClickAction = &QucsView::MouseDoNothing;

  } while(false);   // to perform "break"

  view->viewport()->repaint();
  view->drawn = false;
  return true;
}

// -----------------------------------------------------------------------
// Is called, when "set on grid" action is activated.
void QucsActions::slotOnGrid(bool on)
{
  performToggleAction(on, onGrid, &QucsDoc::elementsOnGrid,
		&QucsView::MMoveOnGrid, &QucsView::MPressOnGrid);
}

// -----------------------------------------------------------------------
// Is called when the rotate toolbar button is pressed.
void QucsActions::slotEditRotate(bool on)
{
  performToggleAction(on, editRotate, &QucsDoc::rotateElements,
		&QucsView::MMoveRotate, &QucsView::MPressRotate);
}

// -----------------------------------------------------------------------
// Is called when the mirror toolbar button is pressed.
void QucsActions::slotEditMirrorX(bool on)
{
  performToggleAction(on, editMirror, &QucsDoc::mirrorXComponents,
		&QucsView::MMoveMirrorX, &QucsView::MPressMirrorX);
}

// -----------------------------------------------------------------------
// Is called when the mirror toolbar button is pressed.
void QucsActions::slotEditMirrorY(bool on)
{
  performToggleAction(on, editMirrorY, &QucsDoc::mirrorYComponents,
		&QucsView::MMoveMirrorY, &QucsView::MPressMirrorY);
}

// -----------------------------------------------------------------------
// Is called when the activate/deactivate toolbar button is pressed.
void QucsActions::slotEditActivate(bool on)
{
  performToggleAction(on, editActivate, &QucsDoc::activateComponents,
		&QucsView::MMoveActivate, &QucsView::MPressActivate);
}

// ------------------------------------------------------------------------
// Is called if "Delete"-Button is pressed.
void QucsActions::slotEditDelete(bool on)
{
  performToggleAction(on, editDelete, &QucsDoc::deleteElements,
		&QucsView::MMoveDelete, &QucsView::MPressDelete);
}

// -----------------------------------------------------------------------
// Is called if "Wire"-Button is pressed.
void QucsActions::slotSetWire(bool on)
{
  performToggleAction(on, insWire, 0,
		&QucsView::MMoveWire1, &QucsView::MPressWire1);
}

// -----------------------------------------------------------------------
void QucsActions::slotInsertLabel(bool on)
{
  performToggleAction(on, insLabel, 0,
		&QucsView::MMoveLabel, &QucsView::MPressLabel);
}

// -----------------------------------------------------------------------
void QucsActions::slotSetMarker(bool on)
{
  performToggleAction(on, setMarker, 0,
		&QucsView::MMoveMarker, &QucsView::MPressMarker);
}

// -----------------------------------------------------------------------
// Is called, when "move component text" action is activated.
void QucsActions::slotMoveText(bool on)
{
  performToggleAction(on, moveText, 0,
		&QucsView::MMoveMoveTextB, &QucsView::MPressMoveText);
}

// -----------------------------------------------------------------------
// Is called, when "Zoom in" action is activated.
void QucsActions::slotZoomIn(bool on)
{
  performToggleAction(on, magPlus, 0,
		&QucsView::MMoveZoomIn, &QucsView::MPressZoomIn);
}

// -----------------------------------------------------------------------
// Is called when the select toolbar button is pressed.
void QucsActions::slotSelect(bool on)
{
  if(performToggleAction(on, select, 0,
		&QucsView::MouseDoNothing, &QucsView::MPressSelect)) {
    view->MouseReleaseAction = &QucsView::MReleaseSelect;
    view->MouseDoubleClickAction = &QucsView::MDoubleClickSelect;
  }
}

// -----------------------------------------------------------------------
void QucsActions::slotEditPaste(bool on)
{
  view->editText->setHidden(true); // disable text edit of component property

  if(!on) {
    view->MouseMoveAction = &QucsView::MouseDoNothing;
    view->MousePressAction = &QucsView::MouseDoNothing;
    view->MouseReleaseAction = &QucsView::MouseDoNothing;
    view->MouseDoubleClickAction = &QucsView::MouseDoNothing;
    App->activeAction = 0;   // no action active
    if(view->drawn) view->viewport()->repaint();
    return;
  }

  if(!view->pasteElements()) {
    editPaste->blockSignals(true); // do not call toggle slot
    editPaste->setOn(false);       // set toolbar button off
    editPaste->blockSignals(false);
    return;   // if clipboard empty
  }

  if(App->activeAction) {
    App->activeAction->blockSignals(true); // do not call toggle slot
    App->activeAction->setOn(false);       // set last toolbar button off
    App->activeAction->blockSignals(false);
  }
  App->activeAction = editPaste;

  view->drawn = false;
  view->MouseMoveAction = &QucsView::MMovePaste;
  view->MousePressAction = &QucsView::MouseDoNothing;
  view->MouseReleaseAction = &QucsView::MouseDoNothing;
  view->MouseDoubleClickAction = &QucsView::MouseDoNothing;
}

// -----------------------------------------------------------------------
// Is called when the mouse is clicked upon the equation toolbar button.
void QucsActions::slotInsertEquation(bool on)
{
  view->editText->setHidden(true); // disable text edit of component property

  if(!on) {
    view->MouseMoveAction = &QucsView::MouseDoNothing;
    view->MousePressAction = &QucsView::MouseDoNothing;
    view->MouseReleaseAction = &QucsView::MouseDoNothing;
    view->MouseDoubleClickAction = &QucsView::MouseDoNothing;
    App->activeAction = 0;   // no action active
    return;
  }
  if(App->activeAction) {
    App->activeAction->blockSignals(true); // do not call toggle slot
    App->activeAction->setOn(false);       // set last toolbar button off
    App->activeAction->blockSignals(false);
  }
  App->activeAction = insEquation;

  if(view->selComp)
    delete view->selComp;  // delete previously selected component

  view->selComp = new Equation();

  if(view->drawn) view->viewport()->repaint();
  view->drawn = false;
  view->MouseMoveAction = &QucsView::MMoveComponent;
  view->MousePressAction = &QucsView::MPressComponent;
  view->MouseReleaseAction = &QucsView::MouseDoNothing;
  view->MouseDoubleClickAction = &QucsView::MouseDoNothing;
}

// -----------------------------------------------------------------------
// Is called when the mouse is clicked upon the ground toolbar button.
void QucsActions::slotInsertGround(bool on)
{
  view->editText->setHidden(true); // disable text edit of component property

  if(!on) {
    view->MouseMoveAction = &QucsView::MouseDoNothing;
    view->MousePressAction = &QucsView::MouseDoNothing;
    view->MouseReleaseAction = &QucsView::MouseDoNothing;
    view->MouseDoubleClickAction = &QucsView::MouseDoNothing;
    App->activeAction = 0;   // no action active
    return;
  }
  if(App->activeAction) {
    App->activeAction->blockSignals(true); // do not call toggle slot
    App->activeAction->setOn(false);       // set last toolbar button off
    App->activeAction->blockSignals(false);
  }
  App->activeAction = insGround;

  if(view->selComp)
    delete view->selComp;  // delete previously selected component

  view->selComp = new Ground();

  if(view->drawn) view->viewport()->repaint();
  view->drawn = false;
  view->MouseMoveAction = &QucsView::MMoveComponent;
  view->MousePressAction = &QucsView::MPressComponent;
  view->MouseReleaseAction = &QucsView::MouseDoNothing;
  view->MouseDoubleClickAction = &QucsView::MouseDoNothing;
}

// -----------------------------------------------------------------------
// Is called when the mouse is clicked upon the port toolbar button.
void QucsActions::slotInsertPort(bool on)
{
  view->editText->setHidden(true); // disable text edit of component property

  if(!on) {
    view->MouseMoveAction = &QucsView::MouseDoNothing;
    view->MousePressAction = &QucsView::MouseDoNothing;
    view->MouseReleaseAction = &QucsView::MouseDoNothing;
    view->MouseDoubleClickAction = &QucsView::MouseDoNothing;
    App->activeAction = 0;   // no action active
    return;
  }
  if(App->activeAction) {
    App->activeAction->blockSignals(true); // do not call toggle slot
    App->activeAction->setOn(false);       // set last toolbar button off
    App->activeAction->blockSignals(false);
  }
  App->activeAction = insPort;

  if(view->selComp)
    delete view->selComp;  // delete previously selected component

  //view->selComp = new SubCirPort(); QUCS-EDIT REMOVE SUBCIRCUIT

  if(view->drawn) view->viewport()->repaint();
  view->drawn = false;
  view->MouseMoveAction = &QucsView::MMoveComponent;
  view->MousePressAction = &QucsView::MPressComponent;
  view->MouseReleaseAction = &QucsView::MouseDoNothing;
  view->MouseDoubleClickAction = &QucsView::MouseDoNothing;
}

// -----------------------------------------------------------------------
// Is called when the mouse is clicked upon the port toolbar button.
void QucsActions::slotInsertPlot(bool on)
{
  view->editText->setHidden(true); // disable text edit of component property

  if(!on) {
    view->MouseMoveAction = &QucsView::MouseDoNothing;
    view->MousePressAction = &QucsView::MouseDoNothing;
    view->MouseReleaseAction = &QucsView::MouseDoNothing;
    view->MouseDoubleClickAction = &QucsView::MouseDoNothing;
    App->activeAction = 0;   // no action active
    return;
  }
  if(App->activeAction) {
    App->activeAction->blockSignals(true); // do not call toggle slot
    App->activeAction->setOn(false);       // set last toolbar button off
    App->activeAction->blockSignals(false);
  }
  App->activeAction = insPlot;

  if(view->selComp)
    delete view->selComp;  // delete previously selected component

  view->selComp = new Plot();

  if(view->drawn) view->viewport()->repaint();
  view->drawn = false;
  view->MouseMoveAction = &QucsView::MMoveComponent;
  view->MousePressAction = &QucsView::MPressComponent;
  view->MouseReleaseAction = &QucsView::MouseDoNothing;
  view->MouseDoubleClickAction = &QucsView::MouseDoNothing;
}

// -----------------------------------------------------------------------
// Is called when the mouse is clicked upon the model toolbar button.
void QucsActions::slotInsertDot_Model(bool on)
{
  view->editText->setHidden(true); // disable text edit of component property

  if(!on) {
    view->MouseMoveAction = &QucsView::MouseDoNothing;
    view->MousePressAction = &QucsView::MouseDoNothing;
    view->MouseReleaseAction = &QucsView::MouseDoNothing;
    view->MouseDoubleClickAction = &QucsView::MouseDoNothing;
    App->activeAction = 0;   // no action active
    return;
  }
  if(App->activeAction) {
    App->activeAction->blockSignals(true); // do not call toggle slot
    App->activeAction->setOn(false);       // set last toolbar button off
    App->activeAction->blockSignals(false);
  }
  App->activeAction = insDot_Model;

  if(view->selComp)
    delete view->selComp;  // delete previously selected component

  view->selComp = new Dot_Model();

  if(view->drawn) view->viewport()->repaint();
  view->drawn = false;
  view->MouseMoveAction = &QucsView::MMoveComponent;
  view->MousePressAction = &QucsView::MPressComponent;
  view->MouseReleaseAction = &QucsView::MouseDoNothing;
  view->MouseDoubleClickAction = &QucsView::MouseDoNothing;
}

// -----------------------------------------------------------------------
// Is called when the mouse is clicked upon the top toolbar button.
void QucsActions::slotInsertDot_Top(bool on)
{
  view->editText->setHidden(true); // disable text edit of component property

  if(!on) {
    view->MouseMoveAction = &QucsView::MouseDoNothing;
    view->MousePressAction = &QucsView::MouseDoNothing;
    view->MouseReleaseAction = &QucsView::MouseDoNothing;
    view->MouseDoubleClickAction = &QucsView::MouseDoNothing;
    App->activeAction = 0;   // no action active
    return;
  }
  if(App->activeAction) {
    App->activeAction->blockSignals(true); // do not call toggle slot
    App->activeAction->setOn(false);       // set last toolbar button off
    App->activeAction->blockSignals(false);
  }
  App->activeAction = insDot_Top;

  if(view->selComp)
    delete view->selComp;  // delete previously selected component

  view->selComp = new Dot_Top();

  if(view->drawn) view->viewport()->repaint();
  view->drawn = false;
  view->MouseMoveAction = &QucsView::MMoveComponent;
  view->MousePressAction = &QucsView::MPressComponent;
  view->MouseReleaseAction = &QucsView::MouseDoNothing;
  view->MouseDoubleClickAction = &QucsView::MouseDoNothing;
}

// -----------------------------------------------------------------------
// Is called when the mouse is clicked upon the Bottom toolbar button.
void QucsActions::slotInsertDot_Bottom(bool on)
{
  view->editText->setHidden(true); // disable text edit of component property

  if(!on) {
    view->MouseMoveAction = &QucsView::MouseDoNothing;
    view->MousePressAction = &QucsView::MouseDoNothing;
    view->MouseReleaseAction = &QucsView::MouseDoNothing;
    view->MouseDoubleClickAction = &QucsView::MouseDoNothing;
    App->activeAction = 0;   // no action active
    return;
  }
  if(App->activeAction) {
    App->activeAction->blockSignals(true); // do not call toggle slot
    App->activeAction->setOn(false);       // set last toolbar button off
    App->activeAction->blockSignals(false);
  }
  App->activeAction = insDot_Bottom;

  if(view->selComp)
    delete view->selComp;  // delete previously selected component

  view->selComp = new Dot_Bottom();

  if(view->drawn) view->viewport()->repaint();
  view->drawn = false;
  view->MouseMoveAction = &QucsView::MMoveComponent;
  view->MousePressAction = &QucsView::MPressComponent;
  view->MouseReleaseAction = &QucsView::MouseDoNothing;
  view->MouseDoubleClickAction = &QucsView::MouseDoNothing;
}

// #######################################################################
// Is called, when "Undo"-Button is pressed.
void QucsActions::slotEditUndo()
{
  view->editText->setHidden(true); // disable text edit of component property
  
  view->Docs.current()->undo();
  view->viewport()->repaint();
  view->drawn = false;
}

// #######################################################################
// Is called, when "Undo"-Button is pressed.
void QucsActions::slotEditRedo()
{
  view->editText->setHidden(true); // disable text edit of component property
  
  view->Docs.current()->redo();
  view->viewport()->repaint();
  view->drawn = false;
}

// #######################################################################
// Is called, when "Align top" action is activated.
void QucsActions::slotAlignTop()
{
  view->editText->setHidden(true); // disable text edit of component property

  if(!view->Docs.current()->aligning(0))
    QMessageBox::information(App, tr("Info"),
		      tr("At least two elements must be selected !"));
  view->viewport()->repaint();
  view->drawn = false;
}

// #######################################################################
// Is called, when "Align bottom" action is activated.
void QucsActions::slotAlignBottom()
{
  view->editText->setHidden(true); // disable text edit of component property

  if(!view->Docs.current()->aligning(1))
    QMessageBox::information(App, tr("Info"),
		      tr("At least two elements must be selected !"));
  view->viewport()->repaint();
  view->drawn = false;
}

// #######################################################################
// Is called, when "Align left" action is activated.
void QucsActions::slotAlignLeft()
{
  view->editText->setHidden(true); // disable text edit of component property

  if(!view->Docs.current()->aligning(2))
    QMessageBox::information(App, tr("Info"),
		      tr("At least two elements must be selected !"));
  view->viewport()->repaint();
  view->drawn = false;
}

// #######################################################################
// Is called, when "Align right" action is activated.
void QucsActions::slotAlignRight()
{
  view->editText->setHidden(true); // disable text edit of component property

  if(!view->Docs.current()->aligning(3))
    QMessageBox::information(App, tr("Info"),
		      tr("At least two elements must be selected !"));
  view->viewport()->repaint();
  view->drawn = false;
}

// #######################################################################
// Is called, when "Distribute horizontally" action is activated.
void QucsActions::slotDistribHoriz()
{
  view->editText->setHidden(true); // disable text edit of component property

  view->Docs.current()->distribHoriz();
  view->viewport()->repaint();
  view->drawn = false;
}

// #######################################################################
// Is called, when "Distribute vertically" action is activated.
void QucsActions::slotDistribVert()
{
  view->editText->setHidden(true); // disable text edit of component property

  view->Docs.current()->distribVert();
  view->viewport()->repaint();
  view->drawn = false;
}

// ---------------------------------------------------------------------
// Is called when the select all action is activated.
void QucsActions::slotSelectAll()
{
  view->editText->setHidden(true); // disable text edit of component property

  view->Docs.current()->selectElements(INT_MIN, INT_MIN,
				INT_MAX, INT_MAX, true);
  view->viewport()->repaint();
  view->drawn = false;
}

// ------------------------------------------------------------------------
// Is called by slotShowLastMsg(), by slotShowLastNetlist() and from the
// component edit dialog.
void QucsActions::editFile(const QString& File)
{
  QStringList com;
  com << QucsSettings.Editor;
  if (!File.isEmpty()) com << File;
  Q3Process *QucsEditor = new Q3Process(com);
  QucsEditor->setCommunication(0);
  if(!QucsEditor->start()) {
    QMessageBox::critical(App, tr("Error"), tr("Cannot start text editor!"));
    delete QucsEditor;
    return;
  }

  // to kill it before qucs ends
  connect(App, SIGNAL(signalKillEmAll()), QucsEditor, SLOT(kill()));
}

// ------------------------------------------------------------------------
// Is called to show the output messages of the last simulation.
void QucsActions::slotShowLastMsg()
{
  editFile(QucsHomeDir.filePath("log.txt"));
}

// ------------------------------------------------------------------------
// Is called to show the netlist of the last simulation.
void QucsActions::slotShowLastNetlist()
{
  editFile(QucsWorkDir.filePath("netlist.net"));
}

// ------------------------------------------------------------------------
// Is called to start the text editor.
void QucsActions::slotCallEditor()
{
  editFile(QString(""));
}

// ------------------------------------------------------------------------
// Is called to start the filter synthesis program.
void QucsActions::slotCallFilter()
{
  Q3Process *QucsFilter =
    new Q3Process(QString(QucsSettings.BinDir + "qucsfilter"));
  if(!QucsFilter->start()) {
    QMessageBox::critical(App, tr("Error"),
                          tr("Cannot start filter synthesis program!"));
    delete QucsFilter;
    return;
  }

  // to kill it before qucs ends
  connect(App, SIGNAL(signalKillEmAll()), QucsFilter, SLOT(kill()));
}

// ------------------------------------------------------------------------
// Is called to start the transmission line calculation program.
void QucsActions::slotCallLine()
{
  Q3Process *QucsLine =
    new Q3Process(QString(QucsSettings.BinDir + "qucstrans"));
  if(!QucsLine->start()) {
    QMessageBox::critical(App, tr("Error"),
                          tr("Cannot start line calculation program!"));
    delete QucsLine;
    return;
  }

  // to kill it before qucs ends
  connect(App, SIGNAL(signalKillEmAll()), QucsLine, SLOT(kill()));
}

// ########################################################################
void QucsActions::slotHelpIndex()
{
  showHTML("index.html");
}

// ########################################################################
void QucsActions::slotGettingStarted()
{
  showHTML("start.html");
}

// ########################################################################
void QucsActions::showHTML(const QString& Page)
{
  QStringList com;
  com << QucsSettings.BinDir + "qucshelp" << Page;
  Q3Process *QucsHelp = new Q3Process(com);
  QucsHelp->setCommunication(0);
  if(!QucsHelp->start()) {
    QMessageBox::critical(App, tr("Error"), tr("Cannot start qucshelp!"));
    delete QucsHelp;
    return;
  }

  // to kill it before qucs ends
  connect(App, SIGNAL(signalKillEmAll()), QucsHelp, SLOT(kill()));
}
