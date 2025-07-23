#include "TauFinder.h"
#include <iostream>
#include <iomanip>

using namespace std;


#ifdef MARLIN_USE_AIDA
#include <marlin/AIDAProcessor.h>
#include <AIDA/IHistogramFactory.h>
#include <AIDA/ICloud1D.h>
//#include <AIDA/IHistogram1D.h>
#endif

#include <EVENT/LCCollection.h>
#include <IMPL/LCCollectionVec.h>
#include <EVENT/LCRelation.h>
#include <IMPL/LCRelationImpl.h>
#include <EVENT/ReconstructedParticle.h>
#include <IMPL/ReconstructedParticleImpl.h>
#include "UTIL/LCRelationNavigator.h"
#include <EVENT/Vertex.h>

#include <marlin/Global.h>

#include "HelixClass.h"
#include "SimpleLine.h"

// ----- Include for verbosity dependent logging ---------
#include "marlin/VerbosityLevels.h"

using namespace lcio ;
using namespace marlin ;
using namespace UTIL;

TauFinder aTauFinder ;

TauFinder::TauFinder() : Processor("TauFinder") 
{
  _description = "TauFinder writes tau candidates as ReconstructedParticles into collection. It runs on a collection of ReconstructedParticels, if you want  to run on MCParticles you have to convert them before hand (use e.g. PrepareRECParticles processor)" ;
  
  // Register steering parameters: name, description, class-variable, default value
  registerInputCollection( LCIO::RECONSTRUCTEDPARTICLE,
			    "PFOCollection",
                            "Collection of PFOs",
                            _inCol ,
                            std::string("PandoraPFOs"));

  registerOutputCollection( LCIO::RECONSTRUCTEDPARTICLE,
			    "RecoTauCollection",
                            "Collection of Tau Candidates",
                            _outCol ,
                            std::string("RecoTaus"));
  
  registerOutputCollection( LCIO::RECONSTRUCTEDPARTICLE,
			    "RecoTauChargedTrackCollection",
                            "Collection of Tau Candidates which Failed Number of Charged Tracks Selection",
                            _outColNChargedTrks ,
                            std::string("RecoTaus_NChargedTrks"));

  registerOutputCollection( LCIO::RECONSTRUCTEDPARTICLE,
			    "RecoTauInvMassCollection",
                            "Collection of Tau Candidates which Failed Invariant Mass Selection",
                            _outColInvMass ,
                            std::string("RecoTaus_InvMass"));

  registerOutputCollection( LCIO::RECONSTRUCTEDPARTICLE,
			    "RecoTauMergeCollection",
                            "Collection of Tau Candidates which Failed Merge",
                            _outColMerge ,
                            std::string("RecoTaus_Merge"));

  registerOutputCollection( LCIO::RECONSTRUCTEDPARTICLE,
			    "RecoTauNParticlesCollection",
                            "Collection of Tau Candidates which Failed Number of Particles Selection",
                            _outColNParticles ,
                            std::string("RecoTaus_NParticles"));

  registerOutputCollection( LCIO::RECONSTRUCTEDPARTICLE,
			    "RecoTauIsoEnergyCollection",
                            "Collection of Tau Candidates which Failed Isolation Energy Selection",
                            _outColIsoEnergy ,
                            std::string("RecoTaus_IsoEnergy"));
  
  registerOutputCollection( LCIO::LCRELATION,
			    "TauPFOLinkCollectionName" , 
			    "Name of the Tau Link to PandoraPFO Collection"  ,
			    _tauPFOLinkCol ,
			    std::string("TauPFOLink") ) ;

  registerProcessorParameter( "fileName" ,
			      "Name of the output file" ,
			      _outputFile ,
			      std::string("taufinder.root") ) ;
  
  registerProcessorParameter( "ptCut" ,
                              "Cut on pt to Suppress Background"  ,
                              _ptCut ,
                              (float)0.2) ;
  
  registerProcessorParameter( "searchConeAngle" ,
                              "Opening Angle of the Search Cone for Tau Jet in rad"  ,
                              _coneAngle ,
                              (float)0.05) ;

  registerProcessorParameter( "isolationConeAngle" ,
                              "Outer Isolation Cone Around Search cone of Tau jet in rad (Relative to Cone Angle)"  ,
                              _isoAngle ,
                              (float)0.02) ;
  
  registerProcessorParameter( "isolationEnergy" ,
                              "Energy Allowed Within Isolation Cone region"  ,
                              _isoE ,
                              (float)5.0) ;
  
  registerProcessorParameter( "ptSeed" ,
                              "Minimum Tranverse Momentum of Tau Seed"  ,
                              _ptSeed ,
                              (float)5.0) ;

  registerProcessorParameter( "invMass" ,
                              "Cut on Invariant Mass of Reconstructed Tau"  ,
                              _invMass ,
                              (float)2.0) ;
}


