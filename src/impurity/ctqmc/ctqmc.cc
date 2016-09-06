#include <cstdlib>
#include <cmath>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <set>
#include <vector>
#include <deque>
#include <list>
#include <map>
#include <algorithm>
#include <list>
#include <fstream>
#include "logging.h"
#include "smesh.h"
#include "sfunction.h"
#include "random.h"
#include "number.h"
#include "mpi.h"
#include "common.h"
#include "intervals.h"
#include "matrixm.h"
#include "state.h"
#include "inout.h"
#include "bcast.h"
#include "operators.h"
#include "local.h"
#include "stateim.h"
#include "hb1.h"
#include "bathp.h"
#include "segment.h"

using namespace std;

typedef list<pair<double,double> > seg_typ;
ostream* xout;

template <class Rand>
class CTQMC{
  Rand& rand;                     // random number generator

  ClusterData& cluster;           // information about the atomic states of the particular problem
  BathProb BathP;          // What is the probability to create a kink in any of the baths
  vector<nIntervals> intervals;   // Stores information to compute the bath determinant and green's function: times and types corresponding to each bath sorted in the proper time order
  NOperators Operators;           // Stores information to compute local trace: all times and types in the proper time order.
public:
  function1D<NState> npraStates;  // list of all atomic states
private:
  function2D<NState> state_evolution_left, state_evolution_right; // evolution of atomic states for fast update of atomic trace
  vector<function2D<double> > MD; // matrix of the inverse of delta which is needed to computer local Green's function
  vector<MatrixM> TMD;            // class which manipulates matrix of deltas and local green's function
  vector<spline1D<double> > Delta;// Delta in imaginary time
  vector<function2D<spline1D<double>* > > Deltat; // Delta in imaginary time but in matrix form (needed for non-diagonal cases)
  
  Number matrix_element;          // current matrix element due to local part of the system
  function1D<Number> Trace;       // current atomic trace (for computing average)
  vector<double> detD;            // bath determinants
  
  mesh1D tau;                     // imaginary time mesh
  mesh1D iom;                     // small imaginary frequency mesh on which local Green's function is manipulated
  mesh1D biom;                    // small bosonic imaginary frequency mesh for susceptibilities
  
  function1D<double> histogram;   // histogram
  
  int Naver;                      // Number of measurements
public:
  vector<function2D<dcomplex> > Gaver; // Current approximation for the local Green's function
  function2D<function1D<dcomplex> > Faver;
private:
  function1D<double> observables; // some zero time quantities needed for calculation of occupancies and susceptibilities
  function1D<double> aver_observables; // the same zero-time quantities but the average in simulation rather than the current state
  function2D<double> Prob;        // Probability for each atomic state in current configuration
  function2D<double> AProb;       // Average Probability
  function1D<double> kaver;       // average perturbation order for each bath - measures kinetic energy
  function2D<double> P_transition;// Transition probability from state to state
public:  
  function2D<double> AP_transition;// Avergae Transition probability from state to state
private:
  int nsusc;
  function2D<double> susc;        // current frequency dependent susceptibility
  function2D<double> aver_susc;   // sampled frequency dependent susceptibility
  function2D<dcomplex> dsusc;       // temporary variable for frequency dependent susceptibility
  function2D<dcomplex> dsusc1;       // temporary variable for frequency dependent susceptibility
  
  vector<function2D<double> > Gtau; // Green's function in imaginary time (not so precise)
  int NGta;                       // number of measurements of Gtau
  function2D<dcomplex> dexp, dexp1;      // differences e^{iom*tau_{l+1}}-e^{iom*tau_l} needed for frequency dependent response functions
  int successful;                 // number of successful moves
  function1D<int> successfullC, successfullM;
  
  function1D<double> dtau;        // temporary for computeaverage
  function2D<double> tMD;         // just temporary used in global flip
  nIntervals tinterval;           // just temporary used in global flip
  function2D<dcomplex> bexp;      // temporary for e^{iom*tau}

  int nomv, nOm;
  vector<function2D<dcomplex> > Mv; // M(om1,om2) matrix in imaginary frequency used to compute two particle vertex
public:
  function5D<dcomplex> VertexH, VertexF;
  int NGtv;                        // number of measurements of vertex
  int gsign;
  double asign;
  vector<deque<double> > Variation;
  function1D<double> Njc;
  double MinTraceRatio;
  deque<pair<int,int> > g_exchange;
private:
  Timer t_trial1, t_trial2, t_trial3, t_accept;
public:
  
  CTQMC(Rand& rand_, ClusterData& cluster_, BathProb& BathP_, int Nmax, const mesh1D& iom_large, const function2D<dcomplex>& Deltaw, const vector<pair<double,double> >& ah, int nom, int nomv, int nomb, int nOm, int Ntau, double  MinTraceRatio, bool IamMaster);
  ~CTQMC();
  
  double sample(long long max_steps); // main function which does the Monte Carlo sampling
 
  void RecomputeCix(); // tried to find better atomic base for the next iteration
  void ComputeFinalAverage(function1D<double>& mom, double& nf, double& TrSigma_G, double& Epot, double& Ekin); // From atomic probabilities computes various zero time quantities
  
  bool SaveStatus(int my_rank);     // saves all kinks to disc for shorter warmup in the next step
  bool RetrieveStatus(int my_rank, long long istep); // retrieves all kinks from disc
  
  const mesh1D& ioms() const { return iom;}
  const mesh1D& iomb() const { return biom;}
  function1D<double>& Histogram() {return histogram;}
  function2D<double>& AverageProbability() {return AProb;}
  function1D<double>& Observables() {return aver_observables;}
  function1D<double>& k_aver() {return kaver;}
  vector<function2D<dcomplex> >& G_aver() { return Gaver;}
  const vector<function2D<double> >& gtau() const { return Gtau;}
  function2D<double>& Susc() {return aver_susc;}
private:
  void Add_One_Kink(long long istep, int ifl);
  void Remove_One_Kink(long long istep, int ifl);
  void Move_A_Kink(long long istep, int ifl);
  void Add_Two_Kinks(long long istep, int ifl);
  void Remove_Two_Kinks(long long istep, int ifl);
  void ComputeAverage(long long istep);
  void CleanUpdate(long long istep);
  void StoreCurrentState(function1D<double>& aver, long long istep);
  void PrintInfo(long long i, const function1D<double>& aver);
  void GlobalFlip(long long istep);
  void GlobalFlipFull(long long istep);
  void Segment_Add_One_Kink(long long istep, int ifl);
  void Segment_Remove_One_Kink(long long istep, int ifl);
  void Segment_Move_A_Kink(long long istep, int ifl);
  void Segment_Exchange_Two_Intervals(long long istep);
public:
  enum AlgorithmType_t {OLD, WERNER, NEW};
  AlgorithmType_t AlgorithmType;
};

template <class Rand>
CTQMC<Rand>::CTQMC(Rand& rand_, ClusterData& cluster_, BathProb& BathP_, int Nmax, const mesh1D& iom_large, const function2D<dcomplex>& Deltaw, const vector<pair<double,double> >& ah, int nom, int nomv_, int nomb, int nOm_, int Ntau, double MinTraceRatio_, bool IamMaster) :
  rand(rand_), cluster(cluster_), BathP(BathP_), intervals(common::N_ifl), Operators(Nmax, cluster),
  npraStates(cluster.nsize), state_evolution_left(cluster.nsize,Nmax), state_evolution_right(cluster.nsize,Nmax),
  MD(common::N_ifl), TMD(common::N_ifl), Delta(cluster.N_unique_fl), Deltat(cluster.N_ifl), Trace(cluster.nsize), detD(common::N_ifl),
  iom(nom), biom(nomb), histogram(Nmax),
  Naver(0), Gaver(cluster.N_ifl), observables(4), aver_observables(4),
  Prob(cluster.nsize,common::max_size), AProb(cluster.nsize,common::max_size),
  kaver(common::N_ifl), nsusc(2), susc(nsusc,nomb), aver_susc(nsusc,nomb), dsusc(nomb,nsusc), dsusc1(nsusc*cluster.nsize, nomb),
  Gtau(common::N_ifl), NGta(0), dexp(Nmax+1,nom), dexp1(Nmax+1,nom), successful(0), 
  dtau(Nmax+1), bexp(Nmax,max(nom,nomb)), nomv(nomv_), nOm(nOm_),
  Mv(cluster.Nvfl),
  NGtv(0), gsign(1), asign(0), successfullC(common::N_ifl), successfullM(common::N_ifl), 
  Faver(common::N_ifl,cluster.fl_fl.size()), Njc(cluster.fl_fl.size()), MinTraceRatio(MinTraceRatio_)
{

  cerr<<"Zatom="<<cluster.Zatom<<" log(Zatom)="<<cluster.Zatom.exp_dbl()<<endl;
  (*xout)<<"log(Zatom)="<<cluster.Zatom.exp_dbl()<<endl;
  // all atomic states are generated
  for (int j=0; j<npraStates.size(); j++) npraStates[j].SetPraState(j,cluster);

  successfullC=0; successfullM=0;
  histogram=0;
  DeltaFourier(Ntau, common::beta, tau, Delta, iom_large, Deltaw, ah);
  
  int ibath=0;
  int ntau=tau.size();
  for (int ifl=0; ifl<cluster.N_ifl; ifl++){
    Deltat[ifl].resize(cluster.ifl_dim[ifl],cluster.ifl_dim[ifl]);
    for (int i1=0; i1<cluster.ifl_dim[ifl]; i1++){
      for (int i2=0; i2<cluster.ifl_dim[ifl]; i2++){
	Deltat[ifl][i1][i2] = new spline1D<double>(Ntau);
	int ib = cluster.tfl_index[ifl][i1][i2];
	int fl = cluster.bfl_index[ifl][ib];

	if (i1==i2){
	  // This is new change! Checks for diagonal Deltas to be causal! changed 18.10.2008
	  // Delta(tau) should be concave. Hence, if it becomes very small at two points, it
	  // should remain zero in all points between the two points. This is very important in
	  // insulators, because Delta(tau) can overshoot to positive values multiple times
	  // and kinks can be trapped in the range between the two crossing points, where Delta(tau)
	  // is causal, but should be zero.
	  int first=tau.size();
	  for (int it=0; it<tau.size(); it++){
	    if (Delta[fl][it]>-common::minDeltat){
	      first=it;
	      break;
	    }
	  }
	  int last=-1;
	  for (int it=tau.size()-1; it>=0; it--){
	    if (Delta[fl][it]>-common::minDeltat){
	      last=it;
	      break;
	    }
	  }
	  for (int it=first; it<=last; it++) Delta[fl][it] = -common::minDeltat; // changed 18.10.2008
	  bool FoundPositive = (first<=last);
	  for (int it=0; it<tau.size(); it++){ // check ones more
	    if (Delta[fl][it]>-common::minDeltat){
	      FoundPositive=true;
	      Delta[fl][it] = -common::minDeltat;
	    }
	  }
	  //if (FoundPositive) cout<<"Found it "<<ifl<<" "<<FoundPositive<<endl;
	  // Here you should probably smoothen
	}

	
	for (int it=0; it<tau.size(); it++){
	  double Dt = Delta[fl][it];
	  if (cluster.conjg[ifl][ib]) Dt = -Delta[fl][ntau-it-1];
	  Dt *= cluster.sign[ifl][ib];
	  (*Deltat[ifl][i1][i2])[it] = Dt;
	}
	double df0 = ((*Deltat[ifl][i1][i2])[1]-(*Deltat[ifl][i1][i2])[0])/(tau[1]-tau[0]);
	int n = tau.size();
	double dfn = ((*Deltat[ifl][i1][i2])[n-1]-(*Deltat[ifl][i1][i2])[n-2])/(tau[n-1]-tau[n-2]);
	(*Deltat[ifl][i1][i2]).splineIt(tau,df0,dfn);
	ibath++;
      }
    }
  }

  if (IamMaster & common::fastFilesystem){
    for (int ifl=0; ifl<cluster.N_ifl; ifl++){
      for (int i1=0; i1<cluster.ifl_dim[ifl]; i1++){
	for (int i2=0; i2<cluster.ifl_dim[ifl]; i2++){
	  ofstream out(NameOfFile_("Delta.tau",ifl,i1*2+i2).c_str());
	  const int Ncheck_points=10000;
	  for (int it=0; it<Ncheck_points; it++){
	    double t = it*common::beta/(Ncheck_points-1.);
	    double Dt = (*Deltat[ifl][i1][i2])(tau.Interp(t));
	    out<<t<<" "<<Dt<<endl;
	  }
	}
      }
    }
  }
  for (int i=0; i<iom.size(); i++) iom[i] = iom_large[i];
  
  for (int i=0; i<nomb; i++) biom[i] = 2*i*M_PI/common::beta;
    
  // propagators are set-up
  for (size_t ifl=0; ifl<intervals.size(); ifl++) intervals[ifl].SetUp(Nmax,iom,biom,common::beta,cluster.ifl_dim[ifl]);
  tinterval.SetUp(Nmax,iom,biom,common::beta,0);
  // matrix M is set up
  for (size_t ifl=0; ifl<MD.size(); ifl++) MD[ifl].resize(Nmax,Nmax);
  for (size_t ifl=0; ifl<TMD.size(); ifl++) TMD[ifl].SetUp(Nmax, tau, Deltat[ifl], cluster.tfl_index[ifl], iom, common::beta, cluster.v2fl_index[ifl], nomv);

  // set-up local part of the system

  StartStateEvolution(state_evolution_left, state_evolution_right, npraStates, Operators, cluster, Trace, Prob);
  
  // approximation for Glocal
  for (int ifl=0; ifl<cluster.N_ifl; ifl++){
    Gaver[ifl].resize(cluster.N_baths(ifl),iom.size());
    Gaver[ifl] = 0;
  }

  if (common::QHB2){
    for (int ifl=0; ifl<cluster.N_ifl; ifl++){
      for (int j=0; j<cluster.fl_fl.size(); j++){
	Faver(ifl,j).resize(iom.size());
	Faver(ifl,j) = 0.0;
      }
    }
  }

  for (size_t ifl=0; ifl<detD.size(); ifl++) detD[ifl] = 1;
  
  for (int j=0; j<cluster.nsize; j++){
    Prob[j].resize(cluster.msize(j+1));
    AProb[j].resize(cluster.msize(j+1));
  }

  if (common::SampleGtau>0)
    for (size_t ifl=0; ifl<Gtau.size(); ifl++){
      Gtau[ifl].resize(sqr(cluster.ifl_dim[ifl]),Ntau);
      Gtau[ifl]=0;
    }
  
  kaver = 0;
  aver_susc = 0;

  if (common::cmp_vertex){

    VertexH.resize(cluster.Nvfl,cluster.Nvfl,2*nOm-1,2*nomv,2*nomv);
    VertexF.resize(cluster.Nvfl,cluster.Nvfl,2*nOm-1,2*nomv,2*nomv);
    
    for (unsigned ifl=0; ifl<Mv.size(); ifl++) Mv[ifl].resize(2*nomv,2*nomv);//(-nomv,nomv,-nomv,nomv);
  }

  // Segment : 1 , Algorithm=NEW,    flips only global flip
  //           2 , Algorithm=NEW,    flips all equivalent baths from cix file
  //           3 , Algorithm=WERNER, flips only global flip
  //           4 , Algorithm=WERNER, flips all equivalent baths from cix file
  //           5 , Algorithm=OLD,    flips only global flip
  //           6 , Algorithm=OLD,    flips all equivalent baths from cix file
  if ((common::Segment+1)/2 == 1)
    AlgorithmType = NEW;
  else if ((common::Segment+1)/2 == 2)
    AlgorithmType = WERNER;
  else if ((common::Segment+1)/2 == 3)
    AlgorithmType = OLD;
  
  for (int iu=0; iu<cluster.gflip_ifl.size(); iu++){
    int ifa = cluster.gflip_ifl[iu].first;
    int ifb = cluster.gflip_ifl[iu].second;
    int dim=cluster.ifl_dim[ifa];
    for (int b1=0; b1<dim; b1++){
      if (common::Segment % 2 == 1){
	int fla = cluster.fl_from_ifl[ifa][b1];
	int flb = cluster.fl_from_ifl[ifb][b1];
	g_exchange.push_back(make_pair(fla,flb));
	g_exchange.push_back(make_pair(flb,fla));
      }else{
	for (int b2=0; b2<dim; b2++){
	  if (cluster.bfl_index[ifa][b1+dim*b1]==cluster.bfl_index[ifb][b2+dim*b2]){
	    int fla = cluster.fl_from_ifl[ifa][b1];
	    int flb = cluster.fl_from_ifl[ifb][b2];
	    g_exchange.push_back(make_pair(fla,flb));
	    g_exchange.push_back(make_pair(flb,fla));
	  }
	}
      }
    }
  }
  
  if (common::SampleTransitionP){
    P_transition.resize(cluster.nsize,2*cluster.N_flavors);
    AP_transition.resize(cluster.nsize,2*cluster.N_flavors);
    AP_transition = 0.0;
  }
}

template <class Rand>
CTQMC<Rand>::~CTQMC()
{
  for (size_t ifl=0; ifl<Deltat.size(); ifl++)
    for (int i1=0; i1<Deltat[ifl].size_N(); i1++)
      for (int i2=0; i2<Deltat[ifl].size_Nd(); i2++)
	delete Deltat[ifl][i1][i2];
}

