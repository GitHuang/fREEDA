/***************************************************************************
                          main.cpp  -  description
 ***************************************************************************/

#include <config.h>

#include <stdlib.h>
#include <math.h>

#include <qapplication.h>
#include <qstring.h>
#include <qtextcodec.h>
#include <qtranslator.h>
#include <qfile.h>
#include <q3textstream.h>
#include <qmessagebox.h>
#include <qregexp.h>
//Added by qt3to4:
#include <Q3PtrList>

#include "gui.h"
#include "guiview.h"
#include "guimain.h"
#include "node.h"

tQucsSettings QucsSettings;

QFont savingFont;    // to remember which font to save in "qucsrc"

QucsApp *QucsMain;   // the Qucs application itself

QDir QucsWorkDir;
QDir QucsHomeDir;

// just dummies for empty lists
Q3PtrList<Wire>      SymbolWires;
Q3PtrList<Node>      SymbolNodes;
Q3PtrList<Diagram>   SymbolDiags;
Q3PtrList<Component> SymbolComps;

// #########################################################################
// Loads the settings file and stores the settings.
bool loadSettings()
{
  QFile file(QucsHomeDir.filePath("qucsrc"));
  if(!file.open(QIODevice::ReadOnly)) return false; // settings file doesn't exist

  Q3TextStream stream(&file);
  QString Line, Setting;

  bool ok;
  while(!stream.atEnd()) {
    Line = stream.readLine();
    Setting = Line.section('=',0,0);
    Line    = Line.section('=',1,1);
    if(Setting == "Position") {
	QucsSettings.x = Line.section(",",0,0).toInt(&ok);
	QucsSettings.y = Line.section(",",1,1).toInt(&ok); }
    else if(Setting == "Size") {
	QucsSettings.dx = Line.section(",",0,0).toInt(&ok);
	QucsSettings.dy = Line.section(",",1,1).toInt(&ok); }
    else if(Setting == "Font") {
	QucsSettings.font.fromString(Line);
	savingFont = QucsSettings.font;

	QucsSettings.largeFontSize
		= floor(4.0/3.0 * QucsSettings.font.pointSize());
	}
    else if(Setting == "BGColor") {
	QucsSettings.BGColor.setNamedColor(Line);
	if(!QucsSettings.BGColor.isValid())
	  QucsSettings.BGColor.setRgb(255, 250, 225); }
    else if(Setting == "maxUndo") {
	QucsSettings.maxUndo = Line.toInt(&ok); }
    else if(Setting == "Editor") {
	QucsSettings.Editor = Line; }
  }

  file.close();
  return true;
}

// #########################################################################
// Saves the settings in the settings file.
bool saveApplSettings(QucsApp *qucs)
{
  QFile file(QucsHomeDir.filePath("qucsrc"));
  if(!file.open(QIODevice::WriteOnly)) {    // settings file cannot be created
    QMessageBox::warning(0, QObject::tr("Warning"),
			QObject::tr("Cannot save settings !"));
    return false;
  }

  QString Line;
  Q3TextStream stream(&file);

  stream << "Settings file, TEST " PACKAGE_VERSION "\n"
    << "Position=" << qucs->x() << "," << qucs->y() << "\n"
    << "Size=" << qucs->width() << "," << qucs->height() << "\n"
    << "Font=" << savingFont.toString() << "\n"
    << "BGColor=" << qucs->view->viewport()->paletteBackgroundColor().name()
    << "\n"
    << "maxUndo=" << QucsSettings.maxUndo << "\n"
    << "Editor=" << QucsSettings.Editor << "\n";
  file.close();

  return true;
}

// #########################################################################
QString complexRect(double real, double imag, int Precision)
{
  QString Text;
  if(fabs(imag) < 1e-250) Text = QString::number(real,'g',Precision);
  else {
    Text = QString::number(imag,'g',Precision);  // Returns a String Equivalent to Number imag 
    if(Text.at(0) == '-') {
      //Text.at(0) = 'j';
      // Added by Shivam
      Text[0] = 'j';     
      Text = '-'+Text;
    }
    else  Text = "+j"+Text;
    Text = QString::number(real,'g',Precision) + Text;
  }
  return Text;
}

