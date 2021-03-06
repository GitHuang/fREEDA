#include "../../../../network/ElementManager.h"
#include "../../../../analysis/FreqMNAM.h"
#include "../../../../analysis/TimeDomainSV.h"
#include "ThermalTransf.h"
#include <cstdio>

#define Np 6			/* Number of terms used in Stehfest
algorithm for numerical Laplace
inversion. */
#define NR_END 0		/* For use in dynamic allocation of arrays. */

#define LN2 0.6931471806

// Static members
// Number of netlist paramenters (remember to update!)
const unsigned ThermalTransf::n_par = 16;

// Element information
ItemInfo ThermalTransf::einfo =
{
  "thermaltransf",
  "Heatsink mounted MMIC: Test of time-step decoupling with tranformation.",
  "Bill Batty, Carlos Christoffersen",
  DEFAULT_ADDRESS"category:thermal",
  "2000_07_20"
};

// Parameter information
// true means the paramenter is required and false means it
// can be omitted in the netlist.
// See ../network/NetListItem.h for a list of types (TR_INT, TR_DOUBLE, etc.)
ParmInfo ThermalTransf::pinfo[] =
{
  {"ntimesteps", "Number of time steps in transient simulation",
	TR_INT, false},
  {"dt", "Length of thermal timestep (s)", TR_DOUBLE, false},
  {"tambient", "Constant heatsink mount temperature (K)", TR_DOUBLE, false},
  {"time_d", "Flag, if true, calculate in the time domain.", TR_BOOLEAN, false},
  {"l", "Die x-dimension in meters.", TR_DOUBLE, false},
  {"w", "Die y-dimension in meters.", TR_DOUBLE, false},
  {"d", "Die z-dimension in meters.", TR_DOUBLE, false},
  {"xl", "x-coordinate of left edge of heating element.", TR_DOUBLE, false},
  {"xr", "x-coordinate of right edge of heating element.", TR_DOUBLE, false},
  {"yu", "y-coordinate of upper edge of heating element.", TR_DOUBLE, false},
  {"yd", "y-coordinate of lower edge of heating element", TR_DOUBLE, false},
  {"ks", "Thermal conductivity (W/m.K).", TR_DOUBLE, false},
  {"rho", "Density (kg.m-3).", TR_DOUBLE, false},
  {"c", "Specific heat (J/kg.K).", TR_DOUBLE, false},
  {"nfingers", "Number of power transistor fingers", TR_INT, false},
  {"b","Exponent in power law temperature dependence of thermal conductivity",TR_DOUBLE,false}
};


ThermalTransf::ThermalTransf(const string& iname) :
Element(&einfo, pinfo, n_par, iname)
{
  // Setup parameters
  paramvalue[0] = &(Ntimesteps = 0);
  paramvalue[1] = &(dt = zero);
  paramvalue[2] = &(Tambient=300.);
  paramvalue[3] = &(time_d = false);
  paramvalue[4] = &(L = 400.0e-6);
  paramvalue[5] = &(W = 400.0e-6);
  paramvalue[6] = &(D = 400.0e-6);
  paramvalue[7] = &(xL = 220.0e-6);
  paramvalue[8] = &(xR = 180.0e-6);
  paramvalue[9] = &(yU = 220.0e-6);
  paramvalue[10] = &(yD = 180.0e-6);
  paramvalue[11] = &(Ks = 46.0);
  paramvalue[12] = &(rho = 5320.0);
  paramvalue[13] = &(C = 350.0);
  paramvalue[14] = &(Nfingers = 1);
  paramvalue[15] = &(b = 1.22);

  // Set the number of terminals
  setNumTerms(2);
  // Set flags
  setFlags(LINEAR | ONE_REF | TR_FREQ_DOMAIN);
  // Set number of states
  setNumberOfStates(1);

  // Set unallocated pointers to 0
  Rth = Pstore = NULL;
}


ThermalTransf::~ThermalTransf()
{
  if (Pstore)
    free_vector(Pstore,1,Ntimesteps);
  if (Rth)
    free_vector(Rth,0,Ntimesteps);
}