void TauFinder::init() 
{ 
  // Usually a good idea to
  printParameters();

  rootfile = new TFile((_outputFile).c_str (), "RECREATE");
  anatree = new TTree("anatree", "TTree with TauFinder parameters");

  if (anatree == 0)
    {
      throw lcio::Exception("Invalid tree pointer!!!");
    }

  anatree->Branch(("ntau"), &_ntau, ("ntau/I"));
  anatree->Branch(TString("t_isoE"), _tau_isoE, "t_isoE[ntau]/F");
  anatree->Branch(TString("t_invMass"), _tau_invMass, TString("t_invMass[ntau]/F"));
  anatree->Branch(TString("t_pt"), _tau_pt, TString("t_pt[ntau]/F"));
  anatree->Branch(TString("t_p"), _tau_p, TString("t_p[ntau]/F"));
  anatree->Branch(TString("t_energy"), _tau_energy, TString("t_energy[ntau]/F"));
  anatree->Branch(TString("t_phi"), _tau_phi, TString("t_phi[ntau]/F"));
  anatree->Branch(TString("t_eta"), _tau_eta, TString("t_eta[ntau]/F"));
  anatree->Branch(("event_num"), _event_num, ("event_num[ntau]/I"));
  anatree->Branch(("run_num"), _run_num, ("run_num[ntau]/I"));

  _nRun = 0;
  _nEvt = 0;
}

void TauFinder::processRunHeader( LCRunHeader* ) {}

