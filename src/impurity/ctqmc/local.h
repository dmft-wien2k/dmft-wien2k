// @Copyright 2007 Kristjan Haule
// 

void StartStateEvolution(function2D<NState>& state_evolution_left,
			 function2D<NState>& state_evolution_right,
			 const function1D<NState>& praStates,
			 NOperators& Operators, const ClusterData& cluster,
			 function1D<Number>& Trace, function2D<double>& Prob)
{
  if (!Operators.empty()){cerr<<"Operators are not empty!"<<endl;}
  function2D<Number> Projection(Prob.size_N(),common::max_size);
  NState nstate(common::max_size,common::max_size), mstate(common::max_size,common::max_size);
  Number ms=0;
  
  for (int i=0; i<cluster.size(); i++){
    if (praStates[i].empty()) {
      state_evolution_left[i].resize(0);
      state_evolution_right[i].resize(0);
      continue;
    }
    state_evolution_left[i].resize(1);
    state_evolution_right[i].resize(1);
    
    nstate = praStates[i];
    mstate.Evolve(nstate, cluster,  Operators.exp_(0));
    state_evolution_left(i,0).SetEqual(mstate);
    
    mstate.Evolve(nstate, cluster, Operators.exp_(0));
    state_evolution_right(i,0).SetEqual(mstate);
    
    Trace[i] = state_evolution_left[i][0].Project_to_Pra(praStates[i], Projection[i]);
    ms += Trace[i];
  }
  for (int i=0; i<cluster.size(); i++)
    for (int l=0; l<Prob[i].size(); l++){
      Prob[i][l] = divide(Projection[i][l],ms);
#ifdef _DEBUG	
      if (fabs(Prob(i,l))>2.) cerr<<"WARNING : Prob>2 "<<i<<" "<<l<<" "<<Prob(i,l)<<endl;
#endif		
    }
}




void Get_N_at_Operator2(functionb<double>& Njc, int ops, const function1D<Number>& Trace, const Number& ms,
			const function2D<NState>& state_evolution_left, const function2D<NState>& state_evolution_right,
			const NOperators& Operators, const function1D<NState>& praStates, const ClusterData& cluster, long long istep)
{// This routine returns the value of operator N measured at an existing creation operator, which has index ops.
 // If we have diagram with the following configuration <...... psi^+(t_s) ........>
 // We evaluate the following quantity:   <...... psi^+(t_s)psi^+{a}(t_s)psi{b}(t_s) ........>/<...... psi^+(t_s) ........>
 // for two baths {a} and {b}, which have a common index ii.
  Njc=0.0;
  static NState lstate(common::max_size,common::max_size), rstate(common::max_size,common::max_size), lpstate(common::max_size,common::max_size);
  static Number mm;
  for (int ist=0; ist<praStates.size(); ist++){
    double Zi = (Trace[ist]/ms).dbl();
    if (state_evolution_left[ist].size()>Operators.size() && state_evolution_right[ist].size()>Operators.size() && fabs(Zi)>1e-10){
      if (ops!=0) lstate = state_evolution_left[ist][ops-1];
      else lstate = praStates[ist];
      rstate = state_evolution_right[ist][Operators.size()-1-ops];
      int istate = lstate.istate;
      int irstate = rstate.istate;
      if (istate!=irstate) cerr<<"ERROR: states are different"<<istate<<" "<<irstate<<" "<<istep<<endl;
      // we need to evolve because it should be before the operator.
      // but we carry out this only if there is at least one non-trivial N value for this istate.
      if (cluster.Njm_z[istate]){
	lpstate.Evolve(lstate, cluster, Operators.exp_(ops));
      }
      for (int fl2=0; fl2<cluster.Nvfl; fl2++){
	switch (cluster.Njm_c(istate,fl2)){
	case 0: // nothing to add because N=0
	  break;
	case 1: // N is just an idenity. No need to multiply.
	  Njc[fl2] += Zi;
	  break;
	case 2: // N is not simple indentity or zero, hence we need to evaluate the product
	  Multiply(lstate.M, cluster.Njm(istate,fl2), lpstate.M);
	  lstate.exponent = lpstate.exponent;
	  Njc[fl2] += (rstate.ScalarProduct(lstate)/ms).dbl();
	  break;
	}
      }
    }
  }
}

inline bool Is_State_Proportional(const NState& nstate0, const NState& nstate, double& r)
{
  if (nstate.istate != nstate0.istate) return false;
  double small=1e-7;
  pair<int,int> ik_max = make_pair(0,0);
  double max_val=0;
  for (int i=0; i<nstate0.M.size_N(); i++){
    for (int k=0; k<nstate0.M.size_Nd(); k++){// find largest element in M[i,:]
      if (fabs(nstate0.M(i,k))>max_val){
	ik_max.first=i;
	ik_max.second=k;
	max_val = fabs(nstate0.M(i,k));
      }
    }
  }
  r = nstate.M(ik_max.first,ik_max.second)/nstate0.M(ik_max.first,ik_max.second); // current ratio
  if (r==0 || nstate0.M(ik_max.first,ik_max.second)==0) return false;
  
  double diff=0.0;
  for (int i=0; i<nstate.M.size_N(); i++){
    for (int j=0; j<nstate.M.size_Nd(); j++){
      diff += fabs(nstate.M(i,j)-r*nstate0.M(i,j));
      if (diff>small) return false;
    }
  }
  bool proportional = diff<small;

  return proportional;
  
  double precise_r=0; int nr=0;
  if (proportional){
    for (int i=0; i<nstate.M.size_N(); i++){
      for (int j=0; j<nstate.M.size_Nd(); j++){
	if (nstate0.M(i,j)>1e-5){
	  precise_r += nstate.M(i,j)/nstate0.M(i,j);
	  nr++;
	}
      }
    }
    if (nr>0) r = precise_r/nr;
  }
  return proportional;
}



void ComputePreciseProbability(function2D<double>& Prob, const Number& ms, const function2D<NState>& state_evolution_left, const function2D<NState>& state_evolution_right,
			       const NOperators& Operators, const function1D<NState>& praStates, const ClusterData& cluster, long long istep)
{
  // This does not need to be done every time. You should only do it when we meassure!!!
  int npra = praStates.size();
  int plda = Prob.lda();

  int nsize = Operators.size();
  if (nsize==0){
    for (int ist=0; ist<npra; ist++)
      for (int m=0; m<Prob[ist].size(); m++)
	Prob(ist,m) = cluster.P_atom(ist,m);
  }else{
    for (int ist=0; ist<npra; ist++){
      for (int m=0; m<Prob[ist].size(); m++) Prob(ist,m) = 0.0;
    }
      
    for (int ist=0; ist<npra; ist++){
      if (state_evolution_left[ist].size()<=Operators.size() || state_evolution_right[ist].size()<=Operators.size()) continue;
	
      for (int l=0; l<nsize-1; l++){
	const NState& lstate = state_evolution_left(ist,l);
	const NState& rstate = state_evolution_right(ist,nsize-2-l);
	int istate = lstate.istate;
	  
	if (lstate.istate != rstate.istate){
	  cout<<"ERROR in computing Probability at istep="<<istep<<": lstate.istate="<<lstate.istate<<" rstate.istate="<<rstate.istate<<" at ist="<<ist<<endl;

	  cout<<"prastate="<<praStates[ist].istate<<endl;
	  for (int i=0; i<state_evolution_left[ist].size()-1; i++) {
	    cout<<"left "<<i<<" "<<state_evolution_left[ist][i].istate<<"    operator:"<<Operators.typ(i)<<endl;
	  }
	  for (int i=0; i<state_evolution_right[ist].size(); i++) {
	    cout<<"right "<<i<<" "<<state_evolution_right[ist][i].istate<<endl;
	  }
	}
	  
	const double* __restrict__ lstateM = lstate.M.MemPt();
	const double* __restrict__ rstateM = rstate.M.MemPt();
	int nk = lstate.M.size_Nd();
	int ldal = lstate.M.lda();
	int ldar = rstate.M.lda();
	  
	const double* __restrict__ expn = &Operators.exp_(l+1)[cluster.Eind(istate,0)];
	double* __restrict__ _Prb_ = &Prob((istate-1),0);
	double dtau = (Operators.t(l+1)-Operators.t(l))/common::beta;
	double dtau_ms_mantisa = dtau/ms.mantisa;
	double out_exponents = lstate.exponent+rstate.exponent-ms.exponent;

	for (int n=0; n<lstate.M.size_N(); n++){
	  double mantisa=0;
	  for (int m=0; m<nk; m++) mantisa += lstateM[n*ldal+m]*rstateM[n*ldar+m];
	  _Prb_[n] += mantisa*dtau_ms_mantisa*exp(expn[n]+out_exponents); // adding exponents because left state ends with apply and right state ends with apply, and the two are connected through single time evolution.
	}
      }
      {
	const NState& lstate = state_evolution_left(ist,nsize);
	double* __restrict__ _Prb1_ = &Prob((lstate.istate-1),0);
	const double* __restrict__ lstateM1 = lstate.M.MemPt();
	int ldal = lstate.M.lda();
    
	double dtau = 1 - (Operators.t(nsize-1)-Operators.t(0))/common::beta;
	double dtau_ms_mantisa_exp = dtau/ms.mantisa * exp(lstate.exponent-ms.exponent); // missing pice associated with e^{-t_0*E}
	for (int n=0; n<lstate.M.size_N(); n++){
	  _Prb1_[n] += lstateM1[n*ldal+n]*dtau_ms_mantisa_exp;
	}
      }
    }
  }
  double sum=0;
  for (int ist=0; ist<npra; ist++){
    for (int l=0; l<Prob[ist].size(); l++){
      sum += Prob(ist,l);
#ifdef _DEBUG	
      if (fabs(Prob(ist,l))>2.) cerr<<"WARNING : Prob>2 "<<ist<<" "<<l<<" "<<Prob(ist,l)<<endl;
#endif		
    }
  }
  if (fabs(sum-1.)>1e-4) cout<<"WARN: Psum = "<<sum<<endl;
}