template <class Rand>
void CTQMC<Rand>::Add_One_Kink(long long istep, int ifl)
{
  if (Operators.full()) return;
  int dim = cluster.ifl_dim[ifl];
  int kn = intervals[ifl].size()/2;
  int bfls = static_cast<int>(dim*rand());
  int bfle = static_cast<int>(dim*rand());
  int fls = cluster.fl_from_ifl[ifl][bfls];
  int fle = cluster.fl_from_ifl[ifl][bfle];
	  
  double t_start = common::beta*rand();
  double t_end = common::beta*rand();

  t_trial1.start();
  bool ssc1;
  //if (common::Segment)
  //  ssc1 = Segment_Try_Add_Cd_C_(bfls, t_start, bfle, t_end, intervals[ifl], istep);
  //else
  ssc1 = Operators.Try_Add_Cd_C_(fls, t_start, fle, t_end, state_evolution_left, state_evolution_right);
  t_trial1.stop();

  if (!ssc1 || t_start==t_end) return;
  
  t_trial2.start();
  pair<int,int> opt = Operators.Try_Add_Cd_C(fls, t_start, fle, t_end);
  int op_i = min(opt.first, opt.second), op_f = max(opt.first, opt.second);
  /*
  // WARNING: THIS SHOULD BE REMOVED !!!! ????
  if (!common::Segment){
    Number matrix_element_new = ComputeTrace(state_evolution_left, state_evolution_right, op_i, op_f, Operators, npraStates, cluster, 2);
    if (matrix_element_new.mantisa==0) return;
  }
  */
  double ratioD = TMD[ifl].AddDetRatio(MD[ifl], kn, t_start, bfls, t_end, bfle, intervals[ifl]);
  t_trial2.stop();
  
  // First flip the coint, and than see if we accept the step.
  double Prand = 1-rand();    // 1-rand() because don't ever want to accept move with zero probability
  double P_part = sqr(common::beta*dim)/sqr(kn+1.)*fabs(ratioD); // all of acceptance prob. except trace ratio
  double P_r = Prand/P_part;

  t_trial3.start();
  double ms;
  bool Accept;
  //if (common::Segment){
  //  double exp_sum = Segment_ComputeTryalExponentSum(state_evolution_left,op_i,Operators, npraStates, cluster, 2,istep);
  //  ms = fabs(divide( Number(1.0, exp_sum), matrix_element));
  //  Accept = P_r<ms;
  //}else{
  if (common::LazyTrace){
    list< pair<int, double> > exp_sums;
    ComputeExpTrace(exp_sums, state_evolution_left, state_evolution_right, op_i, op_f, Operators, npraStates, cluster, 2);
    Accept = LazyComputeTrace(P_r, matrix_element, ms, exp_sums, state_evolution_left, state_evolution_right, op_i, op_f, Operators, npraStates, cluster, 2);
  }else{
    Number matrix_element_new = ComputeTrace(state_evolution_left, state_evolution_right, op_i, op_f, Operators, npraStates, cluster, 2);
    ms = fabs(divide(matrix_element_new, matrix_element));
    Accept = P_r<ms;
  }
    //}
  t_trial3.stop();
  
  if (ms>common::minM && Accept){
    t_accept.start();
    int is, ie;
    intervals[ifl].Find_is_ie(t_start, is, t_end, ie); // SIGN: HERE WE HAVE is,ie
    ratioD = TMD[ifl].AddUpdateMatrix(MD[ifl], kn, t_start, is, bfls, t_end, ie, bfle, intervals[ifl],istep);
    pair<int,int> ipl = intervals[ifl].InsertExponents(t_start, is, bfls, t_end, ie, bfle);
    TMD[ifl].AddUpdate_Gf(intervals[ifl], Mv,istep);
    
    pair<int,int> opt = Operators.Add_Cd_C(fls, t_start, fle, t_end,   IntervalIndex(ifl, nIntervals::cd, ipl.first), IntervalIndex(ifl, nIntervals::c, ipl.second));
    int op_i_ = min(opt.first, opt.second), op_f_ = max(opt.first, opt.second); // Changed March 2013

    // And updates state evolution for fast computation of Trace
    Number new_matrix_element;
    //if (common::Segment)
    //  new_matrix_element = Segment_UpdateStateEvolution(state_evolution_left,op_i,Operators,npraStates, cluster, Trace, Prob, istep);
    //else
    new_matrix_element = UpdateStateEvolution(state_evolution_left, state_evolution_right, op_i_, op_f_, Operators, npraStates, cluster, Trace, Prob, istep); // Changed March 2013
    
    // Operators.sign() :  gives number of crossings + number of forward propagators (sign for the matrix element to bring it to original form)
    // sign_isie: gives permutation of columns and rows of the determinant to bring it to original form
    //
    int sign_isie = 1-2*((is+ie)%2);
    int from_orig = Operators.sign()*sign_isie;
    double ms1 = divide(new_matrix_element, matrix_element);
    double Ps = ratioD*ms1*from_orig;
    gsign *= sigP<0>(Ps);
    detD[ifl] *= ratioD;
    matrix_element = new_matrix_element;
    successful++;
    successfullC[ifl]++;
    ComputeAverage(istep);
    t_accept.stop();
  }
}


template <class Rand>
void CTQMC<Rand>::Remove_One_Kink(long long istep, int ifl)
{
  int kn = intervals[ifl].size()/2;
  if (kn==0) return;
  int dim = cluster.ifl_dim[ifl];
  int ie = static_cast<int>(kn*rand());
  int is = static_cast<int>(kn*rand());
  int bfle = intervals[ifl].btype_e(ie);
  int bfls = intervals[ifl].btype_s(is);
  int iop_s = intervals[ifl].FindSuccessive(0, is, bfls);
  int iop_e = intervals[ifl].FindSuccessive(1, ie, bfle);
  int fle = cluster.fl_from_ifl[ifl][bfle];
  int fls = cluster.fl_from_ifl[ifl][bfls];
  int ipe, ips;

  t_trial1.start();
  bool ssc1;
  //if (common::Segment){
  //  ssc1 = Segment_Try_Remove_C_Cd_(bfle, ie, bfls, is, intervals[ifl], istep);
  //  ipe = Operators.FindSuccessive(2*fle+1, iop_e);
  //  ips = Operators.FindSuccessive(2*fls,   iop_s);
  //  if (ips>ipe) ips--;
  //}else{
  ssc1 = Operators.Try_Remove_C_Cd_(fle, iop_e, fls, iop_s, ipe, ips, state_evolution_left, state_evolution_right);
  //}
  t_trial1.stop();

  if (!ssc1) return;

  t_trial2.start();    
  int op_i = min(ipe, ips);
  int op_f = ipe>ips ? ipe-2 : ips-1;
  Operators.Try_Remove_C_Cd(ipe, ips);
  double ratioD = TMD[ifl].RemoveDetRatio(MD[ifl], kn, is, ie);
  t_trial2.stop();

  // First flip the coint, and than see if we accept the step.
  double Prand = 1-rand();  // 1-rand() because don't ever want to accept move with zero probability
  double P_part = fabs(ratioD)*sqr(kn)/sqr(common::beta*dim);
  double P_r = Prand/P_part;

  t_trial3.start();
  double ms;
  bool Accept;
  //if (common::Segment){
  //  double exp_sum = Segment_ComputeTryalExponentSum(state_evolution_left,op_i,Operators, npraStates, cluster, -2,istep);
  //  ms = fabs(divide( Number(1.0, exp_sum), matrix_element));
  //  Accept = P_r<ms;
  //}else{
  if (common::LazyTrace){
    list< pair<int, double> > exp_sums;
    ComputeExpTrace(exp_sums, state_evolution_left, state_evolution_right, op_i, op_f, Operators, npraStates, cluster, -2);
    Accept = LazyComputeTrace(P_r, matrix_element, ms, exp_sums, state_evolution_left, state_evolution_right, op_i, op_f, Operators, npraStates, cluster, -2);
  }else{
    Number matrix_element_new = ComputeTrace(state_evolution_left, state_evolution_right, op_i, op_f, Operators, npraStates, cluster, -2);
    ms = fabs(divide(matrix_element_new,matrix_element));
    Accept = P_r<fabs(ms);
  }	
  //}
  t_trial3.stop();
  
  if (ms>common::minM && Accept){
    t_accept.start();
    Operators.Remove_C_Cd(ipe, ips);
    // And updates state evolution for fast computation of Trace
    Number new_matrix_element;
    //if (common::Segment)
    //  new_matrix_element = Segment_UpdateStateEvolution(state_evolution_left, op_i, Operators,npraStates, cluster, Trace, Prob, istep);
    //else
    new_matrix_element = UpdateStateEvolution(state_evolution_left, state_evolution_right, op_i, op_f, Operators, npraStates, cluster, Trace, Prob, istep);
    
    TMD[ifl].RemoveUpdate_Gf(MD[ifl], kn, is, ie, intervals[ifl], Mv);
    int sign_isie = 1-2*((is+ie)%2);
    int from_orig = Operators.sign()*sign_isie;
    double ms1 = divide(new_matrix_element, matrix_element);
    double Ps = ratioD*ms1*from_orig;
    gsign *=sigP<0>(Ps);
    double ratioDn = TMD[ifl].RemoveUpdateMatrix(MD[ifl], kn, is, ie, intervals[ifl]);
    intervals[ifl].RemoveExponents(is,ie);
    detD[ifl] *= ratioDn;
    matrix_element = new_matrix_element;
    successful++;
    successfullC[ifl]++;
    ComputeAverage(istep);
    t_accept.stop();
  }
}

template <class Rand>
void CTQMC<Rand>::Move_A_Kink(long long istep, int ifl)
{
  int kn = intervals[ifl].size()/2;
  if (kn==0) return;
  int dim = cluster.ifl_dim[ifl];
  int type = static_cast<int>(rand()*2);
  int to_move = static_cast<int>(kn*rand());
  int bfl = intervals[ifl].btype(type, to_move);
  int fl = cluster.fl_from_ifl[ifl][bfl];
  int opera = 2*fl+type;
  int to_move_o = intervals[ifl].FindSuccessive(type, to_move, bfl);

  double t_old = intervals[ifl].time(type,to_move);
  double t_prev = intervals[ifl].PreviousTime(type,to_move);
  double t_next = intervals[ifl].NextTime(type, to_move);

  double t_new;
  if (t_prev<t_old && t_next>t_old)
    t_new = t_prev + (t_next-t_prev)*rand();
  else if (t_prev>t_old)
    t_new = t_prev-common::beta + (t_next-t_prev+common::beta)*rand(); //t_prev = 0;//-= common::beta; SHOULD BE CORRECTED
  else if (t_next<t_old)
    t_new = t_prev + (t_next+common::beta-t_prev)*rand();//common::beta;//+= common::beta; SHOULD BE CORRECTED
  else if (fabs(t_next-t_prev)<1e-10){
    t_new = common::beta*rand();
  } else{ // have only one time
    cerr<<"DID NOT EXPECT TO HAPPEN"<<endl;
    cout<<"t_old="<<t_old<<" t_next="<<t_next<<" t_prev="<<t_prev<<endl;
    t_new = t_old;
  }

  int iws=0;
  while (t_new<0) {t_new += common::beta; iws++;}
  while (t_new>common::beta) {t_new -= common::beta; iws++;}

  t_trial1.start();
  bool ssc1;
  //if (common::Segment)
  //  ssc1 = Segment_Try_Move_(bfl, type, to_move, t_new, t_old, t_prev, t_next, intervals[ifl], istep);
  //else
  ssc1 = Operators.Try_Move_(opera, t_old, to_move_o, t_new, state_evolution_left, state_evolution_right);
  t_trial1.stop();
  
  if (!ssc1) return;
  
  t_trial2.start();    
  double ratioD;
  if (type==0) ratioD = TMD[ifl].Move_start_DetRatio(MD[ifl], kn, t_old, t_new, bfl, to_move, intervals[ifl]);
  else ratioD = TMD[ifl].Move_end_DetRatio(MD[ifl], kn, t_old, t_new, bfl, to_move, intervals[ifl]);
  int ip_old, ipo, ip_new;
  Operators.TryMove(opera, t_old, to_move_o, ip_old, ipo, t_new, ip_new);
  int op_i = min(ipo, ip_new), op_f = max(ipo, ip_new);
  t_trial2.stop();    

  double Prand = 1-rand();
  double P_part = fabs(ratioD);
  double P_r = Prand/P_part;

  t_trial3.start();
  double ms;
  bool Accept;
  //if (common::Segment){
  //  double exp_sum = Segment_ComputeTryalExponentSum(state_evolution_left,op_i,Operators, npraStates, cluster, 0,istep);
  //  ms = fabs(divide( Number(1.0, exp_sum), matrix_element));
  //  Accept = P_r<ms;
  //}else{
  if (common::LazyTrace){
    list< pair<int, double> > exp_sums;
    ComputeExpTrace(exp_sums, state_evolution_left, state_evolution_right, op_i, op_f, Operators, npraStates, cluster, 0);
    Accept = LazyComputeTrace(P_r, matrix_element, ms, exp_sums, state_evolution_left, state_evolution_right, op_i, op_f, Operators, npraStates, cluster, 0);
  }else{
    Number matrix_element_new = ComputeTrace(state_evolution_left, state_evolution_right, op_i, op_f, Operators, npraStates, cluster, 0);
    ms = fabs(divide(matrix_element_new, matrix_element));
    Accept = P_r<ms;
  }
  //}
  t_trial3.stop();
  
  if (ms>common::minM && Accept){
    t_accept.start();
    Operators.Move(ip_old, ip_new);

    Number new_matrix_element;
    //if (common::Segment)
    //  new_matrix_element = Segment_UpdateStateEvolution(state_evolution_left, op_i, Operators,npraStates, cluster, Trace, Prob, istep);
    //else
    new_matrix_element = UpdateStateEvolution(state_evolution_left, state_evolution_right, op_i, op_f, Operators, npraStates, cluster, Trace, Prob, istep);
    
    int i_new = intervals[ifl].FindIndex(t_new, t_old, type);
    if (type==0) TMD[ifl].Move_start_UpdateMatrix(MD[ifl], kn, t_old, t_new, bfl, to_move, i_new, intervals[ifl]);
    else TMD[ifl].Move_end_UpdateMatrix(MD[ifl], kn, t_old, t_new, bfl, to_move, i_new, intervals[ifl]);
    intervals[ifl].MoveExponents(type, t_old, to_move, t_new, i_new);
    TMD[ifl].MoveUpdate_Gf(intervals[ifl], Mv, istep);
    ratioD *= 1-2*((i_new+to_move+iws)%2);
    int sign_isie = 1-2*((i_new+to_move+iws)%2);
    int from_orig = Operators.sign()*sign_isie;
    double ms1 = divide(new_matrix_element, matrix_element);
    double Ps = ratioD*ms1*from_orig;
    gsign *= sigP<0>(Ps);
    detD[ifl] *= ratioD;
    matrix_element = new_matrix_element;
    successful++;
    successfullM[ifl]++;
    ComputeAverage(istep);
    t_accept.stop();
  }
  
}