// This gets called for every event 
// Usually the working horse...
void TauFinder::processEvent( LCEvent * evt ) 
{

  _ntau = 0;
  _nEvt = evt->getEventNumber();
  _nRun = evt->getRunNumber();

  // Initialize input collection of reconstructed PFOs
  LCCollection *recoCol;
  try {
    recoCol = evt->getCollection( _inCol ) ;
  }
  catch (Exception& e) {
    recoCol = 0;
  }
 
  // LCRelation keeps track of particles from which the tau was made
  LCCollectionVec *relationcol = new LCCollectionVec(LCIO::LCRELATION); // Passed
  relationcol->parameters().setValue(std::string("FromType"),LCIO::RECONSTRUCTEDPARTICLE);
  relationcol->parameters().setValue(std::string("ToType"),LCIO::RECONSTRUCTEDPARTICLE);
  
  // Initialize output collections of reconstructed taus
  LCCollectionVec * taucol = new LCCollectionVec(LCIO::RECONSTRUCTEDPARTICLE); // Passed
  LCCollectionVec * taucol_nchargedtrks = new LCCollectionVec(LCIO::RECONSTRUCTEDPARTICLE); // Failed number of charged tracks
  LCCollectionVec * taucol_invmass = new LCCollectionVec(LCIO::RECONSTRUCTEDPARTICLE); // Failed invariant mass
  LCCollectionVec * taucol_merge = new LCCollectionVec(LCIO::RECONSTRUCTEDPARTICLE); // Failed merge
  LCCollectionVec * taucol_nparticles = new LCCollectionVec(LCIO::RECONSTRUCTEDPARTICLE); // Failed number of particles
  LCCollectionVec * taucol_isoenergy = new LCCollectionVec(LCIO::RECONSTRUCTEDPARTICLE); // Failed isolation energy
  
  // Store all, charged, and neutral particles
  std::vector<ReconstructedParticle*> all_vector; // All particles
  std::vector<ReconstructedParticle*> charged_vector; // Charged particles
  std::vector<ReconstructedParticle*> neutral_vector; // Neutral particles

  // Keep reconstructed PFOs which pass pt selection cut and remove neutrons
  if(recoCol != 0)
    {
      int nReco = recoCol->getNumberOfElements();
      for (int i=0; i<nReco; i++) 
	{
	  ReconstructedParticle *pfo = static_cast<ReconstructedParticle*>(recoCol->getElementAt(i));

	  double pt = sqrt(pfo->getMomentum()[0]*pfo->getMomentum()[0]
			 + pfo->getMomentum()[1]*pfo->getMomentum()[1]);
	  int pfo_type = pfo->getType();

	  if(pt < _ptCut || pfo_type == 2112)
	    {
	      continue;
	    }
	  
	  all_vector.push_back(pfo);
	  if(pfo->getCharge() == 1 || pfo->getCharge() == -1)
	    {
	      charged_vector.push_back(pfo);
	    }
	  else
	    {
	      neutral_vector.push_back(pfo);
	    }
	}
    }

  
  // Sort according to energy (highest to lowest)
  std::sort(charged_vector.begin(), charged_vector.end(), energySort);
  std::sort(neutral_vector.begin(), neutral_vector.end(), energySort);
  
  // Vector to hold tau candidates
  std::vector<std::vector<ReconstructedParticle*> > tau_vec;

  // Perform tau reconstruction
  bool finding_done = false;  
  while(charged_vector.size() && !finding_done)
    {
      finding_done = findTau(charged_vector, neutral_vector, tau_vec);
    }

  // Vector to store reconstructed taus
  std::vector<ReconstructedParticleImpl* > reco_tau_vec;
  
  // Combine associated particles to tau
  for (unsigned int p=0; p<tau_vec.size(); p++)
    {
      // Reconstructed tau
      ReconstructedParticleImpl *reco_tau = new ReconstructedParticleImpl();
      double E = 0;
      double charge = 0;
      int pdg = 15;
      double mom[3] = {0, 0, 0};
      int n_charged = 0;
      
      // Tau candidate
      std::vector<ReconstructedParticle*> tau = tau_vec[p];

      // Loop through particles associated to tau candidate
      for (unsigned int tp=0; tp<tau.size(); tp++)
	{
	  // Add up energy and momentum
	  E += tau[tp]->getEnergy();
	  charge += tau[tp]->getCharge();
	  mom[0] += tau[tp]->getMomentum()[0];
	  mom[1] += tau[tp]->getMomentum()[1];
	  mom[2] += tau[tp]->getMomentum()[2];
	  
	  // Add associated particles to reco tau
	  reco_tau->addParticle(tau[tp]);
	}
      
      if(charge < 0)
	{
	  pdg = -15;
	}

      // Set reco tau parameters
      reco_tau->setEnergy(E);
      reco_tau->setCharge(charge);
      reco_tau->setMomentum(mom);
      reco_tau->setType(pdg);
      
      n_charged = getNCharged(reco_tau);

      double mom_sqrd = mom[0]*mom[0] + mom[1]*mom[1] + mom[2]*mom[2];
      double inv_mass = 0;

      if (E*E < mom_sqrd)
	{
	  inv_mass = E - sqrt(mom_sqrd);
	}
      else
	{
	  inv_mass = sqrt(E*E - mom_sqrd);
	}
      
      // Create relation between reco tau and associated PFOs
      for (unsigned int tp=0; tp<tau.size(); tp++)
	{
	  LCRelationImpl *rel = new LCRelationImpl(reco_tau, tau[tp]);
	  relationcol->addElement(rel);
	}      

      // Check for invariant mass and number of charged particles
      if (inv_mass > _invMass || n_charged > 4 || n_charged == 0)
	{
	  if (inv_mass > _invMass)
	    {
	      // Add reco tau to collection which failed invariant mass selection
	      taucol_invmass->addElement(reco_tau);
	    }
	  if (n_charged > 4 || n_charged == 0)
	    {
	      // Add reco tau to collection which failed number of charged tracks selection
	      taucol_nchargedtrks->addElement(reco_tau);
	    }
	}
      else
	{
	  // Add reco tau to collection which passed selections thus far
	  reco_tau_vec.push_back(reco_tau);
	}
    }
  
  // Merge taus that are very close together
  LCRelationNavigator *relationNavigator = new LCRelationNavigator(relationcol);
  
  if(reco_tau_vec.size() > 1)
    {
      std::vector<ReconstructedParticleImpl*>::iterator iterI = reco_tau_vec.begin();
      std::vector<ReconstructedParticleImpl*>::iterator iterJ = reco_tau_vec.begin();

      double angle = 0;
      int erasecount = 0;

      
      for (unsigned int i=0; i<reco_tau_vec.size(); i++)
	{
	  ReconstructedParticleImpl *tau_i = static_cast<ReconstructedParticleImpl*>(reco_tau_vec[i]);
	  const double *mom_i = tau_i->getMomentum();
	  
	  for (unsigned int j=i+1; j<reco_tau_vec.size(); j++)
	    {
	      iterJ = reco_tau_vec.begin() + j;
	      ReconstructedParticleImpl *tau_j = static_cast<ReconstructedParticleImpl*>(reco_tau_vec[j]);
	      const double *mom_j = tau_j->getMomentum();

	      angle = acos((mom_i[0]*mom_j[0]+mom_i[1]*mom_j[1]+mom_i[2]*mom_j[2])/
			 (sqrt(mom_i[0]*mom_i[0]+mom_i[1]*mom_i[1]+mom_i[2]*mom_i[2])*
			  sqrt(mom_j[0]*mom_j[0]+mom_j[1]*mom_j[1]+mom_j[2]*mom_j[2])));

	      // Merge if angle between two taus is less than search cone angle
	      if(angle < _coneAngle)
		{
		  double energy = tau_i->getEnergy() + tau_j->getEnergy();
		  tau_i->setEnergy(energy);
		  double mom[3] = {mom_i[0]+mom_j[0], mom_i[1]+mom_j[1], mom_i[2]+mom_j[2]};
		  tau_i->setMomentum(mom);
		  tau_i->setCharge(tau_i->getCharge() + tau_j->getCharge());

		  double inv_mass = 0;
		  double mom_sqrd = mom[0]*mom[0] + mom[1]*mom[1] + mom[2]*mom[2];
		  if (energy*energy < mom_sqrd)
		    {
		      inv_mass = energy - sqrt(mom_sqrd);
		    }
		  else
		    {
		      inv_mass = sqrt(energy*energy - mom_sqrd);
		    }

		  // Add particles from one tau to the other
		  std::vector<ReconstructedParticle*> merge_pfos = tau_j->getParticles();
		  for (unsigned int p=0; p<merge_pfos.size(); p++)
		    {
		      tau_i->addParticle(merge_pfos[p]);
		    }

		  // Set the relations
		  EVENT::LCObjectVec pfos_j = relationNavigator->getRelatedToObjects(tau_j);
		  for (unsigned int o=0; o<pfos_j.size(); o++)
		    {
		      ReconstructedParticle *pfo = static_cast<ReconstructedParticle*>(pfos_j[o]);
		      LCRelationImpl *rel = new LCRelationImpl(tau_i, pfo);
		      relationcol->addElement(rel);
		    }

		  int n_charged = getNCharged(tau_i);
		  
		  // Merge fails if invariant mass too large or too many charged particles
		  if (inv_mass > _invMass || n_charged > 4)
		    {
		      // Add tau to collection which failed merge
		      taucol_merge->addElement(tau_i);

		      // delete *iterJ;
		      reco_tau_vec.erase(iterJ);
		      erasecount++;
		      j--;
		      // delete *iterI;
		      iterI = reco_tau_vec.erase(iterI);
		      erasecount++;
		      if (iterI != reco_tau_vec.end())
			{
			  tau_i = *iterI;
			  mom_i = tau_i->getMomentum();
			  // angle = 10000000;
			}
		    }

		  else
		    {
		      // delete *iterJ;
		      reco_tau_vec.erase(iterJ);
		      erasecount++;
		      j--;
		    } 
		}
	    }
	  iterI++;
	}
    }
  delete relationNavigator;
  
  // Test for isolation and too many tracks
  std::vector<ReconstructedParticleImpl*>::iterator iter = reco_tau_vec.begin();
  _ntau = reco_tau_vec.size();
  int erasecount = 0;
  for (unsigned int t=0; t<reco_tau_vec.size(); t++)
    {
      ReconstructedParticleImpl *tau = static_cast<ReconstructedParticleImpl*>(reco_tau_vec[t]);
      const double *mom_tau = tau->getMomentum();
      double iso_energy = 0;
      int n_charged = getNCharged(tau);
      int n_particles = n_charged + getNNeutral(tau);

      const double p_tau = sqrt(mom_tau[0]*mom_tau[0] + mom_tau[1]*mom_tau[1] + mom_tau[2]*mom_tau[2]);
      
      _tau_energy[t+erasecount] = tau->getEnergy();
      _tau_p[t+erasecount] = p_tau;
      _tau_pt[t+erasecount] = sqrt(mom_tau[0]*mom_tau[0] + mom_tau[1]*mom_tau[1]);
      _tau_phi[t+erasecount] = TMath::ATan2(mom_tau[1], mom_tau[0]);
      _tau_eta[t+erasecount] = 0.5*TMath::Log((p_tau + mom_tau[2])/(p_tau - mom_tau[2]));
      _tau_invMass[t+erasecount] = sqrt(tau->getEnergy()*tau->getEnergy() - p_tau*p_tau);
      
      // Too many particles in tau 
      if(n_particles > 10 || n_charged > 4)
	{
	  // Add tau to collection which failed number of particles
	  taucol_nparticles->addElement(tau);

	  // delete *iter;
	  iter = reco_tau_vec.erase(iter);
	  erasecount++;
	  t--;
	  continue;
	}
      
      // Isolation energy exceeds threshold
      for (unsigned int s=0; s<all_vector.size() ;s++ )
	{
	  ReconstructedParticle *pfo = static_cast<ReconstructedParticle*>(all_vector[s]);
	  const double *mom_pfo = pfo->getMomentum();
	  
	  double angle = acos((mom_pfo[0]*mom_tau[0]+mom_pfo[1]*mom_tau[1]+mom_pfo[2]*mom_tau[2])/
			    (sqrt(mom_pfo[0]*mom_pfo[0]+mom_pfo[1]*mom_pfo[1]+mom_pfo[2]*mom_pfo[2])*
			     sqrt(mom_tau[0]*mom_tau[0]+mom_tau[1]*mom_tau[1]+mom_tau[2]*mom_tau[2])));

	  if(angle > _coneAngle && angle < _isoAngle+_coneAngle)
	    {
	      iso_energy += pfo->getEnergy();
	    }
	}

      _tau_isoE[t+erasecount] = iso_energy;
      _event_num[t+erasecount] = _nEvt;
      _run_num[t+erasecount] = _nRun;

      if(iso_energy > _isoE)
	{
     	  // Add tau to collection which failed isolation energy
	  taucol_isoenergy->addElement(tau);

	  // delete *iter;
	  iter=reco_tau_vec.erase(iter);
	  erasecount++;
	  t--;
	}
      else
	{
	  taucol->addElement(tau);  
	  iter++;
	}
    }

  evt->addCollection(taucol, _outCol);
  evt->addCollection(relationcol, _tauPFOLinkCol);
  evt->addCollection(taucol_nchargedtrks, _outColNChargedTrks);
  evt->addCollection(taucol_invmass, _outColInvMass);
  evt->addCollection(taucol_merge, _outColMerge);
  evt->addCollection(taucol_nparticles, _outColNParticles);
  evt->addCollection(taucol_isoenergy, _outColIsoEnergy);

  anatree->Fill();
  
}

