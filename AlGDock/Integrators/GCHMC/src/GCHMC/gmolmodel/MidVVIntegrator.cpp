//==============================================================================
//                           MIDVV INTEGRATOR
//==============================================================================

MidVVIntegrator::MidVVIntegrator(const SimTK::System& sys
                                 , TARGET_TYPE *PrmToAx_po
                                 , TARGET_TYPE *MMTkToPrm_po
                                 , SimTK::CompoundSystem *compoundSystem
                                 , SymSystem *Caller
                                 ) {
    rep = new MidVVIntegratorRep(this, sys, PrmToAx_po, MMTkToPrm_po, compoundSystem, Caller);
}

MidVVIntegrator::MidVVIntegrator(const SimTK::System& sys, SimTK::Real stepSize
                                 , TARGET_TYPE *PrmToAx_po
                                 , TARGET_TYPE *MMTkToPrm_po
                                 , SimTK::CompoundSystem *compoundSystem
                                 , SymSystem *Caller
                                 ) {
    rep = new MidVVIntegratorRep(this, sys, PrmToAx_po, MMTkToPrm_po, compoundSystem, Caller);
    setFixedStepSize(stepSize);
}

MidVVIntegrator::~MidVVIntegrator() {
    delete rep;
}



//==============================================================================
//                          MIDVV INTEGRATOR REP
//==============================================================================
MidVVIntegratorRep::MidVVIntegratorRep(SimTK::Integrator* handle, const SimTK::System& sys
                                       , TARGET_TYPE *PrmToAx_po
                                       , TARGET_TYPE *MMTkToPrm_po
                                       , SimTK::CompoundSystem *compoundSystem
                                       , SymSystem *Caller
                                       )
:   SimTK::AbstractIntegratorRep(handle, sys, 2, 3, "Verlet",  true)
    , c(compoundSystem->getCompound(SimTK::CompoundSystem::CompoundIndex(0)))
    , matter(Caller->system->getMatterSubsystem())
{
  this->Caller = Caller;
  this->PrmToAx_po = PrmToAx_po;
  this->MMTkToPrm_po = MMTkToPrm_po;
  this->coords = Caller->coords;
  this->vels = Caller->vels;
  this->grads = Caller->grads;
  this->compoundSystem = compoundSystem;
  this->shm = Caller->shm;
  this->natms = Caller->lig1->natms;
  this->arrays_cut = 2 + 4*3*(this->natms);
  this->totStepsInCall = 0;

  this->QVector = Caller->QVector;
  this->TVector = Caller->TVector;

  this->Tb = .0;
  ke = pe = .0;
  this->gaurand.setMean(0.0);
  
  this->trouble = 0;

  step0Flag = new int;
  *step0Flag = 0;
  metroFlag = new int;
  *metroFlag = 0;
  beginFlag = new int;
  *beginFlag = 0;

  // Load maps
  SimTK::Compound& mutc1 = compoundSystem->updCompound(SimTK::CompoundSystem::CompoundIndex(0));
  for (SimTK::Compound::AtomIndex aIx(0); aIx < mutc1.getNumAtoms(); ++aIx){
    SimTK::MobilizedBodyIndex mbx = mutc1.getAtomMobilizedBodyIndex(aIx);
    if(mutc1.getAtomLocationInMobilizedBodyFrame(aIx) == 0){
      mbx2aIx.insert(pair<SimTK::MobilizedBodyIndex, SimTK::Compound::AtomIndex>(mbx, aIx));
    }
    aIx2mbx.insert(pair<SimTK::Compound::AtomIndex, SimTK::MobilizedBodyIndex>(aIx, mbx));
  }
}

// * Print functions
void MidVVIntegratorRep::printData(const SimTK::Compound& c, SimTK::State& advanced)
{
    SimTK::Vec3 vertex, vel, acc;
    std::cout<<"MidVV: Positions:"<<std::endl;
    for (SimTK::Compound::AtomIndex aIx(0); aIx < c.getNumAtoms(); ++aIx){
        vertex   = c.calcAtomLocationInGroundFrame(advanced, aIx);
        std::cout<<c.getAtomName(aIx)<<"="<<std::setprecision(8)<<std::fixed
          <<"["<<vertex[0]<<" "<<vertex[1]<<" "<<vertex[2]<<"]"<<std::endl;
    }
    std::cout<<std::endl;
    std::cout<<"MidVV: Velocities:"<<std::endl;
    for (SimTK::Compound::AtomIndex aIx(0); aIx < c.getNumAtoms(); ++aIx){
        vel      = c.calcAtomVelocityInGroundFrame(advanced, aIx);
        std::cout<<c.getAtomName(aIx)<<"="<<std::setprecision(8)<<std::fixed
          <<"["<<vel[0]<<" "<<vel[1]<<" "<<vel[2]<<"]"<<std::endl;
    }
    std::cout<<std::endl;
    std::cout<<"MidVV: Accelerations: \n"<<std::endl; // At least Acceleration Stage
    for (SimTK::Compound::AtomIndex aIx(0); aIx < c.getNumAtoms(); ++aIx){
      acc = c.calcAtomAccelerationInGroundFrame(advanced, aIx);// * c.getAtomMass(aIx);
      std::cout<<c.getAtomName(aIx)<<"="<<std::setprecision(8)<<std::fixed
          <<"["<<acc[0]<<" "<<acc[1]<<" "<<acc[2]<<"]"<<std::endl;
    }
    std::cout<<std::endl;
}

void MidVVIntegratorRep::printPoss(const SimTK::Compound& c, SimTK::State& advanced)
{
    SimTK::Vec3 vertex;
    std::cout<<"MidVV: Positions:"<<std::endl;
    for (SimTK::Compound::AtomIndex aIx(0); aIx < c.getNumAtoms(); ++aIx){
        vertex   = c.calcAtomLocationInGroundFrame(advanced, aIx);
        std::cout<<c.getAtomName(aIx)<<"="<<std::setprecision(8)<<std::fixed
          <<"["<<vertex[0]<<" "<<vertex[1]<<" "<<vertex[2]<<"]"<<std::endl;
    }
    std::cout<<std::endl;
}

void MidVVIntegratorRep::printVels(const SimTK::Compound& c, SimTK::State& advanced)
{
    SimTK::Vec3 vel;
    std::cout<<"MidVV: Velocities:"<<std::endl;
    for (SimTK::Compound::AtomIndex aIx(0); aIx < c.getNumAtoms(); ++aIx){
        vel      = c.calcAtomVelocityInGroundFrame(advanced, aIx);
        std::cout<<c.getAtomName(aIx)<<"="<<std::setprecision(8)<<std::fixed
          <<"["<<vel[0]<<" "<<vel[1]<<" "<<vel[2]<<"]"<<std::endl;
    }
    std::cout<<std::endl;
}

void MidVVIntegratorRep::printAccs(const SimTK::Compound& c, SimTK::State& advanced)
{
    SimTK::Vec3 acc;
    std::cout<<"MidVV: Accelerations:"<<std::endl; // At least Acceleration Stage
    for (SimTK::Compound::AtomIndex aIx(0); aIx < c.getNumAtoms(); ++aIx){
      acc = c.calcAtomAccelerationInGroundFrame(advanced, aIx);
      std::cout<<c.getAtomName(aIx)<<"="<<std::setprecision(8)<<std::fixed
        <<"["<<acc[0]<<" "<<acc[1]<<" "<<acc[2]<<"]"<<std::endl;
    }
    std::cout<<std::endl;
}

void MidVVIntegratorRep::printForces(const SimTK::Compound& c, SimTK::State& advanced)
{
    SimTK::Vec3 acc;
    SimTK::Real mass;
    std::cout<<"MidVV: Forces:"<<std::endl; // At least Acceleration Stage
    for (SimTK::Compound::AtomIndex aIx(0); aIx < c.getNumAtoms(); ++aIx){
      acc = c.calcAtomAccelerationInGroundFrame(advanced, aIx);
      mass = c.getAtomElement(aIx).getMass();
      std::cout<<c.getAtomName(aIx)<<"="<<std::setprecision(8)<<std::fixed
        <<"["<<acc[0]*mass<<" "<<acc[1]*mass<<" "<<acc[2]*mass<<"]"<<std::endl;
    }
    std::cout<<std::endl;
}

void MidVVIntegratorRep::printForcesNorms(const SimTK::Compound& c, SimTK::State& advanced)
{
    SimTK::Vec3 acc;
    SimTK::Real mass;
    std::cout<<"MidVV: Forces norms:"<<std::endl; // At least Acceleration Stage
    for (SimTK::Compound::AtomIndex aIx(0); aIx < c.getNumAtoms(); ++aIx){
      acc = c.calcAtomAccelerationInGroundFrame(advanced, aIx);// * c.getAtomMass(aIx);
      mass = c.getAtomElement(aIx).getMass();
      std::cout<<c.getAtomName(aIx)<<"="<<std::setprecision(8)<<std::fixed
        <<"[ "<<sqrt(sqr(acc[0]*mass) + sqr(acc[1]*mass) + sqr(acc[2]*mass))<<" ]"<<std::endl;
    }
    std::cout<<std::endl;
}


// * Calculate various quants * //
// Compute BAT mass matrix determinant (Jain et al, 2013) - used for Fixman potential
SimTK::Real MidVVIntegratorRep::calcDetMBAT(const SimTK::Compound& c, SimTK::State& advanced)
{
  const SimTK::SimbodyMatterSubsystem& matter = Caller->system->getMatterSubsystem();
  SimTK::Real detMBAT = 1.0;
  SimTK::Real dist, theta, mass;
  for (SimTK::Compound::AtomIndex aIx(0); aIx < c.getNumAtoms(); ++aIx){
    SimTK::MobilizedBodyIndex mbx = c.getAtomMobilizedBodyIndex(aIx);
    const SimTK::MobilizedBody& mobod = matter.getMobilizedBody(mbx);
    SimTK::Transform X_GB = mobod.getBodyTransform(advanced);
    SimTK::Transform X_PF = mobod.getDefaultInboardFrame();

    const SimTK::MobilizedBody& inbMobod =  mobod.getParentMobilizedBody();
    SimTK::MobilizedBodyIndex inbMbx = inbMobod.getMobilizedBodyIndex();
    SimTK::Transform inbX_GB = inbMobod.getBodyTransform(advanced);

    SimTK::Vec3 p_BS = c.getAtomLocationInMobilizedBodyFrame(aIx);
    SimTK::Vec3 p_GS = X_GB * p_BS;
    SimTK::Vec3 A, B;
    mass = c.getAtomElement(aIx).getMass();
    detMBAT *= (mass * mass * mass);
 
    if(int(inbMbx) == 0){
      SimTK::MobilizedBodyIndex outMbx = SimTK::MobilizedBodyIndex(2);
      const SimTK::MobilizedBody& outMobod = matter.getMobilizedBody(outMbx);
      SimTK::Transform outX_GB = outMobod.getBodyTransform(advanced);
      if(p_BS == 0){
        A = inbX_GB.p() - X_GB.p();
        B = X_GB.p() - outX_GB.p();
      }else{
        dist = p_BS.norm();
        detMBAT *= (dist * dist * dist * dist);
        A = X_GB.p() - p_GS;
        B = outX_GB.p() - X_GB.p();
        theta = (SimTK::Pi - std::acos(  dot(A, B) / (A.norm() * B.norm())  ));
        detMBAT *= (std::sin(theta) * std::sin(theta));
      }
    }else{
      if(p_BS == 0){
        if(int(inbMbx) > 0){
          const SimTK::MobilizedBody& inbInbMobod =  inbMobod.getParentMobilizedBody();
          SimTK::MobilizedBodyIndex inbInbMbx = inbInbMobod.getMobilizedBodyIndex();
          dist = X_PF.p().norm();
          detMBAT *= (dist * dist * dist * dist);
          SimTK::Transform inbInbX_GB = inbInbMobod.getBodyTransform(advanced);
          A = inbX_GB.p() - X_GB.p();
          B = inbInbX_GB.p() - inbX_GB.p();
        }
      }
      else{
        dist = p_BS.norm();
        detMBAT *= (dist * dist * dist * dist);
        A = X_GB.p() - p_GS;
        B = inbX_GB.p() - X_GB.p();
      }
      theta = (SimTK::Pi - std::acos(  dot(A, B) / (A.norm() * B.norm())  ));
      detMBAT *= (std::sin(theta) * std::sin(theta));
    }
  }
  return detMBAT;
}

// Compute mass matrix and its determinant
SimTK::Real MidVVIntegratorRep::calcMAndDetM(const SimTK::Compound& c, SimTK::State& advanced)
{
  ; // TODO for Fixman potential
}