template <class Rand>
void CTQMC<Rand>::Segment_Add_One_Kink(long long istep, int ifl)
{
  if (Operators.full()) return;
  int dim = cluster.ifl_dim[ifl];           // dimensionality of this bath
  int kn = intervals[ifl].size()/2;         // total number of kinks/2 in the multidimensional bath
  int bfl  = static_cast<int>(dim*rand());  // type of creation/anhilation operators we attempt to add
  int knb = intervals[ifl].Nbtyp_s[bfl];    // how many operators of this type are there?
  int flse = cluster.fl_from_ifl[ifl][bfl]; // combined index for bath (ifl,bfl)
  double t_start, t_end;
  double P_to_add;

  if (AlgorithmType==OLD){
    t_start = common::beta*rand();
    t_end = common::beta*rand();
    if (!Segment_Try_Add_Cd_C_(bfl, t_start, bfl, t_end, intervals[ifl], istep)) return;
    P_to_add = sqr(common::beta/(knb+1.));
  }else if(AlgorithmType==WERNER) {
    double seg_length;
    double r = rand();
    if (r==0 || r==1) return; // We do now want two times to be exactly the same. The probability should vanish anyway.
    if (rand()<0.5){   // Adding a segment
      t_start = common::beta*rand();     // new creation operator
      double next_t_start, next_t_end, previous_t_start, previous_t_end;
      bool onseg = intervals[ifl].CheckOnSegmentHere(bfl,t_start,next_t_start,next_t_end,previous_t_start,previous_t_end,istep); // On segment, we can not add segment
      if (onseg || t_start==next_t_end) return; // On segment, we can not add segment
      if (t_start==previous_t_end) return; // The new and the old time are exactly the same
      seg_length = next_t_start-t_start;
      t_end = t_start + seg_length*r;
      if (t_end>common::beta) t_end -= common::beta;
    }else{             // Adding antisegment
      t_end = common::beta*rand();       // new anhilation operator
      double next_t_start, next_t_end, previous_t_start, previous_t_end;
      bool onseg = intervals[ifl].CheckOnSegmentHere(bfl,t_end,next_t_start,next_t_end,previous_t_start,previous_t_end,istep); // Can add only on segment
      if (!onseg) return; // Can add anti-segment only on segment
      seg_length = next_t_end-t_end;
      t_start = t_end + seg_length*r;
      if (t_start>common::beta) t_start -= common::beta;
    }
    P_to_add = (common::beta*seg_length)/(knb+1.);
  }else if(AlgorithmType==NEW){
    if (knb==0){
      t_start = common::beta*rand();         // new random time in the existsing interval
      t_end = common::beta*rand();         // new random time in the existsing interval
      P_to_add = sqr(common::beta);
    }else{
      double r1=rand();
      double r2=rand();
      if (r1==0 || r1==1 || r2==0 || r2==1) return;   //  We do now want two times to be exactly the same. The probability should vanish anyway.
      int type = static_cast<int>(rand()*2);          // either c or c^+
      int iop_1 = static_cast<int>(knb*rand());       // which operator from ordered list
      int i1 = iop_1;                                 // index in the multidimensional bath might be different in general
      if (dim>1) i1 = intervals[ifl].FindWhichOneIs(type, bfl, iop_1); // this is the index in multidimensional interval
      double t1 = intervals[ifl].time(type,i1);       // the beginning of the interval
      pair<int,int> i_iop = intervals[ifl].NextTimeIndex_(1-type, t1, bfl); // index of the next operator of the opposite type
      int i2 = i_iop.first;                           // index in the multidimensional bath index
      int iop_2 = i_iop.second;                       // index when counting only this bfl type
      double t2 = intervals[ifl].time(1-type,i2);     // the end of the interval
      double seg_length = t2-t1;                      // length of the segment
      if (seg_length<0) seg_length += common::beta;
      double t1_new = t1 + seg_length*r1;             // new random time in the existsing interval
      double t2_new = t1 + seg_length*r2;             // new random time in the existsing interval
      if (type==nIntervals::c){
	t_start = min(t1_new,t2_new);
	t_end = max(t1_new,t2_new);
      }else{
	t_end = min(t1_new,t2_new);
	t_start = max(t1_new,t2_new);
      }
      if (t_start>common::beta) t_start -= common::beta;
      if (t_end>common::beta) t_end -= common::beta;
      P_to_add = (sqr(seg_length)*knb)/(knb+1.)/2.;
    }
  }
  if (t_start==t_end || Operators.FindExactTime(t_start)>=0) return; // There is already exactly the same time in the list
  
  t_trial2.start();
  pair<int,int> opt = Operators.Try_Add_Cd_C(flse, t_start, flse, t_end); 
  int op_i = min(opt.first, opt.second), op_f = max(opt.first, opt.second);
  double ratioD = TMD[ifl].AddDetRatio(MD[ifl], kn, t_start, bfl, t_end, bfl, intervals[ifl]);
  t_trial2.stop();

  t_trial3.start();
  double exp_sum = Segment_ComputeTryalExponentSum(state_evolution_left,op_i,Operators, npraStates, cluster, 2,istep);
  double ms = fabs(divide( Number(1.0, exp_sum), matrix_element));
  bool Accept = (1-rand()) < ms*fabs(ratioD)*P_to_add;
  //double det = fabs(detD[ifl]*ratioD);
  double det = fabs(ratioD);
  t_trial3.stop();

  if (Accept && ms>common::minM && det>common::minD){
    t_accept.start();
    int is, ie;
    intervals[ifl].Find_is_ie(t_start, is, t_end, ie); // SIGN: HERE WE HAVE is,ie
    ratioD = TMD[ifl].AddUpdateMatrix(MD[ifl], kn, t_start, is, bfl, t_end, ie, bfl, intervals[ifl],istep);
    pair<int,int> ipl = intervals[ifl].InsertExponents(t_start, is, bfl, t_end, ie, bfl);
    TMD[ifl].AddUpdate_Gf(intervals[ifl], Mv,istep);
    
    pair<int,int> opt = Operators.Add_Cd_C(flse, t_start, flse, t_end,   IntervalIndex(ifl, nIntervals::cd, ipl.first), IntervalIndex(ifl, nIntervals::c, ipl.second));
    int op_i_ = min(opt.first, opt.second), op_f_ = max(opt.first, opt.second); // Changed March 2013
    // And updates state evolution for fast computation of Trace
    Number new_matrix_element = Segment_UpdateStateEvolution(state_evolution_left,op_i,Operators,npraStates, cluster, Trace, Prob, istep);
    // Operators.sign() :  gives number of crossings + number of forward propagators (sign for the matrix element to bring it to original form)
    // sign_isie: gives permutation of columns and rows of the determinant to bring it to original form
    //
    int sign_isie = 1-2*((is+ie)%2);
    int from_orig = Operators.sign()*sign_isie;
    double ms1 = divide(new_matrix_element, matrix_element);
    double Ps = ratioD*ms1*from_orig;
    gsign *= sigP<0>(Ps);
    detD[ifl] *= ratioD;
    matrix_element = new_matrix_element;
    successful++;
    successfullC[ifl]++;
    ComputeAverage(istep);
    t_accept.stop();
  }
}
template <class Rand>
void CTQMC<Rand>::Segment_Remove_One_Kink(long long istep, int ifl)
{
  int kn = intervals[ifl].size()/2;
  if (kn==0) return;
  int dim = cluster.ifl_dim[ifl];           // dimension of this bath
  int bfl = static_cast<int>(dim*rand());   // type of the multidimensional bath we want to remove
  int knb = intervals[ifl].Nbtyp_s[bfl];    // how many segments of this type are there?
  if (knb==0) return;
  int flse = cluster.fl_from_ifl[ifl][bfl];
  int is, ie, iop_s, iop_e;
  double P_to_remove;

  if (AlgorithmType==OLD){
    P_to_remove = sqr(knb/common::beta);
    iop_s = static_cast<int>(knb*rand());  // removing this creation operator
    iop_e = static_cast<int>(knb*rand());  // removing this anhilation operator
    is = iop_s;                            // index in the multidimensional bath might be different in general
    if (dim>1) is = intervals[ifl].FindWhichOneIs(nIntervals::cd, bfl, iop_s); // this is the index in multidimensional interval
    ie = iop_e;
    if (dim>1) ie = intervals[ifl].FindWhichOneIs(nIntervals::c, bfl, iop_e); // this is the index in multidimensional interval
    if (!Segment_Try_Remove_C_Cd_(bfl, ie, bfl, is, intervals[ifl], istep)) return;
  }else if (AlgorithmType==WERNER){
    double t_start, t_end;
    double seg_length;
    if (rand()<0.5){   // Removing a segment
      iop_s = static_cast<int>(knb*rand());  // removing this creation operator
      is = iop_s;                            // index in the multidimensional bath might be different in general
      if (dim>1) is = intervals[ifl].FindWhichOneIs(nIntervals::cd, bfl, iop_s); // this is the index in multidimensional interval
      t_start = intervals[ifl].time_s(is);
      pair<int,int> i_iop = intervals[ifl].NextTimeIndex_(nIntervals::c, t_start, bfl); // next anhilation operator
      iop_e = i_iop.second;
      ie    = i_iop.first;
      double t_start_next = intervals[ifl].NextTime(nIntervals::cd, is, true);
      seg_length = t_start_next-t_start;
    }else{             // Removing an antisegment
      if (intervals[ifl].Nbtyp_s[bfl]<2) return; // Can not remove an anti-kink if there is only single kink
      iop_e = static_cast<int>(knb*rand());  // removing this destruction operator
      ie = iop_e;                            // index in the multidimensional bath might be different in general
      if (dim>1) ie = intervals[ifl].FindWhichOneIs(nIntervals::c, bfl, iop_e); // this is the index in multidimensional interval
      t_end = intervals[ifl].time_e(ie);
      pair<int,int> i_iop = intervals[ifl].NextTimeIndex_(nIntervals::cd, t_end, bfl); // next creation operator
      is    = i_iop.first;
      iop_s = i_iop.second;
      double t_end_next = intervals[ifl].NextTime(nIntervals::c, ie, true);
      seg_length = t_end_next-t_end;
    }
    P_to_remove = knb/(common::beta*seg_length);
  }else if (AlgorithmType==NEW){
    if (knb==1){
      P_to_remove = 1./sqr(common::beta);
      iop_s=0;
      is = iop_s;                            // index in the multidimensional bath might be different in general
      if (dim>1) is = intervals[ifl].FindWhichOneIs(nIntervals::cd, bfl, iop_s); // this is the index in multidimensional interval
      iop_e=0;
      ie = iop_e;
      if (dim>1) ie = intervals[ifl].FindWhichOneIs(nIntervals::c, bfl, iop_e); // this is the index in multidimensional interval
    }else{
      int type = static_cast<int>(rand()*2);          // either c or c^+
      int iop_1 = static_cast<int>(knb*rand());       // which operator from ordered list
      int i1 = iop_1;                                 // index in the multidimensional bath might be different in general
      if (dim>1) i1 = intervals[ifl].FindWhichOneIs(type, bfl, iop_1); // this is the index in multidimensional interval
      double t1 = intervals[ifl].time(type,i1);       // beginning of the interval
      pair<int,int> i_iop = intervals[ifl].NextTimeIndex_(1-type, t1, bfl); // index of the next operator of the opposite type
      int iop_2 = i_iop.second;                       // index when counting only this bfl type
      int i2 = i_iop.first;                           // index in the multidimensional bath index
      double t2 = intervals[ifl].time(1-type,i2);       // beginning of the interval
      double t1_previous = intervals[ifl].PreviousTime(1-type, i2, true);
      double t2_next = intervals[ifl].NextTime(type, i1, true);
      double seg_length = t2_next-t1_previous;         // length of the interval
      if (t2<t1) seg_length -= common::beta;           // jumped around twice when calculating t1_previous and t2_next
      if (type==nIntervals::cd){
	is=i1;  // if we started with cd, is=i1
	iop_s=iop_1;
	ie=i2;
	iop_e=iop_2;
      }else{
	ie=i1;
	iop_e=iop_1;
	is=i2;
	iop_s=iop_2;
      }
      P_to_remove = 2.*knb/(knb-1.)/sqr(seg_length);
    }
  }
  int ipe, ips;
  t_trial2.start();    
  ipe = Operators.FindSuccessive(2*flse+1, iop_e);
  ips = Operators.FindSuccessive(2*flse,   iop_s);
  if (ips>ipe) ips--;
  int op_i = min(ipe, ips);
  int op_f = ipe>ips ? ipe-2 : ips-1;
  Operators.Try_Remove_C_Cd(ipe, ips);
  double ratioD = TMD[ifl].RemoveDetRatio(MD[ifl], kn, is, ie);
  t_trial2.stop();

  t_trial3.start();
  double exp_sum = Segment_ComputeTryalExponentSum(state_evolution_left,op_i,Operators, npraStates, cluster, -2,istep);
  double ms = fabs(divide( Number(1.0, exp_sum), matrix_element));
  bool Accept = 1-rand() < ms*fabs(ratioD)*P_to_remove;
  //double det = fabs(detD[ifl]*ratioD);
  double det = fabs(ratioD);
  t_trial3.stop();

  if (Accept && ms>common::minM && det>common::minD){
    t_accept.start();
    Operators.Remove_C_Cd(ipe, ips);
    // And updates state evolution for fast computation of Trace
    Number new_matrix_element = Segment_UpdateStateEvolution(state_evolution_left, op_i, Operators,npraStates, cluster, Trace, Prob, istep);
    TMD[ifl].RemoveUpdate_Gf(MD[ifl], kn, is, ie, intervals[ifl], Mv);
    int sign_isie = 1-2*((is+ie)%2);
    int from_orig = Operators.sign()*sign_isie;
    double ms1 = divide(new_matrix_element, matrix_element);
    double Ps = ratioD*ms1*from_orig;
    gsign *=sigP<0>(Ps);
    double ratioDn = TMD[ifl].RemoveUpdateMatrix(MD[ifl], kn, is, ie, intervals[ifl]);
    intervals[ifl].RemoveExponents(is,ie);
    detD[ifl] *= ratioDn;
    matrix_element = new_matrix_element;
    successful++;
    successfullC[ifl]++;
    ComputeAverage(istep);
    t_accept.stop();
  }
}

template <class Rand>
void CTQMC<Rand>::Segment_Move_A_Kink(long long istep, int ifl)
{
  int kn = intervals[ifl].size()/2;
  if (kn==0) return;
  int dim = cluster.ifl_dim[ifl];
  int type = static_cast<int>(rand()*2);
  int to_move = static_cast<int>(kn*rand());
  int bfl = intervals[ifl].btype(type, to_move);
  int fl = cluster.fl_from_ifl[ifl][bfl];
  int opera = 2*fl+type;
  int to_move_o = intervals[ifl].FindSuccessive(type, to_move, bfl);

  double t_old = intervals[ifl].time(type,to_move);
  pair<double,double> t_pn = intervals[ifl].Closest_Times(1-type, bfl, t_old);
  
  double t_new = t_pn.first + (t_pn.second-t_pn.first)*rand();
  if (t_new==t_pn.first || t_new==t_pn.second) return; // We just do not want to have two times exactly equal
  int iws=0;
  if (t_new<0) {t_new += common::beta; iws=1;}
  if (t_new>common::beta) {t_new -= common::beta; iws=1;}
  
  t_trial2.start();    
  double ratioD;
  if (type==0) ratioD = TMD[ifl].Move_start_DetRatio(MD[ifl], kn, t_old, t_new, bfl, to_move, intervals[ifl]);
  else ratioD = TMD[ifl].Move_end_DetRatio(MD[ifl], kn, t_old, t_new, bfl, to_move, intervals[ifl]);
  int ip_old, ipo, ip_new;
  Operators.TryMove(opera, t_old, to_move_o, ip_old, ipo, t_new, ip_new);
  int op_i = min(ipo, ip_new), op_f = max(ipo, ip_new);
  t_trial2.stop();    

  t_trial3.start();
  double P_part = fabs(ratioD);
  double exp_sum = Segment_ComputeTryalExponentSum(state_evolution_left,op_i,Operators, npraStates, cluster, 0,istep);
  double ms = fabs(divide( Number(1.0, exp_sum), matrix_element));
  bool Accept = 1-rand() < ms*P_part;
  //double det = fabs(detD[ifl]*ratioD);
  double det = fabs(ratioD);
  t_trial3.stop();
  
  if (Accept && ms>common::minM && det>common::minD){
    t_accept.start();
    Operators.Move(ip_old, ip_new);
    Number new_matrix_element = Segment_UpdateStateEvolution(state_evolution_left, op_i, Operators,npraStates, cluster, Trace, Prob, istep);
    int i_new = intervals[ifl].FindIndex(t_new, t_old, type);
    if (type==0) TMD[ifl].Move_start_UpdateMatrix(MD[ifl], kn, t_old, t_new, bfl, to_move, i_new, intervals[ifl]);
    else TMD[ifl].Move_end_UpdateMatrix(MD[ifl], kn, t_old, t_new, bfl, to_move, i_new, intervals[ifl]);
    intervals[ifl].MoveExponents(type, t_old, to_move, t_new, i_new);
    TMD[ifl].MoveUpdate_Gf(intervals[ifl], Mv, istep);
    ratioD *= 1-2*((i_new+to_move+iws)%2);
    
    int sign_isie = 1-2*((i_new+to_move+iws)%2);
    int from_orig = Operators.sign()*sign_isie;
    double ms1 = divide(new_matrix_element, matrix_element);
    double Ps = ratioD*ms1*from_orig;
    gsign *= sigP<0>(Ps);
    
    detD[ifl] *= ratioD;
    matrix_element = new_matrix_element;
    successful++;
    successfullM[ifl]++;
    ComputeAverage(istep);
    t_accept.stop();
  }
}

