// Electro Thermal Model of CMOS NAND Gate
//
//
//                                           Vdd 1
//                                              o
//                                              |
//                      *-----------------------*
//                      |                       |
//                  |---+                   |---+
//                  |                       |
// Input1 2 o--|---O|    Input2 3 o--|-----O|
//             |    |                |      |
//             |    |---+            |      |---+
//             |        |            |          |
//             |        |            |          |
//             |        -------------|----------*--o 4 Output		o 6 thermal node
//             |                     |          |			
//             |                     |          |			
//             |                     |          |		
//             |                     |      |---+			
//             |                     |      |				
//             |                     |------|			
//             |                            |				
//             |                            |---+			
//             |                                |			 o 7 thermal reference node
//             |                                |
//             |                            |---+
//             |                            |
//             |----------------------------|
//                                          |
//                                          |---+
//                                              |
//                                              |
//                                              o
//                                           GND 5
//
//
//        Author:
//              Shivam Priyadarshi
//
#include "../../../../network/ElementManager.h"
#include "../../../../network/ADInterface.h"

#ifndef Cmos2NandT_h
#define Cmos2NandT_h 1

// The Cmos2NandT Class
class Cmos2NandT : public ADInterface
{
	public:

  Cmos2NandT(const string& iname);

  ~Cmos2NandT() {}

  static const char* getNetlistName()
  {
		return einfo.name;
  }

  // Do some local initialization
  virtual void init() throw(string&);
  virtual void getLocalRefIdx(UnsignedVector& local_ref_vec,TerminalVector& term_list);
	private:

  // Evaluation function
  virtual void eval(AD * x, AD * effort, AD * flow);
  //virtual void eval2(adoublev& dy1, adoublev& z1, adoublev& vp, adoublev& ip);
  

  // Parameter Variables
  double  ln, wn, lp, wp;

  // Model Variables
  double epsilonsi, epsilonox, q, k, Tref, vtTnom, vtTref,egTnom;
  double egTref,niTnom;
  double np_n, ns_n,np_p,ns_p,cox_n,cox_p, phi_n,phi_p,kp_n,kp_p,eo_n,eo_p;
  double ucrit_n,ucrit_p,tox_n,tox_p,nsub,psub,vfb_n,vfb_p,uo_n,uo_p, vmax_n,vmax_p,lambda_n,lambda_p,weta_n,weta_p,leta_n,leta_p,qo_n,qo_p;
  double lk_n,lk_p,iba_n,iba_p,ibb_n,ibb_p,ibn_n,ibn_p, bex_n,bex_p,ucex_n,ucex_p,ibbt_n,ibbt_p,avto_n,avto_p,akp_n,akp_p,agamma_n,agamma_p;
  double scale, tnom, tmp,rth;
  double type_n,type_p,tcv_n,tcv_p,vto_n,vto_p,vtoa_n,vtoa_p,eta_n,eta_p;
  double xj_n, xj_p, dw_n, dw_p, dl_n, dl_p,gamma_n,gamma_p;
  AD temp_prev;

  // Element information
  
  static ItemInfo einfo;

  // Number of parameters of this element
  static const unsigned n_par;

  // Parameter information
  static ParmInfo pinfo[];

};

#endif