// Compute Fixman potential
SimTK::Real MidVVIntegratorRep::calcUFix(const SimTK::Compound& c, SimTK::State& advanced)
{
  const SimTK::System& system   = getSystem();
  const SimTK::SimbodyMatterSubsystem& matter = Caller->system->getMatterSubsystem();
  system.realize(advanced, SimTK::Stage::Position);
  TARGET_TYPE RT = getTb() * SimTK_BOLTZMANN_CONSTANT_MD;

  int nu = advanced.getNU();
  SimTK::Real UFix; // Fixman potential
  Eigen::MatrixXd EiM(nu, nu);
  SimTK::Matrix M;
  matter.calcM(advanced, M);
  for(int i=0; i<nu; i++){ // Put M in Eignn
    for(int j=0; j<nu; j++){
      EiM(i, j) = M(i, j);
    }
  }

  UFix = 0.5 * RT * (std::log(EiM.determinant() / calcDetMBAT(c, advanced))); // Jain
  //UFix =  -0.5 * RT * std::log(EiM.determinant()); // Patriciu
  return UFix;
}

// Calculate total linear velocity
SimTK::Real MidVVIntegratorRep::calcTotLinearVel(SimTK::State& advanced)
{
  const SimTK::SimbodyMatterSubsystem& matter = Caller->system->getMatterSubsystem();
  SimTK::Vec3 totVel(.0, .0, .0);
  for (SimTK::MobilizedBodyIndex mbx(1); mbx < matter.getNumBodies(); ++mbx){
    const SimTK::MobilizedBody& mobod = matter.getMobilizedBody(mbx);
    totVel += mobod.getBodyOriginVelocity(advanced);
  }
  return totVel.norm();
}

// Compute kinetic energy from atoms
SimTK::Real MidVVIntegratorRep::calcKEFromAtoms(const SimTK::Compound& c, SimTK::State& advanced)
{
  SimTK::Real ke = .0;
  SimTK::Real mass;
  SimTK::Vec3 vel;
  for (SimTK::Compound::AtomIndex aIx(0); aIx < c.getNumAtoms(); ++aIx){
    vel = c.calcAtomVelocityInGroundFrame(advanced, aIx);
    mass = c.getAtomElement(aIx).getMass();
    ke += mass * dot(vel, vel);
  }
  return ke/2;
}

SimTK::Real MidVVIntegratorRep::calcPEFromMMTK(void)
{
  PyGILState_STATE GILstate = PyGILState_Ensure();
  #ifdef WITH_THREAD
    Caller->evaluator->tstate_save = PyEval_SaveThread();
  #endif
  (*Caller->evaluator->eval_func)(Caller->evaluator, Caller->p_energy_po, Caller->configuration, 0);
  #ifdef WITH_THREAD
    PyEval_RestoreThread(Caller->evaluator->tstate_save);
  #endif
  PyGILState_Release(GILstate);
  return Caller->p_energy_po->energy;
}


// * Initialize velocities from random generalized vels
void MidVVIntegratorRep::initializeVelsFromRandU(const SimTK::Compound& c, SimTK::State& advanced, SimTK::Real TbTemp)
{
  //const SimTK::SimbodyMatterSubsystem& matter = Caller->system->getMatterSubsystem();
  const SimTK::System& system   = getSystem();
  const SimTK::Vector& Qs = advanced.getQ();

  SimTK::Vector V(advanced.getNU());
  for (int i=0; i < advanced.getNU(); ++i){
    V[i] = unirand.getValue();
  }

  advanced.updU() = V;
  system.realize(advanced, SimTK::Stage::Velocity);

  //SimTK::Real T = (2*compoundSystem->calcKineticEnergy(advanced)) / ((c.getNumAtoms()*3-6)*SimTK_BOLTZMANN_CONSTANT_MD);
  //const SimTK::Real scale = std::sqrt(TbTemp/T);
  float KETbTemp = 0.5 * SimTK_BOLTZMANN_CONSTANT_MD * TbTemp * (3*c.getNumAtoms());
  const SimTK::Real scale = std::sqrt( KETbTemp / compoundSystem->calcKineticEnergy(advanced) );
  advanced.updU() *= scale;
  system.realize(advanced, SimTK::Stage::Velocity);
}

// * Initialize velocities initially from Molmodel Velocity Rescaling Thermostat
// * Rewritten according to marginal probabilty (Jain 2012 - Equipartition principle)
void MidVVIntegratorRep::initializeVelsFromVRT(const SimTK::Compound& c, SimTK::State& advanced, SimTK::Real TbTemp)
{
  advanced.updU() = 0.0;

  const SimTK::SimbodyMatterSubsystem& matter = Caller->system->getMatterSubsystem();
  const SimTK::System& system   = getSystem();
  int nq = advanced.getNQ();
  int nu = advanced.getNU();
  int nb = matter.getNumBodies();
  const int m  = advanced.getNMultipliers();

  SimTK::Vector V(nu);
  SimTK::Vector Vmod(nu);
  SimTK::Vector MInvV(nu);
  for (int i=0; i < advanced.getNU(); ++i){
    V[i] = gaurand.getValue();
  }

  system.realize(advanced, SimTK::Stage::Position);
  system.realize(advanced, SimTK::Stage::Acceleration); // We need this stage for the inverse mass matrix

  //// State must have already been realized to Position stage.
  //// MInv must be resizeable or already the right size (nXn).
  SimTK::Matrix M;
  matter.calcM(advanced, M);

  SimTK::Matrix MInv;
  matter.calcMInv(advanced, MInv);

  Eigen::VectorXd EiV(nu);
  Eigen::VectorXd EiVmod(nu);
  Eigen::MatrixXd EiM(nu, nu);
  Eigen::MatrixXd EiMInv(nu, nu);
  for(int i=0; i<nu; i++){ // Put V in Eigen
    EiV(i) = V(i);
  }
  for(int i=0; i<nu; i++){ // Put MInv in Eigen
    for(int j=0; j<nu; j++){
      EiM(i, j) = M(i, j);
      EiMInv(i, j) = MInv(i, j);
    }
  }
  Eigen::SelfAdjointEigenSolver<Eigen::MatrixXd> Eims(EiM);
  Eigen::MatrixXd Eim = Eims.operatorSqrt();
  this->detsqrtMi = Eim.determinant(); // Bottleneck
  Eigen::SelfAdjointEigenSolver<Eigen::MatrixXd> Eils(EiMInv);
  Eigen::MatrixXd Eil = Eils.operatorSqrt();
  EiVmod = Eil * EiV; // Multiply by sqrt of inverse mass matrix
  for(int i=0; i<nu; i++){ // Get Vmod from Eigen
    Vmod(i) = EiVmod(i);
  }
  Vmod *= sqrtkTb; // Set stddev according to temperature

  advanced.updU() = Vmod;
  system.realize(advanced, SimTK::Stage::Velocity);

  SimTK::Real T = (2*compoundSystem->calcKineticEnergy(advanced)) / ((nu)*SimTK_BOLTZMANN_CONSTANT_MD);
  float KETbTemp = 0.5 * kTb * (nu);
  const SimTK::Real scale = std::sqrt(TbTemp/T);
  advanced.updU() *= scale;
  system.realize(advanced, SimTK::Stage::Velocity);
}

// * Initialize vels from atoms inertias
void MidVVIntegratorRep::initializeVelsFromAtoms(const SimTK::Compound& c, SimTK::State& advanced, SimTK::Real TbTemp)
{
  advanced.updU() = 0.0;
  SimTK::Vec3 vertex;
  SimTK::Real kT = SimTK_BOLTZMANN_CONSTANT_MD*TbTemp;
  SimTK::Real invKT = 1/(SimTK_BOLTZMANN_CONSTANT_MD*TbTemp);
  SimTK::Real sqrtInvBeta = std::sqrt(kT);

  const SimTK::SimbodyMatterSubsystem& matter = Caller->system->getMatterSubsystem();
  const SimTK::System& system   = getSystem();

  TARGET_TYPE Is[matter.getNumBodies()-1][3];
  TARGET_TYPE  sigma[matter.getNumBodies()-1];
  TARGET_TYPE wsigma[matter.getNumBodies()-1][3];
  for(int i=0; i<matter.getNumBodies()-1; i++){
    Is[i][0] = Is[i][1] = Is[i][2] = .0;
  }
  for(int i=0; i<matter.getNumBodies()-1; i++){
    sigma[i] = 0;
    wsigma[i][0] = wsigma[i][1] = wsigma[i][2] = .0;
  }
  for (SimTK::Compound::AtomIndex aIx(0); aIx < c.getNumAtoms(); ++aIx){
    SimTK::MobilizedBodyIndex mbx = Caller->lig1->getAtomMobilizedBodyIndex(aIx);
    int mbxi = int(mbx);
    int mbx_1 = mbxi-1;
    vertex   = c.calcAtomLocationInGroundFrame(advanced, aIx);
    if(Caller->lig1->getAtomLocationInMobilizedBodyFrame(aIx) != 0){
      Is[mbx_1][0] += c.getAtomElement(aIx).getMass() * vertex[0] * vertex[0];
      Is[mbx_1][1] += c.getAtomElement(aIx).getMass() * vertex[1] * vertex[1];
      Is[mbx_1][2] += c.getAtomElement(aIx).getMass() * vertex[2] * vertex[2];
    }
  }

  for (SimTK::MobilizedBodyIndex mbx(1); mbx < matter.getNumBodies(); ++mbx){
    int mbxi = int(mbx);
    int mbx_1 = mbxi-1;
      if((Caller->mbxTreeMat)[int(mbx)][1] != 0){
        int pambx = Caller->mbxTreeMat[mbx][1];
        Is[mbx_1][0] += Is[pambx-1][0];
        Is[mbx_1][1] += Is[pambx-1][1];
        Is[mbx_1][2] += Is[pambx-1][2];
      }
  }

  for (SimTK::MobilizedBodyIndex mbx(1); mbx < matter.getNumBodies(); ++mbx){
    int mbxi = int(mbx);
    int mbx_1 = mbxi-1;
    const SimTK::MobilizedBody& mobod = matter.getMobilizedBody(mbx);
    SimTK::MassProperties M_Bo_B = mobod.getBodyMassProperties(advanced);
    sigma[mbx_1]      = 1.0 / std::sqrt(M_Bo_B.getMass()); sigma[mbx_1] *= sqrtInvBeta;
    if(Is[mbx_1][0] != 0){
      wsigma[mbx_1][0]  = 1.0 / std::sqrt(Is[mbx_1][0]); wsigma[mbx_1][0] *= sqrtInvBeta;
      wsigma[mbx_1][1]  = 1.0 / std::sqrt(Is[mbx_1][1]); wsigma[mbx_1][1] *= sqrtInvBeta;
      wsigma[mbx_1][2]  = 1.0 / std::sqrt(Is[mbx_1][2]); wsigma[mbx_1][2] *= sqrtInvBeta;
    }
  }

  for (SimTK::MobilizedBodyIndex mbx(1); mbx < matter.getNumBodies(); ++mbx){
    int mbxi = int(mbx);
    int mbx_1 = mbxi-1;
    const SimTK::MobilizedBody& mobod = matter.getMobilizedBody(mbx);
    const SimTK::Vec3& p_GB = mobod.getBodyOriginLocation(advanced);

    SimTK::Random::Gaussian vrand(0.0, sigma[mbx_1]);
    vrand.setSeed(*Caller->pyseed);

    SimTK::Random::Gaussian wxrand(0.0, wsigma[mbx_1][0]);
    wxrand.setSeed(*Caller->pyseed);
    SimTK::Random::Gaussian wyrand(0.0, wsigma[mbx_1][1]);
    wyrand.setSeed(*Caller->pyseed);
    SimTK::Random::Gaussian wzrand(0.0, wsigma[mbx_1][2]);
    wzrand.setSeed(*Caller->pyseed);

    SimTK::Vec3 v(unirand.getValue()-0.5, unirand.getValue()-0.5, unirand.getValue()-0.5);
    SimTK::Vec3 w(unirand.getValue()-0.5, unirand.getValue()-0.5, unirand.getValue()-0.5);
    mobod.setUToFitLinearVelocity(advanced, v);
    mobod.setUToFitAngularVelocity(advanced, w);
  }

  system.realize(advanced, SimTK::Stage::Velocity);
  SimTK::Real T = (2*compoundSystem->calcKineticEnergy(advanced)) / ((c.getNumAtoms()*3-6)*SimTK_BOLTZMANN_CONSTANT_MD);
  float KETbTemp = 0.5*TbTemp*SimTK_BOLTZMANN_CONSTANT_MD*(3*c.getNumAtoms());
  const SimTK::Real scale = std::sqrt( KETbTemp / compoundSystem->calcKineticEnergy(advanced) );
  advanced.updU() *= scale;
  system.realize(advanced, SimTK::Stage::Velocity);
}
     