template <class Rand>
void CTQMC<Rand>::Segment_Exchange_Two_Intervals(long long istep)
{
  int ir = static_cast<int>(rand()*g_exchange.size());
  int fla = g_exchange[ir].first;
  int flb = g_exchange[ir].second;
  int ifa  = cluster.ifl_from_fl[fla];
  int bfla = cluster.bfl_from_fl[fla];
  int ifb  = cluster.ifl_from_fl[flb];
  int bflb = cluster.bfl_from_fl[flb];
  int kna = intervals[ifa].size()/2;
  int knb = intervals[ifb].size()/2;
  if (kna==0 || knb==0) return;
  int ka = intervals[ifa].Nbtyp_s[bfla];    // how many segments of first type are there?
  int kb = intervals[ifb].Nbtyp_s[bflb];    // how many segments of second type are there?
  if (ka==0 || kb==0) return;               // no segments, can't exchange...
  int dim = cluster.ifl_dim[ifa];           // dimension of this bath
  int typea = static_cast<int>(rand()*2);   // either c or c+ for first segment
  int typeb = static_cast<int>(rand()*2);   // either c or c+ for second segment
  //int typeb = 1-typea;
  //int typeb=typea;
  int iop_a = static_cast<int>(ka*rand());       // which operator from ordered list
  int ia = iop_a;                                 // index in the multidimensional bath might be different in general
  if (dim>1) ia = intervals[ifa].FindWhichOneIs(typea, bfla, iop_a); // this is the index in multidimensional interval
  double t_a = intervals[ifa].time(typea,ia); // time in the first orbital

  pair<double,double> tv = intervals[ifa].Closest_Times(1-typea, bfla, t_a);
  pair<double,double> t_same;
  pair<int,int> ind_same;
  pair<double,double> t_oppo;
  pair<int,int> ind_oppo;
  intervals[ifb].Closest_Times(t_same, ind_same, 1-typeb, bflb, t_a);
  intervals[ifb].Closest_Times(t_oppo, ind_oppo, typeb, bflb, t_a);

  double t_b;
  int ib;
  int op_a = 2*fla+typea;
  int op_b = 2*flb+typeb;
  
  if (typea==nIntervals::c){
    if (t_same.first<t_oppo.first){ //    [tv.1]-----------------[t]    [tv.2]----------------
      if (t_oppo.first>tv.first){   //    ----[ts.1]     [to.1]------------[ts.2]  [to.2]-----
	t_b = t_oppo.first;
	ib = ind_oppo.first;
      } else return;
    }else{
      if (t_oppo.second<tv.second){//  [tv.1]--------[t]            [tv.2]----------------
	t_b = t_oppo.second;       //[to.1]----[ts.1]       [to.2]------------[ts.2]
	ib = ind_oppo.second;
      } else return;
    }
  }else{
    if (t_same.second>t_oppo.second){ // ----[tv.1]     [t]-----------[tv.2]
      if (tv.second>t_oppo.second){   // [to.1] [ts.1]-----[to.2]  [ts.2]-----
	t_b = t_oppo.second;
	ib = ind_oppo.second;
      } else return;
    }else{
      if (tv.first<t_oppo.first){ //----[tv.1]        [t]---------------[tv.2]  
	t_b = t_oppo.first;	  //[ts.1]---[to.1]         [ts.2]-----[to.2]   -----
	ib = ind_oppo.first;
      }else return;
    }
  }
  
  int iws=0;
  if (t_b<0) {t_b += common::beta; iws=1;}
  if (t_b>=common::beta) {t_b -= common::beta; iws=1;}
  if (t_a==t_b) return;

  int ip_a, ip_b;
  if (t_a<t_b){
    ip_a = Operators.FindIndex(op_a, t_a, 0);
    ip_b = Operators.FindIndex(op_b, t_b, ip_a);
  }else{
    ip_b = Operators.FindIndex(op_b, t_b, 0);
    ip_a = Operators.FindIndex(op_a, t_a, ip_b);
  }

  double exp_sum = Segment_ComputeTryalExponentSumForExchange(ip_a,ip_b,Trace,state_evolution_left,Operators,npraStates,cluster,istep);
  double ms = fabs(divide( Number(1.0, exp_sum), matrix_element));
  
  t_trial2.start();    
  double ratioDa, ratioDb;
  if (typea==nIntervals::c)
    ratioDa = TMD[ifa].Move_end_DetRatio(  MD[ifa], kna, t_a, t_b, bfla, ia, intervals[ifa]);
  else
    ratioDa = TMD[ifa].Move_start_DetRatio(MD[ifa], kna, t_a, t_b, bfla, ia, intervals[ifa]);
  if (typeb==nIntervals::c)
    ratioDb = TMD[ifb].Move_end_DetRatio(  MD[ifb], knb, t_b, t_a, bflb, ib, intervals[ifb]);
  else
    ratioDb = TMD[ifb].Move_start_DetRatio(MD[ifb], knb, t_b, t_a, bflb, ib, intervals[ifb]);
  
  t_trial2.stop();
  double P_to_accept = ms*fabs(ratioDa*ratioDb);
  //double det_a = fabs(detD[ifa]*ratioDa);
  //double det_b = fabs(detD[ifb]*ratioDb);
  double det_a = fabs(ratioDa);
  double det_b = fabs(ratioDb);
  bool Accept = 1-rand() < P_to_accept;
  
  if (Accept && ms>common::minM && det_a>common::minD && det_b>common::minD){
    t_accept.start();
    Operators.ExchangeTwo(ip_a, ip_b);
    Number new_matrix_element = Segment_UpdateStateEvolution(state_evolution_left, min(ip_a,ip_b), Operators,npraStates, cluster, Trace, Prob, istep);
    if (fabs(fabs(divide(new_matrix_element,Number(1,exp_sum)))-1)>1e-5) cout<<"ERROR Different matrix elements m_new="<<new_matrix_element.exp_dbl()<<" m_old="<<exp_sum<<endl;
    int ia_new=ia;
    int ib_new=ib;
    if (dim!=1 || iws!=0){
      ia_new = intervals[ifa].FindIndex(t_b, t_a, typea);
      ib_new = intervals[ifb].FindIndex(t_a, t_b, typeb);
    }
    if (typea==nIntervals::c)
      TMD[ifa].Move_end_UpdateMatrix(  MD[ifa], kna, t_a, t_b, bfla, ia, ia_new, intervals[ifa]);
    else
      TMD[ifa].Move_start_UpdateMatrix(MD[ifa], kna, t_a, t_b, bfla, ia, ia_new, intervals[ifa]);
    if (typeb==nIntervals::c)
      TMD[ifb].Move_end_UpdateMatrix(  MD[ifb], knb, t_b, t_a, bflb, ib, ib_new, intervals[ifb]);
    else
      TMD[ifb].Move_start_UpdateMatrix(MD[ifb], knb, t_b, t_a, bflb, ib, ib_new, intervals[ifb]);
    IntervalsExchangeExponents(typea, intervals[ifa], ia, ia_new, typeb, intervals[ifb], ib, ib_new);
    TMD[ifa].MoveUpdate_Gf(intervals[ifa], Mv, istep);
    TMD[ifb].MoveUpdate_Gf(intervals[ifb], Mv, istep);
    int sign_a_isie = 1-2*((ia_new+ia+iws)%2);
    int sign_b_isie = 1-2*((ib_new+ib+iws)%2);
    ratioDa *= sign_a_isie;
    ratioDb *= sign_b_isie;
    int from_orig = -1*sign_a_isie*sign_b_isie;
    double ms1 = divide(new_matrix_element, matrix_element);
    double Ps = ratioDa*ratioDb*ms1*from_orig;
    gsign *= sigP<0>(Ps);
    detD[ifa] *= ratioDa;
    detD[ifb] *= ratioDb;
    matrix_element = new_matrix_element;
    successful++;
    successfullM[ifa]++;
    successfullM[ifb]++;
    ComputeAverage(istep);
    t_accept.stop();
  }
  return;
}


template <class Rand>
void CTQMC<Rand>::Add_Two_Kinks(long long istep, int ifl)
{
  if (Operators.full()) return;
  int dim = cluster.ifl_dim[ifl];
  int kn = intervals[ifl].size()/2;
  int bfls1 = static_cast<int>(dim*rand());
  int bfle1 = static_cast<int>(dim*rand());
  int bfls2 = static_cast<int>(dim*rand());
  int bfle2 = static_cast<int>(dim*rand());
  int fls1 = cluster.fl_from_ifl[ifl][bfls1];
  int fle1 = cluster.fl_from_ifl[ifl][bfle1];
  int fls2 = cluster.fl_from_ifl[ifl][bfls2];
  int fle2 = cluster.fl_from_ifl[ifl][bfle2];

  double t_start1 = common::beta*rand();
  double t_end1 = common::beta*rand();
  double t_start2 = common::beta*rand();
  double t_end2 = common::beta*rand();
	  
  bool ssc1 = Operators.Try_Add_2_Cd_2_C_(fls1, t_start1, fle1, t_end1, fls2, t_start2, fle2, t_end2, state_evolution_left, state_evolution_right);

  if (t_start1==t_end1 || t_start2==t_end2 || t_start1==t_start2 || t_end1==t_end2) return;
  
  if (ssc1){
    double ratioD = TMD[ifl].AddDetRatio(MD[ifl], kn, bfls1, t_start1, bfle1, t_end1, bfls2, t_start2, bfle2, t_end2, intervals[ifl]);

    pair<int,int> opt = Operators.Try_Add_2_Cd_2_C(fls1, t_start1, fle1, t_end1, fls2, t_start2, fle2, t_end2);
    int op_i = min(opt.first, opt.second), op_f = max(opt.first, opt.second);
    Number matrix_element_new = ComputeTrace(state_evolution_left, state_evolution_right, op_i, op_f, Operators, npraStates, cluster, 4);
    double ms = fabs(divide(matrix_element_new, matrix_element));
    double P = sqr(common::beta*dim/(kn+1.))*sqr(common::beta*dim/(kn+2.))*(fabs(ratioD)*ms);
    if (1-rand()<P && ms>common::minM){
      int is1, ie1;
      intervals[ifl].Find_is_ie(t_start1, is1, t_end1, ie1);
      double ratioD1 = TMD[ifl].AddUpdateMatrix(MD[ifl], kn, t_start1, is1, bfls1, t_end1, ie1, bfle1, intervals[ifl],istep);
      pair<int,int> ipl1 = intervals[ifl].InsertExponents(t_start1, is1, bfls1, t_end1, ie1, bfle1);
      TMD[ifl].AddUpdate_Gf(intervals[ifl], Mv,istep);
      
      pair<int,int> opt1 = Operators.Add_Cd_C(fls1, t_start1, fle1, t_end1, IntervalIndex(ifl, nIntervals::cd, ipl1.first), IntervalIndex(ifl, nIntervals::c, ipl1.second));
      
      int op_i1 = min(opt1.first, opt1.second);
      int op_f1 = max(opt1.first, opt1.second);
      Number new_matrix_element1 = UpdateStateEvolution(state_evolution_left, state_evolution_right, op_i1, op_f1, Operators, npraStates, cluster, Trace, Prob, istep);
      
      int is2, ie2;
      intervals[ifl].Find_is_ie(t_start2, is2, t_end2, ie2);
      double ratioD2 = TMD[ifl].AddUpdateMatrix(MD[ifl], kn+1, t_start2, is2, bfls2, t_end2, ie2, bfle2, intervals[ifl],istep);
      pair<int,int> ipl2 = intervals[ifl].InsertExponents(t_start2, is2, bfls2, t_end2, ie2, bfle2);
      TMD[ifl].AddUpdate_Gf(intervals[ifl], Mv,istep);
      
      pair<int,int> opt2 = Operators.Add_Cd_C(fls2, t_start2, fle2, t_end2, IntervalIndex(ifl, nIntervals::cd, ipl2.first), IntervalIndex(ifl, nIntervals::c, ipl2.second));
	
      int op_i2 = min(opt2.first, opt2.second);
      int op_f2 = max(opt2.first, opt2.second);
      Number new_matrix_element2 = UpdateStateEvolution(state_evolution_left, state_evolution_right, op_i2, op_f2, Operators, npraStates, cluster, Trace, Prob, istep);
      
      double ms_new = divide(new_matrix_element2,matrix_element);
      if (fabs(fabs(ms)-fabs(ms_new))>1e-6) cerr<<"Matrix element not the same a2!"<<endl;
      double ratioD_new = ratioD1*ratioD2;
      if (fabs(fabs(ratioD_new)-fabs(ratioD))>1e-6) cerr<<"ratioD not the same!"<<endl;
	      
      detD[ifl] *= ratioD1*ratioD2;
      matrix_element = new_matrix_element2;
      successful++;
      successfullC[ifl]++;
	    
      ComputeAverage(istep);
    }
  }
}

template <class Rand>
void CTQMC<Rand>::Remove_Two_Kinks(long long istep, int ifl)
{
  int kn = intervals[ifl].size()/2;
  if (kn<=1) return;
  int dim = cluster.ifl_dim[ifl];
  int ie1 = static_cast<int>(kn*rand());
  int is1 = static_cast<int>(kn*rand());
  int bfle1 = intervals[ifl].btype_e(ie1);
  int bfls1 = intervals[ifl].btype_s(is1);
  int ie2, bfle2;
  do{
    ie2 = static_cast<int>(kn*rand());
    bfle2 = intervals[ifl].btype_e(ie2);
  } while(ie2==ie1);
  int is2, bfls2;
  do{
    is2 = static_cast<int>(kn*rand());
    bfls2 = intervals[ifl].btype_s(is2);
  } while (is2==is1);
  
  int fle1 = cluster.fl_from_ifl[ifl][bfle1];
  int fls1 = cluster.fl_from_ifl[ifl][bfls1];
  int fle2 = cluster.fl_from_ifl[ifl][bfle2];
  int fls2 = cluster.fl_from_ifl[ifl][bfls2];
	  
  int iop_s1 = intervals[ifl].FindSuccessive(0, is1, bfls1);
  int iop_e1 = intervals[ifl].FindSuccessive(1, ie1, bfle1);
  int iop_s2 = intervals[ifl].FindSuccessive(0, is2, bfls2);
  int iop_e2 = intervals[ifl].FindSuccessive(1, ie2, bfle2);
  
  int ipe1, ips1, ipe2, ips2;
  bool ssc1 = Operators.Try_Remove_2_C_2_Cd_(fle1, iop_e1, fls1, iop_s1, fle2, iop_e2, fls2, iop_s2, ipe1, ips1, ipe2, ips2, state_evolution_left, state_evolution_right);
    
  if (ssc1){
    double ratioD = TMD[ifl].RemoveDetRatio(MD[ifl], kn, is1, ie1, is2, ie2);
    int op_i = Operators.mini();
    int op_f = Operators.maxi()-2;
    Operators.Try_Remove_C_Cd(ipe1, ips1, ipe2, ips2);
    Number matrix_element_new = ComputeTrace(state_evolution_left, state_evolution_right, op_i, op_f, Operators, npraStates, cluster, -4);
    double ms = fabs(divide(matrix_element_new,matrix_element));
    double P = (fabs(ratioD)*ms)*sqr(kn/(common::beta*dim))*sqr((kn-1)/(common::beta*dim));
    //	    cout<<"r "<<setw(5)<<i<<" "<<setw(10)<<P<<" "<<setw(10)<<ms<<" "<<setw(10)<<ratioD<<endl;
    if (1-rand()<P && ms>common::minM){
      //      cout<<"r2 "<<setw(5)<<i<<" "<<setw(10)<<P<<" "<<setw(10)<<ms<<" "<<setw(10)<<ratioD<<" "<<kn<<endl;
      Operators.Remove_C_Cd(ipe1, ips1);
      int op_i = min(ipe1, ips1);
      int op_f = ipe1>ips1 ? ipe1-2 : ips1-1;
      Number new_matrix_element1 = UpdateStateEvolution(state_evolution_left, state_evolution_right, op_i, op_f, Operators, npraStates, cluster, Trace, Prob, istep);
      TMD[ifl].RemoveUpdate_Gf(MD[ifl], kn, is1, ie1, intervals[ifl], Mv);
      double ratioD1 = TMD[ifl].RemoveUpdateMatrix(MD[ifl], kn, is1, ie1, intervals[ifl]);
      intervals[ifl].RemoveExponents(is1,ie1);

      Operators.Remove_C_Cd(ipe2, ips2);
      op_i = min(ipe2, ips2);
      op_f = ipe2>ips2 ? ipe2-2 : ips2-1;
      Number new_matrix_element2 = UpdateStateEvolution(state_evolution_left, state_evolution_right, op_i, op_f, Operators, npraStates, cluster, Trace, Prob, istep);
      if (is1<is2) is2--;
      if (ie1<ie2) ie2--;
      TMD[ifl].RemoveUpdate_Gf(MD[ifl], kn-1, is2, ie2, intervals[ifl], Mv);
      double ratioD2 = TMD[ifl].RemoveUpdateMatrix(MD[ifl], kn-1, is2, ie2, intervals[ifl]);
      intervals[ifl].RemoveExponents(is2,ie2);
		
      double ms_new = divide(new_matrix_element2,matrix_element);
      if (fabs(fabs(ms)-fabs(ms_new))>1e-6) cerr<<"Matrix element not the same r2!"<<endl;
      double ratioD_new = ratioD1*ratioD2;
      if (fabs(fabs(ratioD_new)-fabs(ratioD))>1e-6) cerr<<"ratioD not the same!"<<endl;
		
      detD[ifl] *= ratioD1*ratioD2;
      matrix_element = new_matrix_element2;
      successful++;
      successfullC[ifl]++;
	    
      ComputeAverage(istep);
    }
  }
}

template <class Rand>
void CTQMC<Rand>::ComputeFinalAverage(function1D<double>& mom0, double& nf, double& TrSigma_G, double& Epot, double& Ekin)
{
  double Eimp_na = 0;
  (*xout)<<"mom="<<left;
  mom0.resize(cluster.N_unique_fl*cluster.HF_M.size());
  for (int op=0; op<cluster.HF_M.size(); op++){
    for (int fl=0; fl<cluster.N_unique_fl; fl++){
      double sum=0;
      for (int ist=0; ist<npraStates.size(); ist++)
	for (int m=0; m<cluster.msize(ist+1); m++)
	  sum += cluster.HF_M[op][fl][ist+1][m][m]*AProb[ist][m];
      int ind = fl+op*cluster.N_unique_fl;
      mom0[ind] = sum;
      (*xout)<<setw(8)<<mom0[ind]<<" ";
      // The first operator should be N_{bath}
      // We compute N_{bath}*E_{bath}
      if (op==0) Eimp_na += sum * cluster.epsk[fl];
    }
  }
  (*xout)<<endl;

  Epot=0;
  nf=0;
  for (int ist=0; ist<npraStates.size(); ist++){
    double dmuN = common::mu*cluster.Ns[ist+1];
    double Nm = cluster.Ns[ist+1];
    for (int m=0; m<cluster.msize(ist+1); m++){
      Epot += (cluster.Ene[ist+1][m]+dmuN)*AProb[ist][m];
      nf += Nm*AProb[ist][m];
    }
  }
  (*xout)<<left<<" Epot="<<setw(10)<<Epot<<" ";

  double kt=0;
  for (int ifl=0; ifl<kaver.size(); ifl++) kt += kaver[ifl];
  Ekin = -kt/common::beta;
  (*xout)<<left<<" Ekin="<<setw(10)<<Ekin<<" ";
  
  TrSigma_G = Epot - Eimp_na;
  (*xout)<<left<<"Tr(Sigma*G)="<<setw(10)<<TrSigma_G<<endl;
}

template <int boson_ferm>
inline const funProxy<dcomplex>& find_Op_in_intervals(int ip, const vector<nIntervals>& intervals, const NOperators& Operators)
{
  IntervalIndex p_ifl = Operators.iifl(ip);
  int ifl = p_ifl.ifl;
  int itp = p_ifl.type;
  int in = p_ifl.in;
  return intervals[ifl]. template exp_direct<boson_ferm>(itp,in);
}

template <int boson_ferm>// e^{iom*beta}
double e_om_beta(){ return -100;};
template <>
double e_om_beta<0>(){return 1;}// bosonic
template <>
double e_om_beta<1>(){return -1;}// fermionic