Number UpdateStateEvolution(function2D<NState>& state_evolution_left, function2D<NState>& state_evolution_right, function1D<NState>& state_evolution_copy,
			    int op_i, int op_f, const NOperators& Operators, const function1D<NState>& praStates, const ClusterData& cluster,
			    function1D<Number>& Trace, function2D<double>& Prob, int dsize, long long istep, bool cmp_P=true)
{
  static function2D<Number> Projection(Prob.size_N(),common::max_size);
  static NState nstate(common::max_size,common::max_size), mstate(common::max_size,common::max_size);
  static int N_calculated=0, N_updated=0;
  Number ms=0;
  Projection=0;
  int nsize = Operators.size();
  
  for (int ist=0; ist<praStates.size(); ist++){// op_i is the earliest operator inserted - from this point on, the evolution needs to be changed
    Trace[ist]=0;
    if (op_i>0 && (state_evolution_left[ist].size()<op_i || state_evolution_left[ist][op_i-1].empty())) continue;// this product matrix element is zero
    int ip = op_i;
    
    if (ip==0) nstate = praStates[ist];// we need to start from scratch because op_i iz zero
    else nstate = state_evolution_left[ist][ip-1];// we start from the previous operator and update from there on
    if (nstate.empty()) continue;
      
    bool empty=false;
    for (; ip<Operators.size(); ip++){        // go over the rest of the operators
      mstate.Evolve(nstate, cluster, Operators.exp_(ip)); // state evolution due to local Hamiltonian (is diagonal)
      int iop = Operators.typ(ip);
      nstate.apply(cluster.FM(iop), cluster.Fi(iop), mstate);        // the next operator is applied
      if (nstate.empty()) {
	state_evolution_left[ist][ip].istate=0;
	empty=true;
	break;
      }// maybe matrix element is zero
#define _JUST_UPDATE      
#ifdef _JUST_UPDATE      
      N_calculated++;

      if (ip > op_f+dsize+1 && ip>op_f && istep>0 && state_evolution_left[ist].size()>ip-dsize){
	double r=1;
	if (dsize<=0){
	  if (Is_State_Proportional(state_evolution_left[ist][ip-dsize], nstate, r)){
	    double log_r, cm;
	    if (r<0){
	      log_r = log(-r);
	      cm = -1.0;
	    }else{
	      log_r = log(r);
	      cm = 1.0;
	    }
	    double log_ratio = log_r + nstate.exponent - state_evolution_left[ist][ip-dsize].exponent;
	    if (dsize==0){
	      for (int iip=ip; iip<state_evolution_left[ist].size(); ++iip){
		state_evolution_left[ist][iip].exponent += log_ratio;
		if (cm<0) state_evolution_left[ist][iip].M *= -1.0;
		N_updated++;
	      }
	    }else{
	      for (int iip=ip; iip<state_evolution_left[ist].size(); iip++){
		state_evolution_left[ist][iip].SetEqual(state_evolution_left[ist][iip-dsize]);
		state_evolution_left[ist][iip].exponent += log_ratio;
		if (cm<0) state_evolution_left[ist][iip].M *= -1.0;
		N_updated++;
	      }
	      state_evolution_left[ist].resize(state_evolution_left[ist].size()+dsize); // we have stored state evolution, remember the new size
	    }
	    empty = state_evolution_left[ist].size()<=nsize;
	    if (!empty) mstate = state_evolution_left[ist].last();
	    goto loops_out;
	  }
	}else{
	  if (Is_State_Proportional(state_evolution_copy[ip-dsize], nstate, r)){
	    double log_r, cm;
	    if (r<0){
	      log_r = log(-r);
	      cm = -1.0;
	    }else{
	      log_r = log(r);
	      cm = 1.0;
	    }
	    double log_ratio = log_r + nstate.exponent - state_evolution_copy[ip-dsize].exponent;
	    {
	      for (int iip=state_evolution_left[ist].size()-1+dsize; iip>=ip+dsize; --iip){
		state_evolution_left[ist][iip].SetEqual(state_evolution_left[ist][iip-dsize]);
		state_evolution_left[ist][iip].exponent += log_ratio;
		if (cm<0) state_evolution_left[ist][iip].M *= -1.0;
		N_updated++;
	      }
	      for (int iip=ip+dsize-1; iip>=ip; --iip){
		state_evolution_left[ist][iip].SetEqual(state_evolution_copy[iip-dsize]);
		state_evolution_left[ist][iip].exponent += log_ratio;
		if (cm<0) state_evolution_left[ist][iip].M *= -1.0;
		N_updated++;
	      }
	      state_evolution_left[ist].resize(state_evolution_left[ist].size()+dsize); // we have stored state evolution, remember the new size
	    }
	    empty = state_evolution_left[ist].size()<=nsize;
	    if (!empty) mstate = state_evolution_left[ist].last();
	    goto loops_out;
	  }
	}
      }
      if (dsize>0 && ip > op_f+1){ // we need these states later on to see if proportional, and we should store them
	state_evolution_copy[ip].SetEqual(state_evolution_left[ist][ip]);
      }
#endif      
      state_evolution_left[ist][ip].SetEqual(nstate);      // it is not, store it for next time
    }
    if (!empty){ // from the last operator to beta
      mstate.Evolve(nstate, cluster, Operators.exp_last()); // The last evolution of the state due to H_loc.
      state_evolution_left[ist][ip++].SetEqual(mstate); // store it
    }
    if (ip>state_evolution_left.fullsize_Nd()) {cerr<<"Trying to resize to "<<ip<<" op_size="<<Operators.size()<<endl;}
    state_evolution_left[ist].resize(ip); // we have stored state evolution, remember the new size

  loops_out:;
    
    Number mm=0;
    if (!empty) mm = mstate.Project_to_Pra(praStates[ist], Projection[ist]);
    Trace[ist] = mm;
    ms += mm;
  }
  
  Number ms_right=0;
  for (int ist=0; ist<praStates.size(); ist++){// op_i is the earliest operator inserted - from this point on, the evolution needs to be changed
    int nsize = Operators.size();
    int ip = nsize-1-op_f;
    if (ip>0 && (state_evolution_right[ist].size()<ip || state_evolution_right[ist][ip-1].empty())) continue;// this product matrix element is zero
    if (ip==0) nstate = praStates[ist];// we need to start from scratch because op_i iz zero
    else nstate = state_evolution_right[ist][ip-1];// we start from the previous operator and update from there on
    if (nstate.empty()) continue;
    
    bool empty=false;
    if (!empty && ip<nsize){ // from the last operator to beta
      if (ip>0) mstate.Evolve(nstate, cluster, Operators.exp_(nsize-ip)); // The last evlution of the state due to H_loc.
      else 	mstate.Evolve(nstate, cluster, Operators.exp_last()); // The last evlution of the state due to H_loc.
      int iop = Operators.typ_transpose(nsize-1-ip);// HERE iop==-1 !!!!
      nstate.apply(cluster.FM(iop), cluster.Fi(iop), mstate);        // the next operator is applied
      if (nstate.empty()) {
	empty=true;
	state_evolution_right[ist][ip].istate=0;
      }// maybe matrix element is zero
      if (!empty) state_evolution_right[ist][ip++].SetEqual(nstate); // store it
    }
    if (!empty){
      for (; ip<Operators.size(); ip++){        // go over the rest of the operators
	mstate.Evolve(nstate, cluster, Operators.exp_(nsize-ip)); // state evolution due to local Hamiltonian (is diagonal)
	int iop = Operators.typ_transpose(nsize-1-ip);
	nstate.apply(cluster.FM(iop), cluster.Fi(iop), mstate);        // the next operator is applied
	if (nstate.empty()) {
	  empty=true;
	  state_evolution_right[ist][ip].istate=0;
	  break;
	}// maybe matrix element is zero
#define _JUST_UPDATE2
#ifdef _JUST_UPDATE2
	N_calculated++;
	if (ip>nsize+1-op_i && ip>=nsize-op_i+2+dsize && state_evolution_right[ist].size()>ip-dsize){
	  double r=1;
	  if (dsize<=0){
	    if (Is_State_Proportional(state_evolution_right[ist][ip-dsize], nstate, r)){
	      double log_r, cm;
	      if (r<0){
		log_r = log(-r);
		cm = -1.0;
	      }else{
		log_r = log(r);
		cm = 1.0;
	      }
	      double log_ratio = log_r + nstate.exponent - state_evolution_right[ist][ip-dsize].exponent;
	      if (dsize==0){
		for (int iip=ip; iip<state_evolution_right[ist].size(); ++iip){
		  state_evolution_right[ist][iip].exponent += log_ratio;
		  if (cm<0) state_evolution_right[ist][iip].M *= -1.0;
		  N_updated++;
		}
	      }else{
		for (int iip=ip; iip<state_evolution_right[ist].size(); ++iip){
		  state_evolution_right[ist][iip].SetEqual(state_evolution_right[ist][iip-dsize]);
		  state_evolution_right[ist][iip].exponent += log_ratio;
		  if (cm<0) state_evolution_right[ist][iip].M *= -1.0;
		  N_updated++;
		}
		state_evolution_right[ist].resize(state_evolution_right[ist].size()+dsize); // we have stored state evolution, remember the new size
	      }
	      empty = state_evolution_right[ist].size()<=nsize;
	      if (!empty) mstate = state_evolution_right[ist].last();
	      goto loops_out2;
	    }
	  }else{
	    if (Is_State_Proportional(state_evolution_copy[ip-dsize], nstate, r)){
	      double log_r, cm;
	      if (r<0){
		log_r = log(-r);
		cm = -1.0;
	      }else{
		log_r = log(r);
		cm = 1.0;
	      }
	      double log_ratio = log_r + nstate.exponent - state_evolution_copy[ip-dsize].exponent;
	      {
		for (int iip=state_evolution_right[ist].size()-1+dsize; iip>=ip+dsize; --iip){
		  state_evolution_right[ist][iip].SetEqual(state_evolution_right[ist][iip-dsize]);
		  state_evolution_right[ist][iip].exponent += log_ratio;
		  if (cm<0) state_evolution_right[ist][iip].M *= -1.0;
		  N_updated++;
		}
		for (int iip=ip+dsize-1; iip>=ip; --iip){
		  state_evolution_right[ist][iip].SetEqual(state_evolution_copy[iip-dsize]);
		  state_evolution_right[ist][iip].exponent += log_ratio;
		  if (cm<0) state_evolution_right[ist][iip].M *= -1.0;
		  N_updated++;
		}
		state_evolution_right[ist].resize(state_evolution_right[ist].size()+dsize); // we have stored state evolution, remember the new size
	      }
	      empty = state_evolution_right[ist].size()<=nsize;
	      if (!empty) mstate = state_evolution_right[ist].last();
	      goto loops_out2;
	    }
	  }
	}
	if (dsize>0 && ip>nsize-op_i+1){ // we need these states later on to see if proportional, and we should store them
	  state_evolution_copy[ip].SetEqual(state_evolution_right[ist][ip]);
	}
#endif 	
	state_evolution_right[ist][ip].SetEqual(nstate);      // it is not, store it for next time
      }
    }
    if (!empty){
      mstate.Evolve(nstate, cluster, Operators.exp_(0)); // The last evlution of the state due to H_loc.
      state_evolution_right[ist][ip++].SetEqual(mstate); // store it
    }
    if (ip>state_evolution_right.fullsize_Nd()) {cerr<<"Trying to resize to "<<ip<<" op_size="<<Operators.size()<<endl;}
    state_evolution_right[ist].resize(ip); // we have stored state evolution, remember the new size

  loops_out2:;
    
    Number mm=0;
    if (!empty) mm = mstate.TraceProject(praStates[ist]); // need the trace. The last state needs to be projected to the first state
    ms_right += mm;
  }
  
  if (fabs(fabs(divide(ms_right,ms))-1)>1e-3){
    cout.precision(16);
    double ratio = fabs(divide(ms_right,ms));
    
    cout<<"ERROR at istep = "<<istep<<" ms left and ms right are not the same : ratio="<<ratio<<" msl="<<ms<<" msr="<<ms_right<<endl;
    for (int ist=0; ist<praStates.size(); ist++){
      if (Trace[ist].mantisa!=0){
	cout<<" state_evolution_left["<<ist<<"].size="<<state_evolution_left[ist].size()<<" state_evolution_right.size["<<ist<<"]="<<state_evolution_right[ist].size()<<endl;
	/*
	cout<<praStates[ist]<<endl;
	for (int ip=0; ip<nsize-1; ip++){
	  cout<<state_evolution_left[ist][ip]<<" "<<state_evolution_right[ist][nsize-2-ip]<<endl;
	}
	*/
      }
    }
    if (fabs(divide(ms_right,ms))>1) ms = ms_right;
  }

  // This does not need to be done every time. You should only do it when we meassure!!!
  if (common::PreciseP && cmp_P){ // computes probability more precisely
    ComputePreciseProbability(Prob, ms, state_evolution_left, state_evolution_right, Operators, praStates, cluster, istep);
  }else{
    double sum=0;
    for (int ist=0; ist<praStates.size(); ist++){
      for (int l=0; l<Prob[ist].size(); l++){
	Prob[ist][l] = divide(Projection[ist][l],ms);
	sum += Prob[ist][l];
      }
    }
  }
  return ms;
}

Number ComputeTrace(const function2D<NState>& state_evolution_left,
		    const function2D<NState>& state_evolution_right,
		    int op_i, int op_f,
		    const NOperators& Operators, const function1D<NState>& praStates,
		    const ClusterData& cluster,
		    int dsize)
{
  
  static NState nstate(common::max_size,common::max_size), mstate(common::max_size,common::max_size);
  Number ms_new=0;
  for (int ist=0; ist<praStates.size(); ist++){
    if (op_i>0 && (state_evolution_left[ist].size()<op_i || state_evolution_left[ist][op_i-1].empty())) continue;
    int ip = op_i;
    if (ip==0) nstate = praStates[ist];
    else nstate = state_evolution_left[ist][ip-1];
    if (nstate.empty()) continue;
      
    int nsize = Operators.size();

    // Go through states and check that matrix element is indeed nonzero
    // Do not multiply matrices yet.
    bool alive = true;
    int instate = nstate.istate;
    int ip0=ip;
    for (; ip0<=op_f; ip0++){
      int iop = Operators.tr_typ(ip0);
      instate = cluster.Fi(iop)[instate];
      if (instate==0) {	alive=false; break; }
    }
    if (!alive) continue;
    if (op_f<nsize+dsize-1){
      alive = (state_evolution_right[ist].size()>nsize+dsize-op_f-2 && instate==state_evolution_right[ist][nsize+dsize-op_f-2].istate);
    }else{
      alive = (praStates[ist].istate == instate);
    }
    if (!alive) continue;
        
    Number mm_new=0;
    bool empty=false;
    
    for (; ip<=op_f; ip++){
      mstate.Evolve(nstate, cluster, Operators.tr_exp_(ip));
      int iop = Operators.tr_typ(ip);
      nstate.apply(cluster.FM(iop), cluster.Fi(iop), mstate);
      if (nstate.empty()) {empty=true; break;}
    }
    if (!empty){
      if (op_f<nsize+dsize-1){
	if (state_evolution_right[ist].size()>nsize+dsize-op_f-2 &&
	    nstate.istate==state_evolution_right[ist][nsize+dsize-op_f-2].istate){
	  mstate.Evolve(nstate, cluster, Operators.tr_exp_(ip));
	  mm_new = mstate.ScalarProduct(state_evolution_right[ist][nsize+dsize-op_f-2]);
	}else{
	  mm_new = 0;
	}
      }else{
	mstate.Evolve(nstate, cluster, Operators.tr_exp_last());
	mm_new = mstate.TraceProject(praStates[ist]);
      }
    }
    ms_new += mm_new;
    //cout<<"o_"<<ist<<" "<<mm_new<<endl;
  }
  return abs(ms_new);
}