// * Initialize velocities from mobods spatial inertias
void MidVVIntegratorRep::initializeVelsFromMobods(const SimTK::Compound& c, SimTK::State& advanced, SimTK::Real TbTemp)
{
  const SimTK::System& system   = getSystem();
  system.realize(advanced, SimTK::Stage::Acceleration);

  advanced.updU() = 0.0;
  SimTK::Real sigma;
  SimTK::Real wsigma;
  SimTK::Real vxsigma, vysigma, vzsigma;
  SimTK::Real wxsigma, wysigma, wzsigma;
  SimTK::Real wx, wy, wz, vx, vy, vz;
  wx = wy = wz = vx = vy = vz = 0.0;
  SimTK::Real kT = SimTK_BOLTZMANN_CONSTANT_MD*TbTemp;
  SimTK::Real invKT = 1/(SimTK_BOLTZMANN_CONSTANT_MD*TbTemp);
  SimTK::Real sqrtInvBeta = std::sqrt(kT);


  this->unirand.setSeed(*Caller->pyseed + time(NULL) + getTrial() + getpid());
  this->gaurand.setSeed(*Caller->pyseed + time(NULL) + getTrial() + getpid());
  for (SimTK::MobilizedBodyIndex mbx(1); mbx < matter.getNumBodies(); ++mbx){
    const SimTK::MobilizedBody& mobod = matter.getMobilizedBody(mbx);
    const SimTK::Transform&  X_GF = mobod.getDefaultInboardFrame(); // X_GF.p = dist mobod-mobod
    const SimTK::Transform&  X_FM = mobod.getMobilizerTransform(advanced);
    const SimTK::Transform&  X_GB = mobod.getBodyTransform(advanced);
    const SimTK::Rotation&   R_GB = mobod.getBodyRotation(advanced);
    const SimTK::Vec3& p_GB = mobod.getBodyOriginLocation(advanced);

    SimTK::MassProperties M_Bo_B = mobod.getBodyMassProperties(advanced);
    const SimTK::SpatialInertia SI_Bc_B(M_Bo_B.getMass(), M_Bo_B.getMassCenter(), M_Bo_B.getUnitInertia());
    SimTK::SpatialInertia SI_Bc_G = SI_Bc_B.transform(~X_GB);

    // Add parents Inertia
    for(int j=1; j<matter.getNumBodies(); j++){
      if(Caller->mbxTreeMat[mbx][j] == 0){ break;}
      const SimTK::MobilizedBody& pamobod = matter.getMobilizedBody(SimTK::MobilizedBodyIndex(Caller->mbxTreeMat[mbx][j]));
      SimTK::MassProperties paM_Bo_B = mobod.getBodyMassProperties(advanced);
      const SimTK::SpatialInertia paSI_Bc_B(paM_Bo_B.getMass(), paM_Bo_B.getMassCenter(), paM_Bo_B.getUnitInertia());
      SimTK::SpatialInertia paSI_Bc_G = paSI_Bc_B.transform(~X_GB);
      SI_Bc_G += paSI_Bc_G;
    }
  
    SimTK::SpatialMat mat = SI_Bc_G.toSpatialMat();
  
    const SimTK::Real mobod_mass = M_Bo_B.getMass();
    int pseudoLocalSeed = int(p_GB[0])%10;
    SimTK::Real OriginIInGround = mobod_mass*p_GB.norm();

    SimTK::Real sigma = sqrtInvBeta/std::sqrt(mobod_mass);

    SimTK::SpatialVec TbTempSv(SimTK::Vec3(kT,kT,kT), SimTK::Vec3(kT,kT,kT));
    SimTK::SpatialVec invTbTempSv(SimTK::Vec3(invKT,invKT,invKT), SimTK::Vec3(invKT,invKT,invKT));
    SimTK::SpatialVec sqrSigmaSv = SI_Bc_G * invTbTempSv;
    SimTK::SpatialVec absSqrSigmaSv = sqrSigmaSv.abs();
    SimTK::SpatialVec invAbsSqrSigmaSv(SimTK::Vec3(1/absSqrSigmaSv[0][0], 1/absSqrSigmaSv[0][1], 1/absSqrSigmaSv[0][2]),
                                SimTK::Vec3(1/absSqrSigmaSv[1][0], 1/absSqrSigmaSv[1][1], 1/absSqrSigmaSv[1][2]));
    SimTK::SpatialVec sigmaSv = invAbsSqrSigmaSv.sqrt();

    gaurand.setStdDev(sigmaSv[0][0]); wx = gaurand.getValue();
    gaurand.setStdDev(sigmaSv[0][1]); wy = gaurand.getValue();
    gaurand.setStdDev(sigmaSv[0][2]); wz = gaurand.getValue();
    gaurand.setStdDev(sigmaSv[1][0]); vx = gaurand.getValue();
    gaurand.setStdDev(sigmaSv[1][1]); vy = gaurand.getValue();
    gaurand.setStdDev(sigmaSv[1][2]); vz = gaurand.getValue();
    SimTK::SpatialVec sv(SimTK::Vec3(wx, wy, wz), SimTK::Vec3(vx, vy, vz));

    mobod.setUToFitVelocity(advanced, sv);
  }

  system.realize(advanced, SimTK::Stage::Velocity);
  SimTK::Real dofs = (c.getNumAtoms()*3-6);
  SimTK::Real T = (2*compoundSystem->calcKineticEnergy(advanced)) / (dofs*SimTK_BOLTZMANN_CONSTANT_MD);
  const SimTK::Real scale = std::sqrt(TbTemp/T);
  advanced.updU() *= scale;
  system.realize(advanced, SimTK::Stage::Velocity);
}


// * Initialize velocities from mobods spatial inertias
void MidVVIntegratorRep::initializeVelsFromJacobian(const SimTK::Compound& c, SimTK::State& advanced, SimTK::Real TbTemp)
{
  const SimTK::System& system   = getSystem();
  system.realize(advanced, SimTK::Stage::Acceleration);
  advanced.updU() = 0.0;

  SimTK::Real kT = SimTK_BOLTZMANN_CONSTANT_MD*TbTemp;
  const int nq = advanced.getNQ();
  const int nu = advanced.getNU();
  const int nb = matter.getNumBodies();

  this->gaurand.setSeed(*Caller->pyseed + time(NULL) + getTrial() + getpid());

  SimTK::Real m[nb]; // masses
  SimTK::Vec3 I[nb]; // moments of inertia
  SimTK::Vector_<SimTK::SpatialVec> T6n(nb, SimTK::SpatialVec(
    SimTK::Vec3(gaurand.getValue(), gaurand.getValue(), gaurand.getValue()),
    SimTK::Vec3(gaurand.getValue(), gaurand.getValue(), gaurand.getValue())));
  SimTK::Vector u_vec(nu);
  for (SimTK::MobilizedBodyIndex mbx(1); mbx < nb; ++mbx){
    const SimTK::MobilizedBody& mobod = matter.getMobilizedBody(mbx);
    SimTK::MassProperties M_Bo_B = mobod.getBodyMassProperties(advanced);
    m[mbx] = M_Bo_B.getMass();
    I[mbx] = M_Bo_B.getInertia().getMoments();
  }
  for(int i=1; i<nb; i++){
    T6n[i][0] *= std::sqrt(kT/m[i]);
    if(I[i][0]){
      T6n[i][1][0] *= std::sqrt(kT/I[i][0]);
      T6n[i][1][1] *= std::sqrt(kT/I[i][1]);
      T6n[i][1][2] *= std::sqrt(kT/I[i][2]);
    }else{
      T6n[i][1][0] = T6n[i][1][1] = T6n[i][1][2] = 0.0;
    }
  }
  T6n[0] = SimTK::SpatialVec(0);
  SimTK::Matrix_<SimTK::SpatialVec> J_G(nb, nu, SimTK::SpatialVec(SimTK::Vec3(0)));
  matter.calcSystemJacobian(advanced, J_G); // nb X nu
  std::cout<<"MidVV inivels J_G "<<J_G<<std::endl; fflush(stdout);
  SimTK::Matrix_<double> JM(6*nb, nu);
  for(int i=0; i<nb; i++){
    for(int j=0; j<nu; j++){
      for(int k=0; k<6; k++){
        if(k<3){
          JM[6*i+k][j] = J_G[i][j][0][k];
        }
        else{
          JM[6*i+k][j] = J_G[i][j][1][k-3];
        }
      }
    }
  }
  std::cout<<"MidVV inivels JM "<<JM<<std::endl; fflush(stdout);
  advanced.updU() = u_vec;
  system.realize(advanced, SimTK::Stage::Velocity);

  // Scale to temperature
  float dofs = (c.getNumAtoms()*3-6);
  SimTK::Real T = (2*compoundSystem->calcKineticEnergy(advanced)) / (dofs*SimTK_BOLTZMANN_CONSTANT_MD);
  SimTK::Real KETbTemp = 0.5*kT*dofs;
  const SimTK::Real scale = std::sqrt( KETbTemp / compoundSystem->calcKineticEnergy(advanced) );
  advanced.updU() *= scale;
  system.realize(advanced, SimTK::Stage::Velocity);
}


// * Initialize velocities from const vector of spatial vels
void MidVVIntegratorRep::initializeVelsFromConst(const SimTK::Compound& c, SimTK::State& advanced, SimTK::SpatialVec *specVels)
{
  const SimTK::System& system   = getSystem();
  for (SimTK::MobilizedBodyIndex mbx(1); mbx < matter.getNumBodies(); ++mbx){
    const SimTK::MobilizedBody& mobod = matter.getMobilizedBody(mbx);
    mobod.setUToFitVelocity(advanced, specVels[int(mbx)]);
  }
  system.realize(advanced, SimTK::Stage::Velocity);
}

// * Write data into shm for exchange with other funcs
void MidVVIntegratorRep::writePossVelsToShm(const SimTK::Compound& c, SimTK::State& advanced)
{
  SimTK::Vec3 vertex(0,0,0);
  SimTK::Vec3    vel(0,0,0);

  int ix = 0;
  for (SimTK::Compound::AtomIndex aIx(0); aIx < c.getNumAtoms(); ++aIx){
    vertex   = c.calcAtomLocationInGroundFrame(advanced, aIx);
    vel      = c.calcAtomVelocityInGroundFrame(advanced, aIx);
      
    (this->coords)[ix][0] = vertex[0];
    (this->coords)[ix][1] = vertex[1];
    (this->coords)[ix][2] = vertex[2];
  
    (this->vels)[ix][0] = vel[0];
    (this->vels)[ix][1] = vel[1];
    (this->vels)[ix][2] = vel[2];
 
    ix++;
  }
}

// * Write data into shm for exchange with other funcs
void MidVVIntegratorRep::writePossToShm(const SimTK::Compound& c, SimTK::State& advanced)
{
  SimTK::Vec3 vertex(0,0,0);

  int ix = 0;
  for (SimTK::Compound::AtomIndex aIx(0); aIx < c.getNumAtoms(); ++aIx){
    vertex   = c.calcAtomLocationInGroundFrame(advanced, aIx);
      
    (this->coords)[ix][0] = vertex[0];
    (this->coords)[ix][1] = vertex[1];
    (this->coords)[ix][2] = vertex[2];
  
    ix++;
  }
}

// * Set TVector of transforms from mobods * //
void MidVVIntegratorRep::setTVector(SimTK::State& advanced)
{
  const SimTK::SimbodyMatterSubsystem& matter = Caller->system->getMatterSubsystem();
  int i = 0;
  for (SimTK::MobilizedBodyIndex mbx(1); mbx < matter.getNumBodies(); ++mbx){
    const SimTK::MobilizedBody& mobod = matter.getMobilizedBody(mbx);
    const SimTK::Vec3& vertex = mobod.getBodyOriginLocation(advanced);
    TVector[i] = mobod.getMobilizerTransform(advanced);
    i++;
  } 
}

// * Transfer coordinates from TVector to compound * //
void MidVVIntegratorRep::assignConfFromTVector(SimTK::State& advanced)
{
  const SimTK::SimbodyMatterSubsystem& matter = Caller->system->getMatterSubsystem();
  int i = 0;
  for (SimTK::MobilizedBodyIndex mbx(1); mbx < matter.getNumBodies(); ++mbx){
    const SimTK::MobilizedBody& mobod = matter.getMobilizedBody(mbx);
    mobod.setQToFitTransform(advanced, TVector[i]);
    i++;
  }
}