template <int boson_ferm>
void ComputeFrequencyPart(const vector<nIntervals>& intervals, const NOperators& Operators, int omsize, function2D<dcomplex>& dexp)
{ // This function computes the following factor:
  //            exp(iom*tau_{l+1})-exp(iom(tau_l)
  //            for frequencies iom sampled and all kink times tau_l.
  int N = Operators.size();
  dexp.resize(N+1, omsize);
  double eom_beta = e_om_beta<boson_ferm>();
  
  if (N==0){
    for (int im=0; im<omsize; im++)  dexp(0,im) = eom_beta-1;// e^{iom*beta}-e^{iom*0}
    return;
  }
  
  {// first kink
    const funProxy<dcomplex>& exp1 = find_Op_in_intervals<boson_ferm>(0, intervals, Operators);
    funProxy<dcomplex>& dexp_ = dexp[0];
    for (int im=0; im<omsize; im++)  dexp_[im] = exp1[im]-1; // exp(iom*0)==1
  }
  for (int ip=1; ip<N; ip++){// most of the kinks
    const funProxy<dcomplex>& exp0 = find_Op_in_intervals<boson_ferm>(ip-1, intervals, Operators);
    const funProxy<dcomplex>& exp1 = find_Op_in_intervals<boson_ferm>(ip,   intervals, Operators);
    funProxy<dcomplex>& dexp_ = dexp[ip];
    for (int im=0; im<omsize; im++)  dexp_[im] = exp1[im]-exp0[im];
  }
  {// after last kink
    const funProxy<dcomplex>& exp0 = find_Op_in_intervals<boson_ferm>(N-1, intervals, Operators);
    funProxy<dcomplex>& dexp_ = dexp[N];
    for (int im=0; im<omsize; im++)  dexp_[im] = eom_beta-exp0[im]; // exp(iom*beta)==+-1
  }
}


void ComputeFrequencyPart_Slow(const NOperators& Operators, const mesh1D& iom, function2D<dcomplex>& bexp, function2D<dcomplex>& dexp)
{ // This function computes the following factor:
  //            exp(iom*tau_{l+1})-exp(iom(tau_l)
  //            for frequencies iom sampled and all kink times tau_l.
  int N = Operators.size();
  int nom = iom.size();
  dexp.resize(N+1, iom.size());

  double eom_beta = cos(iom[0]*common::beta);
    
  if (N==0){
    dexp=0; // e^{iom*beta}-e^{iom*0} ==0
    return;
  }
  // This could be done at every insertion or removal of kink
  // But here it is done for all kinks every time
  for (int ip=0; ip<N; ip++){
    double tau = Operators.t(ip);
    for (int im=0; im<nom; im++){
      double phase = iom[im]*tau;
      bexp(ip,im).Set(cos(phase),sin(phase));
    }
  }

  const funProxy<dcomplex>& exp1 = bexp[0];
  funProxy<dcomplex>& dexp_ = dexp[0];
  for (int im=0; im<nom; im++)  dexp_[im] = exp1[im]-1; // exp(iom*0)==1
  
  for (int ip=1; ip<N; ip++){
    const funProxy<dcomplex>& exp0 = bexp[ip-1];
    const funProxy<dcomplex>& exp1 = bexp[ip];
    funProxy<dcomplex>& dexp_ = dexp[ip];
    for (int im=0; im<nom; im++)  dexp_[im] = exp1[im]-exp0[im];
  }

  {
    const funProxy<dcomplex>& exp0 = bexp[N-1];
    funProxy<dcomplex>& dexp_ = dexp[N];
    for (int im=0; im<nom; im++)  dexp_[im] = eom_beta-exp0[im]; // exp(iom*beta)==1
  }
}
template <class Rand>
void CTQMC<Rand>::ComputeAverage(long long istep)
{
  // components of observables:
  // 0 : nf
  // 1 : chi_{zz} = <Sz(t) Sz>
  // 2 : chi(density-density) = <1/2 n(t) 1/2 n>
  // 3 : <Sz>
  if (observables.size()!=4) cerr<<"Resize observables!"<<endl;
  observables=0;

  if (common::SampleSusc){
    susc=0;
    //    ComputeFrequencyPart_Slow(Operators, biom, bexp, dexp);
    ComputeFrequencyPart<0>(intervals, Operators, biom.size(), dexp);
  }

  if (common::SampleTransitionP) P_transition = 0;
  
  // time differences can be precomputed
  int N = Operators.size();
  dtau.resize(N+1);
  if (N>0){
    dtau[0] = Operators.t(0);
    for (int ip=1; ip<N; ip++) dtau[ip] = Operators.t(ip)-Operators.t(ip-1);
    dtau[N] = common::beta-Operators.t(N-1);
  } else dtau[0] = common::beta;

  deque<int> contrib;
  for (int ist=0; ist<npraStates.size(); ist++){
    // Checking which states do not contribute to trace
    // This was here before, but now I removed state_evolution!!
    //if (state_evolution_left[ist].size()<=Operators.size() || state_evolution_left[ist].last().istate!=npraStates[ist].istate) continue;
    if (npraStates[ist].empty()) continue;
    double P_i = fabs(divide(Trace[ist],matrix_element)); // probability for this state

    if (fabs(P_i)<1e-10) continue; // Probability negligible
    contrib.push_back(ist);
    int istate = npraStates[ist].istate;    
    double Ns = cluster.Ns[istate];
    double Sz = cluster.Sz[istate];
    double Ns2 = Ns/2.;
    
    double dnf = Ns*dtau[0];
    double dmg = Sz*dtau[0];
 
    if (common::SampleSusc){
      for (int im=1; im<dexp.size_Nd(); im++){
	dsusc(im,0) = dexp[0][im] * Sz;
	dsusc(im,1) = dexp[0][im] * Ns2;
      }
    }

    double fact = P_i/common::beta;
    //int c_state=istate;
    for (int ip=0; ip<Operators.size(); ip++){
      int jstate = state_evolution_left(ist,ip).istate;
      double Ns = cluster.Ns[jstate];
      double Sz = cluster.Sz[jstate];
      double Ns2 = Ns/2.;
      
      dnf += Ns*dtau[ip+1];
      dmg += Sz*dtau[ip+1];

      //if (common::SampleTransitionP) P_transition(c_state-1, Operators.typ(ip) ) += fact;
      //c_state = jstate;
      
      if (common::SampleSusc){
	const funProxy<dcomplex>& dexp_ = dexp[ip+1];
	for (int im=1; im<dexp.size_Nd(); im++){
	  dsusc(im,0) += dexp_[im] * Sz;
	  dsusc(im,1) += dexp_[im] * Ns2;
	}
      }
    }
    if (common::SampleTransitionP){
      int c_state=istate;
      for (int ip=0; ip<Operators.size(); ip++){
	int jstate = state_evolution_left(ist,ip).istate;
	P_transition(c_state-1, Operators.typ(ip) ) += fact;
	c_state = jstate;
      }
    }
    
    observables[0] += dnf*fact;
    observables[1] += sqr(dmg)*fact;
    observables[2] += sqr(0.5*dnf)*fact;
    observables[3] += dmg*fact;
   
    if (common::SampleSusc){
      for (int im=1; im<susc.size_Nd(); im++){
	susc(0,im) += norm(dsusc(im,0))*fact/sqr(biom[im]);
	susc(1,im) += norm(dsusc(im,1))*fact/sqr(biom[im]);
      }
    }
  }
  susc(0,0) = observables[1];
  susc(1,0) = observables[2];
}


template <class Rand>
void CTQMC<Rand>::CleanUpdate(long long istep)
{
  static function2D<dcomplex> backup_Gf(4,iom.size());
  static function2D<double> backup_MD(MD[0].size_N(),MD[0].size_Nd());

  
  for (int ifl=0; ifl<common::N_ifl; ifl++){
    backup_MD.resize(MD[ifl].size_N(),MD[ifl].size_Nd());
    backup_MD = MD[ifl];
	
    TMD[ifl].CleanUpdateMatrix(MD[ifl], intervals[ifl].size()/2, intervals[ifl]);

    for (int il=0; il<MD[ifl].size_N(); il++)
      for (int jl=0; jl<MD[ifl].size_Nd(); jl++)
	if (fabs(backup_MD[il][jl]-MD[ifl][il][jl])>1e-5) cerr<<"Difference in MD at "<<istep<<" "<<il<<" "<<jl<<" "<<backup_MD[il][jl]-MD[ifl][il][jl]<<endl;
	
    backup_Gf = TMD[ifl].gf();
	
    TMD[ifl].CleanUpdateGf(MD[ifl], intervals[ifl].size()/2, intervals[ifl], Mv,istep);
	
    for (int il=0; il<backup_Gf.size_N(); il++)
      for (int jl=0; jl<backup_Gf.size_Nd(); jl++)
	if (norm(backup_Gf[il][jl]-TMD[ifl].gf()[il][jl])>1e-5) cerr<<"Difference in Gf at "<<istep<<" "<<il<<" "<<jl<<" "<<backup_Gf[il][jl]-TMD[ifl].gf()[il][jl]<<endl;
  }
}

template <class Rand>
void CTQMC<Rand>::StoreCurrentState(function1D<double>& aver_observables, long long istep)
{
  int trusign = gsign;
  
  for (size_t ifl=0; ifl<Gaver.size(); ifl++)  Gaver[ifl].AddPart(TMD[ifl].gf(), trusign);
  
  AProb.AddPart(Prob, trusign);
  aver_observables.AddPart(observables, trusign);
  if (common::SampleTransitionP) AP_transition.AddPart(P_transition, trusign);
  
  asign += trusign;
  
  for (int ifl=0; ifl<kaver.size(); ifl++) kaver[ifl] += intervals[ifl].size()/2*trusign;

  if (common::SampleSusc) aver_susc.AddPart(susc, trusign);

  if (common::QHB2){
    int N_ifl = cluster.N_ifl;
    for (int ip=0; ip<Operators.size(); ip++){// We go over all operators of all baths
      int op = Operators.typ(ip);
      if (op%2 == nIntervals::cd){
	IntervalIndex p_ifl = Operators.iifl(ip);
	int ifl = p_ifl.ifl; // type of bath
	//int itp = p_ifl.type; // constructor or destructor
	//if (itp!=nIntervals::cd) cout<<"ERROR"<<endl;
	int iis = intervals[ifl].index_s_1[p_ifl.in]; // which contructor

	if (fabs(intervals[ifl].time_s(iis)-Operators.t(ip))>1e-10) cout<<"Times are different in StoreCurrentState t_interval="<<intervals[ifl].time_s(iis)<<" t_operators="<<Operators.t(ip)<<endl;
	  
	if (common::Segment)
	  Segment_Get_N_at_Operator2(Njc, ip, Trace, matrix_element, state_evolution_left, Operators, npraStates, cluster, istep);
	else
	  Get_N_at_Operator2(Njc,ip,Trace,matrix_element,state_evolution_left,state_evolution_right,Operators,npraStates,cluster, istep);
	
	dcomplex* __restrict__ _Ff = TMD[ifl].Ff[iis].MemPt();
	for (int ii=0; ii<Njc.size(); ii++){
	  double nj_sign = Njc[ii]*trusign;
	  dcomplex* __restrict__ _Faver = Faver(ifl,ii).MemPt();
	  for (int im=0; im<iom.size(); im++) _Faver[im] += _Ff[im]*nj_sign;
	  //xaxpy(iom.size(), nj_sign, _Ff, _Faver);
	}
      }
    }
  }
  Naver++;
}


template <class Rand>
void CTQMC<Rand>::PrintInfo(long long i, const function1D<double>& aver_observables)
{
  cout<<setw(9)<<i+1<<" ";
  double asignd = asign/(Naver+0.0);
  double nf = aver_observables[0]/cluster.Nm/asign;
  double mf = aver_observables[3]/cluster.Nm/asign;

  cout.precision(4);
  cout<<left<<setw(3)<<common::my_rank<<"  ";
  cout<<left<<" nf="<<setw(7)<<nf<<"  ";
  cout<<left<<" <Sz>="<<setw(8)<<mf<<"  ";
  cout<<left<<" <s>="<<setw(8)<<asignd<<"  ";
  double chiS = aver_observables[1]/cluster.Nm/asign;
  double chiC = aver_observables[2]/cluster.Nm/asign;
  cout<<left<<" chiS0="<<setw(12)<<(chiS)<<" ";
  //cout<<left<<" chiS="<<setw(12)<<(chiS-sqr(mf)*common::beta)<<"  ";
  cout<<left<<" chiD="<<setw(12)<<(chiC-sqr(0.5*nf)*common::beta)<<"  ";
  
  {
    double Epot=0;
    for (int ist=0; ist<npraStates.size(); ist++){
      double dmuN = common::mu*cluster.Ns[ist+1];
      for (int m=0; m<cluster.msize(ist+1); m++)
	Epot += (cluster.Ene[ist+1][m]+dmuN)*AProb[ist][m];
    }
    Epot/= asign;
    cout<<left<<" Epot="<<setw(10)<<Epot<<" ";

    double kt=0;
    for (int ifl=0; ifl<kaver.size(); ifl++) kt += kaver[ifl];
    kt/= asign;
    double Ekin = -kt/common::beta;
    cout<<left<<" Ekin="<<setw(10)<<Ekin<<" ";
  }
  
  for (int op=0; op<cluster.HF_M.size(); op++){
    for (int fl=0; fl<cluster.N_unique_fl; fl++){
      
      double sum=0;
      for (int ist=0; ist<npraStates.size(); ist++)
	for (int m=0; m<cluster.msize(ist+1); m++)
	  sum += cluster.HF_M[op][fl][ist+1][m][m]*AProb[ist][m];
      sum/= asign;

      cout<<setw(8)<<sum<<" ";
    }
  }
  cout<<right<<endl;
  
  if (common::fastFilesystem && (i+1)%common::Naver==0){
    ofstream tout(NameOfFile_("Gaver",common::my_rank,i+1).c_str());
    tout.precision(16);
    for (int im=0; im<iom.size(); im++){
      tout<<setw(20)<<iom[im]<<" ";
      for (int ifl=0; ifl<common::N_ifl; ifl++)
	for (int b=0; b<Gaver[ifl].size_N(); b++)
	  tout<<setw(20)<<Gaver[ifl][b][im]/(asign)<<" ";
      tout<<endl;
    }
    ofstream pout(NameOfFile_("Probability",common::my_rank,i+1).c_str());
    pout.precision(16);
    for (int j=0; j<AProb.size_N(); j++)
      for (int k=0; k<AProb[j].size(); k++)
	pout<<setw(3)<<j+1<<" "<<setw(3)<<k<<" "<<setw(20)<<AProb[j][k]/(asign)<<endl;

    if (common::SampleGtau){
      ofstream tout(NameOfFile_("Gatau",common::my_rank,i+1).c_str());
      tout.precision(16);
    
      for (int it=0; it<tau.size(); it++){
	tout<<tau[it]<<" ";
	for (int ifl=0; ifl<cluster.N_ifl; ifl++){
	  for (int ib=0; ib<cluster.N_baths(ifl); ib++){
	    double gf = Gtau[ifl](ib,it);
	    if (cluster.conjg[ifl][ib]) gf = -Gtau[ifl](ib,tau.size()-it-1);
	    gf *= cluster.sign[ifl][ib];
	    tout<<setw(20)<<gf<<" ";
	  }
	}
	tout<<endl;
      }
    }
  }
}

template<class container>
double variation(const container& gt, int dt=2)
{
  double gmax=0;
  //double gaver=0;
  int istart = max(static_cast<int>(0.05*gt.size()), dt);
  int iend = min(static_cast<int>(0.95*gt.size()), gt.size()-dt-1);
  //cout<<"istart="<<istart<<" iend="<<iend<<endl;
  if (iend>istart){
    for (int it=istart; it<iend; it++){
      double sm=0;
      for (int j=it-dt; j<it; j++) sm += gt[j];
      for (int j=it+1; j<it+dt+1; j++) sm += gt[j];    
      double ga = sm/(2*dt);
      if (fabs(ga)>1e-7 && fabs(gt[it])>1e-4) gmax = max(gmax, fabs(gt[it]/ga-1.));
      //if (fabs(ga)>1e-5) gaver += abs(gt[it]/ga-1.);
      //ga=(gt[it-1]+gt[it+1])/2.;
      //if (fabs(ga)>1e-5) gaver += gt[it]/ga;
    }
    //gaver *= 1./(iend-istart);
  }
  return gmax;
}