// This is the initialization routine.
void ThermalTransf::init() throw(string&)
{
  //Initialize some variables
  storedDeltaT = storedDeltaT_new = zero;
  last_tau = lastTStime = tfrac = Pav = Pold = last_x = zero;
  n_tts = n_steps = im1 = 0;
  bm1 = b - one;
  dtheta = DenseDoubleVector(20);
  storedDeltaT_base = DenseDoubleVector(20);
  knorm = DenseDoubleVector(20);
  for (int i = 0; i < knorm.length(); i++)
    knorm[i] = one;
  time = DenseDoubleVector(20);

  if (time_d) {
    if (!Ntimesteps || !dt)
      throw(getInstanceName() +
		": Both Ntimesteps and dt must be specified if time_d=1");
    // Change element flags
    setFlags(NONLINEAR | ONE_REF | TR_TIME_DOMAIN);

    Rth = newvector(0,Ntimesteps); /* Dynamically allocate storage for
		thermal impedance Rth(t) evaluated
		at specified times t. */
    Rth[0] = 0.0; /* At t = 0 temperature rise is always 0. */

    Pstore = newvector(1,Ntimesteps); /* Dynamically allocate storage
		for the self-consistently
		calculated power dissipation
		(Watts) at each timestep. */

    printf("\nPrecomputation for the Rth(t) ...\n\n");
    for (int n = 1; n <= Ntimesteps; n++) {
      double t = n*dt;
      double deltaT = TimeDomain(Tambient,t); /* Calculate temperature rise
			from Tambient as a function
			of time t. */
      Rth[n] = deltaT; /* deltaT = Rth.P and power P is normalised to
			1 W (over specified area of heating
			element). */

      printf("%f %f \n",t,Rth[n] + Tambient);
    }
    printf("\nPrecomputation for the Rth(t) complete.\n\n");

  }
}

void ThermalTransf::fillMNAM(FreqMNAM* mnam)
{
  double SMALL=1.0e0; /* Should be a safe `small' number, essentially zero. */
  double_complex rth;
  double_complex s(zero, twopi * mnam->getFreq());

  if (abs(s) < SMALL)
  	rth = TimeIndependent(Tambient);
  else if (abs(s) >= SMALL)
  	rth = CLaplaceDomain(Tambient, s);

  // For debugging purposes only
  cout << s << "\t" << rth << endl;
  double_complex gth = one / rth;
  // Ask my terminals the row numbers
  mnam->setAdmittance(getTerminal(0)->getRC(), getTerminal(1)->getRC(), gth);
}

// This routine is called at each time step
// (Actually, several times at each time step because of the nonlinear
// iterations)
void ThermalTransf::svTran(TimeDomainSV* tdsv)
{
  if(tdsv->DC()) {
    tdsv->i(0) = tdsv->getX(0);
    tdsv->u(0) = 0;
  }
  else {
    // We need to be careful when to reset the firstTime() flag in
    // tdsv to do this.
    if (tdsv->firstTime()) {
      // This is a new time step.
      // Correct previous step variables
      if (im1 != n_steps) {
				getDeltaTheta(last_x, last_tau);
      }
      // Save previous state
      storedDeltaT = storedDeltaT_new;
      // Last confirmed index
      n_tts += n_steps;
      // Last confirmed temperature increase
      dtheta[0] = dtheta[n_steps];
      knorm[0] = knorm[n_steps];
      time[0] = time[n_steps];
      // Last thermal TS time
      lastTStime = dt * n_tts;
      prev_tau = last_tau;
      tfrac = last_tau - lastTStime;
      // Last average power
      Pold = Pav;
      n_steps = 0;
    }
    tdsv->u(0) = getDeltaT(tdsv->getX(0),
		tdsv->getCurrentTime(), tdsv->getdt());
    tdsv->i(0) = tdsv->getX(0);
  }
}

void ThermalTransf::deriv_svTran(TimeDomainSV* tdsv)
{
  // Fix derivative (for the multi thermal-step case)

  tdsv->getJi()(0,0) = one;
  if(tdsv->DC())
    tdsv->getJu()(0,0) = zero;
  else {
    assert(!tdsv->firstTime());
    // Try numerical derivative
    double x1 = tdsv->getX(0);
    double ctime = tdsv->getCurrentTime();
    const double& cdt =  tdsv->getdt();
    double deltaT1 = getDeltaT(x1, ctime, cdt);
    const double deltaX = 1e-8;
    x1 += deltaX;
    double deltaT2 = getDeltaT(x1, ctime, cdt);
    tdsv->getJu()(0,0) = (deltaT2 - deltaT1) * deltaX;
  }
}