// * Transfer coordinates from shm to compound 0 * /
// * Order: shm[indexMap] -> atomTargets * /
void MidVVIntegratorRep::assignConfAndTVectorFromShm0(SimTK::State& advanced)
{
  // Declare vars
  int i;
  const SimTK::SimbodyMatterSubsystem& matter = Caller->system->getMatterSubsystem();
  vector3 *xMid;
  int tx, tshm; // x -> shm index correspondence
  SimTK::Compound& mutc1 = compoundSystem->updCompound(SimTK::CompoundSystem::CompoundIndex(0));
  SimTK::Transform X_CoAt[mutc1.getNumAtoms()];
  SimTK::Vec3 p_G[mutc1.getNumAtoms()];

  // Store initial configuration
  i = 0;
  for (SimTK::MobilizedBodyIndex mbx(1); mbx < matter.getNumBodies(); ++mbx){
    const SimTK::MobilizedBody& mobod = matter.getMobilizedBody(mbx);
    const SimTK::Vec3& vertex = mobod.getBodyOriginLocation(advanced);
    TVector[i] = mobod.getMobilizerTransform(advanced);
    i++;
  }
 
  // Declare maps
  std::map<SimTK::Compound::AtomIndex, SimTK::Vec3>           atomTargets;
  std::map<SimTK::MobilizedBodyIndex, SimTK::Compound::AtomIndex> mbx2aIx;
  std::map<SimTK::Compound::AtomIndex, SimTK::MobilizedBodyIndex> aIx2mbx;

  // Load maps
  int ix = 0;
  for (SimTK::Compound::AtomIndex aIx(0); aIx < mutc1.getNumAtoms(); ++aIx){
    SimTK::MobilizedBodyIndex mbx = mutc1.getAtomMobilizedBodyIndex(aIx);
    if(mutc1.getAtomLocationInMobilizedBodyFrame(aIx) == 0){
      mbx2aIx.insert(pair<SimTK::MobilizedBodyIndex, SimTK::Compound::AtomIndex>(mbx, aIx));
    }
    aIx2mbx.insert(pair<SimTK::Compound::AtomIndex, SimTK::MobilizedBodyIndex>(aIx, mbx));

    SimTK::Vec3 v(coords[ Caller->_indexMap[ix][1] ][0],
           coords[ Caller->_indexMap[ix][1] ][1],
           coords[ Caller->_indexMap[ix][1] ][2]);
    atomTargets.insert(pair<SimTK::Compound::AtomIndex, SimTK::Vec3>(((Caller->lig1->bAtomList)[ix]).atomIndex, v));
    ix++;
  }

  // Match
  mutc1.matchDefaultTopLevelTransform(atomTargets);
  mutc1.matchDefaultConfiguration(atomTargets, SimTK::Compound::Match_Exact, true, 150.0); //Compound::Match_Idealized
  i = 0;
  for (SimTK::Compound::AtomIndex aIx(0); aIx < mutc1.getNumAtoms(); ++aIx){
    X_CoAt[i] = mutc1.calcDefaultAtomFrameInCompoundFrame(aIx); // OK
    i++;
  }

  // Calculate the transforms
  const SimTK::Transform& X_Top = mutc1.getTopLevelTransform();
  SimTK::Transform X_PF[matter.getNumBodies()];
  SimTK::Transform X_FMAt[matter.getNumBodies()];
  SimTK::Transform X_BM[matter.getNumBodies()];
  SimTK::Transform X_AtInAt[mutc1.getNumAtoms()];
  mbx2aIx.insert(std::pair<SimTK::MobilizedBodyIndex, SimTK::Compound::AtomIndex> (SimTK::MobilizedBodyIndex(0), SimTK::Compound::AtomIndex(99999999)));
  aIx2mbx.insert(std::pair<SimTK::Compound::AtomIndex, SimTK::MobilizedBodyIndex> (SimTK::Compound::AtomIndex(99999999), SimTK::MobilizedBodyIndex(0)));

  i=0;
  X_PF[0]    = (matter.getMobilizedBody(SimTK::MobilizedBodyIndex(0))).getDefaultInboardFrame();
  X_FMAt[0]  = (matter.getMobilizedBody(SimTK::MobilizedBodyIndex(0))).getMobilizerTransform(advanced);
  X_BM[matter.getNumBodies()-1] = X_PF[0];
  for (SimTK::MobilizedBodyIndex ChildMbx(1); ChildMbx < matter.getNumBodies(); ++ChildMbx){ // Child mobod
    const SimTK::MobilizedBody& ChildMobod = matter.getMobilizedBody(ChildMbx);
    const SimTK::MobilizedBody& ParentMobod =  ChildMobod.getParentMobilizedBody();
    SimTK::MobilizedBodyIndex ParentMbx = ParentMobod.getMobilizedBodyIndex();
    SimTK::Compound::AtomIndex ChildAix = mbx2aIx.at(ChildMbx); //which aIx correspond to Child  mobod
    SimTK::Compound::AtomIndex ParentAix = mbx2aIx.at(ParentMbx); //which aIx correspond to Parent mobod
    int iChildMb=int(ChildMbx), iParentMb=int(ParentMbx);
    int iChildAt=int(ChildAix), iParentAt=int(ParentAix);

    const SimTK::Vec3& vertex = ChildMobod.getBodyOriginLocation(advanced);
    X_PF[iChildMb] = ChildMobod.getDefaultInboardFrame();
    if(iChildMb < matter.getNumBodies() - 1){
      X_BM[iChildMb] = ChildMobod.getDefaultOutboardFrame();
    }

    if(int(ParentAix) != 99999999){
      X_AtInAt[ChildAix] = ~(X_CoAt[ParentAix]) * X_CoAt[ChildAix];
      X_FMAt[iChildMb] = ~(X_PF[iChildMb]) * X_AtInAt[ChildAix] * X_BM[iChildMb];
    }
    else{
      X_AtInAt[ChildAix] = X_Top;
      X_FMAt[iChildMb] = ~(X_PF[iChildMb]) * X_AtInAt[ChildAix] * X_BM[iChildMb];
    }

    i++;
  }

  // Set the Qs
  i = 0;
  for (SimTK::Compound::AtomIndex aIx(0); aIx < mutc1.getNumAtoms(); ++aIx){
    SimTK::MobilizedBodyIndex mbx = mutc1.getAtomMobilizedBodyIndex(aIx);
    SimTK::Vec3 AtomLocInMobod = mutc1.getAtomLocationInMobilizedBodyFrame(aIx);
    const SimTK::MobilizedBody& mobod = matter.getMobilizedBody(mbx);
    if(AtomLocInMobod == 0){
      mobod.setQToFitTransform(advanced, X_FMAt[mbx]); //
    }
    i++;
  }

  // Store initial configuration in TVector
  i = 0;
  for (SimTK::MobilizedBodyIndex mbx(1); mbx < matter.getNumBodies(); ++mbx){
    const SimTK::MobilizedBody& mobod = matter.getMobilizedBody(mbx);
    const SimTK::Vec3& vertex = mobod.getBodyOriginLocation(advanced);
    TVector[i] = X_FMAt[int(mbx)];
    i++;
  } 
} // END assignConfAndTVectorFromShm0()



// * Transfer coordinates from shm to compound 1 * /
// * Order: shm[indexMap] -> atomTargets * /
void MidVVIntegratorRep::assignConfAndTVectorFromShm1(SimTK::State& advanced)
{
  // Declare vars
  int i;
  const SimTK::System& system   = getSystem();
  const SimTK::SimbodyMatterSubsystem& matter = Caller->system->getMatterSubsystem();
  vector3 *xMid;
  int tx, tshm; // x -> shm index correspondence
  SimTK::Compound& mutc1 = compoundSystem->updCompound(SimTK::CompoundSystem::CompoundIndex(0));

  SimTK::Transform X_CoAt[mutc1.getNumAtoms()];
  SimTK::Vec3 p_G[mutc1.getNumAtoms()];

  // Declare maps
  std::map<SimTK::Compound::AtomIndex, SimTK::Vec3>           atomTargets;
  std::map<SimTK::MobilizedBodyIndex, SimTK::Compound::AtomIndex> mbx2aIx;
  std::map<SimTK::Compound::AtomIndex, SimTK::MobilizedBodyIndex> aIx2mbx;
  std::map<SimTK::Compound::AtomIndex, SimTK::Vec3>::iterator atiter;

  // Load maps
  int ix = 0;
  for (SimTK::Compound::AtomIndex aIx(0); aIx < mutc1.getNumAtoms(); ++aIx){
    SimTK::MobilizedBodyIndex mbx = mutc1.getAtomMobilizedBodyIndex(aIx);
    if(mutc1.getAtomLocationInMobilizedBodyFrame(aIx) == 0){
      mbx2aIx.insert(pair<SimTK::MobilizedBodyIndex, SimTK::Compound::AtomIndex>(mbx, aIx));
    }
    aIx2mbx.insert(pair<SimTK::Compound::AtomIndex, SimTK::MobilizedBodyIndex>(aIx, mbx));

    SimTK::Vec3 v(coords[ Caller->_indexMap[ix][1] ][0],
           coords[ Caller->_indexMap[ix][1] ][1],
           coords[ Caller->_indexMap[ix][1] ][2]);  
    atomTargets.insert(pair<SimTK::Compound::AtomIndex, SimTK::Vec3>(((Caller->lig1->bAtomList)[ix]).atomIndex, v));
    ix++;
  }
  
  // Match
  mutc1.matchDefaultTopLevelTransform(atomTargets);
  mutc1.matchDefaultConfiguration(atomTargets, SimTK::Compound::Match_Exact, true, 150.0); //Compound::Match_Idealized

  i = 0;
  for (SimTK::Compound::AtomIndex aIx(0); aIx < mutc1.getNumAtoms(); ++aIx){
    X_CoAt[i] = mutc1.calcDefaultAtomFrameInCompoundFrame(aIx);
    i++;
  }

  // Calculate the transforms
  const SimTK::Transform& X_Top = mutc1.getTopLevelTransform();
  SimTK::Transform X_PF[matter.getNumBodies()];
  SimTK::Transform X_FMAt[matter.getNumBodies()];
  SimTK::Transform X_BM[matter.getNumBodies()];
  SimTK::Transform X_AtInAt[mutc1.getNumAtoms()];
  mbx2aIx.insert(std::pair<SimTK::MobilizedBodyIndex, SimTK::Compound::AtomIndex> (SimTK::MobilizedBodyIndex(0), SimTK::Compound::AtomIndex(99999999)));
  aIx2mbx.insert(std::pair<SimTK::Compound::AtomIndex, SimTK::MobilizedBodyIndex> (SimTK::Compound::AtomIndex(99999999), SimTK::MobilizedBodyIndex(0)));

  i=0;
  X_PF[0]    = (matter.getMobilizedBody(SimTK::MobilizedBodyIndex(0))).getDefaultInboardFrame();
  X_FMAt[0]  = (matter.getMobilizedBody(SimTK::MobilizedBodyIndex(0))).getMobilizerTransform(advanced);
  X_BM[matter.getNumBodies()-1] = X_PF[0];
  for (SimTK::MobilizedBodyIndex ChildMbx(1); ChildMbx < matter.getNumBodies(); ++ChildMbx){ // Child mobod
    const SimTK::MobilizedBody& ChildMobod = matter.getMobilizedBody(ChildMbx);
    const SimTK::MobilizedBody& ParentMobod =  ChildMobod.getParentMobilizedBody();
    SimTK::MobilizedBodyIndex ParentMbx = ParentMobod.getMobilizedBodyIndex();
    SimTK::Compound::AtomIndex ChildAix = mbx2aIx.at(ChildMbx); //which aIx correspond to Child  mobod
    SimTK::Compound::AtomIndex ParentAix = mbx2aIx.at(ParentMbx); //which aIx correspond to Parent mobod
    int iChildMb=int(ChildMbx), iParentMb=int(ParentMbx);
    int iChildAt=int(ChildAix), iParentAt=int(ParentAix);

    const SimTK::Vec3& vertex = ChildMobod.getBodyOriginLocation(advanced);
    X_PF[iChildMb] = ChildMobod.getDefaultInboardFrame();
    if(iChildMb < matter.getNumBodies() - 1){
      X_BM[iChildMb] = ChildMobod.getDefaultOutboardFrame();
    }

    if(int(ParentAix) != 99999999){
      X_AtInAt[ChildAix] = ~(X_CoAt[ParentAix]) * X_CoAt[ChildAix];
      X_FMAt[iChildMb] = ~(X_PF[iChildMb]) * X_AtInAt[ChildAix] * X_BM[iChildMb];
    }
    else{
      X_AtInAt[ChildAix] = X_Top;
      X_FMAt[iChildMb] = ~(X_PF[iChildMb]) * X_AtInAt[ChildAix] * X_BM[iChildMb];
    }

    i++;
  }

  // Set the Qs
  i = 0;
  for (SimTK::Compound::AtomIndex aIx(0); aIx < mutc1.getNumAtoms(); ++aIx){
    SimTK::MobilizedBodyIndex mbx = mutc1.getAtomMobilizedBodyIndex(aIx);
    SimTK::Vec3 AtomLocInMobod = mutc1.getAtomLocationInMobilizedBodyFrame(aIx);
    const SimTK::MobilizedBody& mobod = matter.getMobilizedBody(mbx);
    if(AtomLocInMobod == 0){
      mobod.setQToFitTransform(advanced, X_FMAt[mbx]); //
    }
    i++;
  }

  // Store initial configuration in TVector
  i = 0;
  for (SimTK::MobilizedBodyIndex mbx(1); mbx < matter.getNumBodies(); ++mbx){
    const SimTK::MobilizedBody& mobod = matter.getMobilizedBody(mbx);
    const SimTK::Vec3& vertex = mobod.getBodyOriginLocation(advanced);
    TVector[i] = X_FMAt[int(mbx)];
    i++;
  } 

} // END assignConfAndTVectorFromShm1()