Number ComputeGlobalTrace(const NOperators& Operators, const function1D<NState>& praStates, const ClusterData& cluster, const functionb<int>& gflip_fl)
{
  static NState nstate, mstate;
  Number ms_new=0;
  int nsize = Operators.size();
  for (int ist=0; ist<praStates.size(); ist++){
    int ip = 0;
    nstate = praStates[ist];
    if (nstate.empty()) continue;
    
    Number mm_new=0;
    bool empty=false;
    
    for (; ip<nsize; ip++){
      mstate.Evolve(nstate, cluster, Operators.exp_(ip));
      int iop = Operators.typ(ip);
      int nfl = gflip_fl[iop/2];
      int nop = 2*nfl + iop%2;
      
      nstate.apply(cluster.FM(nop), cluster.Fi(nop), mstate);
      if (nstate.empty()) {empty=true; break;}
    }
    if (!empty){
      mstate.Evolve(nstate, cluster, Operators.exp_last());
      mm_new = mstate.TraceProject(praStates[ist]);
    }
    ms_new += mm_new;
  }
  return ms_new;
}

void ComputeMoments(const ClusterData& cluster, const mesh1D& omi, int iom_start, double nf,
		    const function1D<double>& mom, const function1D<double>& kaver, const function2D<double>& nt,
		    function2D<dcomplex>& Sigma, bool CorrectMoments=true, int Ncorrect=-1, int aom=3, double sderiv=0.1)
{
  if (Ncorrect<0) Ncorrect=cluster.N_unique_fl;
  stringstream script; script.precision(15);
  script<<"#!/usr/bin/perl\n";
  script<<"use Math::Trig;\nuse Math::Complex;\n\n"; 
  script<<"$nf="<<nf<<";\n";
  script<<"$T="<<(1/common::beta)<<";\n";
  script<<"$U="<<common::U<<";\n";
  script<<"$mu="<<common::mu<<";\n";
  script<<"$Nm="<<cluster.Nm<<";\n";
  script<<"$NK="<<cluster.N_unique_fl<<";\n";
  script<<"$NKc="<<Ncorrect<<";\n";
  for (int i=0; i<nt.size_N(); i++){
    script<<"@nt"<<i<<"=(";
    for (int j=0; j<nt.size_Nd()-1; j++) script<<nt[i][j]<<",";
    script<<nt[i].last()<<");\n";
  }
  for (int op=0; op<cluster.HF_M.size(); op++){
    script<<"@Op"<<op+1<<"=(";
    for (int ik=0; ik<cluster.N_unique_fl; ik++){
      script<<mom[ik+op*cluster.N_unique_fl];
      if (ik<cluster.N_unique_fl-1) script<<",";
      else script<<");\n";
    }
  }
  script<<"@epsk=(";
  for (int ik=0; ik<cluster.N_unique_fl; ik++){
    script<<cluster.epsk[ik];
    if (ik<cluster.N_unique_fl-1) script<<",";
    else script<<");\n";
  }
  
  script<<"@kaver=(";
  for (int ik=0; ik<kaver.size(); ik++){
    script<<kaver[ik];
    if (ik<kaver.size()-1) script<<",";
    else script<<");\n";
  }
  
  script<<"@om=(";
  for (int im=iom_start; im<omi.size(); im++){
    script<<omi[im];
    if (im<omi.size()-1) script<<",";
    else script<<");\n";
  }
  script<<"\n";
  script<<"for ($im=0; $im<=$#om; $im++){\n";
  script<<"  for ($k=0; $k<$NK; $k++){\n";
  script<<"    "<<cluster.RealSigma<<";"<<endl;
  script<<"    "<<cluster.ImagSigma<<";"<<endl;
  script<<"    $$Sig[$k][$im] = $ReSigma + $ImSigma*i;\n";
  script<<"  }\n";
  //  script<<"  print \"\\n\";\n";
  script<<"}\n";

  if (CorrectMoments){
    // Corrects high frequency by interpolation
    script<<endl<<endl;
    script<<"@sb=(";
    for (int ik=0; ik<cluster.N_unique_fl; ik++){
      dcomplex sb=0;
      for (int im=iom_start-aom; im<iom_start; im++) sb += Sigma[ik][im];
      sb/=aom;
      script<<sb.real()<<" ";
      if (sb.imag()>=0) script<<"+";
      script<<sb.imag()<<"*i";
      if (ik<cluster.N_unique_fl-1) script<<",";
      else script<<");"<<endl;
    }
    script<<endl;
    script<<"$omb = "<<omi[iom_start-aom/2]<<";"<<endl;
    script<<"$sderiv = "<<sderiv<<";"<<endl<<endl;
    script<<"for ($k=0; $k<$NKc; $k++) {$se[$k] = Re($$Sig[$k][$#om-1]);}"<<endl;
  
    script<<"for ($im=0; $im<=$#om; $im++){"<<endl;
    script<<"    for ($k=0; $k<$NKc; $k++){"<<endl;
    script<<"	$sr = $se[$k] + (Re($sb[$k])-$se[$k])*($omb/$om[$im])**2;"<<endl;
    script<<"	$$Sig[$k][$im] = $sr + Im($$Sig[$k][$im])*i;"<<endl;
    script<<"    }"<<endl;
    script<<"}"<<endl;
    script<<endl;
    script<<"for ($k=0; $k<$NKc; $k++){"<<endl;
    script<<"    for ($iw=1; $iw<$#om; $iw++){"<<endl;
    script<<"	$df0 = Im($$Sig[$k][$iw]-$sb[$k])/($om[$iw]-$omb);"<<endl;
    script<<"	$df1 = Im($$Sig[$k][$iw]-$$Sig[$k][$iw-1])/($om[$iw]-$om[$iw-1]);"<<endl;
    script<<"	if (abs($df0-$df1)<$sderiv) {last;}"<<endl;
    script<<"    }"<<endl;
    script<<"    $niw[$k] = $iw;"<<endl;
    script<<"    $se[$k] = Im($$Sig[$k][$iw]);"<<endl;
    script<<"    $oe[$k] = $om[$iw];"<<endl;
    script<<"}"<<endl;
    script<<endl;
    script<<"for ($k=0; $k<$NKc; $k++){"<<endl;
    script<<"    for ($im=0; $im<$niw[$k]; $im++){"<<endl;
    script<<"	$si = Im($sb[$k]) + ($se[$k]-Im($sb[$k]))*($om[$im]-$omb)/($oe[$k]-$omb);"<<endl;
    script<<"	$$Sig[$k][$im] = Re($$Sig[$k][$im]) + $si*i;"<<endl;
    script<<"    }"<<endl;
    script<<"}"<<endl;
  }
  script<<endl;
  script<<"for ($im=0; $im<=$#om; $im++){"<<endl;
  script<<"  print $om[$im], \"  \";"<<endl;
  script<<"  for ($k=0; $k<$NK; $k++){"<<endl;
  script<<"    print Re($$Sig[$k][$im]), \" \", Im($$Sig[$k][$im]), \"  \";"<<endl;
  script<<"  }\n";
  script<<"  print \"\\n\";\n";
  script<<"}\n";
  script<<endl;
  
  
  ofstream scrpt("pscript.pl");
  scrpt<<script.str()<<endl;
  
  string command;
  command = "perl pscript.pl > moments.temp";
  int ifail = system(command.c_str());

  ifstream inp("moments.temp");
  if (ifail){cerr<<"Seems your moments are not working or could not find perl!"<<endl;}
  for (int im=iom_start; im<omi.size(); im++){
    double omega;
    inp>>omega;
    if (fabs(omega-omi[im])>1e-6) {cerr<<"Did not succeed in reading moments. Exiting!"<<endl; break;}
    for (int ik=0; ik<cluster.N_unique_fl; ik++) inp>>Sigma(ik,im);
    inp.ignore(1000,'\n');
    if (!inp) {cerr<<"Did not succeed in reading moments. Exiting!"<<endl; break;}
  }
}




double ComputeExponentSum(int ist, const function2D<NState>& state_evolution_left, const function2D<NState>& state_evolution_right, int op_i, int op_f, const NOperators& Operators, const function1D<NState>& praStates, const ClusterData& cluster, int dsize)
{
  NState nstate = (op_i == 0) ? praStates[ist] : state_evolution_left[ist][op_i-1];
  int istate = nstate.istate;  // istate index superstates from 1 (vacuum = 0)
  double exp_sum = nstate.exponent;

  double mants=0;
  for (int i=0; i<nstate.M.size_N(); i++)
    for (int j=0; j<nstate.M.size_Nd(); j++)
      mants += fabs(nstate.M(i,j));
  exp_sum += log(mants);
  
  int last_op = Operators.size() + dsize - 1;  // index of last operator
  
  int ip = op_i;
  for ( ; ip<=op_f; ++ip) {
    //const funProxy<double>& exps = Operators.tr_exp_(ip)[istate];
    //exp_sum += *max_element(exps.begin(), exps.end());

    const double* __restrict__ _exps_ = &Operators.tr_exp_(ip)[cluster.Eind(istate,0)];
    double max_elem=-1e100;
    for (int j=0; j<cluster.msize(istate); j++)
      if (_exps_[j]>max_elem) max_elem = _exps_[j];
    exp_sum += max_elem;
    
    int iop = Operators.tr_typ(ip);
    istate = cluster.Fi(iop, istate);
  }

  if (op_f < last_op){
    //const funProxy<double>& exps = Operators.tr_exp_(ip)[istate];
    //exp_sum += *max_element(exps.begin(), exps.end());

    const double* __restrict__ _exps_ = &Operators.tr_exp_(ip)[cluster.Eind(istate,0)];
    double max_elem=-1e100;
    for (int j=0; j<cluster.msize(istate); j++)
      if (_exps_[j]>max_elem) max_elem = _exps_[j];
    exp_sum += max_elem;
    
    nstate = state_evolution_right[ist][last_op-op_f-1];
    exp_sum += nstate.exponent;

    double mants=0;
    for (int i=0; i<nstate.M.size_N(); i++)
      for (int j=0; j<nstate.M.size_Nd(); j++)
	mants += fabs(nstate.M(i,j));
    exp_sum += log(mants);
    
  }else{
    //const funProxy<double>& exps = Operators.tr_exp_last()[istate];
    //exp_sum += *max_element(exps.begin(), exps.end());
    
    const double* __restrict__ _exps_ = &Operators.tr_exp_last()[cluster.Eind(istate,0)];
    double max_elem=-1e100;
    for (int j=0; j<cluster.msize(istate); j++)
      if (_exps_[j]>max_elem) max_elem = _exps_[j];
    exp_sum += max_elem;
  }
  return exp_sum;
}