// Sort energy from highest to lowest
bool TauFinder::energySort(ReconstructedParticle *p1, ReconstructedParticle *p2)
{
  return fabs(p1->getEnergy()) > fabs(p2->getEnergy());
}

// Get number of charged particles
int TauFinder::getNCharged(ReconstructedParticleImpl *p)
{
  std::vector<ReconstructedParticle*> pfos = p->getParticles();
  int n_charged = 0;
  for (unsigned int i=0; i<pfos.size(); i++)
    {
      if (pfos[i]->getCharge())
	{
	  n_charged++;
	}
    }

  return n_charged;
}

// Get number of neutral particles
int TauFinder::getNNeutral(ReconstructedParticleImpl *p)
{
  std::vector<ReconstructedParticle*> pfos = p->getParticles();
  int n_neutral = 0;
  for (unsigned int i=0; i<pfos.size(); i++)
    {
      if (!pfos[i]->getCharge())
	{
	  n_neutral++;
	}
    }

  return n_neutral;
}

// Calculate pseudorapidity (eta)
double TauFinder::computeEta(const double mom[3])
{
  double pz = mom[2];
  double pt = sqrt(mom[0]*mom[0] + mom[1]*mom[1]);
  return 0.5 * std::log((sqrt(pt*pt + pz*pz) + pz)/(sqrt(pt*pt + pz*pz) - pz));
}