// * Transfer coordinates from shm to compound 2 * /
// * Order: shm[indexMap] -> atomTargets * /
void MidVVIntegratorRep::assignConfAndTVectorFromShm2(SimTK::State& advanced)
{
  // Declare vars
  int i;
  const SimTK::System& system   = getSystem();
  const SimTK::SimbodyMatterSubsystem& matter = Caller->system->getMatterSubsystem();
  SimTK::Compound& mutc1 = compoundSystem->updCompound(SimTK::CompoundSystem::CompoundIndex(0));

  // Declare maps
  std::map<SimTK::Compound::AtomIndex, SimTK::Vec3>           atomTargets;
  std::map<SimTK::MobilizedBodyIndex, SimTK::Compound::AtomIndex> mbx2aIx;
  std::map<SimTK::Compound::AtomIndex, SimTK::MobilizedBodyIndex> aIx2mbx;

  // Load maps
  int ix = 0;
  for (SimTK::Compound::AtomIndex aIx(0); aIx < mutc1.getNumAtoms(); ++aIx){
    SimTK::MobilizedBodyIndex mbx = mutc1.getAtomMobilizedBodyIndex(aIx);
    if(mutc1.getAtomLocationInMobilizedBodyFrame(aIx) == 0){
      mbx2aIx.insert(pair<SimTK::MobilizedBodyIndex, SimTK::Compound::AtomIndex>(mbx, aIx));
    }
    aIx2mbx.insert(pair<SimTK::Compound::AtomIndex, SimTK::MobilizedBodyIndex>(aIx, mbx));

    SimTK::Vec3 v(coords[ Caller->_indexMap[ix][1] ][0],
           coords[ Caller->_indexMap[ix][1] ][1],
           coords[ Caller->_indexMap[ix][1] ][2]);
    atomTargets.insert(pair<SimTK::Compound::AtomIndex, SimTK::Vec3>(((Caller->lig1->bAtomList)[ix]).atomIndex, v));
    ix++;
  }

  // Try Assembler
  Caller->system->realizeTopology();
  SimTK::Assembler assembler(Caller->forces->getMultibodySystem()); 
  SimTK::Markers *AC = new SimTK::Markers();

  SimTK::Array_<SimTK::Markers::MarkerIx, unsigned int> obsOrd;
  ix = 0;
  for (SimTK::Compound::AtomIndex aIx(0); aIx < mutc1.getNumAtoms(); ++aIx){
    SimTK::MobilizedBodyIndex mbx = mutc1.getAtomMobilizedBodyIndex(aIx);
    SimTK::Markers::MarkerIx MIx = AC->addMarker("", mbx, mutc1.getAtomLocationInMobilizedBodyFrame(aIx));
    obsOrd.push_back(MIx);
    ++ix;
  }

  AC->defineObservationOrder(obsOrd);

  SimTK::Array_<SimTK::Vec3> obss;
  ix = 0;
  for (SimTK::Compound::AtomIndex aIx(0); aIx < mutc1.getNumAtoms(); ++aIx){
    obss.push_back(atomTargets[aIx]);
    ++ix;
  }
  ix = 0;
  AC->moveAllObservations(obss);
  SimTK::AssemblyConditionIndex ACIx = assembler.adoptAssemblyGoal(AC);

  try{
    assembler.setUseRMSErrorNorm(true);
    assembler.initialize(advanced);
    SimTK::Real tolerance = assembler.assemble();
    assembler.updateFromInternalState(advanced);
  }
  catch(const std::exception& exc){
    std::cout<<"MidVV Assembly failed: "<<exc.what()<<std::endl;
    std::cout<<"MidVV Return to prev conf: "<<exc.what()<<std::endl;
    assignConfFromTVector(advanced);
  }
  
  system.realize(advanced, SimTK::Stage::Position);

  // Store initial configuration in TVector - different from assignConfAndTVectorFromShm1
  i = 0;
  for (SimTK::MobilizedBodyIndex mbx(1); mbx < matter.getNumBodies(); ++mbx){
    const SimTK::MobilizedBody& mobod = matter.getMobilizedBody(mbx);
    const SimTK::Vec3& vertex = mobod.getBodyOriginLocation(advanced);
    TVector[i] = mobod.getMobilizerTransform(advanced);
    i++;
  } 

} // END assignConfAndTVectorFromShm2()


SimTK::State& MidVVIntegratorRep::assignConfAndTVectorFromShm3Opt(SimTK::State& advanced){
  int i;
  const SimTK::System& system   = getSystem();
  SimTK::SimbodyMatterSubsystem& matter = Caller->system->updMatterSubsystem();
  SimTK::Compound& mutc1 = compoundSystem->updCompound(SimTK::CompoundSystem::CompoundIndex(0));

  // Get atomTargets
  std::map<SimTK::Compound::AtomIndex, SimTK::Vec3>           atomTargets;
  int ix = 0;
  for (SimTK::Compound::AtomIndex aIx(0); aIx < mutc1.getNumAtoms(); ++aIx){
    SimTK::Vec3 v(coords[ Caller->_indexMap[ix][1] ][0],
                  coords[ Caller->_indexMap[ix][1] ][1],
                  coords[ Caller->_indexMap[ix][1] ][2]);  
    atomTargets.insert(pair<SimTK::Compound::AtomIndex, SimTK::Vec3>(((Caller->lig1->bAtomList)[ix]).atomIndex, v));
    ix++;
  }
  // Match
  mutc1.matchDefaultTopLevelTransform(atomTargets);
  mutc1.matchDefaultConfiguration(atomTargets, SimTK::Compound::Match_Exact, true, 150.0); //Compound::Match_Idealized
  // Get transforms: P_X_Ms(=T_X_Bs -> X_PFs), T_X_Mrs
  SimTK::Transform G_X_T = mutc1.getTopLevelTransform();
  SimTK::Transform M_X_pin = SimTK::Rotation(-90*SimTK::Deg2Rad, SimTK::YAxis); // Moves rotation from X to Z
  SimTK::Transform P_X_M[matter.getNumBodies()]; // related to X_PFs
  SimTK::Transform T_X_Mr[matter.getNumBodies()]; // related to CompoundAtom.frameInMobilizedBodyFrame s
  SimTK::Angle inboardBondDihedralAngles[matter.getNumBodies()]; // related to X_FMs
  SimTK::Vec3 locs[mutc1.getNumAtoms()];
  P_X_M[1] = G_X_T * mutc1.calcDefaultAtomFrameInCompoundFrame(SimTK::Compound::AtomIndex(0));
  T_X_Mr[1] = mutc1.calcDefaultAtomFrameInCompoundFrame(SimTK::Compound::AtomIndex(0));
  for (SimTK::Compound::AtomIndex aIx(1); aIx < mutc1.getNumAtoms(); ++aIx){
    if(mutc1.getAtomLocationInMobilizedBodyFrame(aIx) == 0){
      SimTK::MobilizedBodyIndex mbx = mutc1.getAtomMobilizedBodyIndex(aIx);
      const SimTK::MobilizedBody& mobod = matter.getMobilizedBody(mbx);
      const SimTK::MobilizedBody& parentMobod =  mobod.getParentMobilizedBody();
      SimTK::MobilizedBodyIndex parentMbx = parentMobod.getMobilizedBodyIndex();
      SimTK::Compound::AtomIndex parentAIx = mbx2aIx.at(parentMbx);
      inboardBondDihedralAngles[int(mbx)] = mutc1.bgetDefaultInboardDihedralAngle(aIx);
      SimTK::Transform Mr_X_M0 = SimTK::Rotation(inboardBondDihedralAngles[int(mbx)], SimTK::XAxis);
      T_X_Mr[int(mbx)] = mutc1.calcDefaultAtomFrameInCompoundFrame(aIx);
      SimTK::Transform T_X_M0 = T_X_Mr[int(mbx)] * Mr_X_M0;
      const SimTK::Transform& T_X_Fr = mutc1.calcDefaultAtomFrameInCompoundFrame(parentAIx);
      SimTK::Transform Fr_X_T = ~T_X_Fr;
      SimTK::Transform Fr_X_M0 = Fr_X_T * T_X_M0;
      P_X_M[int(mbx)] = Fr_X_M0;
    }
  }
  // Set frameInMobilizedModyFrame s in Compound
  for (SimTK::Compound::AtomIndex aIx(1); aIx < mutc1.getNumAtoms(); ++aIx){
    SimTK::MobilizedBodyIndex mbx = mutc1.getAtomMobilizedBodyIndex(aIx);
    if(mutc1.getAtomLocationInMobilizedBodyFrame(aIx) != 0){
      SimTK::Transform B_X_T = ~(T_X_Mr[int(mbx)]);
      SimTK::Transform T_X_atom =  mutc1.calcDefaultAtomFrameInCompoundFrame(aIx);
      SimTK::Transform B_X_atom = B_X_T * T_X_atom;
      mutc1.bsetFrameInMobilizedBodyFrame(aIx, B_X_atom);
      locs[int(aIx)] = B_X_atom.p();
    }
    else{
      locs[int(aIx)] = SimTK::Vec3(0);
    }
  }
  // Set stations and AtomPLacements for leftover atoms in DuMM
  for (SimTK::Compound::AtomIndex aIx(1); aIx < mutc1.getNumAtoms(); ++aIx){
    SimTK::MobilizedBodyIndex mbx = mutc1.getAtomMobilizedBodyIndex(aIx);
    SimTK::MobilizedBody& mobod = matter.updMobilizedBody(mbx);
      SimTK::DuMM::AtomIndex dAIx = mutc1.getDuMMAtomIndex(aIx);
      SimTK::Transform mutc1B_X_atom = mutc1.getFrameInMobilizedBodyFrame(aIx);
      Caller->forceField->bsetAtomStationOnBody( dAIx, locs[int(aIx)] );
      Caller->forceField->updIncludedAtomStation(dAIx) = (locs[int(aIx)]);
      Caller->forceField->bsetAtomPlacementStation(dAIx, mbx, locs[int(aIx)] );
  }

  // Set X_PF, Q - Bottleneck!
  i = 0;
  for (SimTK::MobilizedBodyIndex mbx(1); mbx < matter.getNumBodies(); ++mbx){
    SimTK::MobilizedBody& mobod = matter.updMobilizedBody(mbx);
    if(int(mbx) == 1){ // This is dangerous TODO
      ((SimTK::MobilizedBody::Free&)mobod).setDefaultInboardFrame(P_X_M[int(mbx)]);
    }else{
      ((SimTK::MobilizedBody::Pin&)mobod).setDefaultInboardFrame(P_X_M[int(mbx)] * M_X_pin);
      ((SimTK::MobilizedBody::Pin&)mobod).setDefaultQ(inboardBondDihedralAngles[int(mbx)]);
    }
    i++;
  }
  Caller->system->realizeTopology();
 
  SimTK::State& astate = Caller->system->updDefaultState();
  
  system.realize(astate, SimTK::Stage::Position);
  #ifdef DEBUG_WRITEALLPDBS
    writePdb(mutc1, astate, "pdbs", "sb_", 8, "PFs", advanced.getTime());
  #endif

  // Store initial configuration in TVector - different from assignConfAndTVectorFromShm1
  i = 0;
  for (SimTK::MobilizedBodyIndex mbx(1); mbx < matter.getNumBodies(); ++mbx){
    const SimTK::MobilizedBody& mobod = matter.getMobilizedBody(mbx);
    TVector[i] = mobod.getMobilizerTransform(astate);
    i++;
  }
  
  return astate;
}