bool keep_superstate(int ist, const function2D<NState>& state_evolution_left, const function2D<NState>& state_evolution_right, int op_i, int op_f, const NOperators& Operators, const function1D<NState>& praStates, const ClusterData& cluster, int dsize)
{
  // simple integer lookup of whether evolution annihilates the superstate
  const int VACUUM = 0; // when superstate are indexed from 1, vacuum state is 0
  
  // check if this superstate has survived to point in time (op_i) where we want to insert first operator
  if (op_i>0 && (state_evolution_left[ist].size()<op_i || state_evolution_left[ist][op_i-1].empty())) return false;
  // superstate has survived to first operator insertion -- get evolution of praState up to this point
  NState nstate = (op_i == 0) ? praStates[ist] : state_evolution_left[ist][op_i-1];

  // check if state will survive until second operator insertion = end of modified time segment
  int evolved_state = nstate.istate;  // evolved_state index superstates from 1 (vacuum = 0)
  for (int ip = op_i; ip<=op_f; ++ip) {
    evolved_state = cluster.Fi(Operators.tr_typ(ip), evolved_state);
    if (evolved_state == VACUUM) return false;
  }

  int last_op = Operators.size() + dsize - 1;  // index of last operator
  if (op_f == last_op) {
    // if op_f is the last operator
    return (evolved_state == praStates[ist].istate);
  } else {
    // if op_f is not the last operator
    return (state_evolution_right[ist].size() > last_op-op_f-1 && evolved_state == state_evolution_right[ist][last_op-op_f-1].istate);// make sure we're not out of bounds
  }
}

bool compare_exp_sums(const pair<int, double> exp_sum1, const pair<int, double> exp_sum2)
{
  return (exp_sum1.second > exp_sum2.second);  // use > (instead of <) to place largest exponent at beginning of list
}

void ComputeExpTrace(list<pair<int,double> >& exp_sums, const function2D<NState>& state_evolution_left, const function2D<NState>& state_evolution_right, int op_i, int op_f, const NOperators& Operators, const function1D<NState>& praStates, const ClusterData& cluster, int dsize)
{
  // Algorithm
  // for istate = 1 .. N_superstates
  //   if superstate doesn't survive, continue
  //   otherwise, compute exponent sum and store
  // sort list of stored exponent sums
  // compute approximation to trace trace_approx = max(stored exponent sums)

  for (int ist=0; ist<praStates.size(); ist++){
    if ( keep_superstate(ist, state_evolution_left, state_evolution_right, op_i, op_f, Operators, praStates, cluster, dsize) ) {
      double exp_sum = ComputeExponentSum(ist, state_evolution_left, state_evolution_right, op_i, op_f, Operators, praStates, cluster, dsize);
      exp_sums.push_back( pair<int, double>(ist, exp_sum) );
      //LOGN0(TRACE, ist << "(" << exp_sum << ") ");
    }
  }
  //LOGN0(TRACE,endl);
  
  // sort list of stored exponent sums
  exp_sums.sort(compare_exp_sums); // largest element is first
}

Number ComputePartialTrace(int ist, const function2D<NState>& state_evolution_left, const function2D<NState>& state_evolution_right, int op_i, int op_f, const NOperators& Operators, const function1D<NState>& praStates, const ClusterData& cluster, int dsize)
{
  // compute trace for one string of superstates
  static NState nstate, mstate;
  Number mm_new = 0.0;

  // superstate has survived.  Get evolution of praState up to the point where we want to insert first operator
  int ip = op_i;
  nstate = (ip == 0) ? praStates[ist] : state_evolution_left[ist][ip-1];
  if (nstate.empty()) return mm_new;
  
  int nsize = Operators.size();
  int last_op = Operators.size() + dsize - 1;

  // evolve state with new operator
  for (; ip<=op_f; ip++){
    // first apply exponential (time) evolution
    mstate.Evolve(nstate, cluster, Operators.tr_exp_(ip));
    // then apply creation/annihilation operator ("kink")
    int iop = Operators.tr_typ(ip);
    nstate.apply(cluster.FM(iop), cluster.Fi(iop), mstate);
    if (nstate.empty()) return mm_new;
  }

  // apply final exponential (time) evolution
  if (op_f < last_op){
    if (state_evolution_right[ist].size()>last_op-op_f-1 && nstate.istate==state_evolution_right[ist][last_op-op_f-1].istate){
      mstate.Evolve(nstate, cluster, Operators.tr_exp_(ip));
      mm_new = mstate.ScalarProduct(state_evolution_right[ist][last_op-op_f-1]);
    }
  }else{
    mstate.Evolve(nstate, cluster, Operators.tr_exp_last());
    mm_new = mstate.TraceProject(praStates[ist]);
  }
  return mm_new;
}


bool LazyComputeTrace(double P_r, const Number& matrix_element, double& ms, const list<pair<int,double> >& exp_sums, const function2D<NState>& state_evolution_left, const function2D<NState>& state_evolution_right, int op_i, int op_f, const NOperators& Operators, const function1D<NState>& praStates, const ClusterData& cluster, int dsize)
{
  // Algorithm: see PRB , 005100 (2014).
  
  Number amatrix_element = abs(matrix_element);  // matrix element of the previous QMC step -- need for ratio
  
  Number approx_sum=0.0; // Sum of all approximate traces -- first we just add up exponents
  for (list<pair<int,double> >::const_iterator exp_sums_iter = exp_sums.begin(); exp_sums_iter!=exp_sums.end(); ++exp_sums_iter)
    approx_sum += Number(1,exp_sums_iter->second);
  
  double ms_max = divide(approx_sum,amatrix_element);  // this gives the first upper bound for probability
  if (ms_max<=P_r) {
    //if (P_exact>P_r) 
    //  cout<<"ERROR at the end a0 "<<P_exact<<" "<<P_r<<endl;
    ms=ms_max;
    return false; // Reject
  }
  
  
  Number total_trace=0.0;
  list<pair<int,double> >::const_iterator exp_sums_iter = exp_sums.begin();
  double ms_min;
  
  //int ii=0;
  while (exp_sums_iter!=exp_sums.end()){ 
    // Finds first non-zero partial trace and evaluate it exactly
    Number largest_partial_trace=0.0;
    while (exp_sums_iter != exp_sums.end() && largest_partial_trace.mantisa==0.0) {
      int ist = exp_sums_iter->first;
      Number partial_trace = ComputePartialTrace(ist, state_evolution_left, state_evolution_right, op_i, op_f, Operators, praStates, cluster, dsize);
      largest_partial_trace = partial_trace;
      ++exp_sums_iter;
    }
    total_trace += largest_partial_trace; // exact part of the trace up to now
    
    approx_sum=0.0; // Sum of all approximate traces -- the rest, which has not been computed exactly
    list<pair<int,double> >::const_iterator exp_sums_itern = exp_sums_iter;
    for (; exp_sums_itern!=exp_sums.end(); ++exp_sums_itern)
      approx_sum += Number(1,exp_sums_itern->second);
    
    ms_max = divide( abs(total_trace) + approx_sum, amatrix_element);  // upper bound for probability
    ms_min = divide( abs(total_trace) - approx_sum, amatrix_element);  // lower bound for probability
    //if (ms_max+1e-13<P_exact){ cout<<"ERROR 2b,"<<ii<<": "<<ms_max<<" "<<P_exact<<" "<<ms_max-P_exact<<endl;}
    //if (ms_min-1e-13>P_exact){ cout<<"ERROR 2c,"<<ii<<": "<<ms_min<<" "<<P_exact<<" "<<P_exact-ms_min<<endl;}
    //ii++;
    
    if (ms_max<=P_r) {
      //if (P_exact>P_r) 
      //  cout<<"ERROR at the end a "<<P_exact<<" "<<P_r<<endl;
      ms=ms_max;
      return false; // Reject
    }
    if (ms_min>=P_r) {
      //if (P_exact<P_r)
      //  cout<<"ERROR at the end b "<<P_exact<<" "<<P_r<<endl;
      ms=ms_min;
      return true;  //Accept
    }
  }
  if (fabs(ms_min-ms_max)>1e-13) cout<<"ERROR : ms_min and ms_max Should be equal in Lazy Trace : ms_min="<<ms_min<<" ms_max="<<ms_max<<endl;
  ms = ms_min;
  return P_r<ms_min;
  //return P_r<P_exact;
}

Number ComputeTraceFromScratch(const NOperators& Operators, const function1D<NState>& praStates, const ClusterData& cluster)
{
  static NState nstate, mstate;
  Number ms_new=0;
  int nsize = Operators.size();
  for (int ist=0; ist<praStates.size(); ist++){
    int ip = 0;
    nstate = praStates[ist];
    if (nstate.empty()) continue;
    
    Number mm_new=0;
    bool empty=false;
    
    for (; ip<nsize; ip++){
      mstate.Evolve(nstate, cluster, Operators.exp_(ip));
      int iop = Operators.typ(ip);
      nstate.apply(cluster.FM(iop), cluster.Fi(iop), mstate);
      if (nstate.empty()) {empty=true; break;}
    }
    if (!empty){
      mstate.Evolve(nstate, cluster, Operators.exp_last());
      mm_new = mstate.TraceProject(praStates[ist]);
      //mm_new = mstate.Project_to_Pra(praStates[ist], Projection[ist]);
    }
    ms_new += mm_new;
  }
  return ms_new;
}

bool LookupForExchangeInside1(int ist, int ip_a, int ip_b, const NOperators& Operators,
			      const function1D<NState>& praStates, const ClusterData& cluster, long long istep)
{
  const int VACUUM = 0; // when superstate are indexed from 1, vacuum state is 0
  int istate = praStates[ist].istate;  // istate index superstates from 1 (vacuum = 0)
  //exp_sum=0.0;
  for (int ip=0; ip<Operators.size(); ++ip){
    //const double* __restrict__ _exps_ = &Operators.exp_(ip)[cluster.Eind(istate,0)];
    //double max_elem=_exps_[0];
    //for (int j=1; j<cluster.msize(istate); j++)
    //  if (_exps_[j]>max_elem) max_elem = _exps_[j];
    //exp_sum += max_elem;
    int iop = Operators.typ(ip);
    if (ip==ip_a) iop = Operators.typ(ip_b);
    if (ip==ip_b) iop = Operators.typ(ip_a);
    istate = cluster.Fi(iop, istate);
    if (istate == VACUUM) return false;
  }
  if (istate==praStates[ist].istate){
    //const double* __restrict__ _exps_ = &Operators.exp_last()[cluster.Eind(istate,0)];
    //double max_elem=_exps_[0];
    //for (int j=1; j<cluster.msize(istate); j++)
    //  if (_exps_[j]>max_elem) max_elem = _exps_[j];
    //exp_sum += max_elem;
    return true;
  }
  return false;
}
double ExponentSumForExchangeInside1(int ist, int ip_a, int ip_b, const NOperators& Operators,
				     const function1D<NState>& praStates, const ClusterData& cluster, long long istep)
{
  const int VACUUM = 0; // when superstate are indexed from 1, vacuum state is 0
  int istate = praStates[ist].istate;  // istate index superstates from 1 (vacuum = 0)
  double exp_sum=0.0;
  for (int ip=0; ip<Operators.size(); ++ip){
    const double* __restrict__ _exps_ = &Operators.exp_(ip)[cluster.Eind(istate,0)];
    double max_elem=_exps_[0];
    for (int j=1; j<cluster.msize(istate); j++)
      if (_exps_[j]>max_elem) max_elem = _exps_[j];
    exp_sum += max_elem;
    int iop = Operators.typ(ip);
    if (ip==ip_a) iop = Operators.typ(ip_b);
    if (ip==ip_b) iop = Operators.typ(ip_a);
    istate = cluster.Fi(iop, istate);
    if (istate == VACUUM) return -std::numeric_limits<double>::max();
  }
  if (istate==praStates[ist].istate){
    const double* __restrict__ _exps_ = &Operators.exp_last()[cluster.Eind(istate,0)];
    double max_elem=_exps_[0];
    for (int j=1; j<cluster.msize(istate); j++)
      if (_exps_[j]>max_elem) max_elem = _exps_[j];
    exp_sum += max_elem;
    return exp_sum;
  }
  return -std::numeric_limits<double>::max();
}