template <class Rand>
double CTQMC<Rand>::sample(long long max_steps)
{
  Number Zloc = 0;
  for (int i=0; i<Trace.size(); i++) Zloc += Trace[i];
  matrix_element = Zloc;
  ComputeAverage(0);
  aver_observables=0;
  Timer t_measure;

  for (long long i=0; i<max_steps; i++){
    if (common::PChangeOrder>rand() || i<common::warmup/2){// change order, in warmup one should reach average order
      int ifl = BathP.ifl(rand());
      if (rand()>0.5){
	LOG(TRACE, i<<" Trying to add a kink: "<<ifl);
	if (common::Segment) Segment_Add_One_Kink(i, ifl);
	else Add_One_Kink(i, ifl);
      } else {
	LOG(TRACE, i<<" Trying to rem a kink: "<<ifl);
	if (common::Segment) Segment_Remove_One_Kink(i,ifl);
	else Remove_One_Kink(i, ifl);
      }
    }else{
      int ifl = BathP.ifl(rand());
      LOG(TRACE, i<<" Trying to mov a kink: "<<ifl);
      if (common::Segment){
	if (common::PMove>1-rand())
	  Segment_Move_A_Kink(i, ifl);
	else
	  Segment_Exchange_Two_Intervals(i);
      }
      else Move_A_Kink(i, ifl);      // Keep the same number of kinks
    }
    
    if ((i+1)%common::CleanUpdate==0) CleanUpdate(i);
    if (common::GlobalFlip>0 && (i+1)%common::GlobalFlip==0) GlobalFlipFull(i);
    //if (common::GlobalFlip>0 && (i+1)%common::GlobalFlip==0) GlobalFlip(i);

    t_measure.start();
    histogram[Operators.size()/2]++;
    if ((i+1)%common::tsample==0 && i>common::warmup) StoreCurrentState(aver_observables,i);
    if (common::SampleGtau>0 && (i+1)%common::SampleGtau==0){
      for (size_t ifl=0; ifl<TMD.size(); ifl++) TMD[ifl].ComputeGtau(MD[ifl], intervals[ifl], Gtau[ifl]);
      NGta++;
    }

    if (common::cmp_vertex && (i+1)%common::SampleVertex==0){
      
      for (unsigned i0=0; i0<Mv.size(); i0++){
        for (int im1=0; im1<nomv; im1++){
	  int nomv2 = 2*nomv-1;
	  funProxy<dcomplex>& _Mvd = Mv[i0][im1];
	  const funProxy<dcomplex>& _Mvs = Mv[i0][nomv2-im1];
          for (int im2=0; im2<2*nomv; im2++)
            _Mvd[im2] = _Mvs[nomv2-im2].conj();
        }
      }

      for (int i0=0; i0<VertexH.N0; i0++){
        for (int i1=0; i1<VertexH.N1; i1++){
	  const function2D<dcomplex>& Mv0 = Mv[i0];
	  const function2D<dcomplex>& Mv1 = Mv[i1];
          for (int iOm=0; iOm<2*nOm-1; iOm++){
	    int dOm = nOm-1-iOm;
            int sm1 = max(0, -dOm);
            int em1 = min(2*nomv, 2*nomv-dOm);
            for (int im1=sm1; im1<em1; im1++){
              int sm2 = max(0, -dOm);
              int em2 = min(2*nomv, 2*nomv-dOm);
	      dcomplex* VH = &VertexH(i0,i1,iOm,im1,0);
	      dcomplex* VF = &VertexF(i0,i1,iOm,im1,0);
	      dcomplex mv0 = Mv0(im1, im1+dOm);
	      const funProxy<dcomplex>& _Mv0 = Mv0[im1];
              for (int im2=sm2; im2<em2; im2++){
                VH[im2] += mv0 * Mv1(im2+dOm, im2);
                VF[im2] += _Mv0[im2]*Mv1(im2+dOm,im1+dOm);
              }
            }
          }
        }
      }
      NGtv++;
    }
    t_measure.stop();
      
    if ((i+1)%common::Ncout==0 && i>2*common::warmup){
      PrintInfo(i, aver_observables);
      if (t_trial1.elapsed()>0.0)cout<<"t_trial1="<<t_trial1.elapsed()<<" t_trial2="<<t_trial2.elapsed()<<" t_trial3="<<t_trial3.elapsed()<<" t_accept="<<t_accept.elapsed()<<" t_measure="<<t_measure.elapsed()<<" t_evolve="<<NState::t_evolve.elapsed()<<" t_apply="<<NState::t_apply.elapsed()<<endl;
    }
  }

  for (size_t ifl=0; ifl<Gaver.size(); ifl++) Gaver[ifl] *= 1./(asign);
  if (common::QHB2){
    for (int ifl=0; ifl<Faver.size_N(); ifl++)
      for (int ii=0; ii<Faver.size_Nd(); ii++)
	Faver(ifl,ii) *= 1./(asign);
  }
  kaver *= 1./(asign);
  aver_observables *= 1./(asign*cluster.Nm);

  aver_observables[2] -= sqr(0.5*aver_observables[0])*common::beta;
  
  if (common::SampleSusc){
    aver_susc *= 1./(asign*cluster.Nm);
    aver_susc[0][0] = aver_observables[1];
    aver_susc[1][0] = aver_observables[2];
  }
  if (common::cmp_vertex){
    VertexH *= (1.0/NGtv);
    VertexF *= (1.0/NGtv);
  }
  
  if (common::SampleGtau>0){
    double fct = Gtau[0].size_Nd()/(NGta+0.0)/(common::beta*common::beta);
    for (size_t ifl=0; ifl<TMD.size(); ifl++) Gtau[ifl] *= fct;
  }


  //ofstream outv(NameOfFile_("Variation",common::my_rank,0).c_str());
  Variation.resize(cluster.N_ifl);
  if (common::SampleGtau>0){
    ostream &outv = cout;
    outv<<common::my_rank<<":variation=";
    for (int ifl=0; ifl<cluster.N_ifl; ifl++)
      for (int ib=0; ib<cluster.N_baths(ifl); ib++){
	double var = variation(Gtau[ifl][ib]);
	Variation[ifl].push_back( var );
	outv<<var<<" ";
      }
    outv<<endl;
  }else{
    for (int ifl=0; ifl<cluster.N_ifl; ifl++)
      for (int ib=0; ib<cluster.N_baths(ifl); ib++)
	Variation[ifl].push_back(0.0);
  }
  //outv.close();

  double succM=0, succAR;
  for (int ifl=0; ifl<common::N_ifl; ifl++){
    succM += successfullM[ifl];
    succAR += successfullC[ifl];
  }
  (*xout)<<"successful="<<successful<<" add/rm="<<succAR/(max_steps*common::PChangeOrder)<<"  mv="<<succM/(max_steps*(1.-common::PChangeOrder+1e-20))<<" sign at finish "<<gsign<<endl;
  for (int ifl=0; ifl<common::N_ifl; ifl++){
    (*xout)<<"successfulC["<<ifl<<"]="<<successfullC[ifl]/static_cast<double>(successful)<<" ";
    (*xout)<<"successfulM["<<ifl<<"]="<<successfullM[ifl]/static_cast<double>(successful)<<" "<<endl;
  }
  // HERE YOU SHOULD CHECK GTAU VARIATION
  return aver_observables[0];
}

template <class Rand>
bool CTQMC<Rand>::SaveStatus(int my_rank)
{
  ofstream out(NameOfFile("status",my_rank).c_str());
  out.precision(16);
  if (!out){cerr<<"Could not save status!"<<endl; return false;}
  out<<"# Status file for ctqmc. Used to save all times t_s, t_e."<<endl;
  out<<intervals.size()<<endl;
  for (size_t ifl=0; ifl<intervals.size(); ifl++){
    out<<intervals[ifl].size()/2<<endl;
    for (int it=0; it<intervals[ifl].size()/2; it++){
      out<<intervals[ifl].time_s(it)<<" "<<intervals[ifl].time_e(it)<<" "<<intervals[ifl].btype_s(it)<<" "<<intervals[ifl].btype_e(it)<<endl;
    }
  }
  return true;
}

template <class Rand>
bool CTQMC<Rand>::RetrieveStatus(int my_rank, long long istep)
{
  string str;
  ifstream inp(NameOfFile("status",my_rank).c_str());
  if (!inp) return false;
  getline(inp,str); // comment
  int tN_ifl;
  inp>>tN_ifl;
  getline(inp,str);
  if (!inp) return false;
  for (size_t ifl=0; ifl<intervals.size(); ifl++){
    int dim = cluster.ifl_dim[ifl];
    int nt;
    inp>>nt;
    getline(inp,str);
    if (!inp) {clog<<"Only partly restored file status."<<my_rank<<endl; break;}
    if (nt<0 || nt>=intervals[ifl].fullsize()/2){cerr<<"Too many input times. Skipping...."<<endl; continue;}
    for (int it=0; it<nt; it++){
      double t_start, t_end;
      inp>>t_start>>t_end;
      if (t_start<0 || t_start>common::beta || t_end<0 || t_end>common::beta) {cerr<<"The times from starting file are wrong. Skipping..."<<endl; continue;}
      int bfls, bfle;
      inp>>bfls>>bfle;
      if (bfls<0 || bfls>dim || bfle<0 || bfle>dim) {cerr<<"The baths from starting file are wrong. Skipping..."<<endl; continue;}
      getline(inp,str);
      
      int kn = intervals[ifl].size()/2;// should be same as it!
      int fls = cluster.fl_from_ifl[ifl][bfls];
      int fle = cluster.fl_from_ifl[ifl][bfle];
      
      int is, ie;      
      intervals[ifl].Find_is_ie(t_start, is, t_end, ie);// shoule be same as it
      double ratioD = TMD[ifl].AddUpdateMatrix(MD[ifl], kn, t_start, is, bfls, t_end, ie, bfle, intervals[ifl],-1);
      pair<int,int> ipl = intervals[ifl].InsertExponents(t_start, is, bfls, t_end, ie, bfle);
      TMD[ifl].AddUpdate_Gf(intervals[ifl], Mv,-1);
      
      pair<int,int> opt = Operators.Add_Cd_C(fls, t_start, fle, t_end, IntervalIndex(ifl, nIntervals::cd, ipl.first), IntervalIndex(ifl, nIntervals::c, ipl.second));
      
      int op_i = min(opt.first, opt.second);
      int op_f = max(opt.first, opt.second);

      Number new_matrix_element = UpdateStateEvolution(state_evolution_left, state_evolution_right, op_i, op_f, Operators, npraStates, cluster, Trace, Prob, istep, false);

      //cout<<"new_matrix_element="<<new_matrix_element<<endl;
      
      gsign *= sigP<0>(ratioD*Operators.sign());
      
      detD[ifl] *= ratioD;
      matrix_element = new_matrix_element;
    }
  }
  gsign *= sigP<0>(matrix_element.mantisa);
  
  cout<<"Old status retrieved with sign="<<gsign<<" and determinants ";
  for (int ifl=0; ifl<common::N_ifl; ifl++) cout<<ifl<<":"<<detD[ifl]<<" ";
  cout<<endl;
  ComputeAverage(0);
  CleanUpdate(0);
  return true;
}

template <class Rand>
void CTQMC<Rand>::RecomputeCix()
{  cluster.RecomputeCix(AProb,asign,common::treshold,npraStates);}

template <class Rand>
void CTQMC<Rand>::GlobalFlip(long long istep)
{
  for (int iu=0; iu<cluster.gflip_ifl.size(); iu++){
    int ifa = cluster.gflip_ifl[iu].first;
    int ifb = cluster.gflip_ifl[iu].second;
    int sizea = cluster.ifl_dim[ifa];
    int sizeb = cluster.ifl_dim[ifb];
    if (sizea!=sizeb) cerr<<"Sizes of symmetric baths are different! This is not supported!"<<endl;
    //    int ka = intervals[ifa].size()/2;
    //    int kb = intervals[ifb].size()/2;

    double ratioDb=1, ratioDa=1;
    if (!cluster.ifl_equivalent[ifa][ifb]){
      ratioDb = TMD[ifa].GlobalFlipDetRatio(intervals[ifa], MD[ifa], Deltat[ifb]);
      ratioDa = TMD[ifb].GlobalFlipDetRatio(intervals[ifb], MD[ifb], Deltat[ifa]);
    }
    Number matrix_element_new = ComputeGlobalTrace(Operators, npraStates, cluster, cluster.gflip_fl[iu]);
    
    double ms = divide(matrix_element_new,matrix_element);
    double P = fabs(ms*ratioDa*ratioDb);

    //cout<<"Global flip: ms="<<ms<<" ratioD="<<ratioDa*ratioDb<<endl;
    
    if (P>1-rand()){
      tinterval.copy_data(intervals[ifa]); intervals[ifa].copy_data(intervals[ifb]); intervals[ifb].copy_data(tinterval);//swap
      
      if (cluster.ifl_equivalent[ifa][ifb]){
	tMD = MD[ifa]; MD[ifa] = MD[ifb]; MD[ifb] = tMD;
	swapGf(TMD[ifa], TMD[ifb], Mv);
      }else{
	TMD[ifa].CleanUpdateMatrix(MD[ifa], intervals[ifa].size()/2, intervals[ifa]);
	TMD[ifb].CleanUpdateMatrix(MD[ifb], intervals[ifb].size()/2, intervals[ifb]);
	TMD[ifa].CleanUpdateGf(MD[ifa], intervals[ifa].size()/2, intervals[ifa], Mv,istep);
	TMD[ifb].CleanUpdateGf(MD[ifb], intervals[ifb].size()/2, intervals[ifb], Mv,istep);
      }

      Operators.GlobalFlipChange(cluster.gflip_fl[iu], ifa, ifb);
      
      Number new_matrix_element = UpdateStateEvolution(state_evolution_left, state_evolution_right, 0, Operators.size()-1, Operators, npraStates, cluster, Trace, Prob, istep);
      if (fabs(fabs(divide(matrix_element_new,new_matrix_element))-1)>1e-6) {cerr<<"Matrix elements are not the same in global flip "<<matrix_element_new<<" "<<new_matrix_element<<endl;}

      swap(detD[ifa],detD[ifb]);
      detD[ifa] *= ratioDa;
      detD[ifb] *= ratioDb;
      
      matrix_element = new_matrix_element;
      successful++;
      ComputeAverage(0);
    }
  }
}

template <class Rand>
void CTQMC<Rand>::GlobalFlipFull(long long istep)
{
  if (cluster.gflip_ifl.size()==0) return; // Nothing to flip.

  //cout<<"Trying global flip"<<endl;
  vector<pair<double,double> > Dratios(cluster.gflip_ifl.size());
  double ratioD=1;
  for (int iu=0; iu<cluster.gflip_ifl.size(); iu++){
    int ifa = cluster.gflip_ifl[iu].first;
    int ifb = cluster.gflip_ifl[iu].second;
    int sizea = cluster.ifl_dim[ifa];
    int sizeb = cluster.ifl_dim[ifb];
    if (sizea!=sizeb) cerr<<"Sizes of symmetric baths are different! This is not supported!"<<endl;    
    double ratioDb=1, ratioDa=1;
    if (!cluster.ifl_equivalent[ifa][ifb]){
      ratioDb = TMD[ifa].GlobalFlipDetRatio(intervals[ifa], MD[ifa], Deltat[ifb]);
      ratioDa = TMD[ifb].GlobalFlipDetRatio(intervals[ifb], MD[ifb], Deltat[ifa]);
    }
    Dratios[iu].first = ratioDa;
    Dratios[iu].second = ratioDb;
    ratioD *= ratioDa*ratioDb;
  }

  //cout<<"RatioD="<<ratioD<<endl;
  //cout<<"size="<<cluster.gflip_fl.size_N()<<endl;
  
  Number matrix_element_new = ComputeGlobalTrace(Operators, npraStates, cluster, cluster.gflip_fl[cluster.gflip_fl.size_N()-1]);
  double ms = divide(matrix_element_new,matrix_element);
  double P = fabs(ms*ratioD);

  //cout<<"Global flip on all pairs of baths  ratioD="<<ratioD<<" ms="<<ms<<endl;//" check="<<divide(mm_brisi,matrix_element)<<endl;
  
  if (P>1-rand()){
    cout<<"Global flip succeeded: P="<<P<<" ratioD="<<ratioD<<" ms="<<ms<<endl;
    for (int iu=0; iu<cluster.gflip_ifl.size(); iu++){
      int ifa = cluster.gflip_ifl[iu].first;
      int ifb = cluster.gflip_ifl[iu].second;
      tinterval.copy_data(intervals[ifa]); intervals[ifa].copy_data(intervals[ifb]); intervals[ifb].copy_data(tinterval);//swap
      if (cluster.ifl_equivalent[ifa][ifb]){
	tMD = MD[ifa]; MD[ifa] = MD[ifb]; MD[ifb] = tMD;
	swapGf(TMD[ifa], TMD[ifb], Mv);
      }else{
	TMD[ifa].CleanUpdateMatrix(MD[ifa], intervals[ifa].size()/2, intervals[ifa]);
	TMD[ifb].CleanUpdateMatrix(MD[ifb], intervals[ifb].size()/2, intervals[ifb]);
	TMD[ifa].CleanUpdateGf(MD[ifa], intervals[ifa].size()/2, intervals[ifa], Mv,istep);
	TMD[ifb].CleanUpdateGf(MD[ifb], intervals[ifb].size()/2, intervals[ifb], Mv,istep);
      }
    }

    Operators.GlobalFlipChange(cluster.gflip_fl[cluster.gflip_fl.size_N()-1], cluster.gflip_ifl);
      
    Number new_matrix_element = UpdateStateEvolution(state_evolution_left, state_evolution_right, 0, Operators.size()-1, Operators, npraStates, cluster, Trace, Prob, istep);

    if (fabs(fabs(divide(matrix_element_new,new_matrix_element))-1)>1e-6) {cerr<<"Matrix elements are not the same in global flip "<<matrix_element_new<<" "<<new_matrix_element<<endl;}

    for (int iu=0; iu<cluster.gflip_ifl.size(); iu++){
      int ifa = cluster.gflip_ifl[iu].first;
      int ifb = cluster.gflip_ifl[iu].second;
      double ratioDa = Dratios[iu].first;
      double ratioDb = Dratios[iu].second;
      swap(detD[ifa],detD[ifb]);
      detD[ifa] *= ratioDa;
      detD[ifb] *= ratioDb;
    }
    double mms = divide(new_matrix_element,matrix_element);
    matrix_element = new_matrix_element;
    successful++;
    ComputeAverage(0);
  }
}

bool Qnonzero(const functionb<dcomplex>& Gf){
  // Check that the function is nonzero
  for (int im=0; im<min(10,Gf.size()); im++)
    if (abs(Gf[im])<1e-10) return false;
  double wsum=0;
  for (int im=0; im<min(10,Gf.size()); im++) wsum += abs(Gf[im]);
  if (wsum<1e-5) return false;
  return true;
}

struct CustomLess{
  function2D<int>* pF_i;
  int ist;
  CustomLess(function2D<int>* pF_i_, int ist_) : pF_i(pF_i_), ist(ist_){}
  bool operator()(int op1, int op2)
  {   
    return pF_i->operator()(op1,ist) < pF_i->operator()(op2,ist);
  }
};