double ThermalTransf::getDeltaT(const double& x, const double& ctime,
const double& cdt)
{
  // n1: index of last vector element
  // Assume deltaTau1 = dt_circuit by now
  double tau1 = prev_tau + cdt;
  int n1 = getDeltaTheta(x, tau1);

  // calculate kS / k(theta) and time vectors
  int i = 0;
  do {
    i++;
    knorm[i] = pow(one - bm1 * dtheta[i] / Tambient, -b/bm1);
    time[i] = time[i-1] + dt * (knorm[i-1] + knorm[i]) * .5;
  } while (time[i] < ctime);
  assert(i < n1);
  im1 = i - 1;

  // Do interpolation
  double deltaT = dtheta[im1]
	+ (dtheta[i] - dtheta[im1]) * (ctime - time[im1]) / (time[i] - time[im1]);

  // Calculate last_tau
  last_tau = dt * (n_tts + im1) + (ctime - time[im1]) * .5 *
	(one / knorm[im1] + pow(one - bm1 * deltaT / Tambient, b/bm1));

  // Implement inverse Kirchhoff transformation
  deltaT = Tambient * (pow(one - bm1 * deltaT / Tambient, -one / bm1) - one);
  return deltaT;
}

int ThermalTransf::getDeltaTheta(const double& x, const double& tau)
{
  // n_ps points to the next Pstore[] element
  int n_ps = n_tts + 1;
  last_x = x;
  if (tau <= lastTStime + dt) {
    n_steps = 0;
    // Calculate average power in this TTS so far
    Pav = (Pold * tfrac + x * (dt - tfrac)) / dt;
    Pstore[n_ps] = Pav;
    dtheta[1] = storedDeltaT + (Rth[1] - Rth[0]) * Pstore[n_ps];
    storedDeltaT_new = storedDeltaT;
  }
  else {
    // Calculate how many thermal TS we need
    int n_steps1(int((tau - lastTStime) / dt));
    assert(n_ps + n_steps1 < Ntimesteps);
    // Prepare input power vector and save current state (just in
    // case this circuit time step gets rejected later
    Pstore[n_ps] = (Pold * tfrac + x * (dt - tfrac)) / dt;
    // for loop for additional time steps
    for (int n1 = n_ps + 1; n1 <= n_ps + n_steps1; n1++)
      Pstore[n1] = x;

    Pav = x;
    n_ps += n_steps1;
    // Check if we already have part of the summations available
    if (n_steps < n_steps1) {
      storedDeltaT_base[0] = storedDeltaT;
      for (int i=0; i < n_steps1 - n_steps; i++) {
				storedDeltaT_base[n_steps1 - i] = zero;
				for (int m = n_ps - i; m > n_steps1 + 1 - i; m--)
					storedDeltaT_base[n_steps1 - i] +=
				(Rth[m] - Rth[m-1]) * Pstore[n_ps-m+1-i];
      }
    }
    // Save for next iteration
    n_steps = n_steps1;
    // Calculate increment in each vector component.
    for (int i=0; i <= n_steps1; i++) {
      dtheta[n_steps1 + 1 - i] = storedDeltaT_base[n_steps1 - i];
      for (int m = n_steps1 + 1 - i; m > 0; m--)
				dtheta[n_steps1 + 1 - i] += (Rth[m] - Rth[m-1]) * Pstore[n_ps-m+1-i];
    }
    // Save for next iteration
    storedDeltaT_new = dtheta[n_steps1 + 1] - (Rth[1] - Rth[0]) * Pstore[n_ps];
  }

  // Return length of theta vector
  return n_steps + 2;
}