QString complexDeg(double real, double imag, int Precision)
{
  QString Text;
  if(fabs(imag) < 1e-250) Text = QString::number(real,'g',Precision);
  else {
    Text  = QString::number(sqrt(real*real+imag*imag),'g',Precision) + " / ";
    Text += QString::number(180.0/M_PI*atan2(imag,real),'g',Precision) + '�';
  }
  return Text;
}

QString complexRad (double real, double imag, int Precision)
{
  QString Text;
  if(fabs(imag) < 1e-250) Text = QString::number(real,'g',Precision);
  else {
    Text  = QString::number(sqrt(real*real+imag*imag),'g',Precision);
    Text += " / " + QString::number(atan2(imag,real),'g',Precision) + "rad";
  }
  return Text;
}

// #########################################################################
QString StringNum(double num, char form, int Precision)
{
  int a = 0;
  char *p, Buffer[512], Format[6] = "%.00g";
  QString s;

  if(Precision < 0) {
    Format[1]  = form;
    Format[2]  = 0;
  }
  else {
    Format[4]  = form;
    Format[2] += Precision / 10;
    Format[3] += Precision % 10;
  }
  sprintf(Buffer, Format, num);
  p = strchr(Buffer, 'e');
  if(p) {
    p++;
    if(*(p++) == '+') { a = 1; }   // remove '+' of exponent
    if(*p == '0') { a++; p++; }    // remove leading zeros of exponent
    if(a > 0)
      do {
        *(p-a) = *p;
      } while(*(p++) != 0);    // override characters not needed
  }

  s = Buffer;
  return s;
}

// #########################################################################
void str2num(const QString& s_, double& Number, QString& Unit, double& Factor)
{
  QString str = s_.stripWhiteSpace();

/*  int i=0;
  bool neg = false;
  if(str[0] == '-') {      // check sign
    neg = true;
    i++;
  }
  else if(str[0] == '+')  i++;

  double num = 0.0;
  for(;;) {
    if(str[i] >= '0')  if(str[i] <= '9') {
      num = 10.0*num + double(str[i]-'0');
    }
  }*/

  QRegExp Expr( QRegExp("[^0-9\\x2E\\x2D\\x2B]") );
  int i = str.find( Expr );
  if(i >= 0)
    if((str.at(i).latin1() | 0x20) == 'e') {
      int j = str.find( Expr , ++i);
      if(j == i)  j--;
      i = j;
    }

  Number = str.left(i).toDouble();
  Unit   = str.mid(i).stripWhiteSpace();

  switch(Unit.at(0).latin1()) {
    case 'T': Factor = 1e12;  break;
    case 'G': Factor = 1e9;   break;
    case 'M': Factor = 1e6;   break;
    case 'k': Factor = 1e3;   break;
    case 'c': Factor = 1e-2;  break;
    case 'm': Factor = 1e-3;  break;
    case 'u': Factor = 1e-6;  break;
    case 'n': Factor = 1e-9;  break;
    case 'p': Factor = 1e-12; break;
    case 'f': Factor = 1e-15; break;
//    case 'd':
    default:  Factor = 1.0;
  }

  return;
}


// #########################################################################
// ##########                                                     ##########
// ##########                  Program Start                      ##########
// ##########                                                     ##########
// #########################################################################