// * Transfer coordinates from shm to compound 3 * /
// * Order: shm[indexMap] -> atomTargets; use matchDefault AND Assembler * /
SimTK::State& MidVVIntegratorRep::assignConfAndTVectorFromShm3(SimTK::State& advanced)
{
  // Declare vars
  int i;
  const SimTK::System& system   = getSystem();
  SimTK::SimbodyMatterSubsystem& matter = Caller->system->updMatterSubsystem();
  SimTK::Compound& mutc1 = compoundSystem->updCompound(SimTK::CompoundSystem::CompoundIndex(0));

  // Get atomTargets
  std::map<SimTK::Compound::AtomIndex, SimTK::Vec3>           atomTargets;

  int ix = 0;
  for (SimTK::Compound::AtomIndex aIx(0); aIx < mutc1.getNumAtoms(); ++aIx){
    SimTK::Vec3 v(coords[ Caller->_indexMap[ix][1] ][0],
           coords[ Caller->_indexMap[ix][1] ][1],
           coords[ Caller->_indexMap[ix][1] ][2]);  
    atomTargets.insert(pair<SimTK::Compound::AtomIndex, SimTK::Vec3>(((Caller->lig1->bAtomList)[ix]).atomIndex, v));
    ix++;
  }

  // Match
  mutc1.matchDefaultTopLevelTransform(atomTargets);
  mutc1.matchDefaultConfiguration(atomTargets, SimTK::Compound::Match_Exact, true, 150.0); //Compound::Match_Idealized

  // Get transforms: P_X_Ms(=T_X_Bs -> X_PFs), T_X_Mrs
  SimTK::Transform G_X_T = mutc1.getTopLevelTransform();
  SimTK::Transform M_X_pin = SimTK::Rotation(-90*SimTK::Deg2Rad, SimTK::YAxis); // Moves rotation from X to Z

  SimTK::Transform P_X_M[matter.getNumBodies()]; // related to X_PFs
  SimTK::Transform T_X_Mr[matter.getNumBodies()]; // related to CompoundAtom.frameInMobilizedBodyFrame s
  SimTK::Angle inboardBondDihedralAngles[matter.getNumBodies()]; // related to X_FMs
  SimTK::Vec3 locs[mutc1.getNumAtoms()];
  P_X_M[1] = G_X_T * mutc1.calcDefaultAtomFrameInCompoundFrame(SimTK::Compound::AtomIndex(0));
  T_X_Mr[1] = mutc1.calcDefaultAtomFrameInCompoundFrame(SimTK::Compound::AtomIndex(0));
  for (SimTK::Compound::AtomIndex aIx(1); aIx < mutc1.getNumAtoms(); ++aIx){
    if(mutc1.getAtomLocationInMobilizedBodyFrame(aIx) == 0){
      SimTK::MobilizedBodyIndex mbx = mutc1.getAtomMobilizedBodyIndex(aIx);
      const SimTK::MobilizedBody& mobod = matter.getMobilizedBody(mbx);

      // Get parents
      const SimTK::MobilizedBody& parentMobod =  mobod.getParentMobilizedBody();
      SimTK::MobilizedBodyIndex parentMbx = parentMobod.getMobilizedBodyIndex();
      SimTK::Compound::AtomIndex parentAIx = mbx2aIx.at(parentMbx);

      inboardBondDihedralAngles[int(mbx)] = mutc1.bgetDefaultInboardDihedralAngle(aIx);

      // Reset child frame to zero dihedral angle
      SimTK::Transform Mr_X_M0 = SimTK::Rotation(inboardBondDihedralAngles[int(mbx)], SimTK::XAxis);
      T_X_Mr[int(mbx)] = mutc1.calcDefaultAtomFrameInCompoundFrame(aIx);
      SimTK::Transform T_X_M0 = T_X_Mr[int(mbx)] * Mr_X_M0;

      // Express atom frame relative to parent atom
      const SimTK::Transform& T_X_Fr = mutc1.calcDefaultAtomFrameInCompoundFrame(parentAIx);
      SimTK::Transform Fr_X_T = ~T_X_Fr;
      SimTK::Transform Fr_X_M0 = Fr_X_T * T_X_M0;
      P_X_M[int(mbx)] = Fr_X_M0;

    }
  }

  // Set frameInMobilizedModyFrame s in Compound
  for (SimTK::Compound::AtomIndex aIx(1); aIx < mutc1.getNumAtoms(); ++aIx){
    SimTK::MobilizedBodyIndex mbx = mutc1.getAtomMobilizedBodyIndex(aIx);
    if(mutc1.getAtomLocationInMobilizedBodyFrame(aIx) != 0){
      SimTK::Transform B_X_T = ~(T_X_Mr[int(mbx)]);
      SimTK::Transform T_X_atom =  mutc1.calcDefaultAtomFrameInCompoundFrame(aIx);
      SimTK::Transform B_X_atom = B_X_T * T_X_atom;
      mutc1.bsetFrameInMobilizedBodyFrame(aIx, B_X_atom);
      locs[int(aIx)] = B_X_atom.p();
    }
    else{
      locs[int(aIx)] = SimTK::Vec3(0);
    }
  }

  // Set stations and AtomPLacements for leftover atoms in DuMM
  for (SimTK::Compound::AtomIndex aIx(1); aIx < mutc1.getNumAtoms(); ++aIx){
    SimTK::MobilizedBodyIndex mbx = mutc1.getAtomMobilizedBodyIndex(aIx);
    SimTK::MobilizedBody& mobod = matter.updMobilizedBody(mbx);
      SimTK::DuMM::AtomIndex dAIx = mutc1.getDuMMAtomIndex(aIx);
      SimTK::Transform mutc1B_X_atom = mutc1.getFrameInMobilizedBodyFrame(aIx);
      Caller->forceField->bsetAtomStationOnBody( dAIx, locs[int(aIx)] );
      Caller->forceField->updIncludedAtomStation(dAIx) = (locs[int(aIx)]);
      Caller->forceField->bsetAtomPlacementStation(dAIx, mbx, locs[int(aIx)] );
  }
  
  // Set matter Q's
  i = 0;
  for (SimTK::MobilizedBodyIndex mbx(1); mbx < matter.getNumBodies(); ++mbx){
    SimTK::MobilizedBody& mobod = matter.updMobilizedBody(mbx);
    if(mobod.getNumQ(Caller->system->getDefaultState()) == 7){
      ((SimTK::MobilizedBody::Free&)mobod).setDefaultInboardFrame(P_X_M[int(mbx)]);
      Caller->system->realizeTopology();
    }else{
      ((SimTK::MobilizedBody::Pin&)mobod).setDefaultInboardFrame(P_X_M[int(mbx)] * M_X_pin);
      ((SimTK::MobilizedBody::Pin&)mobod).setDefaultQ(inboardBondDihedralAngles[int(mbx)]);
      Caller->system->realizeTopology();
    }
    i++;
  }

  SimTK::State& astate = Caller->system->updDefaultState();
  system.realize(astate, SimTK::Stage::Position);
  #ifdef DEBUG_WRITEPDBS
    writePdb(mutc1, astate, "pdbs", "sb_", 8, "PFs", advanced.getTime());
  #endif

  // Store initial configuration in TVector - different from assignConfAndTVectorFromShm1
  i = 0;
  for (SimTK::MobilizedBodyIndex mbx(1); mbx < matter.getNumBodies(); ++mbx){
    const SimTK::MobilizedBody& mobod = matter.getMobilizedBody(mbx);
    TVector[i] = mobod.getMobilizerTransform(astate);
    i++;
  }
  
  return astate;
}


// Write positions in MMTK configuration pointer for PE calculations
void MidVVIntegratorRep::setMMTKConf(const SimTK::Compound& c, SimTK::State& state)
{
  int tx, tshm;
  vector3 *x;
  x = (vector3 *)Caller->configuration->data;
  for (int a=0; a<(c.getNumAtoms()); a++){
    tx = Caller->_indexMap[ a ][2];
    tshm = ((Caller->_indexMap[ a ][1]) * 3) + 2;
    x[tx][0] = c.calcAtomLocationInGroundFrame(state, SimTK::Compound::AtomIndex(Caller->_indexMap[ a ][1]))[0];
    x[tx][1] = c.calcAtomLocationInGroundFrame(state, SimTK::Compound::AtomIndex(Caller->_indexMap[ a ][1]))[1];
    x[tx][2] = c.calcAtomLocationInGroundFrame(state, SimTK::Compound::AtomIndex(Caller->_indexMap[ a ][1]))[2];
  }
}


// * Trouble controls * //
int MidVVIntegratorRep::isTrouble(void)
{
  return this->trouble;
}

void MidVVIntegratorRep::setTrouble(void)
{
  this->trouble = 1;
}

void MidVVIntegratorRep::resetTrouble(void)
{
  this->trouble = 0;
}

// * Energy controls * //
void MidVVIntegratorRep::setTb(TARGET_TYPE val)
{
  this->Tb = val;
  shm[arrays_cut + 2] = val;
  this->kTb = SimTK_BOLTZMANN_CONSTANT_MD * val;
  this->sqrtkTb = std::sqrt(kTb);
  this->invsqrtkTb = 1 / sqrtkTb;

}

TARGET_TYPE MidVVIntegratorRep::getTb(void)
{
  return this->Tb;
}

void MidVVIntegratorRep::setKe(TARGET_TYPE val)
{
  this->ke = val;
  shm[arrays_cut + 8] = val;
}

TARGET_TYPE MidVVIntegratorRep::getKe(void)
{
  return this->ke;
}

void MidVVIntegratorRep::setPe(TARGET_TYPE val)
{
  this->pe = val;
  shm[arrays_cut + 5] = val;
}

TARGET_TYPE MidVVIntegratorRep::getPe(void)
{
  return this->pe;
}

// * Time controls * //
void MidVVIntegratorRep::setTimeToReach(TARGET_TYPE val)
{
  this->timeToReach = val;
}

TARGET_TYPE MidVVIntegratorRep::getTimeToReach(void)
{
  return this->timeToReach;
}

// * Step controls * //
void MidVVIntegratorRep::resetStep(void)
{
  this->step = 0;
  shm[arrays_cut] = 0;
}

unsigned long int MidVVIntegratorRep::getStep(void)
{
  return this->step;
}

void MidVVIntegratorRep::incrStep(void)
{
  this->step += 1;
  shm[arrays_cut] += 1;
}

void MidVVIntegratorRep::incrStep(unsigned int amount)
{
  this->step += amount;
  shm[arrays_cut] += amount;
}

// * Total # of steps in Call controls * //
void MidVVIntegratorRep::resetTotStepsInCall(void)
{
  this->totStepsInCall = 0;
}

unsigned long int MidVVIntegratorRep::getTotStepsInCall(void)
{
  return this->totStepsInCall;
}

void MidVVIntegratorRep::incrTotStepsInCall(void)
{
  this->totStepsInCall += 1;
}

void MidVVIntegratorRep::incrTotStepsInCall(unsigned int amount)
{
  this->totStepsInCall += amount;
}

// * Trial controls * //
void MidVVIntegratorRep::setTrial(long int val)
{
  this->trial = val;
  shm[arrays_cut + 10] = val;
}

long int MidVVIntegratorRep::getTrial(void)
{
  return this->trial;
}

void MidVVIntegratorRep::incrTrial(void)
{
  this->trial += 1;
  shm[arrays_cut + 10] += 1.0;
}

// * No steps controls * //
void MidVVIntegratorRep::setNoSteps(unsigned long int val)
{
  this->noSteps = val;
  shm[arrays_cut + 1] = val;
}

unsigned long int MidVVIntegratorRep::getNoSteps(void)
{
  return this->noSteps;
}

// * Trials controls * //
void MidVVIntegratorRep::setStepsPerTrial(unsigned long int val)
{
  this->stepsPerTrial = val;
  shm[arrays_cut + 9] = val;
}

unsigned long int MidVVIntegratorRep::getStepsPerTrial(void)
{
  return this->stepsPerTrial;
}