double ThermalTransf::TimeIndependent(double Tambient)
{
  int m,n,delta_m0,delta_n0,mMAX,nMAX;
  double result, P, lambda_m,mu_n,gamma_mn, k, Imn;


  k = Ks/(rho*C);	/* Diffusivity. */

	P = Nfingers*1.0/((xL-xR)*(yU-yD)); /* W.m-2 */ /* Areal power dissipation
	over grid array heating area (total
	power set at 1 Watt for
	normalisation). */

  mMAX = nMAX = 100;	/* Number of terms in the m and n summations
	of the double series for the thermal
	impedance. */

  result = 0.0;
  for (m = 0; m <= mMAX; m++){
    if (m == 0) delta_m0 = 1; else delta_m0 = 0;
    lambda_m = m*pi/L;
    for (n = 0; n <= nMAX; n++){
      if (n == 0) delta_n0 = 1; else delta_n0 = 0;
      mu_n = n*pi/W;
      gamma_mn = sqrt(lambda_m*lambda_m + mu_n*mu_n);

      /* Imn are area integrals over the heating element. */

      if (m != 0 && n != 0)
				Imn = (sin(lambda_m*xL) - sin(lambda_m*xR))
			*(sin(mu_n*yU) - sin(mu_n*yD))/(lambda_m*mu_n);
      if (m == 0 && n != 0)
				Imn = (xL - xR)*(sin(mu_n*yU) - sin(mu_n*yD))/mu_n;
      if (m != 0 && n == 0)
				Imn = (sin(lambda_m*xL) - sin(lambda_m*xR))*(yU - yD)/lambda_m;
      if (m == 0 && n == 0)
				Imn = (xL - xR)*(yU - yD);

      /* Insert into analytical expression for thermal impedance. */
			if (!(m == 0 && n == 0)){
				result +=
				4.0*(Imn*Imn*(1.0/((xL - xR)*(yU - yD))) / ((1.0 + delta_m0)*(1.0 + delta_n0)))
				*tanh(gamma_mn*D)*(Imn/((xL - xR)*(yU - yD)))/(gamma_mn);
			}
    }
  }

  result = result + D*(xL - xR)*(yU - yD);
  result = result*P/(Ks*L*W);

  return result;
}

/* Function LaplaceDomain returns the Laplace s-space thermal
impedance. */
/* This subroutine contains all the geometrical and material details
required for analytical construction of the s-space thermal
impedance. */
double ThermalTransf::LaplaceDomain(double Tambient, double s)
{
  int m,n,delta_m0,delta_n0,mMAX,nMAX;
  double result, P, lambda_m,mu_n,gamma_mn, k, Imn;

  k = Ks/(rho*C);	/* Diffusivity. */

  P = Nfingers*1.0/((xL-xR)*(yU-yD)); /* W.m-2 */ /* Areal power dissipation
	over grid array heating area (total
	power set at 1 Watt for
	normalisation). */

  mMAX = nMAX = 100;	/* Number of terms in the m and n summations
	of the double series for the thermal
	impedance. */

  result = 0.0;
  for (m = 0; m <= mMAX; m++){
    if (m == 0) delta_m0 = 1; else delta_m0 = 0;
    lambda_m = m*pi/L;
    for (n = 0; n <= nMAX; n++){
      if (n == 0) delta_n0 = 1; else delta_n0 = 0;
      mu_n = n*pi/W;
      gamma_mn = sqrt(lambda_m*lambda_m + mu_n*mu_n + s/k);

      /* Imn are area integrals over the heating element. */

      if (m != 0 && n != 0)
				Imn = (sin(lambda_m*xL) - sin(lambda_m*xR))
			*(sin(mu_n*yU) - sin(mu_n*yD))/(lambda_m*mu_n);
      if (m == 0 && n != 0)
				Imn = (xL - xR)*(sin(mu_n*yU) - sin(mu_n*yD))/mu_n;
      if (m != 0 && n == 0)
				Imn = (sin(lambda_m*xL) - sin(lambda_m*xR))*(yU - yD)/lambda_m;
      if (m == 0 && n == 0)
				Imn = (xL - xR)*(yU - yD);

      /* Insert into analytical expression for thermal impedance. */

      result +=
			(Imn*Imn*(1.0/((xL - xR)*(yU - yD))) / ((1.0 + delta_m0)*(1.0 + delta_n0)))*tanh(gamma_mn*D)/(gamma_mn*s);
    }
  }

  result = result*4.0*P/(Ks*L*W);

  return result;
}