/*
bool LookupForExchangeInside2(int ist, int ip_1, int iop_1, int ip_2, int iop_2, const function1D<Number>& Trace,
			      const function2D<NState>& state_evolution_left,
			      const NOperators& Operators, const function1D<NState>& praStates, const ClusterData& cluster, long long istep)
{
  const int VACUUM = 0; // when superstate are indexed from 1, vacuum state is 0
  int istate = (ip_1!=0) ? state_evolution_left(ist,ip_1-1).istate : praStates[ist].istate;
  if (istate == VACUUM) return false;
  //exp_sum=0.0;
  istate = cluster.Fi(iop_1, istate); // not applying iop but iop_1, which is different that what we have in state_evolution.
  if (istate == VACUUM) return false;
  for (int ip=ip_1+1; ip<ip_2; ++ip){
    //const double* __restrict__ _exps_ = &Operators.exp_(ip)[cluster.Eind(istate,0)];
    //double max_elem=_exps_[0];
    //for (int j=1; j<cluster.msize(istate); j++)
    //  if (_exps_[j]>max_elem) max_elem = _exps_[j];
    //exp_sum += max_elem;
    int iop = Operators.typ(ip);
    istate = cluster.Fi(iop, istate);
    if (istate == VACUUM) return false;
  }
  //{
  //  const double* __restrict__ _exps_ = &Operators.exp_(ip_2)[cluster.Eind(istate,0)];
  //  double max_elem=_exps_[0];
  //  for (int j=1; j<cluster.msize(istate); j++)
  //    if (_exps_[j]>max_elem) max_elem = _exps_[j];
  //  exp_sum += max_elem;
  //}
  istate = cluster.Fi(iop_2, istate);
  if (istate == VACUUM) return false;
  
  //if (ip_1>0 && istate!=state_evolution_left(ist,ip_2).istate){
  //  connects=true;
  //  //cout<<istep<<" ERROR It should not happen ist="<<ist<<" istate="<<istate<<" istate'="<<state_evolution_left(ist,ip_2).istate<<" ip_1="<<ip_1<<" ip_2="<<ip_2<<" exp_sum="<<exp_sum<<endl;
  //}else{ connects=false;}
  return true;
}

*/


bool LookupForExchangeInside2(int ip_1, int iop_1, int ip_2, int iop_2, const funProxy<NState>& state_evolution_left_,
			      const NOperators& Operators, const NState& praState_, const ClusterData& cluster, long long istep)
{
  const int VACUUM = 0; // when superstate are indexed from 1, vacuum state is 0
  int istate = (ip_1!=0) ? state_evolution_left_[ip_1-1].istate : praState_.istate;
  if (istate == VACUUM) return false;
  istate = cluster.Fi(iop_1, istate); // not applying iop but iop_1, which is different that what we have in state_evolution.
  if (istate == VACUUM) return false;
  for (int ip=ip_1+1; ip<ip_2; ++ip){
    int iop = Operators.typ(ip);
    istate = cluster.Fi(iop, istate);
    if (istate == VACUUM) return false;
  }
  istate = cluster.Fi(iop_2, istate);  // not applying iop but iop_2, which is different that what we have in state_evolution.
  if (istate == VACUUM) return false;
  return true;
}



class compare_exp{
  function1D<double>& vals;
public:
  compare_exp(function1D<double>& vals_) : vals(vals_){};
  bool operator()(int a, int b){
    return vals[a]>vals[b];
  }
};


/*
bool ComputeTryalExponentSumForExchange(int ip_a, int ip_b, double P_r, Number& ms, const function1D<Number>& Trace, const function2D<NState>& state_evolution_left,
					const function2D<NState>& state_evolution_right, const NOperators& Operators, const function1D<NState>& praStates,
					const ClusterData& cluster, const Number& matrix_element, long long istep)
{
  static function1D<double> exp_sum(praStates.size());
  double smallest = -std::numeric_limits<double>::max();
  for (int ist=0; ist<praStates.size(); ist++) exp_sum[ist]=smallest;

  int ip_1 = min(ip_a,ip_b);
  int ip_2 = max(ip_a,ip_b);
  double max_val=smallest;
  for (int ist=0; ist<praStates.size(); ist++){
    double final_exp_sum=smallest;
    bool can_do_it_fast=false;
    if (ip_1>0){
      if (state_evolution_left[ist].size()<ip_1) continue;// this product matrix element is zero
      double old_exp_sum, new_exp_sum;
      bool connects1, connects2;
      bool old_survived = ExponentSumForExhangeInside2(old_exp_sum, connects1, ist, ip_1, Operators.typ(ip_1), ip_2, Operators.typ(ip_2), Trace, state_evolution_left, Operators, praStates, cluster, istep);
      bool new_survived = ExponentSumForExhangeInside2(new_exp_sum, connects2, ist, ip_1, Operators.typ(ip_2), ip_2, Operators.typ(ip_1), Trace, state_evolution_left, Operators, praStates, cluster, istep);
      if (new_survived && old_survived){
	final_exp_sum = Trace[ist].exp_dbl()+new_exp_sum-old_exp_sum;
	can_do_it_fast=true;
      }else if (!new_survived && !old_survived){
	final_exp_sum = smallest;
	can_do_it_fast=true;
      };
    }
    if (ip_1==0 || !can_do_it_fast){
      double survived_exp_sum;
      bool survived = ExponentSumForExhangeInside1(survived_exp_sum,ist,ip_a,ip_b,Operators,praStates,cluster,istep);
      if (survived){
	exp_sum[ist] = survived_exp_sum;
	if (survived_exp_sum>max_val) max_val=survived_exp_sum;
      }else{
	exp_sum[ist] = smallest;
      }
    }else{
      exp_sum[ist] =  final_exp_sum;
      if (final_exp_sum>max_val) max_val=final_exp_sum;
    }
  }
  // this is just a sum of numbers, which vary a lot in their magntide. If we have
  // res = e^{a1} + e^{a2} + e^{a3} + ....
  // and if a1 is the largest among them, we can write
  // log(res) = a1 + log(1 + e^{a2-a1} + e^{a3-a1} + ....)
  double dval=0.0;
  for (int ist=0; ist<praStates.size(); ist++){
    if (exp_sum[ist]-max_val > -300) dval += exp(exp_sum[ist]-max_val);
  }
  exp_sum= max_val + log(dval);
  
  double ms_max = fabs(divide( Number(1.0, exp_sum), matrix_element));

  if (ms_max <= P_r){
    ms = max_max;
    return false; // reject, because we already know the answer
  }
  // sorting exp_sum array staring with the largest component
  list<int> eindex(praStates.size());
  int i=0;
  for (list<int>::iterator it=eindex.begin(); it!=eindex.end(); ++it,++i) (*it)=i;
  compare cmp(exp_sum);
  eindex.sort(cmp);
  
  //for (list<int>::const_iterator it=eindex.begin(); it!=eindex.end(); ++it)
  //  cout<<" "<<exp_sum[*it]<<endl;

  Number total_trace=0.0;
  list<int>::const_iterator exp_sums_iter = eindex.begin();
  double ms_min;

  //exp_sum[*exp_sums_iter];

  while (exp_sums_iter != eindex.end()){ 
    // Finds first non-zero partial trace and evaluate it exactly
    Number largest_partial_trace=0.0;
    while (exp_sums_iter != exp_sums.end() && largest_partial_trace.mantisa==0.0) {
      int ist = exp_sums_iter->first;
      Number partial_trace = ComputePartialTrace(ist, state_evolution_left, state_evolution_right, op_i, op_f, Operators, praStates, cluster, dsize);
      largest_partial_trace = partial_trace;
      ++exp_sums_iter;
    }
    total_trace += largest_partial_trace; // exact part of the trace up to now
    
    approx_sum=0.0; // Sum of all approximate traces -- the rest, which has not been computed exactly
    list<int>::const_iterator exp_sums_itern = exp_sums_iter;
    for (; exp_sums_itern!=eindex.end(); ++exp_sums_itern)
      approx_sum += Number(1, exp_sum[*exp_sums_itern]);
    
    ms_max = divide( abs(total_trace) + approx_sum, amatrix_element);  // upper bound for probability
    ms_min = divide( abs(total_trace) - approx_sum, amatrix_element);  // lower bound for probability
    //if (ms_max+1e-13<P_exact){ cout<<"ERROR 2b,"<<ii<<": "<<ms_max<<" "<<P_exact<<" "<<ms_max-P_exact<<endl;}
    //if (ms_min-1e-13>P_exact){ cout<<"ERROR 2c,"<<ii<<": "<<ms_min<<" "<<P_exact<<" "<<P_exact-ms_min<<endl;}
    //ii++;
    
    if (ms_max<=P_r) {
      //if (P_exact>P_r) 
      //  cout<<"ERROR at the end a "<<P_exact<<" "<<P_r<<endl;
      ms=ms_max;
      return false; // Reject
    }
    if (ms_min>=P_r) {
      //if (P_exact<P_r)
      //  cout<<"ERROR at the end b "<<P_exact<<" "<<P_r<<endl;
      ms=ms_min;
      return true;  //Accept
    }
  }
  if (fabs(ms_min-ms_max)>1e-13) cout<<"ERROR : Should be equal "<<ms_min<<" "<<ms_max<<endl;
  ms = ms_min;
  return P_r<ms;
}

*/

void CheckForExchange(int ip_a, int ip_b, double P_r, double& ms, const function1D<Number>& Trace, const function2D<NState>& state_evolution_left,
		      const function2D<NState>& state_evolution_right, const NOperators& Operators, const function1D<NState>& praStates,
		      const ClusterData& cluster, const Number& matrix_element, long long istep)
{
  int ip_1 = min(ip_a,ip_b);
  int ip_2 = max(ip_a,ip_b);
  
  static NState nstate, mstate, nstate0, mstate0;
  Number ms_new=0;
  Number ms_old=0;
  int nsize = Operators.size();
  
  cout<<"istep="<<istep<<endl;
  cout<<"type_a="<<Operators.typ(ip_a)<<" type_b="<<Operators.typ(ip_b)<<endl;
  cout<<"index of operator1="<<ip_a<<" index of operator2="<<ip_b<<" total size="<<nsize<<endl;
  
  cout<<"Checking the following time evolution:"<<endl;
  cout<<"oper"<<setw(3)<<" "<<" ";
  for (int ip=0; ip<nsize; ip++){
    int iop = Operators.typ(ip);
    cout<<setw(3)<<iop<<" ";
  }
  cout<<endl;
  cout<<"oper"<<setw(3)<<" "<<" ";
  for (int ip=0; ip<nsize; ip++){
    int iop = Operators.typ(ip);
    if (ip==ip_a) iop = Operators.typ(ip_b);
    if (ip==ip_b) iop = Operators.typ(ip_a);
    cout<<setw(3)<<iop<<" ";
  }
  cout<<endl;
  for (int ist=0; ist<praStates.size(); ist++){

    if (state_evolution_left[ist].size()<ip_1) continue;
    
    int istate0 =praStates[ist].istate;
    cout<<"ist_old="<<setw(3)<<ist<<" ";
    for (int ip=0; ip<nsize; ip++){
      int iop = Operators.typ(ip);
      istate0 = cluster.Fi(iop)[istate0];
      cout<<setw(3)<<istate0<<" ";
      if (istate0==0) break;
    }
    cout<<endl;
    cout<<"ist_new="<<setw(3)<<ist<<" ";
    int istate = praStates[ist].istate;
    for (int ip=0; ip<nsize; ip++){
      int iop = Operators.typ(ip);
      if (ip==ip_a) iop = Operators.typ(ip_b);
      if (ip==ip_b) iop = Operators.typ(ip_a);
      istate = cluster.Fi(iop)[istate];
      cout<<setw(3)<<istate<<" ";
      if (istate==0) break;
    }
    cout<<endl;
  }
  

  
  for (int ist=0; ist<praStates.size(); ist++){
    int ip = 0;
    nstate = praStates[ist];
    //if (nstate.empty()) continue;
    nstate0 =praStates[ist];
      
    Number mm_new=0;
    Number mm_old=0;
    bool empty=false;
    bool empty0=false;
    cout<<"ist="<<setw(3)<<ist<<" ";
    for (; ip<nsize; ip++){
      if (!empty)  mstate.Evolve(nstate, cluster, Operators.exp_(ip));
      if (!empty0) mstate0.Evolve(nstate0, cluster, Operators.exp_(ip));
      
      int iop = Operators.typ(ip);
      if (!empty0) nstate0.apply(cluster.FM(iop), cluster.Fi(iop), mstate0);
      
      if (ip==ip_a) iop = Operators.typ(ip_b);
      if (ip==ip_b) iop = Operators.typ(ip_a);
      if (!empty) nstate.apply(cluster.FM(iop), cluster.Fi(iop), mstate);
      
      if (nstate.empty()) {empty=true;}
      if (nstate0.empty()){empty0=true;}
      if (!empty && !empty0){
	if (ip>=ip_1-2 && ip<=ip_2+1){
	  cout<<setw(4)<<nstate.istate<<"|"<<setw(4)<<nstate0.istate<<" "<<endl;
	  cout<<nstate<<endl;
	  cout<<nstate0<<endl;
	  cout<<endl;
	}
      }
      if (!empty && !empty0){
	if (ip>=ip_2){
	  int k_max=0;
	  double r;
	  for (int i=0; i<nstate0.M.size_N(); i++){
	    for (int k=0; k<nstate0.M.size_Nd(); k++)
	      if (fabs(nstate0.M(i,k))>fabs(nstate0.M(i,k_max))) k_max=k;
	    if (fabs(nstate0.M(i,k_max))>1e-10){
	      r = nstate.M(i,k_max)/nstate0.M(i,k_max);
	      break;
	    }
	  }
	  double diff=0.0;
	  for (int i=0; i<nstate.M.size_N(); i++){
	    for (int j=0; j<nstate.M.size_Nd(); j++){
	      diff += fabs(nstate.M(i,j)-r*nstate0.M(i,j));
	    }
	  }
	  if (diff<=1e-7) cout<<"diff="<<diff<<" ratio="<<r<<"  real ratio="<<r * exp(nstate.exponent-nstate0.exponent)<<endl;
	  if (isnan(diff)){
	    cout<<"M ="<<nstate.M<<endl;
	    cout<<"M0="<<nstate0.M<<endl;
	  }else if (diff>1e-7){
	    double diff=0.0;
	    function1D<double> ratios(nstate.M.size_N());
	    for (int i=0; i<nstate0.M.size_N(); i++){
	      int k_max=0;
	      for (int k=0; k<nstate0.M.size_Nd(); k++)
		if (fabs(nstate0.M(i,k))>fabs(nstate0.M(i,k_max))) k_max=k;
	      double r = nstate.M(i,k_max)/nstate0.M(i,k_max);
	      ratios[i]=r;
	      for (int j=0; j<nstate.M.size_Nd(); j++){
		diff += fabs(nstate.M(i,j)-r*nstate0.M(i,j));
	      }
	    }
	    cout<<"     diff="<<diff<<" ratios=";
	    for (int i=0; i<nstate0.M.size_N(); i++) cout<<ratios[i]*exp(nstate.exponent-nstate0.exponent)<<" ";
	    cout<<endl;
	  }
	}
      }
      if (ip>ip_1 && ( (empty && !empty0) || (!empty && empty0) )){
	cout<<"Did not expect"<<endl;
	if (!empty) cout<<"nstate="<<nstate<<endl;
	if (!empty0) cout<<"nstate0="<<nstate0<<endl;
      }
    }
    cout<<endl;
    if (!empty){
      mstate.Evolve(nstate, cluster, Operators.exp_last());
      mm_new = mstate.TraceProject(praStates[ist]);
    }
    if (!empty0){
      mstate0.Evolve(nstate0, cluster, Operators.exp_last());
      mm_old = mstate.TraceProject(praStates[ist]);
    }
    ms_new += mm_new;
    ms_old += mm_old;
  }
  ms = divide(ms_new,matrix_element);
  cout<<istep<<" ratio="<<ms<<endl;
  exit(0);
  return;
}