void MidVVIntegratorRep::setNtrials(unsigned long int val)
{
  this->ntrials = val;
}

unsigned long int MidVVIntegratorRep::getNtrials(void)
{
  return this->ntrials;
}

void MidVVIntegratorRep::metropolis(const SimTK::Compound& compound, SimTK::State& advanced)
{
      raiseMetroFlag();

      const SimTK::System& system   = getSystem();
      const SimTK::SimbodyMatterSubsystem& matter = Caller->system->getMatterSubsystem();
      int nu = advanced.getNU();

      TARGET_TYPE rand_no = boostRealRand(rng);

      TARGET_TYPE ke_n = Caller->system->calcKineticEnergy(advanced); // from Sim
      setMMTKConf(compound, advanced);
      TARGET_TYPE pe_n = calcPEFromMMTK(); // from MMTK
      //TARGET_TYPE pe_n = Caller->system->calcPotentialEnergy(advanced); // from Sim

      system.realize(advanced, SimTK::Stage::Position);

      // Fixman potential version
      //this->UFixj = calcUFix(c, advanced); // Fixman potential
      //TARGET_TYPE en = ke_n + pe_n + UFixj;
      //TARGET_TYPE eo = getKe() + getPe() + UFixi;

      // Without Fixman potential
      TARGET_TYPE en = ke_n + pe_n;
      TARGET_TYPE eo = getKe() + getPe();
      TARGET_TYPE RT = getTb() * SimTK_BOLTZMANN_CONSTANT_MD;
      
      Eigen::MatrixXd EiM(nu, nu);
      SimTK::Matrix M;
      matter.calcM(advanced, M);
      for(int i=0; i<nu; i++){ // Put MInv in Eigen
        for(int j=0; j<nu; j++){
          EiM(i, j) = M(i, j);
        }
      }

      // Fixman determinant version
      //Eigen::SelfAdjointEigenSolver<Eigen::MatrixXd> Eims(EiM);
      //Eigen::MatrixXd Eim = Eims.operatorSqrt();
      //detsqrtMj = Eim.determinant();

      TARGET_TYPE expression = exp(-(en-eo)/RT);
  
      if(isnan(en)){
        shm[arrays_cut + 7] = 0.0;
      }else{ 
        if ((en<eo) or (rand_no<expression)){
          shm[arrays_cut + 7] = 1.0;
        }else{
          shm[arrays_cut + 7] = 0.0;
        }
      }
      if(shm[arrays_cut + 7] < 0.5){ // Move not accepted - assign old conf TVector
        assignConfFromTVector(advanced);
        advanced.updU() = 0.0;
      }
      else{                          // Move accepted - set new TVector
        setTVector(advanced);
        advanced.updU() = 0.0; // to advance Stage
        writePossToShm(compound, advanced); // OPTIM
        setKe(ke_n);
        setPe(pe_n);
        *this->Caller->sysAccs += 1.0;
      } // else // Move accepted
}

// * Print data * //
void MidVVIntegrator::printData(const SimTK::Compound& c, SimTK::State& advanced)
{
  ((MidVVIntegratorRep *)rep)->printData(c, advanced);
}

void MidVVIntegrator::printPoss(const SimTK::Compound& c, SimTK::State& advanced)
{
  ((MidVVIntegratorRep *)rep)->printPoss(c, advanced);
}

void MidVVIntegrator::printVels(const SimTK::Compound& c, SimTK::State& advanced)
{
  ((MidVVIntegratorRep *)rep)->printVels(c, advanced);
}

void MidVVIntegrator::printAccs(const SimTK::Compound& c, SimTK::State& advanced)
{
  ((MidVVIntegratorRep *)rep)->printAccs(c, advanced);
}

void MidVVIntegrator::printForces(const SimTK::Compound& c, SimTK::State& advanced)
{
  ((MidVVIntegratorRep *)rep)->printForces(c, advanced);
}

void MidVVIntegrator::printForcesNorms(const SimTK::Compound& c, SimTK::State& advanced)
{
  ((MidVVIntegratorRep *)rep)->printForcesNorms(c, advanced);
}

SimTK::Real MidVVIntegrator::calcDetMBAT(const SimTK::Compound& c, SimTK::State& advanced)
{
  return ((MidVVIntegratorRep *)rep)->calcDetMBAT(c, advanced);
}

SimTK::Real MidVVIntegrator::calcMAndDetM(const SimTK::Compound& c, SimTK::State& advanced)
{
  return ((MidVVIntegratorRep *)rep)->calcMAndDetM(c, advanced);
}

SimTK::Real MidVVIntegrator::calcUFix(const SimTK::Compound& c, SimTK::State& advanced)
{
  return ((MidVVIntegratorRep *)rep)->calcUFix(c, advanced);
}

SimTK::Real MidVVIntegrator::calcTotLinearVel(SimTK::State& advanced)
{
  return ((MidVVIntegratorRep *)rep)->calcTotLinearVel(advanced);
}

SimTK::Real MidVVIntegrator::calcKEFromAtoms(const SimTK::Compound& c, SimTK::State& advanced)
{
  return ((MidVVIntegratorRep *)rep)->calcKEFromAtoms(c, advanced);
}

SimTK::Real MidVVIntegrator::calcPEFromMMTK(void)
{
  return ((MidVVIntegratorRep *)rep)->calcPEFromMMTK();
}

// * Initialize velocities from random generalized vels
void MidVVIntegrator::initializeVelsFromRandU(const SimTK::Compound& c, SimTK::State& advanced, SimTK::Real TbTemp)
{
  ((MidVVIntegratorRep *)rep)->initializeVelsFromRandU(c, advanced, TbTemp);
}

// * Initialize velocities according to marginal probability
//(old Velocity Rescaling Thermostat)
void MidVVIntegrator::initializeVelsFromVRT(const SimTK::Compound& c, SimTK::State& advanced, SimTK::Real TbTemp)
{
  ((MidVVIntegratorRep *)rep)->initializeVelsFromVRT(c, advanced, TbTemp);
}

// * Initialize velocities from atoms inertias
void MidVVIntegrator::initializeVelsFromAtoms(const SimTK::Compound& c, SimTK::State& advanced, SimTK::Real TbTemp)
{
  ((MidVVIntegratorRep *)rep)->initializeVelsFromAtoms(c, advanced, TbTemp);
}

// * Initialize velocities from mobods inertias
void MidVVIntegrator::initializeVelsFromMobods(const SimTK::Compound& c, SimTK::State& advanced, SimTK::Real TbTemp)
{
  ((MidVVIntegratorRep *)rep)->initializeVelsFromMobods(c, advanced, TbTemp);
}

// * Initialize velocities from mobods inertias
void MidVVIntegrator::initializeVelsFromJacobian(const SimTK::Compound& c, SimTK::State& advanced, SimTK::Real TbTemp)
{
  ((MidVVIntegratorRep *)rep)->initializeVelsFromJacobian(c, advanced, TbTemp);
}

// * Initialize velocities from const vector of spatial vels
void MidVVIntegrator::initializeVelsFromConst(const SimTK::Compound& c, SimTK::State& advanced, SimTK::SpatialVec *specVels)
{
  ((MidVVIntegratorRep *)rep)->initializeVelsFromConst(c, advanced, specVels);
}

// * Write data into shm for exchange with other funcs * //
void MidVVIntegrator::writePossVelsToShm(const SimTK::Compound& c, SimTK::State& advanced)
{
  ((MidVVIntegratorRep *)rep)->writePossVelsToShm(c, advanced);
}

// * Write data into shm for exchange with other funcs * //
void MidVVIntegrator::writePossToShm(const SimTK::Compound& c, SimTK::State& advanced)
{
  ((MidVVIntegratorRep *)rep)->writePossToShm(c, advanced);
}

// * Set TVector of transforms from mobods * //
void MidVVIntegrator::setTVector(SimTK::State& advanced)
{
  ((MidVVIntegratorRep *)rep)->setTVector(advanced);
}

// * Transfer coordinates from TVector * //
void MidVVIntegrator::assignConfFromTVector(SimTK::State& advanced)
{
  ((MidVVIntegratorRep *)rep)->assignConfFromTVector(advanced);
}

// * Transfer coordinates  0 * //
void MidVVIntegrator::assignConfAndTVectorFromShm0(SimTK::State& advanced)
{
  ((MidVVIntegratorRep *)rep)->assignConfAndTVectorFromShm0(advanced);
}

// * Transfer coordinates 1 * //
void MidVVIntegrator::assignConfAndTVectorFromShm1(SimTK::State& advanced)
{
  ((MidVVIntegratorRep *)rep)->assignConfAndTVectorFromShm1(advanced);
}

// * Transfer coordinates 2 * //
void MidVVIntegrator::assignConfAndTVectorFromShm2(SimTK::State& advanced)
{
  ((MidVVIntegratorRep *)rep)->assignConfAndTVectorFromShm2(advanced);
}

// * Transfer coordinates 3 * //
SimTK::State& MidVVIntegrator::assignConfAndTVectorFromShm3Opt(SimTK::State& advanced)
{
  return ((MidVVIntegratorRep *)rep)->assignConfAndTVectorFromShm3Opt(advanced);
}
SimTK::State& MidVVIntegrator::assignConfAndTVectorFromShm3(SimTK::State& advanced)
{
  return ((MidVVIntegratorRep *)rep)->assignConfAndTVectorFromShm3(advanced);
}

void MidVVIntegrator::setMMTKConf(const SimTK::Compound& c, SimTK::State& state)
{
  ((MidVVIntegratorRep *)rep)->setMMTKConf(c, state);
}


// * Trouble controls * //
int MidVVIntegrator::isTrouble(void)
{
  return ((MidVVIntegratorRep *)rep)->isTrouble();
}

void MidVVIntegrator::setTrouble(void)
{
  ((MidVVIntegratorRep *)rep)->setTrouble();
}

void MidVVIntegrator::resetTrouble(void)
{
  ((MidVVIntegratorRep *)rep)->resetTrouble();
}

// * Energy controls * //
void MidVVIntegrator::setTb(TARGET_TYPE val)
{
  ((MidVVIntegratorRep *)rep)->setTb(val);
}

TARGET_TYPE MidVVIntegrator::getTb(void)
{
  return ((MidVVIntegratorRep *)rep)->getTb();
}

void MidVVIntegrator::setKe(TARGET_TYPE val)
{
  ((MidVVIntegratorRep *)rep)->setKe(val);
}

TARGET_TYPE MidVVIntegrator::getKe(void)
{
  return ((MidVVIntegratorRep *)rep)->getKe();
}

void MidVVIntegrator::setPe(TARGET_TYPE val)
{
  ((MidVVIntegratorRep *)rep)->setPe(val);
}

TARGET_TYPE MidVVIntegrator::getPe(void)
{
  return ((MidVVIntegratorRep *)rep)->getPe();
}

// * Time controls * //
void MidVVIntegrator::setTimeToReach(TARGET_TYPE val)
{
  ((MidVVIntegratorRep *)rep)->setTimeToReach(val);
}

TARGET_TYPE MidVVIntegrator::getTimeToReach(void)
{
  return ((MidVVIntegratorRep *)rep)->getTimeToReach();
}

// * Step controls * //
void MidVVIntegrator::resetStep(void)
{
  ((MidVVIntegratorRep *)rep)->resetStep();
}

unsigned long int MidVVIntegrator::getStep(void)
{
  return ((MidVVIntegratorRep *)rep)->getStep();
}

void MidVVIntegrator::incrStep(void)
{
  ((MidVVIntegratorRep *)rep)->incrStep();
}

void MidVVIntegrator::incrStep(unsigned int amount)
{
  ((MidVVIntegratorRep *)rep)->incrStep(amount);
}

// * Total # of steps in Call controls * //
void MidVVIntegrator::resetTotStepsInCall(void)
{
  ((MidVVIntegratorRep *)rep)->resetTotStepsInCall();
}

unsigned long int MidVVIntegrator::getTotStepsInCall(void)
{
  return ((MidVVIntegratorRep *)rep)->getTotStepsInCall();
}

void MidVVIntegrator::incrTotStepsInCall(void)
{
  ((MidVVIntegratorRep *)rep)->incrTotStepsInCall();
}

void MidVVIntegrator::incrTotStepsInCall(unsigned int amount)
{
  ((MidVVIntegratorRep *)rep)->incrTotStepsInCall(amount);
}

// * Trial controls * //
void MidVVIntegrator::setTrial(long int val)
{
  ((MidVVIntegratorRep *)rep)->setTrial(val);
}

