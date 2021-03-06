// D-Flip Flop
//               VDD 1
//               o
//               | 
//               |
//         --------------
//         |   	        |
// 2 Do----|            |---o Q 4
//         |D Flip Flop |
// 3 CLKo--|            |---o Qb 5
//         --------------
//                |
//                |
//                o GND 6


#ifndef DFlipFlop_h
#define DFlipFlop_h 1

class DFlipFlop : public Element
{
	public:
	       // This is the prototype of the creator routine
  		DFlipFlop(const string& iname);

		// This is the prototype of the destructor routine
  		~DFlipFlop() {}
         static const char* getNetlistName()
	 {
		return einfo.name;
	 }

          // local initialization
	virtual void init() throw(string&);
            private:

               // Set up the variable to store element information
		  static ItemInfo einfo;
		// Set up the variable to store the number of parameters of this element
		   static const unsigned n_par;

                  // Parameter Variables
		   double  ln, wn, lp, wp;

                 // Variables
			double C,Rs;
       
		//Parameter information. Space is allocated for the pointer to the pinfo vector      
        	static ParmInfo pinfo[];
};
#endif