bool compare(const NState& nstate1, const NState& nstate2)
{
  if (nstate1.istate==0 && nstate2.istate==0){// both states empty.
    return true;
  }
  if (nstate1.istate != nstate2.istate){
    cerr<<"ERROR in compare : nstate1.istate="<<nstate1.istate<<" nstate2.istate="<<nstate2.istate<<endl;
    return false;
  }
  if (nstate1.M.size_N() != nstate2.M.size_N() || nstate1.M.size_Nd() != nstate2.M.size_Nd()){
    cerr<<"ERROR in compare : shape(nstate1)="<<nstate1.M.size_N()<<","<<nstate1.M.size_Nd()<<" while shape(nstate2)="<<nstate2.M.size_N()<<","<<nstate2.M.size_Nd()<<endl;
    return false;
  }
  
  double max_val=0;
  pair<int,int> ik_max = make_pair(0,0);
  for (int i=0; i<nstate1.M.size_N(); i++){
    for (int k=0; k<nstate1.M.size_Nd(); k++){
      if (fabs(nstate1.M(i,k))>max_val){
	ik_max.first=i;
	ik_max.second=k;
	max_val = fabs(nstate1.M(i,k));
      }
    }
  }
  
  double r1 = fabs(nstate1.M(ik_max.first,ik_max.second));
  double r2 = fabs(nstate2.M(ik_max.first,ik_max.second));

  if ( fabs(nstate1.exponent - nstate2.exponent + log(r1) - log(r2)) > 1e-5 ){
    cerr<<"ERROR in compare : exponent1 = "<<nstate1.exponent+log(fabs(r1))<<" and exponent2="<<nstate2.exponent+log(fabs(r2))<<" where pure exponents are "<<nstate1.exponent<<" "<<nstate2.exponent<<endl;
    cerr<<"Difference is "<< (nstate1.exponent - nstate2.exponent + log(r1) - log(r2)) << endl;
    return false;
  }
  double diff=0.0;
  for (int i=0; i<nstate1.M.size_N(); i++)
    for (int k=0; k<nstate1.M.size_Nd(); k++)
      diff += fabs(nstate1.M(i,k)/r1 - nstate2.M(i,k)/r2);
  
  if (diff>1e-5){
    cerr<<"ERROR in compare : difference in the two states too large "<<diff<<endl;
    cerr<<"state1="<<nstate1<<endl;
    cerr<<"state2="<<nstate2<<endl;
    return false;
  }
  return true;
}

void CheckUpdateForExchange(int ip_a, int ip_b,
			    const function2D<NState>& state_evolution_left, const function2D<NState>& new_state_evolution_left,
			    const function2D<NState>& state_evolution_right, const function2D<NState>& new_state_evolution_right,
			    const Number& new_matrix_element, const NOperators& Operators, const function1D<NState>& praStates,
			    const ClusterData& cluster, long long istep)
{
  int ip_1 = min(ip_a,ip_b);
  int ip_2 = max(ip_a,ip_b);
  
  static NState nstate, mstate, nstate0, mstate0;
  Number ms_new=0;
  Number ms_old=0;
  int nsize = Operators.size();
  cout<<"Checking "<<istep<<endl;// bug at istep=250082
  
  for (int ist=0; ist<praStates.size(); ist++){
    int ip = 0;
    nstate = praStates[ist];
    nstate0 =praStates[ist];
      
    Number mm_new=0;
    Number mm_old=0;
    bool empty=false;
    bool empty0=false;
    for (; ip<nsize; ip++){
      if (!empty)  mstate.Evolve(nstate, cluster, Operators.exp_(ip));
      if (!empty0) mstate0.Evolve(nstate0, cluster, Operators.exp_(ip));
      
      int iop = Operators.typ(ip);
      if (!empty0) nstate0.apply(cluster.FM(iop), cluster.Fi(iop), mstate0);
      
      if (ip==ip_a) iop = Operators.typ(ip_b);
      if (ip==ip_b) iop = Operators.typ(ip_a);
      if (!empty) nstate.apply(cluster.FM(iop), cluster.Fi(iop), mstate);
      
      if (nstate.empty()) {empty=true;}
      if (nstate0.empty()){empty0=true;}
      
      if( empty && new_state_evolution_left[ist][ip].empty()) break; // both are finished
      
      if (!empty || !empty0)
	if (!compare( new_state_evolution_left[ist][ip], nstate)){
	  cout<<"ist="<<ist<<" ip="<<ip<<" iop="<<iop<<endl;
	  cout<<"new_state_evolution="<<new_state_evolution_left[ist][ip]<<" nstate="<<nstate<<endl;
	}
    }
    if (!empty){
      mstate.Evolve(nstate, cluster, Operators.exp_last());
      if (!compare( new_state_evolution_left[ist].last(), mstate)){
	cout<<"new_state_evolution.size="<<new_state_evolution_left[ist].size()<<" ip="<<ip<<endl;
	cout<<"ist="<<ist<<" ip_last="<<ip<<" just evolve"<<endl;
	cout<<"new_state_evolution="<<new_state_evolution_left[ist][ip]<<" mstate="<<mstate<<endl;
      }
      mm_new = mstate.TraceProject(praStates[ist]);
    }
    if (!empty0){
      mstate0.Evolve(nstate0, cluster, Operators.exp_last());
      mm_old = mstate0.TraceProject(praStates[ist]);
    }
    ms_new += mm_new;
    ms_old += mm_old;
  }
  double ms = divide(ms_new,new_matrix_element);
  if (fabs(ms-1.0)>1e-4){
    cout<<"ERROR at istep="<<istep<<" matrix elements are different="<<ms<<endl;
    cout<<"ms_new="<<ms_new<<" and new_matrix_element="<<new_matrix_element<<endl;
  }
  return;
}