long int MidVVIntegrator::getTrial(void)
{
  return ((MidVVIntegratorRep *)rep)->getTrial();
}

void MidVVIntegrator::incrTrial(void)
{
  ((MidVVIntegratorRep *)rep)->incrTrial();
}

// * No steps controls * //
void MidVVIntegrator::setNoSteps(unsigned long int val)
{
  ((MidVVIntegratorRep *)rep)->setNoSteps(val);
}

unsigned long int MidVVIntegrator::getNoSteps(void)
{
  return ((MidVVIntegratorRep *)rep)->getNoSteps();
}

// * Trials controls * //
void MidVVIntegrator::setStepsPerTrial(unsigned long int val)
{
  ((MidVVIntegratorRep *)rep)->setStepsPerTrial(val);
}

unsigned long int MidVVIntegrator::getStepsPerTrial(void)
{
  return ((MidVVIntegratorRep *)rep)->getStepsPerTrial();
}

void MidVVIntegrator::setNtrials(unsigned long int val)
{
  ((MidVVIntegratorRep *)rep)->setNtrials(val);
}

unsigned long int MidVVIntegrator::getNtrials(void)
{
  return ((MidVVIntegratorRep *)rep)->getNtrials();
}

void MidVVIntegrator::metropolis(const SimTK::Compound& compound, SimTK::State& astate) // Metropolis function
{
  ((MidVVIntegratorRep *)rep)->metropolis(compound, astate);
}

//==============================================================================
//==============================================================================
//                            ATTEMPT DAE STEP
//==============================================================================
// Note that Verlet overrides the entire DAE step because it can't use the
// default ODE-then-DAE structure. Instead the constraint projections are
// interwoven here.
bool MidVVIntegratorRep::attemptDAEStep
   (SimTK::Real t1, SimTK::Vector& yErrEst, int& errOrder, int& numIterations)
{
    #ifdef DEBUG_TIME
    //boost::timer boost_timer;
    #endif

    int tx, tshm; // x -> shm index correspondence
    vector3 *xMid;

    // ******************** //
    // * Main integration * //
    // ******************** //
    const SimTK::System& system   = getSystem();
    SimTK::State& advanced = updAdvancedState();
    starttime = advanced.getTime();

    SimTK::Vector dummyErrEst; // for when we don't want the error estimate projected
    statsStepsAttempted++;

    const SimTK::Real    t0        = getPreviousTime();       // nicer names
    const SimTK::Vector& q0        = getPreviousQ();
    const SimTK::Vector& u0        = getPreviousU();
    const SimTK::Vector& z0        = getPreviousZ();
    const SimTK::Vector& qdot0     = getPreviousQDot();
    const SimTK::Vector& udot0     = getPreviousUDot();
    const SimTK::Vector& zdot0     = getPreviousZDot();
    const SimTK::Vector& qdotdot0  = getPreviousQDotDot();

    const SimTK::Real h = t1-t0;
    const int nq = advanced.getNQ();
    const int nu = advanced.getNU();
    const int nz = advanced.getNZ();
  
    bool converged;
    // We will catch any exceptions thrown by realize() or project() and simply
    // treat that as a failure to take a step due to the step size being too 
    // big. The idea is that the caller should reduce the step size and try 
    // again, giving up only when the step size goes below the allowed minimum.

  try
  {
    int i;
    step = totStepsInCall % stepsPerTrial;

    // * CHECKPOINT Increment trial * //
    if((step == 0) && (*step0Flag == 0)){
      if(totStepsInCall == 0){ // Begining of Call (trial == 0)
        if(*beginFlag == 0){ 
          raiseBeginFlag();
          advanced = assignConfAndTVectorFromShm3Opt(advanced);
          advanced.setTime(starttime);
          setMMTKConf(c, advanced);
        }
      }
      raiseStep0Flag();
      incrTrial();
      setTb(shm[arrays_cut + 2]);
      initializeVelsFromVRT(c, advanced, getTb());
      this->UFixi = calcUFix(c, advanced);
      // Set initial energies after initialize vels
      setKe(Caller->system->calcKineticEnergy(advanced));
      setPe(calcPEFromMMTK());
      system.realize(advanced, SimTK::Stage::Acceleration);
      #ifdef DEBUG_WRITEALLPDBS
        writePdb(c, advanced, "pdbs", "sb_i", 10, "");
        //if(( (int(advanced.getTime() * 10000) % 150000) < 150)){writePdb(c, advanced, "pdbs", "sb_i", 10, "");} // every 1000 step
      #endif
    } else{ // BEGIN DAE

      numIterations = 0;
  
      SimTK::VectorView qErrEst  = yErrEst(    0, nq);       // all 3rd order estimates
      SimTK::VectorView uErrEst  = yErrEst(   nq, nu);
      SimTK::VectorView zErrEst  = yErrEst(nq+nu, nz);
      SimTK::VectorView uzErrEst = yErrEst(   nq, nu+nz);    // all 2nd order estimates
      
      // Calculate the new positions q (3rd order) and initial (1st order) 
      // estimate for the velocities u and auxiliary variables z.
      
      // These are final values (the q's will get projected, though).
      advanced.updTime() = t1;
      advanced.updQ()    = q0 + h*qdot0 + (h*h/2)*qdotdot0;
  
      // Now make an initial estimate of first-order variable u and z.
      const SimTK::Vector u1_est = u0 + h*udot0;
      const SimTK::Vector z1_est = z0 + h*zdot0;
  
      advanced.updU() = u1_est; // u's and z's will change in advanced below
      advanced.updZ() = z1_est;
  
      system.realize(advanced, SimTK::Stage::Time);
      system.prescribeQ(advanced);
      system.realize(advanced, SimTK::Stage::Position);
  
      // Consider position constraint projection. (See AbstractIntegratorRep
      // for how we decide not to project.)
      const SimTK::Real projectionLimit = 
          std::max(2*getConstraintToleranceInUse(), 
                      std::sqrt(getConstraintToleranceInUse()));
  
      bool anyChangesQ;
      if (!localProjectQAndQErrEstNoThrow(advanced, dummyErrEst, anyChangesQ, 
                                          projectionLimit))
          return false; // convergence failure for this step
  
      // q is now at its final integrated, prescribed, and projected value.
      // u and z still need refinement.
  
      system.prescribeU(advanced);
      system.realize(advanced, SimTK::Stage::Velocity);
  
      // No u projection yet.
  
      // Get new values for the derivatives.
      realizeStateDerivatives(advanced);
      
      // We're going to integrate the u's and z's with the 2nd order implicit
      // trapezoid rule: u(t+h) = u(t) + h*(f(u(t))+f(u(t+h)))/2. Unfortunately 
      // this is an implicit method so we have to iterate to refine u(t+h) until
      // this equation is acceptably satisfied. We're using functional iteration 
      // here which has a very limited radius of convergence.
      
      const SimTK::Real tol = std::min(1e-4, 0.1*getAccuracyInUse());
      SimTK::Vector usave(nu), zsave(nz); // temporaries
      //bool converged = false;
      converged = false;
      SimTK::Real prevChange = SimTK::Infinity; // use this to quit early
      for (int i = 0; !converged && i < 10; ++i) {
          ++numIterations;
          // At this point we know that the advanced state has been realized
          // through the Acceleration level, so its uDot and zDot reflect
          // the u and z state values it contains.
          usave = advanced.getU();
          zsave = advanced.getZ();
  
          // Get these references now -- as soon as we change u or z they
          // will be invalid.
          const SimTK::Vector& udot1 = advanced.getUDot();
          const SimTK::Vector& zdot1 = advanced.getZDot();
          
          // Refine u and z estimates.
          //cout<<"MidVVIntegratorRep: updU:"<<endl;
          advanced.setU(u0 + (h/2)*(udot0 + udot1));
          advanced.setZ(z0 + (h/2)*(zdot0 + zdot1));
  
          // Fix prescribed u's which may have been changed here.
          system.prescribeU(advanced);
          system.realize(advanced, SimTK::Stage::Velocity);
  
          // No projection yet.
  
          // Calculate fresh derivatives UDot and ZDot.
          //cout<<"MidVVIntegratorRep: realizeStateDerivatives:"<<endl;
          shm[arrays_cut + 6] = i + 2.0; // DAE done
          realizeStateDerivatives(advanced);
  
          // Calculate convergence as the ratio of the norm of the last delta to
          // the norm of the values prior to the last change. We're using the 
          // 2-norm but this ratio would be the same if we used the RMS norm. 
          // TinyReal is there to keep us out of trouble if we started at zero.
          
          const SimTK::Real convergenceU = (advanced.getU()-usave).norm()
                                    / (usave.norm()+SimTK::TinyReal);
          const SimTK::Real convergenceZ = (advanced.getZ()-zsave).norm()
                                    / (zsave.norm()+SimTK::TinyReal);
          const SimTK::Real change = std::max(convergenceU,convergenceZ);
          converged = (change <= tol);
          if (i > 1 && (change > prevChange))
              break; // we're headed the wrong way after two iterations -- give up
          prevChange = change;
      }
  
      // Now that we have achieved 2nd order estimates of u and z, we can use 
      // them to calculate a 3rd order error estimate for q and 2nd order error 
      // estimates for u and z. Note that we have already realized the state with
      // the new values, so QDot reflects the new u's.
  
      qErrEst = q0 + (h/2)*(qdot0+advanced.getQDot()) // implicit trapezoid rule integral
                - advanced.getQ();                    // Verlet integral
  
      uErrEst = u1_est                    // explicit Euler integral
                - advanced.getU();        // implicit trapezoid rule integral
      zErrEst = z1_est - advanced.getZ(); // ditto for z's
  
      // TODO: because we're only projecting velocities here, we aren't going to 
      // get our position errors reduced here, which is a shame. Should be able 
      // to do this even though we had to project q's earlier, because the 
      // projection matrix should still be around.
  
      bool anyChangesU;
      if (!localProjectUAndUErrEstNoThrow(advanced, yErrEst, anyChangesU,
                                          projectionLimit))
          return false; // convergence failure for this step
  
      // Two different integrators were used to estimate errors: trapezoidal for 
      // Q, and explicit Euler for U and Z.  This means that the U and Z errors
      // are of a different order than the Q errors.  We therefore multiply them 
      // by h so everything will be of the same order.
      
      // TODO: (sherm) I don't think this is valid. Although it does fix the
      // order, it also changes the absolute errors (typically by reducing
      // them since h is probably < 1) which will affect whether the caller
      // decides to accept the step. Instead, a different error order should
      // be used when one of these is driving the step size.
  
      uErrEst *= h; zErrEst *= h; // everything is 3rd order in h now
      errOrder = 3;
  
      //errOrder = qErrRMS > uzErrRMS ? 3 : 2;

      // * CHECKPOINT Increment step & total # of steps * //
      int steps_done = int(round( (advanced.getTime() - starttime) / (shm[arrays_cut + 3]) ));
      if(steps_done){
        #ifdef DEBUG_WRITEALLPDBS
          writePdb(c, advanced, "pdbs", "sb_all", 10, "");
        #endif
        incrStep();
        incrTotStepsInCall();

        *beginFlag = *step0Flag = 0;

        // * CHECKPOINT Metropolize * //
        if(step == stepsPerTrial){ // End of MD cycle 
          metropolis(c, advanced);

          #ifdef DEBUG_WRITEPDBS
          //if(( (int(advanced.getTime() * 10000) % 1500) < 150)){writePdb(c, advanced, "pdbs", "sb_", 10, "");} // every 10 step
          if((advanced.getTime() < 100.0)){writePdb(c, advanced, "pdbs", "sb_", 10, "");}
          #endif

          if((trial > ntrials) || (trial <= 0)){
            fprintf(stderr, "MidVV: Return pointers index out of range: %d\n", trial - 1);
            exit(1);
          }
          this->Caller->sysRetPotEsPoi[trial - 1] = getPe(); // RetPotEsPoi[trial] = PE

          // * Set return configuration * //
          xMid = (vector3 *)(Caller->sysRetConfsPois[trial - 1]);
          for(int a=0; a<this->natms; a++){
            tx = Caller->_indexMap[ a ][2];
            tshm = ((Caller->_indexMap[ a ][1]) * 3) + 2;
            xMid[tx][0] = shm[ tshm +0];
            xMid[tx][1] = shm[ tshm +1];
            xMid[tx][2] = shm[ tshm +2];
          }
          
        }
      }
  
    } // else DAE

    #ifdef DEBUG_TIME
    //printf("Time %.3lf\n", boost_timer.elapsed());
    #endif

    return converged;
  }
  catch (std::exception ex) {
    return false;
  }
}