// Calculate azimuthal angle (phi)
double TauFinder::computePhi(const double mom[3])
{
  return atan2(mom[1], mom[0]);
}

// Compute deltaR between two particles
double TauFinder::computeDeltaR(const double mom1[3], const double mom2[3])
{
  double eta1 = computeEta(mom1);
  double eta2 = computeEta(mom2);

  double phi1 = computePhi(mom1);
  double phi2 = computePhi(mom2);

  double deltaEta = eta1 - eta2;
  double deltaPhi = phi1 - phi2;

  // Ensure deltaPhi is within the range [-π, π]
  if (deltaPhi > M_PI)
    deltaPhi -= 2 * M_PI;
  if (deltaPhi < -M_PI)
    deltaPhi += 2 * M_PI;

  return sqrt(deltaEta*deltaEta + deltaPhi*deltaPhi);
}

// Compute dynamic cone angle
float TauFinder::computeDynamicCone(float pt)
{
  if (pt < 10.0) return 0.6;
  else if (pt > 10.0 && pt < 120.0) return std::max(6.0/pt, 0.05);
  else return 0.05;
}
  

bool TauFinder::findTau(std::vector<ReconstructedParticle*> &charged_vec, std::vector<ReconstructedParticle*> &neutral_vec,
			std::vector<std::vector<ReconstructedParticle*>> &tau_vec)
{
  // Initialize tau candidate
  std::vector<ReconstructedParticle*> tau;

  // Stop searching for tau if there are no charged particles in event
  if(charged_vec.size()==0)
    {
      return true;
    }
  
  // Find a tau seed 
  ReconstructedParticle *tau_seed = NULL;
  
  std::vector<ReconstructedParticle*>::iterator iterS = charged_vec.begin();
  for (unsigned int s=0; s<charged_vec.size(); s++)
    {
      tau_seed = static_cast<ReconstructedParticle*>(charged_vec[s]);

      std::vector<double> mom(3);
      for (int i=0; i<3; ++i)
	{
	  mom[i] = tau_seed->getMomentum()[i];
	}
      
      double pt = sqrt(mom[0]*mom[0] + mom[1]*mom[1]);

      // Stop searching for seed if pt exceeds threshold
      if(pt > _ptSeed)
	{
	  break;
	}
      else
	{
	  iterS++;
	  tau_seed=NULL;
	}
    }

  // Stop searching for tau if no seed is found
  if(!tau_seed)
    {
      return true;
    }

  // Remove seed from charged particle vector
  charged_vec.erase(iterS);

  // Add seed to tau candidate and set energy and momentum
  tau.push_back(tau_seed);
  double tau_energy = tau_seed->getEnergy();
  std::vector<double> tau_mom(3);
  tau_mom[0] = tau_seed->getMomentum()[0];
  tau_mom[1] = tau_seed->getMomentum()[1];
  tau_mom[2] = tau_seed->getMomentum()[2];

  // Dynamic cone
  float tau_dynamic_pt = std::sqrt(tau_mom[0]*tau_mom[0] + tau_mom[1]*tau_mom[1]);
  float coneAngle = computeDynamicCone(tau_dynamic_pt);
  
  // Assign charged particles to tau candidate
  std::vector<ReconstructedParticle*>::iterator iterQ = charged_vec.begin();
  for (unsigned int q=0; q<charged_vec.size(); q++)
    {
      ReconstructedParticle *charged = static_cast<ReconstructedParticle*>(charged_vec[q]);

      std::vector<double> charged_mom(3);
      charged_mom[0] = charged->getMomentum()[0];
      charged_mom[1] = charged->getMomentum()[1];
      charged_mom[2] = charged->getMomentum()[2];
      
      double angle = acos((charged_mom[0]*tau_mom[0]+charged_mom[1]*tau_mom[1]+charged_mom[2]*tau_mom[2])/
			(sqrt(charged_mom[0]*charged_mom[0]+charged_mom[1]*charged_mom[1]+charged_mom[2]*charged_mom[2])*
			 sqrt(tau_mom[0]*tau_mom[0]+tau_mom[1]*tau_mom[1]+tau_mom[2]*tau_mom[2])));

      // Add charged particle to tau candidate if inside search cone
      if(angle < coneAngle)
	{
	  tau.push_back(charged_vec[q]);
	  tau_energy += charged_vec[q]->getEnergy();
	  for(int i=0; i<3; i++)
	    {
	      tau_mom[i] += charged_mom[i];
	    }

	  // Update dynamic cone
	  tau_dynamic_pt = std::sqrt(tau_mom[0]*tau_mom[0] + tau_mom[1]*tau_mom[1]);
	  coneAngle = computeDynamicCone(tau_dynamic_pt);
	  
	  // Remove charged particle from charged particle vector
	  charged_vec.erase(iterQ);
	  q--;
	}
      else
	iterQ++;
    }
  
  // Assign neutral particles to tau candidate
  std::vector<ReconstructedParticle*>::iterator iterN = neutral_vec.begin();
  for (unsigned int n=0; n<neutral_vec.size(); n++)
    {
      ReconstructedParticle *neutral = neutral_vec[n];

      std::vector<double> neutral_mom(3);
      neutral_mom[0] = neutral->getMomentum()[0];
      neutral_mom[1] = neutral->getMomentum()[1];
      neutral_mom[2] = neutral->getMomentum()[2];
      
      double angle = acos((neutral_mom[0]*tau_mom[0]+neutral_mom[1]*tau_mom[1]+neutral_mom[2]*tau_mom[2])/
			(sqrt(neutral_mom[0]*neutral_mom[0]+neutral_mom[1]*neutral_mom[1]+neutral_mom[2]*neutral_mom[2])*
			 sqrt(tau_mom[0]*tau_mom[0]+tau_mom[1]*tau_mom[1]+tau_mom[2]*tau_mom[2])));

      // Add neutral particle to tau candidate if inside search cone
      if(angle < coneAngle)
	{
	  tau.push_back(neutral_vec[n]);
	  tau_energy += neutral_vec[n]->getEnergy();
	  for(int i=0; i<3; i++)
	    {
	      tau_mom[i] += neutral_mom[i];
	    }

	  // Update dynamic cone
	  tau_dynamic_pt = std::sqrt(tau_mom[0]*tau_mom[0] + tau_mom[1]*tau_mom[1]);
	  coneAngle = computeDynamicCone(tau_dynamic_pt);
	  
	  // Remove neutral particle from neutral particle vector
	  neutral_vec.erase(iterN);
	  n--;
	}
      else 
	iterN++;
    }

  // Add tau candidate to tau candidate vector
  tau_vec.push_back(tau);

  // Continue to search for tau candidates
  return false;
}

void TauFinder::check( LCEvent* ) {
  // Nothing to check here - could be used to fill checkplots in reconstruction processor
}


void TauFinder::end(){
  anatree->Write();
  rootfile->Write();
  delete anatree;
  delete rootfile;
}