Number UpdateForExchange(int ip_a, int ip_b,  function2D<NState>& state_evolution_left,  function2D<NState>& state_evolution_right,
			 function1D<Number>& Trace, const NOperators& Operators, const function1D<NState>& praStates,
			 const ClusterData& cluster, function2D<double>& Prob, long long istep)
{
  static long long N_calculated=0;
  static long long N_updated=0;
  static function2D<Number> Projection(Prob.size_N(),common::max_size);
  static NState nstate(common::max_size,common::max_size), mstate(common::max_size,common::max_size);
  Number ms=0;
  Projection=0;

  int ip_1 = min(ip_a,ip_b);
  int ip_2 = max(ip_a,ip_b);
  
  int nsize = Operators.size();
  //cout<<"ip_1="<<ip_1<<" ip_2="<<ip_2<<" nsize="<<nsize<<endl;
  
  for (int ist=0; ist<praStates.size(); ist++){
    Trace[ist]=0;
    if (ip_1>0 && (state_evolution_left[ist].size()<ip_1 || state_evolution_left[ist][ip_1-1].empty())) continue;// this product matrix element is zero
    int ip = ip_1;
    if (ip==0) nstate = praStates[ist];// we need to start from scratch because op_i iz zero
    else nstate = state_evolution_left[ist][ip-1];// we start from the previous operator and update from there on
    if (nstate.empty()) continue;

    bool empty=false;
    for (; ip<Operators.size(); ip++){        // go over the rest of the operators
      mstate.Evolve(nstate, cluster, Operators.exp_(ip)); // state evolution due to local Hamiltonian (is diagonal)
      int iop = Operators.typ(ip);
      if (ip==ip_a) iop = Operators.typ(ip_b);
      if (ip==ip_b) iop = Operators.typ(ip_a);
      nstate.apply(cluster.FM(iop), cluster.Fi(iop), mstate);   // the next operator is applied
      if (nstate.empty()) {
	empty=true;
	state_evolution_left[ist][ip].istate=0;
	break;
      }             // maybe matrix element is zero
      N_calculated++;
      
      if (ip>=ip_2 && state_evolution_left[ist].size()>ip){
	double r=1;
	if (Is_State_Proportional(state_evolution_left[ist][ip], nstate, r)){
	  //const NState& nstate0 = state_evolution_left[ist][ip];
	  //double real_ratio = r * exp(nstate.exponent-nstate0.exponent);
	  double log_r, cm;
	  if (r<0){
	    log_r = log(-r);
	    cm = -1.0;
	  }else{
	    log_r = log(r);
	    cm = 1.0;
	  }
	  double log_ratio = log_r + nstate.exponent - state_evolution_left[ist][ip].exponent;
	  for (int iip=ip; iip<state_evolution_left[ist].size(); iip++){
	    state_evolution_left[ist][iip].exponent += log_ratio;
	    if (cm<0) state_evolution_left[ist][iip].M *= -1.0;
	    N_updated++;
	  }
	  empty = state_evolution_left[ist].size()<=nsize;
	  if (!empty) mstate = state_evolution_left[ist].last();
	  goto loops_out;
	}
      }
      state_evolution_left[ist][ip].SetEqual(nstate);      // it is not, store it for next time
    }
    if (!empty){ // from the last operator to beta
      mstate.Evolve(nstate, cluster, Operators.exp_last()); // The last evolution of the state due to H_loc.
      state_evolution_left[ist][ip++].SetEqual(mstate); // store it
    }
    if (ip>state_evolution_left.fullsize_Nd()) {cerr<<"Trying to resize to "<<ip<<" op_size="<<Operators.size()<<endl;}
    state_evolution_left[ist].resize(ip); // we have stored state evolution, remember the new size

  loops_out:;
    
    Number mm=0;
    if (!empty) mm = mstate.Project_to_Pra(praStates[ist], Projection[ist]);
    Trace[ist] = mm;
    //if (!empty) cout<<"ist "<<ist<<" survived with partial trace "<<mm<<endl;
    ms += mm;
  }

  Number ms_right=0;
  for (int ist=0; ist<praStates.size(); ist++){// ip_1 is the earliest operator inserted - from this point on, the evolution needs to be changed
    int nsize = Operators.size();
    int ip = nsize-1-ip_2;
    if (ip>0 && (state_evolution_right[ist].size()<ip || state_evolution_right[ist][ip-1].empty())) continue;// this product matrix element is zero
    if (ip==0) nstate = praStates[ist];// we need to start from scratch because op_i iz zero
    else nstate = state_evolution_right[ist][ip-1];// we start from the previous operator and update from there on
    if (nstate.empty()) continue;
    
    bool empty=false;
    if (ip<nsize){ // firts operator to correct
      if (ip>0) mstate.Evolve(nstate, cluster, Operators.exp_(nsize-ip)); // The last evlution of the state due to H_loc.
      else 	mstate.Evolve(nstate, cluster, Operators.exp_last()); // The last evlution of the state due to H_loc.
      int iop = Operators.typ_transpose(ip_1);
      nstate.apply(cluster.FM(iop), cluster.Fi(iop), mstate);        // the next operator is applied
      if (nstate.empty()) {
	empty=true;
	state_evolution_right[ist][ip].istate=0;
      }// maybe matrix element is zero
      if (!empty) state_evolution_right[ist][ip++].SetEqual(nstate); // store it
      N_calculated++;
    }
    if (!empty){
      for (; ip<Operators.size(); ip++){        // go over the rest of the operators
	mstate.Evolve(nstate, cluster, Operators.exp_(nsize-ip)); // state evolution due to local Hamiltonian (is diagonal)
	int iop = Operators.typ_transpose(nsize-1-ip);
	if (nsize-1-ip==ip_a) iop = Operators.typ_transpose(ip_b);
	if (nsize-1-ip==ip_b) iop = Operators.typ_transpose(ip_a);
	nstate.apply(cluster.FM(iop), cluster.Fi(iop), mstate);        // the next operator is applied
	if (nstate.empty()) {
	  empty=true;
	  state_evolution_right[ist][ip].istate=0;
	  break;
	}// maybe matrix element is zero
	N_calculated++;
	if (ip>=nsize-1-ip_1 && state_evolution_right[ist].size()>ip){
	  double r=1;
	  if (Is_State_Proportional(state_evolution_right[ist][ip], nstate, r)){
	    //const NState& nstate0 = state_evolution_right[ist][ip];
	    //double real_ratio = r * exp(nstate.exponent-nstate0.exponent);
	    double log_r, cm;
	    if (r<0){
	      log_r = log(-r);
	      cm = -1.0;
	    }else{
	      log_r = log(r);
	      cm = 1.0;
	    }
	    double log_ratio = log_r + nstate.exponent - state_evolution_right[ist][ip].exponent;
	    for (int iip=ip; iip<state_evolution_right[ist].size(); iip++){
	      //NState& state = state_evolution_right[ist][iip];
	      state_evolution_right[ist][iip].exponent += log_ratio;
	      if (cm<0) state_evolution_right[ist][iip].M *= -1.0;
	      N_updated++;
	    }
	    empty = state_evolution_right[ist].size()<=nsize;
	    if (!empty) mstate = state_evolution_right[ist].last();
	    goto loops_out2;
	  }
	}
	state_evolution_right[ist][ip].SetEqual(nstate);      // it is not, store it for next time
      }
    }
    if (!empty){
      mstate.Evolve(nstate, cluster, Operators.exp_(0)); // The last evlution of the state due to H_loc.
      state_evolution_right[ist][ip++].SetEqual(mstate); // store it
    }
    if (ip>state_evolution_right.fullsize_Nd()) {cerr<<"Trying to resize to "<<ip<<" op_size="<<Operators.size()<<endl;}
    state_evolution_right[ist].resize(ip); // we have stored state evolution, remember the new size
  loops_out2:;
    Number mm=0;
    if (!empty) {
      mm = mstate.TraceProject(praStates[ist]); // need the trace. The last state needs to be projected to the first state
    }
    ms_right += mm;
  }
  if (fabs(fabs(divide(ms_right,ms))-1)>1e-3){
    cout.precision(16);
    double ratio = fabs(divide(ms_right,ms));
    cout<<"ERROR at istep = "<<istep<<" ms left and ms right are not the same : ratio="<<ratio<<" msl="<<ms<<" msr="<<ms_right<<endl;
    if (ratio>1) ms = ms_right;
  }
  // SHOULD PRINT THIS SOMEWHERE
  //cout<<"P_calculated="<<(N_calculated/(0.0+N_calculated+N_updated))<<" P_updated="<<(N_updated/(0.0+N_calculated+N_updated))<<endl;

  //  return ms;
  
  // This does not need to be done every time. You should only do it when we meassure!!!
  if (common::PreciseP){ // computes probability more precisely
    int npra = praStates.size();
    int plda = Prob.lda();

    int nsize = Operators.size();
    if (nsize==0){
      for (int ist=0; ist<npra; ist++)
	for (int m=0; m<Prob[ist].size(); m++)
	  Prob(ist,m) = cluster.P_atom(ist,m);
    }else{
      for (int ist=0; ist<npra; ist++)
	for (int m=0; m<Prob[ist].size(); m++) Prob(ist,m) = 0.0;
      
      for (int ist=0; ist<npra; ist++){
	if (state_evolution_left[ist].size()<=Operators.size() || state_evolution_right[ist].size()<=Operators.size()) continue;
	if (ip_1>0 && (state_evolution_left[ist].size()<ip_1 || state_evolution_left[ist][ip_1-1].empty())) continue;// this product matrix element is zero

	for (int l=0; l<nsize-1; l++){
	  const NState& lstate = state_evolution_left(ist,l);
	  const NState& rstate = state_evolution_right(ist,nsize-l-2);
	  int istate = lstate.istate;
	  
	  if (lstate.istate != rstate.istate){
	    cout<<"In computing Probability ERROR: lstate.istate="<<lstate.istate<<" rstate.istate="<<rstate.istate<<endl;
	    cout<<"prastate="<<praStates[ist].istate<<endl;
	    for (int i=0; i<state_evolution_left[ist].size()-1; i++) {
	      cout<<"left "<<i<<" "<<state_evolution_left[ist][i].istate<<"    operator:"<<Operators.typ(i)<<endl;
	    }
	    for (int i=0; i<state_evolution_right[ist].size(); i++) {
	      cout<<"right "<<i<<" "<<state_evolution_right[ist][i].istate<<endl;
	    }
	  }
	  
	  const double* __restrict__ lstateM = lstate.M.MemPt();
	  const double* __restrict__ rstateM = rstate.M.MemPt();
	  int nk = lstate.M.size_Nd();
	  int ldal = lstate.M.lda();
	  int ldar = rstate.M.lda();
	  
	  const double* expn = &Operators.exp_(l+1)[cluster.Eind(istate,0)];
	  double* __restrict__ _Prb_ = &Prob((istate-1),0);
	  double dtau = (Operators.t(l+1)-Operators.t(l))/common::beta;
	  double dtau_ms_mantisa = dtau/ms.mantisa;
	  double out_exponents = lstate.exponent+rstate.exponent-ms.exponent;

	  for (int n=0; n<lstate.M.size_N(); n++){
	    double mantisa=0;
	    for (int m=0; m<nk; m++) mantisa += lstateM[n*ldal+m]*rstateM[n*ldar+m];
	    _Prb_[n] += mantisa*dtau_ms_mantisa*exp(expn[n]+out_exponents);
	  }
	}
	{
	  const NState& lstate = state_evolution_left(ist,nsize);
	  double* __restrict__ _Prb1_ = &Prob((lstate.istate-1),0);
	  const double* __restrict__ lstateM1 = lstate.M.MemPt();
	  int ldal = lstate.M.lda();
    
	  double dtau = 1 - (Operators.t(nsize-1)-Operators.t(0))/common::beta;
	  double dtau_ms_mantisa_exp = dtau/ms.mantisa * exp(lstate.exponent-ms.exponent); // missing pice associated with e^{-t_0*E}
	  for (int n=0; n<lstate.M.size_N(); n++){
	    _Prb1_[n] += lstateM1[n*ldal+n]*dtau_ms_mantisa_exp;
	  }
	}
      }
    }
    double sum=0;
    for (int ist=0; ist<npra; ist++){
      for (int l=0; l<Prob[ist].size(); l++){
	sum += Prob(ist,l);
#ifdef _DEBUG	
	if (fabs(Prob(ist,l))>2.) cerr<<"WARNING : Prob>2 "<<ist<<" "<<l<<" "<<Prob(ist,l)<<endl;
#endif		
      }
    }
    if (fabs(sum-1.)>1e-4) cout<<"WARN: Psum = "<<sum<<endl;

  }else{
    double sum=0;
    for (int ist=0; ist<praStates.size(); ist++){
      for (int l=0; l<Prob[ist].size(); l++){
	Prob[ist][l] = divide(Projection[ist][l],ms);
	sum += Prob[ist][l];
      }
    }
  }
  return ms;
}


