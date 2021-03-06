// This is a Single Frequency FM voltage source
//
//       n1     + ---  -   n2
//          o----( ~ )-----o
//                ---
//
// by Satish V. Uppathil

#ifndef VsfFM_h
#define VsfFM_h 1

class VsfFM : public Element
{
	public:

  VsfFM(const string& iname) ;

  ~VsfFM() {}

  static const char* getNetlistName()
  {
    return einfo.name ;
  }

  // This element adds equations to the MNAM
  virtual unsigned getExtraRC(const unsigned& eqn_number,
	const MNAMType& type) ;
  virtual void getExtraRC(unsigned& first_eqn, unsigned& n_rows) const ;

  // fill MNAM
  virtual void fillMNAM(TimeMNAM* mnam) ;
  virtual void fillSourceV(TimeMNAM* mnam) ;
  virtual void fillMNAM(FreqMNAM* mnam) ;

  // State variable transient analysis
  virtual void svTran(TimeDomainSV *tdsv) ;
  virtual void deriv_svTran(TimeDomainSV *tdsv)  ;


	private:

  // row assigned to this instance by the FreqMNAM
  unsigned my_row ;

  // Element information
  static ItemInfo einfo ;

  // Number of parameters of this element
  static const unsigned n_par ;

  // Parameter variables
  double vo, va, fcarrier, mdi, fsignal ;

  // Parameter information
  static ParmInfo pinfo[] ;

} ;

#endif

