// EKV MOSFET MODEL
//
//                Drain 1
//                  o
//                  |
//                  |
//              |---+
//              |
// Gate 2 o-----|------o 4 Bulk
//              |
//              |---+
//                  |
//                  |
//                  o
//               Source 3
//
//
//	  Author: Wonhoon Jang

#ifndef Mospekv_h
#define Mospekv_h 1

#include "../../../../network/ADInterface.h"
#include "../../../../analysis/TimeDomainSV.h"

#define PMOS -1
class Mospekv:public ADInterface
{
	public:

  Mospekv(const string& iname);

  ~Mospekv() {}

  static const char* getNetlistName()
  {
    return einfo.name;
  }

  // Do some local initialization
  virtual void init() throw(string&);

	private:

  virtual void eval(AD * x, AD * effort, AD * flow);

  // Element information
  static ItemInfo einfo;

  // Number of parameters of this element
  static const unsigned n_par;

  double epsilonsi, epsilonox, q, k, Tref, vtT, vtTnom, vtTref, egT, egTnom;
  double egTref, niT, niTnom, eta, vtoT, kpT, ucritT, phiT, ibbT;

  // Parameter variables
  double type, l, w, np, ns, cox, xj, dw, dl, vto, gamma, phi, kp, eo;
  double ucrit, tox, nsub, vfb, uo, vmax, theta, lambda, weta, leta, qo;
  double lk, iba, ibb,ibn, tcv, bex, ucex, ibbt, avto, akp, agamma, kf;
  double af, nqs, satlim, xqc, scale, tnom, tmp;

  // Parameter information
  static ParmInfo pinfo[];
};

#endif