int main(int argc, char *argv[])
{
  
  setvbuf (stdout, NULL, _IONBF, BUFSIZ);
  int my_rank, mpi_size, Master;
  MPI_Init(argc, argv, my_rank, mpi_size, Master);

  if (my_rank==Master){
    xout = new ofstream("ctqmc.log");
  }else{
    xout = &cout;
  }
  
#ifdef _LOGGING
  ofstream otrace(NameOfFile("traces_log",my_rank).c_str());  
  slog[TRACE]=&otrace;
  ofstream ostats(NameOfFile("stats_log",my_rank).c_str());  
  slog[STATS] = &ostats;
#endif
  
  string inputf = "PARAMS";      // filename of the parameter file
  string fDelta   = "Delta.inp"; // input bath function Delta(iom)
  string fcix     = "imp.cix";   // input file with atom eigenvalues and eigenvectors (exact diagonalization results)
  string fGf      = "Gf.out";    // output file for green's function G(iom)
  string fSig     = "Sig.out";   // output file for self-energy Sig(iom)
  double mu       = 0;           // chemical potential
  double beta     = 100;         // inverse temperature
  double U        = 0;           // Coulomb repulsion (should be used only in single site DMFT)
  long long M     = 1000000;     // number of all qmc steps
  int    Ntau     = 200;         // number of imaginary time points for Delta(tau)
  int    Nmax     = 2048;        // maximum number of kinks
  int    nom      = 200;         // number of frequency points sample in qmc
  int    nomD     = 200;         // number of frequency points for self-energy, obtained by the Dyson equation.
                                 // The rest will be computed by two particle correlation function (the direct trick)
  int    nomb     = 50;          // number of bosonic frequencies for susceptibilities
  double PChangeOrder = 0.9;     // probability to change the order (as compared to move a kink)
  double PMove = 0.0;            // Probability to move a kink, versus permute times. Currently relevant only in the segment picture.
  int    tsample  = 30;          // how often to record measurements (each tsample steps)
  int    warmup   = 0;           // how many qmc steps should be ignored
  int CleanUpdate = 1000000;     // clean update is sometimes useful
  double minM     = 1e-15;       // trace shuld always be larger than this minimal value when accepting the step
  double minD     = 0.0;       // determinant of hybrodization should always be larger than this number when accepting the step
  int    Ncout    = 500000;      // screen output for basic information every Ncout steps
  long long  Naver= 50000000000;  // much more information about simulation every Naver steps
  double TwoKinks = 0.;          // probability to add two kinks
  int    GlobalFlip= -1;         // global flip is tried after GlobalFlip qmc steps
  double treshold = 1e-10;       // energy to throw away atomic states when creating new.cix for next iteration
  int  SampleGtau = -1;          // How often to sample for Gtau (if at all)
  int SampleVertex =-1;          // How often to sample two particle vertex
  int  ReduceComm = 0;           // only part of Gtau is transfered between nodes (if slow communication)
  int    Ncorrect = -1;          // baths with index higher than Ncorrect should not be added a high-frequency tail (if -1, all tails are added)
  int         aom = 3;           // number of frequency points to find Sigma(om_last_sampled)
  int         som = 3;           // number of frequency points to find Susc(om_last_sampled)
  int    PreciseP = 1;           // computes probabilities more precisely
  double   sderiv = 0.02;         // maximum mismatch when matching high and low frequency of imaginary part of Sigmas
  double minDeltat= 1e-7;        // Delta(tau) is sometimes not causal due to numerical error. In this case we set it to small value minDeltat.
  bool SampleSusc = false;       // If spin and charge dynamic susceptibility should be sampled during simulation
  int        nomv = 50;          // number of fermionic frequencies for computing vertex
  int         nOm = 1;           // number of bosonic frequencies for vertex calculation
  double maxNoise = 1e100;        // Maximum amount of noise allowed in Green's function
  vector<double> BathProbability(100); // If bath is choosen randomly or depending on number of successful kinks in the bath
  bool  LazyTrace = true;        // This will speed up the code for Full-U calculation. It computes trace only for a few superstate, and neglects some small contributions to trace. While this seems to work very well, it is not exact.
  double MinTraceRatio=1e-6;     // For LazyTrace we need a cutoff for how precise we want to compute trace.
  int     AllRead = 0;           // Should all processes read cix file, or just Master?
  int fastFilesystem = 0;        // Should we write-out Delta.tau and Gaver...
  int iseed_start = 0;           // starting seed
  int Segment = -1;              // Assuming superstates/blocks are all one dimensional -- Segment picture
  int Nhigh   = 40;              // The last Nhigh number of Matsubara points will be used to determine the tails
  bool SampleTransitionP = false;// Should we sample transition probability?
  ifstream inp(inputf.c_str());
  if (!inp.good()) {cerr<<"Missing input file!... exiting "<<endl; return 1;}
  for (int i=0; i<BathProbability.size(); i++) BathProbability[i]=1.0;
  while (inp){
    string str;
    inp>>str;
    if (str[0]=='#') inp.ignore(2000,'\n');
    if (str=="Delta")       inp>>fDelta;
    if (str=="cix")         inp>>fcix;
    if (str=="mu")          inp>>mu;
    if (str=="beta")        inp>>beta;
    if (str=="U")           inp>>U;
    if (str=="M"){
      double dM;
      inp>>dM;
      M = static_cast<long long>(dM);
    }
    if (str=="Ntau")        inp>>Ntau;
    if (str=="Nmax")        inp>>Nmax;
    if (str=="nom")         inp>>nom;
    if (str=="iseed")       inp>>iseed_start;
    if (str=="nomD")        inp>>nomD;
    if (str=="nomv")        inp>>nomv;
    if (str=="nomb")        inp>>nomb;
    if (str=="nOm")         inp>>nOm;
    if (str=="tsample")     inp>>tsample;
    if (str=="warmup")      inp>>warmup;
    if (str=="CleanUpdate") inp>>CleanUpdate;
    if (str=="minM")        inp>>minM;
    if (str=="minD")        inp>>minD;
    if (str=="PChangeOrder")inp>>PChangeOrder;
    if (str=="PMove")       inp>>PMove;
    if (str=="Ncout")       inp>>Ncout;
    if (str=="Naver")       inp>>Naver;
    if (str=="Gf")          inp>>fGf;
    if (str=="Sig")         inp>>fSig;
    if (str=="TwoKinks")    inp>>TwoKinks;
    if (str=="GlobalFlip")  inp>>GlobalFlip;
    if (str=="treshold")    inp>>treshold;
    if (str=="SampleGtau")  inp>>SampleGtau;
    if (str=="ReduceComm")  inp>>ReduceComm;
    if (str=="Ncorrect")    inp>>Ncorrect;
    if (str=="aom")         inp>>aom;
    if (str=="som")         inp>>som;
    if (str=="PreciseP")    inp>>PreciseP;
    if (str=="sderiv")      inp>>sderiv;
    if (str=="minDeltat")   inp>>minDeltat;
    if (str=="SampleSusc")  inp>>SampleSusc;
    if (str=="SampleVertex")inp>>SampleVertex;
    if (str=="maxNoise")    inp>>maxNoise;
    if (str=="LazyTrace")   inp>>LazyTrace;
    if (str=="Segment")     inp>>Segment;
    if (str=="MinTraceRatio")inp>>MinTraceRatio;
    if (str=="Nhigh")       inp>>Nhigh;
    if (str=="SampleTransitionP")  inp>>SampleTransitionP;
    if (str=="BathProbability"){
      char chr[1001];
      inp.getline(chr, 1000, '\n');
      //cout<<"BathProbability="<<chr<<endl;
      stringstream str(chr);
      int ii=0;
      while(str){
	double x;
	str>>x;
	if (!str) break;
	BathProbability[ii++]=x;
	//cout<<x<<endl;
      }
    }
    if (str=="AllRead")     inp>>AllRead;
    if (str=="fastFilesystem") inp>>fastFilesystem;
  }
  common::SaveStatus=true;
  if (nomv>nom) nomv = nom;
  if (nOm>nom) nOm = nom;
  if (SampleVertex<=0){nomv=0; nOm=0; }
  (*xout)<<"Input parameters are:"<<endl;
  (*xout)<<"fDelta="<<fDelta<<endl;
  (*xout)<<"fcix="<<fcix<<endl;
  (*xout)<<"mu="<<mu<<endl;
  (*xout)<<"beta="<<beta<<endl;
  (*xout)<<"U="<<U<<endl;
  (*xout)<<"M="<<M<<endl;
  (*xout)<<"Ntau="<<Ntau<<endl;
  (*xout)<<"Nmax="<<Nmax<<endl;
  (*xout)<<"nom="<<nom<<endl;
  (*xout)<<"nomD="<<nomD<<endl;
  (*xout)<<"nomv="<<nomv<<endl;
  (*xout)<<"nomb="<<nomb<<endl;
  (*xout)<<"nOm="<<nOm<<endl;
  (*xout)<<"PChangeOrder="<<PChangeOrder<<endl;
  (*xout)<<"PMove="<<PMove<<endl;
  (*xout)<<"tsample="<<tsample<<endl;
  (*xout)<<"warmup="<<warmup<<endl;
  (*xout)<<"CleanUpdate="<<CleanUpdate<<endl;
  (*xout)<<"minM="<<minM<<endl;
  (*xout)<<"minD="<<minD<<endl;
  (*xout)<<"Ncout="<<Ncout<<endl;
  (*xout)<<"Naver="<<Naver<<endl;
  (*xout)<<"Gf="<<fGf<<endl;
  (*xout)<<"Sig="<<fSig<<endl;
  (*xout)<<"TwoKinks="<<TwoKinks<<endl;
  (*xout)<<"GlobalFlip="<<GlobalFlip<<endl;
  (*xout)<<"treshold="<<treshold<<endl;
  (*xout)<<"SampleGtau="<<SampleGtau<<endl;
  (*xout)<<"ReduceComm="<<ReduceComm<<endl;
  (*xout)<<"Ncorrect="<<Ncorrect<<endl;
  (*xout)<<"aom="<<aom<<endl;
  (*xout)<<"som="<<som<<endl;
  (*xout)<<"PreciseP="<<PreciseP<<endl;
  (*xout)<<"sderiv="<<sderiv<<endl;
  (*xout)<<"minDeltat="<<minDeltat<<endl;
  (*xout)<<"SampleSusc="<<SampleSusc<<endl;
  (*xout)<<"SampleVertex="<<SampleVertex<<endl;
  (*xout)<<"maxNoise="<<maxNoise<<endl;
  (*xout)<<"AllRead="<<AllRead<<endl;
  (*xout)<<"LazyTrace="<<LazyTrace<<endl;
  (*xout)<<"MinTraceRatio"<<MinTraceRatio<<endl;
  (*xout)<<"fastFilesystem="<<fastFilesystem<<endl;
  (*xout)<<"SampleTransitionP="<<SampleTransitionP<<endl;
  (*xout)<<"SaveStatus="<<common::SaveStatus<<endl;
  ClusterData cluster;
  ifstream incix(fcix.c_str());
  

  if (AllRead){
    cluster.ParsInput(incix);
  }else{
    if (my_rank==Master) cluster.ParsInput(incix);
    cluster.BcastClusterData(my_rank, Master);
  }

  if (!common::QHB2) nomD = nom;
  
  BathProb BathP(BathProbability, cluster);

  cluster.EvaluateMa(beta,mu,U);
  
  if (Segment<0){
    if (cluster.max_size==1) Segment=1; //Segment can be used
    else Segment=0;
  }
  clog<<"Segment="<<Segment<<endl;
  
  common::SetParameters(my_rank,mpi_size,mu,U,beta,cluster.max_size,cluster.N_flavors,cluster.N_ifl,tsample,warmup,
			CleanUpdate,minM,minD,PChangeOrder,PMove,Ncout,Naver,TwoKinks,GlobalFlip,treshold,SampleGtau,PreciseP,
			minDeltat,SampleSusc, SampleVertex, maxNoise,LazyTrace,Segment,fastFilesystem,SampleTransitionP);


  mesh1D iom_large;
  function2D<dcomplex> Deltaw;
  vector<pair<double,double> > ah;
  
  if (AllRead){
    ifstream input(fDelta.c_str());
    int n, m;
    if (!CheckStream(input, n, m)) { cerr<<"Something wrong in reading input Delta="<<fDelta<<endl; return 1;}
    if (!ReadDelta(cluster.N_unique_fl, input, n, m, iom_large, Deltaw, ah, common::beta,Nhigh)){ cerr<<"Something wrong in reading input Delta="<<fDelta<<endl; return 1;}
  }else{
    if (my_rank==Master){
      ifstream input(fDelta.c_str());
      int n, m;
      if (!CheckStream(input, n, m)) { cerr<<"Something wrong in reading input Delta="<<fDelta<<endl; return 1;}
      if (!ReadDelta(cluster.N_unique_fl, input, n, m, iom_large, Deltaw, ah, common::beta,Nhigh)){ cerr<<"Something wrong in reading input Delta="<<fDelta<<endl; return 1;}
    }
    BCastDelta(my_rank, Master, iom_large, Deltaw, ah);
  }
  
  
  //int iseed = time(0)+my_rank*10;
  int iseed = my_rank*10 + iseed_start;
  //int iseed=0;
  RanGSL rand(iseed);
  cout<<"iseed="<<iseed<<" in rank "<<my_rank<<endl;
  
  
  if (common::QHB2) cluster.Compute_Nmatrices();

  CTQMC<RanGSL> ctqmc(rand, cluster, BathP, Nmax*2, iom_large, Deltaw, ah, nom, nomv, nomb, nOm, Ntau, MinTraceRatio, my_rank==Master);

  // Main part of the code : sampling
  ctqmc.RetrieveStatus(my_rank,-1);

  
  double nf = ctqmc.sample(M);
  if (common::SaveStatus) ctqmc.SaveStatus(my_rank);

  // Green's function is compressed to few components only
  function2D<dcomplex> Gd(cluster.N_unique_fl,ctqmc.ioms().size());
  function1D<int> Gd_deg(cluster.N_unique_fl);
  Gd_deg=0;
  Gd=0;
  for (int ifl=0; ifl<cluster.N_ifl; ifl++){
    for (int ib=0; ib<cluster.N_baths(ifl); ib++){
      //cout<<"variation="<<ctqmc.Variation[ifl][ib]<<" and maxvaraition="<<common::maxNoise<<endl;
      if (ctqmc.Variation[ifl][ib]>common::maxNoise) continue;
      int i_unique = cluster.bfl_index[ifl][ib];
      for (int im=0; im<ctqmc.ioms().size(); im++){
	dcomplex gf = ctqmc.G_aver()[ifl](ib,im);
	if (cluster.conjg[ifl][ib]) gf = gf.conj();
	gf *= cluster.sign[ifl][ib];
	Gd(i_unique,im) += gf;
      }
      Gd_deg[i_unique]++;
    }
  }
  
  function2D<dcomplex> Sd;
  if (common::QHB2){
    Sd.resize(cluster.N_unique_fl,ctqmc.ioms().size());
    Sd=0;
    for (int ifl=0; ifl<cluster.N_ifl; ifl++){
      //cout<<"variation="<<ctqmc.Variation[ifl][0]<<" and maxvaraition="<<common::maxNoise<<endl;
      if (ctqmc.Variation[ifl][0]>common::maxNoise) continue;
      for (int im=0; im<ctqmc.ioms().size(); im++){
	dcomplex Fsum=0.0;
	for (int ii=0; ii<cluster.fl_fl.size(); ii++){
	  int ifl1 = cluster.fl_fl[ii].first;
	  int ifl2 = cluster.fl_fl[ii].second;
	  Fsum += ctqmc.Faver(ifl,ii)[im]*cluster.Uc[ifl](ifl1,ifl2);
	}
	dcomplex Sigma = Fsum/ctqmc.G_aver()[ifl](0,im);
	int i_unique = cluster.bfl_index[ifl][0];
	Sd(i_unique,im) += Sigma;
      }
    }
  }
  
  
  // G(tau) is also compressed to few components only
  // the number of time slices is reduces to first and last 3 in case ReduceComm is set to true
  // In this case, full Gtau will not be exchaned or printed, but only 6 times will be printed
  // This is done to prevent SIGFAULT in parallel execution due to low communication pipeline
  int ntau = ctqmc.gtau()[0].size_Nd();
  function2D<double> Gtau;
  function1D<int> tau_ind;
  if (!ReduceComm || ntau<6){
    Gtau.resize(cluster.N_unique_fl,ntau);
    tau_ind.resize(ntau);
    for (int it=0; it<tau_ind.size(); it++) tau_ind[it]=it;
  }  else {
    Gtau.resize(cluster.N_unique_fl,6);
    tau_ind.resize(6);
    for (int it=0; it<3; it++) tau_ind[it] = it;
    for (int it=0; it<3; it++) tau_ind[6-it-1] = ntau-it-1;
  }
  if (common::SampleGtau>0){
    //    double dt = common::beta/ntau;
    Gtau=0;
    for (int iti=0; iti<tau_ind.size(); iti++){
      int it = tau_ind[iti];
      for (int ifl=0; ifl<cluster.N_ifl; ifl++){
	for (int ib=0; ib<cluster.N_baths(ifl); ib++){
	  double gf = ctqmc.gtau()[ifl](ib,it);
	  if (cluster.conjg[ifl][ib]) gf = -ctqmc.gtau()[ifl](ib,ntau-it-1);
	  gf *= cluster.sign[ifl][ib];
	  Gtau(cluster.bfl_index[ifl][ib],iti) += gf;
	}
      }
      for (int fl=0; fl<cluster.N_unique_fl; fl++) Gtau(fl,iti) /= cluster.fl_deg[fl];
    }
  }
  
  Reduce(my_rank, Master, mpi_size, ctqmc.Histogram(), Gd, Sd, ctqmc.AverageProbability(), ctqmc.asign, ctqmc.Observables(),
	 ctqmc.k_aver(), ctqmc.Susc(), Gtau, ctqmc.VertexH, ctqmc.VertexF, Gd_deg, ctqmc.AP_transition, common::cmp_vertex, common::QHB2, common::SampleTransitionP);
  
  if (my_rank==Master){

    (*xout)<<"Number of successfully sampled Green's functions is ";
    for (int i=0; i<Gd_deg.size(); i++) (*xout)<<Gd_deg[i]<<" ";
    (*xout)<<endl;
    // Renormalizing Green's function
    for (int im=0; im<ctqmc.ioms().size(); im++)
      for (int fl=0; fl<cluster.N_unique_fl; fl++)
	Gd(fl,im) /= Gd_deg[fl];
    
    if (common::QHB2){
      for (int im=0; im<ctqmc.ioms().size(); im++)
	for (int fl=0; fl<cluster.N_unique_fl; fl++)
	  Sd(fl,im) /= Gd_deg[fl];
    }
    
    ctqmc.RecomputeCix();
    function1D<double> mom;
    double TrSigmaG=0, Epot=0, Ekin=0;
    ctqmc.ComputeFinalAverage(mom, nf, TrSigmaG, Epot, Ekin);
    (*xout)<<"nf="<<ctqmc.Observables()[0]<<" chiS0="<<ctqmc.Observables()[1]<<" chiD="<<ctqmc.Observables()[2]<<" <<Sz>>="<<ctqmc.Observables()[3]<<" TrSigmaG="<<TrSigmaG<<endl;

    function2D<dcomplex> Gf(cluster.N_unique_fl,iom_large.size());
    function2D<dcomplex> Sd0(cluster.N_unique_fl,ctqmc.ioms().size());
    function2D<dcomplex> Gf_1(cluster.N_unique_fl,iom_large.size());
    function1D<bool> nonzero(cluster.N_ifl), nonzerou(cluster.N_unique_fl);
    nonzero = true; nonzerou = true;
    for (int ifl=0; ifl<cluster.N_ifl; ifl++) // NO DOT INVERSE IF GF=0
      nonzero[ifl] = Qnonzero(Gd[cluster.bfl_index[ifl][0]]);
    for (int fl=0; fl<cluster.N_unique_fl; fl++)
      nonzerou[fl] = Qnonzero(Gd[fl]);
    
    Sd0=0;
    // Computes the high frequency part of Green's function and self-energy 
    Gf=0;
    Gf_1=0;
    for (int im=0; im<ctqmc.ioms().size(); im++){
      dcomplex iomega(0,ctqmc.ioms()[im]);
      for (int fl=0; fl<cluster.N_unique_fl; fl++) Gf(fl,im) = Gd(fl, im);
      for (int ifl=0; ifl<cluster.N_ifl; ifl++){
	if (nonzero[ifl]) // NO DOT INVERSE IF GF=0
	  Inverse_Gf(cluster.ifl_dim[ifl], cluster.N_unique_fl, cluster.bfl_index[ifl], cluster.sign[ifl], cluster.conjg[ifl], im, Gf, Gf_1);
      }
      for (int fl=0; fl<cluster.N_unique_fl; fl++) Gf_1(fl,im) /= cluster.fl_deg[fl];
      for (int fl=0; fl<cluster.N_unique_fl; fl++){
	if (nonzerou[fl])
	  Sd0(fl,im) = (iomega+mu)*cluster.Id[fl]-cluster.epsk[fl]-Gf_1(fl,im)-Deltaw(fl,im);
      }
    }
    function2D<double> nt(2,cluster.N_unique_fl);
    nt=0;
    double dt = common::beta/ntau;
    if (common::SampleGtau>0){
      int nti = tau_ind.size();
      for (int fl=0; fl<cluster.N_unique_fl; fl++){
	if (nonzerou[fl]){
	  nt[0][fl] = expin(0.0,make_pair(0.5*dt,Gtau[fl][0]),make_pair(1.5*dt,Gtau[fl][1]));
	  nt[1][fl] = expin(common::beta,make_pair((ntau-0.5)*dt,Gtau[fl][nti-1]),make_pair((ntau-1.5)*dt,Gtau[fl][nti-2]));
	  cout<<"nt[0]["<<fl<<"]="<<nt[0][fl]<<endl;
	  cout<<"nt[1]["<<fl<<"]="<<nt[1][fl]<<endl;
	}
      }
    }

    function2D<dcomplex> Sigma(cluster.N_unique_fl,iom_large.size());
    Sigma=0;

    for (int fl=0; fl<cluster.N_unique_fl; fl++){
      for (int im=0; im<min(nomD,ctqmc.ioms().size()); im++) Sigma(fl,im) = Sd0(fl,im); // Sigma from Dyson
      for (int im=min(nomD,ctqmc.ioms().size()); im<ctqmc.ioms().size(); im++) Sigma(fl,im) = Sd(fl,im); // Sigma by S-trick
    }
	   
    if (cluster.QHB1){
      cluster.Read_for_HB1(incix, mu, U);
      cluster.HB1(beta, mu, iom_large, ctqmc.AverageProbability(), nom, aom, Deltaw, Sigma, nonzero, nonzerou, sderiv);
    }else{
      ComputeMoments(cluster, iom_large, ctqmc.ioms().size(), nf, mom, ctqmc.k_aver(), nt, Sigma, true, Ncorrect, aom, sderiv);
    }
    Gf=0;
    Gf_1=0;
    for (int im=0; im<iom_large.size(); im++){
      dcomplex iomega(0,iom_large[im]);
      for (int fl=0; fl<cluster.N_unique_fl; fl++){
	if (nonzerou[fl])
	  Gf_1(fl,im) = (iomega+mu)*cluster.Id[fl]-cluster.epsk[fl]-Sigma(fl,im)-Deltaw(fl,im);
      }
      for (int ifl=0; ifl<cluster.N_ifl; ifl++){
	if (nonzero[ifl])
	  Inverse_Gf(cluster.ifl_dim[ifl], cluster.N_unique_fl, cluster.bfl_index[ifl], cluster.sign[ifl], cluster.conjg[ifl], im, Gf_1, Gf);
      }
      for (int fl=0; fl<cluster.N_unique_fl; fl++) Gf(fl,im) /= cluster.fl_deg[fl];
    }
    
    ofstream gout(fGf.c_str()); gout.precision(16);
    gout<<"# nf="<<nf<<" mu="<<mu<<" T="<<1/beta<<" TrSigmaG="<<TrSigmaG<<" Epot="<<Epot<<" Ekin="<<Ekin;
    gout<<" mom=[";
    for (int i=0; i<mom.size(); i++) gout<<mom[i]<<",";
    gout<<"]"<<endl;
    for (int im=0; im<iom_large.size(); im++){
      gout<<iom_large[im]<<" ";
      for (int fl=0; fl<Gf.size_N(); fl++) gout<<Gf(fl,im)<<" ";
      gout<<endl;
    }
    ofstream sout(fSig.c_str()); sout.precision(16);
    sout<<"# nf="<<nf<<" mu="<<mu<<" T="<<1/beta<<" TrSigmaG="<<TrSigmaG<<" Epot="<<Epot<<" Ekin="<<Ekin;
    sout<<" mom=[";
    for (int i=0; i<mom.size(); i++) sout<<mom[i]<<",";
    sout<<"]"<<endl;
    for (int im=0; im<iom_large.size(); im++){
      sout<<iom_large[im]<<" ";
      for (int fl=0; fl<Sigma.size_N(); fl++) sout<<Sigma(fl,im)<<" ";
      sout<<endl;
    }
    double sum=0;
    for (int i=0; i<ctqmc.Histogram().size(); i++) sum += ctqmc.Histogram()[i];
    for (int i=0; i<ctqmc.Histogram().size(); i++) ctqmc.Histogram()[i]/=sum;
    ofstream hout("histogram.dat");
    hout<<"# nf="<<nf<<" TrSigmaG="<<TrSigmaG<<" mom=[";
    for (int i=0; i<mom.size(); i++) hout<<mom[i]<<",";
    hout<<"]"<<endl;
    for (int i=0; i<ctqmc.Histogram().size()/2; i++)
      hout<<setw(3)<<i<<"  "<<setw(10)<<ctqmc.Histogram()[i]<<endl;

    if (common::QHB2){
      ofstream sout((fSig+"D").c_str()); sout.precision(16);
      sout<<"# nf="<<nf<<" mu="<<mu<<" T="<<1/beta<<" TrSigmaG="<<TrSigmaG<<" mom=[";
      for (int i=0; i<mom.size(); i++) sout<<mom[i]<<",";
      sout<<"]"<<endl;
      for (int im=0; im<ctqmc.ioms().size(); im++){
	sout<<ctqmc.ioms()[im]<<" ";
	for (int fl=0; fl<Sd0.size_N(); fl++) sout<<Sd0(fl,im)<<" ";
	sout<<endl;
      }
      sout.close();
      ofstream fout((fSig+"B").c_str()); fout.precision(16);
      fout<<"# nf="<<nf<<" mu="<<mu<<" T="<<1/beta<<" TrSigmaG="<<TrSigmaG<<" mom=[";
      for (int i=0; i<mom.size(); i++) fout<<mom[i]<<",";
      fout<<"]"<<endl;
      for (int im=0; im<ctqmc.ioms().size(); im++){
	fout<<ctqmc.ioms()[im]<<" ";
	for (int fl=0; fl<Sd.size_N(); fl++){
	  fout<<Sd(fl,im)<<" ";
	}
	fout<<endl;
      }
      fout.close();
    }

    if (common::SampleGtau>0){
      ofstream out("Gtau.dat"); out.precision(16);
      double dt = common::beta/ntau;
      for (int iti=0; iti<Gtau.size_Nd(); iti++){
	int it = tau_ind[iti];
	out<<setw(20)<<dt*(it+0.5)<<" ";
	for (int fl=0; fl<cluster.N_unique_fl; fl++){
	  out<<setw(25)<<Gtau[fl][iti]<<" ";
	}
	out<<endl;
      }
    }

    if (common::SampleSusc){
      ofstream out("susc.dat"); out.precision(16);
      int nsusc = ctqmc.Susc().size_N();
      int nom_s = ctqmc.iomb().size();
      int nom_l = iom_large.size();
      out<<0.0<<" "<<ctqmc.Observables()[1]<<" "<<ctqmc.Observables()[2]<<endl;
      for (int im=0; im<nom_s; im++){
	out<<ctqmc.iomb()[im]<<" ";
	for (int l=0; l<nsusc; l++) out<<ctqmc.Susc()(l,im)<<" ";
	out<<endl;
      }
      function1D<double> alpha_susc(nsusc);
      alpha_susc=0;
      for (int im=nom_s-1; im>nom_s-1-som; im--){
	for (int l=0; l<nsusc; l++) alpha_susc[l] += ctqmc.Susc()(l,im);
      }
      double om2 = sqr(ctqmc.iomb()[nom_s-1-som/2]);
      alpha_susc *= om2/som;
      for (int im=nom_s; im<nom_l; im++){
	double omega = 2*im*M_PI/common::beta;
	out<<omega<<" ";
	for (int l=0; l<nsusc; l++) out<<alpha_susc[l]/sqr(omega)<<" ";
	out<<endl;
      }
    }
    if (common::SampleTransitionP){
      ofstream out("TProbability.dat"); out.precision(16);
      out<<"# asign="<<ctqmc.asign<<endl;
      for (int ist=1; ist<=ctqmc.AP_transition.size_N(); ist++){
	// For convenience we sort the operators such that final states are sorted in the output
	vector<int> index(2*common::N_flavors);
	for (int op=0; op<2*common::N_flavors; op++) index[op]=op;
	CustomLess customLess(&cluster.F_i,ist);
	std::sort(index.begin(), index.end(), customLess);
	
	for (int op=0; op<2*common::N_flavors; op++){
	  int op_sorted = index[op];
	  int inew = cluster.F_i(op_sorted,ist);
	  if (inew>0)
	    out<<setw(3)<<ist<<" "<<setw(3)<<inew<<" "<<setw(20)<<ctqmc.AP_transition[ist-1][op_sorted]/ctqmc.asign<<endl;
	}
      }
    }
  }

  if (common::cmp_vertex && my_rank==Master){
    
    function2D<dcomplex> BubbleH(2*nomv,2*nomv), BubbleF(2*nOm-1,2*nomv);

    ofstream cvout("tvertex.dat");
    cvout<<"# beta, Nvfl, nomv, nOm nom"<<endl;
    cvout<<beta<<" "<<cluster.Nvfl<<" "<<nomv<<" "<<nOm<<" "<<nom<<endl;
    for (int i0=0; i0<cluster.Nvfl; i0++){
      TBath ind0 = cluster.vfli_index[i0];
      int ind0t = cluster.bfl_index[ind0.ifl][ind0.bind];      
      cvout<<i0<<" "<<ind0t<<endl;
    }
    cvout<<"# b0 b1 Om om1"<<endl;
    
    cout<<"Printing vertex"<<endl;
    for (int i0=0; i0<ctqmc.VertexH.N0; i0++){
      TBath ind0 = cluster.vfli_index[i0];
      int ind0t = cluster.bfl_index[ind0.ifl][ind0.bind];      
      for (int i1=0; i1<ctqmc.VertexH.N1; i1++){
	TBath ind1 = cluster.vfli_index[i1];
	int ind1t = cluster.bfl_index[ind1.ifl][ind1.bind];
	funProxy<dcomplex>& _Gd0 = Gd[ind0t];
	funProxy<dcomplex>& _Gd1 = Gd[ind1t];

	// BubbleH
	for (int im1=0; im1<2*nomv; im1++){
	  dcomplex gd0 = (im1>=nomv) ? _Gd0[im1-nomv] : _Gd0[nomv-im1-1].conj();
	  for (int im2=0; im2<2*nomv; im2++){
	    dcomplex gd1 = (im2>=nomv) ? _Gd1[im2-nomv] : _Gd1[nomv-im2-1].conj();
	    BubbleH(im1,im2) = gd0*gd1;
	  }
	}
	
	// BubbleF
	for (int iOm=0; iOm<2*nOm-1; iOm++){
	  int dOm = nOm-1-iOm;
	  int sm1 = max(0, -dOm);
	  int em1 = min(2*nomv, 2*nomv-dOm);
	  for (int im1=sm1; im1<em1; im1++){
	    dcomplex gd0 = (im1>=nomv) ? _Gd0[im1-nomv] : _Gd0[nomv-im1-1].conj();	    
	    int im2 = im1+dOm;
	    if (im2<0 || im2>=2*nomv) continue;
	    dcomplex gd1 = (im2>=nomv) ? _Gd1[im2-nomv] : _Gd1[nomv-im2-1].conj();
	    BubbleF(iOm,im1) = gd0*gd1;
	  }
	}
      
      
	for (int iOm=0; iOm<2*nOm-1; iOm++){
	  int dOm = nOm-1-iOm;
	  int sm1 = max(0, -dOm);
	  int em1 = min(2*nomv, 2*nomv-dOm);
	  for (int im1=sm1; im1<em1; im1++){
	    int sm2 = max(0, -dOm);
	    int em2 = min(2*nomv, 2*nomv-dOm);

	    stringstream strc;
	    strc<<"vertex."<<i0<<"."<<i1<<"."<<iOm<<"."<<im1;
	    ofstream vout(strc.str().c_str());

	    cvout<<i0<<" "<<i1<<" "<<2*(iOm-nOm+1)*M_PI/beta<<" "<<(2*(im1-nomv)+1)*M_PI/beta<<endl;
	    
	    for (int im2=sm2; im2<em2; im2++){
	      dcomplex bH = (iOm == nOm-1) ? BubbleH(im1,im2) : 0.0;
	      dcomplex bF = (im1 == im2) ? BubbleF(iOm,im1) : 0.0;
	      double om2 = (2*(im2-nomv)+1)*M_PI/beta;
	      vout<<om2<<" ";
	      vout<<ctqmc.VertexH(i0,i1,iOm,im1,im2)-bH<<" ";
	      vout<<bH<<" ";
	      vout<<ctqmc.VertexF(i0,i1,iOm,im1,im2)-bF<<" ";
	      vout<<bF<<endl;
	      
	      cvout<<om2<<" "<<ctqmc.VertexH(i0,i1,iOm,im1,im2)-bH<<"   "<<ctqmc.VertexF(i0,i1,iOm,im1,im2)-bF<<endl;
	    }
	  }
	}
	
      }
    }
  }
  if (my_rank==Master){
    delete xout;
  }
  
  MPI_finalize();
  return 0;
}
double common::beta;
double common::U;
double common::mu;
int common::max_size;
int common::N_flavors;
double common::minM;
double common::minD;
int common::tsample;
int common::warmup;
int common::CleanUpdate;
double common::PChangeOrder;
double common::PMove;
int common::Ncout;
long long common::Naver;
const int nIntervals::c;
const int nIntervals::cd;
int common::my_rank;
int common::mpi_size;
int common::N_ifl;
double common::TwoKinks;
double common::treshold;
int common::GlobalFlip;
int common::SampleGtau;
int common::PreciseP;
double common::minDeltat;
bool common::SampleSusc;
bool common::cmp_vertex;
int common::SampleVertex;
double common::maxNoise;
int common::fastFilesystem;
bool common::LazyTrace;
bool common::QHB2;
Timer NState::t_evolve;
Timer NState::t_apply;
double common::smallest_dt;
int common::Segment;
bool common::SampleTransitionP;
bool common::SaveStatus;
