#ifndef TauFinder_h
#define TauFinder_h 1

#include "marlin/Processor.h"
#include "lcio.h"
#include <string>
#include "TTree.h"
#include "TNtuple.h"
#include "TH1D.h"
#include "TFile.h"
#include "TMath.h"
#include "TSystem.h"
#include <EVENT/MCParticle.h>
#include <EVENT/ReconstructedParticle.h>
#include <IMPL/ReconstructedParticleImpl.h>

using namespace lcio ;
using namespace marlin ;

#define NTAU_MAX 15000

/** TauFinder processor for marlin.
 * 
 * @author A. Muennich, CERN
 * 
 */

class TauFinder : public Processor {
  
 public:
  
  virtual Processor*  newProcessor() { return new TauFinder ; }
  
  
  TauFinder() ;
  TauFinder(const TauFinder&) = delete;
  TauFinder& operator=(const TauFinder&) = delete;

  // Called at the begin of the job before anything is read
  virtual void init() ;
  
  // Called for every run
  virtual void processRunHeader( LCRunHeader* run ) ;
  
  // Called for every event - the working horse
  virtual void processEvent( LCEvent * evt ) ; 
  
  
  virtual void check( LCEvent * evt ) ; 
  
  
  // Called after data processing for clean up
  virtual void end() ;  
  
 protected:

  // Input collection name
  std::string _inCol{};

  // Output relation collection name
  std::string _tauPFOLinkCol{};

  // Output reco tau collection names
  std::string _outCol{}, _outColNChargedTrks{}, _outColInvMass{}, _outColMerge{}, _outColNParticles{}, _outColIsoEnergy{};

  // Output file name
  std::string _outputFile{};
  
  // TauFinder selection cuts
  float _ptCut = 0.0, _ptSeed = 0.0, _coneAngle = 0.0, _isoAngle = 0.0, _isoE = 0.0, _invMass = 0.0;

  int _nRun = -1;
  int _nEvt = -1;

  TFile *rootfile = NULL;
  TTree *anatree = NULL;
  
  // Tau reconstruction
  bool findTau(std::vector<ReconstructedParticle*> &charged_vec,std::vector<ReconstructedParticle*> &neutral_vec,
	       std::vector<std::vector<ReconstructedParticle*> > &tau_vec);

 private:

  int _ntau{};
  float _tau_isoE[NTAU_MAX]{};
  float _tau_invMass[NTAU_MAX]{};
  float _tau_pt[NTAU_MAX]{};
  float _tau_p[NTAU_MAX]{};
  float _tau_energy[NTAU_MAX]{};
  float _tau_phi[NTAU_MAX]{};
  float _tau_eta[NTAU_MAX]{};
  int _event_num[NTAU_MAX]{};
  int _run_num[NTAU_MAX]{};
  
  // Energy sort 
  static bool energySort(ReconstructedParticle *p1, ReconstructedParticle *p2);

  // Get number of charged particles
  static int getNCharged(ReconstructedParticleImpl *p);

  // Get number of neutral particles
  static int getNNeutral(ReconstructedParticleImpl *p);
  
  // Compute pseudorapidity (eta)
  static double computeEta(const double mom[3]);

  // Compute azimuthal angle (phi)
  static double computePhi(const double mom[3]);

  // Compute delta R
  static double computeDeltaR(const double mom1[3], const double mom2[3]);

};
  
#endif