double_complex ThermalTransf::CLaplaceDomain(double Tambient, double_complex s)
{
  int m,n,delta_m0,delta_n0,mMAX,nMAX;
  double_complex result, gamma_mn, i(0.0,1.0);
  double P, lambda_m,mu_n, k, Imn;

  k = Ks/(rho*C);	/* Diffusivity. */

  P = Nfingers*1.0/((xL-xR)*(yU-yD)); /* W.m-2 */ /* Areal power dissipation
	over grid array heating area (total
	power set at 1 Watt for
	normalisation). */

  mMAX = nMAX = 100;	/* Number of terms in the m and n summations
	of the double series for the thermal
	impedance. */

  result = 0.0;
  for (m = 0; m <= mMAX; m++){
    if (m == 0) delta_m0 = 1; else delta_m0 = 0;
    lambda_m = m*pi/L;
    for (n = 0; n <= nMAX; n++){
      if (n == 0) delta_n0 = 1; else delta_n0 = 0;
      mu_n = n*pi/W;
      gamma_mn = sqrt(lambda_m*lambda_m + mu_n*mu_n + s/k);

      /* Imn are area integrals over the heating element. */

      if (m != 0 && n != 0)
				Imn = (sin(lambda_m*xL) - sin(lambda_m*xR))
			*(sin(mu_n*yU) - sin(mu_n*yD))/(lambda_m*mu_n);
      if (m == 0 && n != 0)
				Imn = (xL - xR)*(sin(mu_n*yU) - sin(mu_n*yD))/mu_n;
      if (m != 0 && n == 0)
				Imn = (sin(lambda_m*xL) - sin(lambda_m*xR))*(yU - yD)/lambda_m;
      if (m == 0 && n == 0)
				Imn = (xL - xR)*(yU - yD);

      /* Insert into analytical expression for thermal impedance. */

			// Calculate exp(-gamma_mn*D)
      double_complex expx = -gamma_mn*D;
      expx = exp(expx.real())*(cos(expx.imag())+i*sin(expx.imag()));

			result += (1.0 / ((1.0+delta_m0)*(1.0+delta_n0)))*
			Imn*Imn*(1.0/((xL - xR)*(yU - yD)))*((1.0-expx*expx)/(1.0+expx*expx))  /(gamma_mn);

    }
  }

  result = result*4.0*P/(Ks*L*W);
  return result;
}

/* Function TimeDomain performs the inverse Laplace transform
numerically based on Stehfest's algorithm. */
/* In fact the inversion is easy to do analytically, but the numerical
approach is general and computationally cheap. */
double ThermalTransf::TimeDomain(double Tambient, double t)
{
  int v;
  double result, increment;

  increment = result = 0;

  for (v=1;v<=Np;v++)
	{
		increment = CalculateWeight(v) * LaplaceDomain(Tambient, v*LN2/t);
		result = result + increment;
	}
  result = result*LN2/t;
  return result;
}

/* Function CalculateWeight constructs the weights for the numerical
Laplace inversion. */
double ThermalTransf::CalculateWeight(int v)
{
  int NumberOfTerms, k, start;
  double result, increment;

  if (v<(Np/2))
    NumberOfTerms = v;
  else
    NumberOfTerms = Np/2;

  if (v==1) start = 1;
  if (v==2) start = 1;
  if (v==3) start = 2;
  if (v==4) start = 2;
  if (v==5) start = 3;
  if (v==6) start = 3;
  result = increment = 0;

  for(k = start; k<=NumberOfTerms; k++)
	{
		increment = pow((double)k,Np/2) * fact(2*k);
		increment = increment / (fact(Np/2-k) * fact(k) * fact(k-1) * fact(v-k) * fact(2*k-v));
		result = result + increment;
	}
  result = result*pow((double)-1,(double)(Np/2+v));
  return result;
}

/* Function factorial returns x! */
double ThermalTransf::fact(int x)
{
  int i;
  double cumul = 1;
  for(i=1;i<=x;i++) cumul = cumul *  i;
  if (x==0) cumul = 1.0;
  if (x < 0) {
    printf("Argument less than zero in factorial\n");
    exit(1);
  }
  return cumul;
}

/* nrerror prints an error message to stderr on failure. */
void ThermalTransf::nrerror(const char *error_text)
{
  fprintf(stderr,"Run time error....\n");
  fprintf(stderr,"%s\n",error_text);
  fprintf(stderr,"... now exiting to system ..\n");
  exit(1);
}

/* Dynamic memory allocation and deallocation routines. */
double* ThermalTransf::newvector(int nl,int nh)
{
  double *v;
  v=(double *) malloc((size_t) ((nh-nl+1+NR_END)*sizeof(double)));
  if (!v) nrerror("Allocation failure in newvector()");
  return v-nl+NR_END;
}

void ThermalTransf::free_vector(double *v,int nl,int nh)
{
  free((char*) (v+nl-NR_END));
}