Number UpdateForExchange2(int ip_a, int ip_b,  function2D<NState>& state_evolution_left,  function2D<NState>& state_evolution_right,
			  function1D<Number>& Trace, const NOperators& Operators, const function1D<NState>& praStates,
			  const ClusterData& cluster, function2D<double>& Prob, long long istep)
{
  static long long N_calculated=0;
  static long long N_updated=0;
  static function2D<Number> Projection(Prob.size_N(),common::max_size);
  static NState nstate(common::max_size,common::max_size), mstate(common::max_size,common::max_size);
  Number ms=0;
  Projection=0;

  int ip_1 = min(ip_a,ip_b);
  int ip_2 = max(ip_a,ip_b);
  
  int nsize = Operators.size();
  
  for (int ist=0; ist<praStates.size(); ist++){
    Trace[ist]=0;
    if (ip_1>0 && (state_evolution_left[ist].size()<ip_1 || state_evolution_left[ist][ip_1-1].empty())) continue;// this product matrix element is zero
    int ip = ip_1;
    if (ip==0) nstate = praStates[ist];// we need to start from scratch because op_i iz zero
    else nstate = state_evolution_left[ist][ip-1];// we start from the previous operator and update from there on
    if (nstate.empty()) continue;

    bool empty=false;
    for (; ip<Operators.size(); ip++){        // go over the rest of the operators
      mstate.Evolve(nstate, cluster, Operators.exp_(ip)); // state evolution due to local Hamiltonian (is diagonal)
      int iop = Operators.typ(ip);
      //if (ip==ip_a) iop = Operators.typ(ip_b); No need for this anymore, since they were exchanged in operators
      //if (ip==ip_b) iop = Operators.typ(ip_a);
      nstate.apply(cluster.FM(iop), cluster.Fi(iop), mstate);   // the next operator is applied
      if (nstate.empty()) {
	empty=true;
	state_evolution_left[ist][ip].istate=0;
	break;
      }             // maybe matrix element is zero
      N_calculated++;
      
      if (ip>=ip_2 && state_evolution_left[ist].size()>ip){
	double r=1;
	if (Is_State_Proportional(state_evolution_left[ist][ip], nstate, r)){
	  //const NState& nstate0 = state_evolution_left[ist][ip];
	  //double real_ratio = r * exp(nstate.exponent-nstate0.exponent);
	  double log_r, cm;
	  if (r<0){
	    log_r = log(-r);
	    cm = -1.0;
	  }else{
	    log_r = log(r);
	    cm = 1.0;
	  }
	  double log_ratio = log_r + nstate.exponent - state_evolution_left[ist][ip].exponent;
	  for (int iip=ip; iip<state_evolution_left[ist].size(); iip++){
	    state_evolution_left[ist][iip].exponent += log_ratio;
	    if (cm<0) state_evolution_left[ist][iip].M *= -1.0;
	    N_updated++;
	  }
	  empty = state_evolution_left[ist].size()<=nsize;
	  if (!empty) mstate = state_evolution_left[ist].last();
	  goto loops_out;
	}
      }
      state_evolution_left[ist][ip].SetEqual(nstate);      // it is not, store it for next time
    }
    if (!empty){ // from the last operator to beta
      mstate.Evolve(nstate, cluster, Operators.exp_last()); // The last evolution of the state due to H_loc.
      state_evolution_left[ist][ip++].SetEqual(mstate); // store it
    }
    if (ip>state_evolution_left.fullsize_Nd()) {cerr<<"Trying to resize to "<<ip<<" op_size="<<Operators.size()<<endl;}
    state_evolution_left[ist].resize(ip); // we have stored state evolution, remember the new size

  loops_out:;
    
    Number mm=0;
    if (!empty) mm = mstate.Project_to_Pra(praStates[ist], Projection[ist]);
    Trace[ist] = mm;
    //if (!empty) cout<<"ist "<<ist<<" survived with partial trace "<<mm<<endl;
    ms += mm;
  }

  Number ms_right=0;
  for (int ist=0; ist<praStates.size(); ist++){// ip_1 is the earliest operator inserted - from this point on, the evolution needs to be changed
    int nsize = Operators.size();
    int ip = nsize-1-ip_2;
    if (ip>0 && (state_evolution_right[ist].size()<ip || state_evolution_right[ist][ip-1].empty())) continue;// this product matrix element is zero
    if (ip==0) nstate = praStates[ist];// we need to start from scratch because op_i iz zero
    else nstate = state_evolution_right[ist][ip-1];// we start from the previous operator and update from there on
    if (nstate.empty()) continue;
    
    bool empty=false;
    if (ip<nsize){ // firts operator to correct
      if (ip>0) mstate.Evolve(nstate, cluster, Operators.exp_(nsize-ip)); // The last evlution of the state due to H_loc.
      else 	mstate.Evolve(nstate, cluster, Operators.exp_last()); // The last evlution of the state due to H_loc.
      int iop = Operators.typ_transpose(ip_2); // HERE WAS MISTAKE
      nstate.apply(cluster.FM(iop), cluster.Fi(iop), mstate);        // the next operator is applied
      if (nstate.empty()) {
	empty=true;
	state_evolution_right[ist][ip].istate=0;
      }// maybe matrix element is zero
      if (!empty) state_evolution_right[ist][ip++].SetEqual(nstate); // store it
      N_calculated++;
    }
    if (!empty){
      for (; ip<Operators.size(); ip++){        // go over the rest of the operators
	mstate.Evolve(nstate, cluster, Operators.exp_(nsize-ip)); // state evolution due to local Hamiltonian (is diagonal)
	int iop = Operators.typ_transpose(nsize-1-ip);
	//if (nsize-1-ip==ip_a) iop = Operators.typ_transpose(ip_b);
	//if (nsize-1-ip==ip_b) iop = Operators.typ_transpose(ip_a);
	nstate.apply(cluster.FM(iop), cluster.Fi(iop), mstate);        // the next operator is applied
	if (nstate.empty()) {
	  empty=true;
	  state_evolution_right[ist][ip].istate=0;
	  break;
	}// maybe matrix element is zero
	N_calculated++;
	if (ip>=nsize-1-ip_1 && state_evolution_right[ist].size()>ip){
	  double r=1;
	  if (Is_State_Proportional(state_evolution_right[ist][ip], nstate, r)){
	    //const NState& nstate0 = state_evolution_right[ist][ip];
	    //double real_ratio = r * exp(nstate.exponent-nstate0.exponent);
	    double log_r, cm;
	    if (r<0){
	      log_r = log(-r);
	      cm = -1.0;
	    }else{
	      log_r = log(r);
	      cm = 1.0;
	    }
	    double log_ratio = log_r + nstate.exponent - state_evolution_right[ist][ip].exponent;
	    for (int iip=ip; iip<state_evolution_right[ist].size(); iip++){
	      //NState& state = state_evolution_right[ist][iip];
	      state_evolution_right[ist][iip].exponent += log_ratio;
	      if (cm<0) state_evolution_right[ist][iip].M *= -1.0;
	      N_updated++;
	    }
	    empty = state_evolution_right[ist].size()<=nsize;
	    if (!empty) mstate = state_evolution_right[ist].last();
	    goto loops_out2;
	  }
	}
	state_evolution_right[ist][ip].SetEqual(nstate);      // it is not, store it for next time
      }
    }
    if (!empty){
      mstate.Evolve(nstate, cluster, Operators.exp_(0)); // The last evlution of the state due to H_loc.
      state_evolution_right[ist][ip++].SetEqual(mstate); // store it
    }
    if (ip>state_evolution_right.fullsize_Nd()) {cerr<<"Trying to resize to "<<ip<<" op_size="<<Operators.size()<<endl;}
    state_evolution_right[ist].resize(ip); // we have stored state evolution, remember the new size
  loops_out2:;
    Number mm=0;
    if (!empty) {
      mm = mstate.TraceProject(praStates[ist]); // need the trace. The last state needs to be projected to the first state
    }
    ms_right += mm;
  }
  if (fabs(fabs(divide(ms_right,ms))-1)>1e-3){
    cout.precision(16);
    double ratio = fabs(divide(ms_right,ms));
    cout<<"ERROR at istep = "<<istep<<" ms left and ms right are not the same : ratio="<<ratio<<" msl="<<ms<<" msr="<<ms_right<<endl;
    if (ratio>1) ms = ms_right;
  }
  // SHOULD PRINT THIS SOMEWHERE
  //cout<<"P_calculated="<<(N_calculated/(0.0+N_calculated+N_updated))<<" P_updated="<<(N_updated/(0.0+N_calculated+N_updated))<<endl;

  // This does not need to be done every time. You should only do it when we meassure!!!
  if (common::PreciseP){ // computes probability more precisely
    int npra = praStates.size();
    int plda = Prob.lda();

    int nsize = Operators.size();
    if (nsize==0){
      for (int ist=0; ist<npra; ist++)
	for (int m=0; m<Prob[ist].size(); m++)
	  Prob(ist,m) = cluster.P_atom(ist,m);
    }else{
      for (int ist=0; ist<npra; ist++)
	for (int m=0; m<Prob[ist].size(); m++) Prob(ist,m) = 0.0;
      
      for (int ist=0; ist<npra; ist++){
	if (state_evolution_left[ist].size()<=Operators.size() || state_evolution_right[ist].size()<=Operators.size()) continue;
	if (ip_1>0 && (state_evolution_left[ist].size()<ip_1 || state_evolution_left[ist][ip_1-1].empty())) continue;// this product matrix element is zero

	for (int l=0; l<nsize-1; l++){
	  const NState& lstate = state_evolution_left(ist,l);
	  const NState& rstate = state_evolution_right(ist,nsize-l-2);
	  int istate = lstate.istate;
	  
	  if (lstate.istate != rstate.istate){
	    cout<<"In computing Probability ERROR: lstate.istate="<<lstate.istate<<" rstate.istate="<<rstate.istate<<endl;
	    cout<<"prastate="<<praStates[ist].istate<<endl;
	    for (int i=0; i<state_evolution_left[ist].size()-1; i++) {
	      cout<<"left "<<i<<" "<<state_evolution_left[ist][i].istate<<"    operator:"<<Operators.typ(i)<<endl;
	    }
	    for (int i=0; i<state_evolution_right[ist].size(); i++) {
	      cout<<"right "<<i<<" "<<state_evolution_right[ist][i].istate<<endl;
	    }
	  }
	  
	  const double* __restrict__ lstateM = lstate.M.MemPt();
	  const double* __restrict__ rstateM = rstate.M.MemPt();
	  int nk = lstate.M.size_Nd();
	  int ldal = lstate.M.lda();
	  int ldar = rstate.M.lda();
	  
	  const double* expn = &Operators.exp_(l+1)[cluster.Eind(istate,0)];
	  double* __restrict__ _Prb_ = &Prob((istate-1),0);
	  double dtau = (Operators.t(l+1)-Operators.t(l))/common::beta;
	  double dtau_ms_mantisa = dtau/ms.mantisa;
	  double out_exponents = lstate.exponent+rstate.exponent-ms.exponent;

	  for (int n=0; n<lstate.M.size_N(); n++){
	    double mantisa=0;
	    for (int m=0; m<nk; m++) mantisa += lstateM[n*ldal+m]*rstateM[n*ldar+m];
	    _Prb_[n] += mantisa*dtau_ms_mantisa*exp(expn[n]+out_exponents);
	  }
	}
	{
	  const NState& lstate = state_evolution_left(ist,nsize);
	  double* __restrict__ _Prb1_ = &Prob((lstate.istate-1),0);
	  const double* __restrict__ lstateM1 = lstate.M.MemPt();
	  int ldal = lstate.M.lda();
    
	  double dtau = 1 - (Operators.t(nsize-1)-Operators.t(0))/common::beta;
	  double dtau_ms_mantisa_exp = dtau/ms.mantisa * exp(lstate.exponent-ms.exponent); // missing pice associated with e^{-t_0*E}
	  for (int n=0; n<lstate.M.size_N(); n++){
	    _Prb1_[n] += lstateM1[n*ldal+n]*dtau_ms_mantisa_exp;
	  }
	}
      }
    }
    double sum=0;
    for (int ist=0; ist<npra; ist++){
      for (int l=0; l<Prob[ist].size(); l++){
	sum += Prob(ist,l);
#ifdef _DEBUG	
	if (fabs(Prob(ist,l))>2.) cerr<<"ERROR : Prob>2 "<<ist<<" "<<l<<" "<<Prob(ist,l)<<endl;
#endif		
      }
    }
    if (fabs(sum-1.)>1e-4) cout<<"WARN: Psum = "<<sum<<endl;

  }else{
    double sum=0;
    for (int ist=0; ist<praStates.size(); ist++){
      for (int l=0; l<Prob[ist].size(); l++){
	Prob[ist][l] = divide(Projection[ist][l],ms);
	sum += Prob[ist][l];
      }
    }
  }
  return ms;
}



Number ComputeTryalForExchange(int ip_a, int ip_b,  const function2D<NState>& state_evolution_left,  const function2D<NState>& state_evolution_right,
			       const NOperators& Operators, const function1D<NState>& praStates, const ClusterData& cluster, long long istep)
{
  static NState nstate(common::max_size,common::max_size), mstate(common::max_size,common::max_size);
  int ip_1 = min(ip_a,ip_b);
  int ip_2 = max(ip_a,ip_b);
  int nsize = Operators.size();
  Number m_new = 0.0;
  for (int ist=0; ist<praStates.size(); ist++){
    Number partial_trace = 0.0;
    if (state_evolution_left[ist].size()<ip_1) continue;// this product matrix element is zero
    if (state_evolution_right[ist].size()<nsize-1-ip_2) continue;// this product matrix element is zero
    bool survived = LookupForExchangeInside2(ip_1, Operators.typ(ip_2), ip_2, Operators.typ(ip_1), state_evolution_left[ist], Operators, praStates[ist], cluster, istep);
    if (survived){
      int ip = ip_1;
      // we start from the previous operator and update from there on
      nstate = (ip==0) ? praStates[ist] : state_evolution_left[ist][ip-1];
      {
	mstate.Evolve(nstate, cluster, Operators.exp_(ip_1));       // state evolution due to local Hamiltonian (is diagonal)
	int iop = Operators.typ(ip_2);                            // here we use ip_2 rather than ip_1 because of exchange
	nstate.apply(cluster.FM(iop), cluster.Fi(iop), mstate);   // the next operator is applied
	++ip;
      }
      for (; ip<ip_2; ++ip){        // go over the rest of the operators between ip_1 and ip_2
	mstate.Evolve(nstate, cluster, Operators.exp_(ip)); // state evolution due to local Hamiltonian (is diagonal)
	int iop = Operators.typ(ip);
	nstate.apply(cluster.FM(iop), cluster.Fi(iop), mstate);   // the next operator is applied
      }
      {
	mstate.Evolve(nstate, cluster, Operators.exp_(ip_2)); // state evolution due to local Hamiltonian (is diagonal)
	int iop = Operators.typ(ip_1);                        // here we use ip_1 rather than ip_2 because of exchange
	nstate.apply(cluster.FM(iop), cluster.Fi(iop), mstate);   // the next operator is applied
	++ip;
      }
      // apply final exponential (time) evolution
      if (ip_2 < nsize-1){
	int ip_r = nsize-1-ip_2-1;
	if (state_evolution_right[ist].size()>ip_r && nstate.istate==state_evolution_right[ist][ip_r].istate){
	  mstate.Evolve(nstate, cluster, Operators.exp_(ip)); // what is ip here? ip_2+1;
	  partial_trace = mstate.ScalarProduct(state_evolution_right[ist][ip_r]);
	}
      }else{
	mstate.Evolve(nstate, cluster, Operators.exp_last());
	partial_trace = mstate.TraceProject(praStates[ist]);
      }
      m_new += partial_trace;
    }
  }
  return m_new;
}