#include <qregexp.h>
int main(int argc, char *argv[])
{
#if 0
  double zD = 1.0e5;
  qDebug(StringNum(zD, 'e', -1));

  zD = 1.1e5;
  qDebug(StringNum(zD, 'e', -1));

  zD = 1.12e5;
  qDebug(StringNum(zD, 'e', -1));

  zD = 1.123e5;
  qDebug(StringNum(zD, 'e', -1));

  zD = 1.1234567e5;
  qDebug(StringNum(zD, 'e', -1));
  qDebug(" ");

  zD = 1.0e5;
  qDebug(StringNum(zD, 'f', -1));

  zD = 1.1e5;
  qDebug(StringNum(zD, 'f', -1));

  zD = 1.12e5;
  qDebug(StringNum(zD, 'f', -1));

  zD = 1.123e5;
  qDebug(StringNum(zD, 'f', -1));

  zD = 1.1234567e5;
  qDebug(StringNum(zD, 'f', -1));
  qDebug(" ");

  zD = 1.0e5;
  qDebug(StringNum(zD, 'g', -1));

  zD = 1.1e5;
  qDebug(StringNum(zD, 'g', -1));

  zD = 1.12e5;
  qDebug(StringNum(zD, 'g', -1));

  zD = 1.123e5;
  qDebug(StringNum(zD, 'g', -1));

  zD = 1.1234567e5;
  qDebug(StringNum(zD, 'g', -1));
  qDebug(" ");

  // ------------------------------------

  zD = 1.0e-5;
  qDebug(StringNum(zD, 'e', -1));

  zD = 1.1e-5;
  qDebug(StringNum(zD, 'e', -1));

  zD = 1.12e-5;
  qDebug(StringNum(zD, 'e', -1));

  zD = 1.123e-5;
  qDebug(StringNum(zD, 'e', -1));
 
  zD = 1.1234567e-5;
  qDebug(StringNum(zD, 'e', -1));
  qDebug("\n");

  zD = 1.0e-5;
  qDebug(StringNum(zD, 'f', -1));

  zD = 1.1e-5;
  qDebug(StringNum(zD, 'f', -1));

  zD = 1.12e-5;
  qDebug(StringNum(zD, 'f', -1));

  zD = 1.123e-5;
  qDebug(StringNum(zD, 'f', -1));

  zD = 1.1234567e-5;
  qDebug(StringNum(zD, 'f', -1));
  qDebug(" ");

  zD = 1.0e-5;
  qDebug(StringNum(zD, 'g', -1));

  zD = 1.1e-5;
  qDebug(StringNum(zD, 'g', -1));

  zD = 1.12e-5;
  qDebug(StringNum(zD, 'g', -1));

  zD = 1.123e-5;
  qDebug(StringNum(zD, 'g', -1));

  zD = 1.1234567e-5;
  qDebug(StringNum(zD, 'g', -1));
  qDebug(" ");

  return 0;
#endif
#if 0
  double d, Factor;
  QString Unit, s("-2.9E+02eco");
  str2num(s, d, Unit, Factor);
  qDebug("String: %s, Zahl: %g, Einheit: %s, Faktor: %g",
	 s.latin1(), d, Unit.latin1(), Factor);

  s = "2.5eco";
  str2num(s, d, Unit, Factor);
  qDebug("String: %s, Zahl: %g, Einheit: %s, Faktor: %g",
	 s.latin1(), d, Unit.latin1(), Factor);
  return 0;
#endif

  // apply default settings
  QucsSettings.x = 0;
  QucsSettings.y = 0;
  QucsSettings.dx = 600;
  QucsSettings.dy = 400;
  QucsSettings.font = QFont("Helvetica", 12);
  QucsSettings.largeFontSize = 16.0;
  QucsSettings.BGColor = QColor(255, 250, 225);
  QucsSettings.maxUndo = 20;

  // is application relocated?
  char * var = getenv ("QUCSDIR");
  if (var != NULL) {
    QDir QucsDir = QDir (var);
    QString QucsDirStr = QucsDir.canonicalPath ();
    QucsSettings.BinDir =
      QDir::convertSeparators (QucsDirStr + "/bin/");
    QucsSettings.BitmapDir =
      QDir::convertSeparators (QucsDirStr + "/share/qucs/bitmaps/");
    QucsSettings.LangDir =
      QDir::convertSeparators (QucsDirStr + "/share/qucs/lang/");
  } else {
    QucsSettings.BinDir = "../simulator/"; //BINARYDIR;
    QucsSettings.BitmapDir = "./bitmaps/"; //BITMAPDIR;
    //QucsSettings.LangDir = LANGUAGEDIR;
  }
  QucsSettings.Editor = QucsSettings.BinDir + "qucsedit";

  QucsWorkDir.setPath(QDir::homeDirPath()+QDir::convertSeparators ("/freeda/projects"));
  QucsHomeDir.setPath(QDir::homeDirPath()+QDir::convertSeparators ("/freeda"));
  loadSettings();

  QApplication a(argc, argv);
  a.setFont(QucsSettings.font);

  QTranslator tor( 0 );
  tor.load( QString("qucs_") + QTextCodec::locale(), QucsSettings.LangDir);
  a.installTranslator( &tor );

  QucsMain = new QucsApp();
  a.setMainWidget(QucsMain);
  QucsMain->show();
  int result = a.exec();
  saveApplSettings(QucsMain);
  return result;
}
