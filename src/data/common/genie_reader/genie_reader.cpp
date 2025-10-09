#include <TFile.h>
#include <TTree.h>

#include <ufw/data.hpp>
#include <ufw/config.hpp>
#include <ufw/context.hpp>

#include <genie_reader/genie_reader.hpp>


namespace sand {
  genie_reader::genie_reader() {}

  truth_adapter::value_type& truth_adapter::at(const index_type&) {
    UFW_FATAL("Not yet implemented");
  }

  bool truth_adapter::valid(const index_type&) {
    UFW_FATAL("Not yet implemented");
  }

}

ufw::data::factory<sand::genie_reader>::factory(const ufw::config& cfg) : input_file(nullptr) {
  auto path = cfg.path_at("uri");
  input_file.reset(TFile::Open(path.c_str()));
  input_tree = input_file->Get<TTree>("gRooTracker");
  if(!input_tree){
    UFW_DEBUG("gRooTracker tree not found in file '{}'.", path.c_str());
    UFW_DEBUG("Trying the edepsim path.", path.c_str());
    
    input_tree = input_file->Get<TTree>("DetSimPassThru/gRooTracker");
    if(!input_tree){
      UFW_ERROR("gRooTracker tree not found in file '{}'.", path.c_str());
    }
  }

  auto set = [&](const char* b, auto* addr) {
    if (auto br = input_tree->GetBranch(b)) {
      br->SetAddress(addr);
    }
    else UFW_WARN("Missing expected branch '{}'", b);
  };

  set("EvtNum", &EvtNum);
  set("EvtXSec", &EvtXSec);
  set("EvtDXSec", &EvtDXSec);
  set("EvtKPS", &EvtKPS);
  set("EvtWght", &EvtWght);
  set("EvtProb", &EvtProb);
  set("EvtVtx", &EvtVtx);
  set("EvtCode", &EvtCode);
  set("EvtFlags", &EvtFlags);
  
  set("StdHepN", &StdHepN);
  set("StdHepPdg", &StdHepPdg);
  set("StdHepStatus", &StdHepStatus);
  set("StdHepRescat", &StdHepRescat);
  set("StdHepX4", &StdHepX4);
  set("StdHepP4", &StdHepP4);
  set("StdHepPolz", &StdHepPolz);
  set("StdHepFd", &StdHepFd);
  set("StdHepLd", &StdHepLd);
  set("StdHepFm", &StdHepFm);
  set("StdHepLm", &StdHepLm);

  if (input_tree->GetBranch("NuParentPdg")) {
    hasNuParent = true;
    set("NuParentPdg", &NuParentPdg);
    set("NuParentDecMode", &NuParentDecMode);
    set("NuParentDecP4", &NuParentDecP4);
    set("NuParentDecX4", &NuParentDecX4);
    set("NuParentProP4", &NuParentProP4);
    set("NuParentProX4", &NuParentProX4);
    set("NuParentProNVtx", &NuParentProNVtx);
  } else {
    UFW_DEBUG("NuParent branches missing — skipping NuParent setup.");
  }
  
  if (input_tree->GetBranch("NumiFluxRun")) {
    hasNumiFlux = true;
    set("NumiFluxRun", &NumiFluxRun);
    set("NumiFluxEvtno", &NumiFluxEvtno);
    set("NumiFluxNdxdz", &NumiFluxNdxdz);
    set("NumiFluxNdydz", &NumiFluxNdydz);
    set("NumiFluxNpz", &NumiFluxNpz);
    set("NumiFluxNenergy", &NumiFluxNenergy);
    set("NumiFluxNdxdznea", &NumiFluxNdxdznea);
    set("NumiFluxNdydznea", &NumiFluxNdydznea);
    set("NumiFluxNenergyn", &NumiFluxNenergyn);
    set("NumiFluxNwtnear", &NumiFluxNwtnear);
    set("NumiFluxNdxdzfar", &NumiFluxNdxdzfar);
    set("NumiFluxNdydzfar", &NumiFluxNdydzfar);
    set("NumiFluxNenergyf", &NumiFluxNenergyf);
    set("NumiFluxNwtfar", &NumiFluxNwtfar);
    set("NumiFluxNorig", &NumiFluxNorig);
    set("NumiFluxNdecay", &NumiFluxNdecay);
    set("NumiFluxNtype", &NumiFluxNtype);
    set("NumiFluxVx", &NumiFluxVx);
    set("NumiFluxVy", &NumiFluxVy);
    set("NumiFluxVz", &NumiFluxVz);
    set("NumiFluxPdpx", &NumiFluxPdpx);
    set("NumiFluxPdpy", &NumiFluxPdpy);
    set("NumiFluxPdpz", &NumiFluxPdpz);
    set("NumiFluxPpdxdz", &NumiFluxPpdxdz);
    set("NumiFluxPpdydz", &NumiFluxPpdydz);
    set("NumiFluxPppz", &NumiFluxPppz);
    set("NumiFluxPpenergy", &NumiFluxPpenergy);
    set("NumiFluxPpmedium", &NumiFluxPpmedium);
    set("NumiFluxPtype", &NumiFluxPtype);
    set("NumiFluxPpvx", &NumiFluxPpvx);
    set("NumiFluxPpvy", &NumiFluxPpvy);
    set("NumiFluxPpvz", &NumiFluxPpvz);
    set("NumiFluxMuparpx", &NumiFluxMuparpx);
    set("NumiFluxMuparpy", &NumiFluxMuparpy);
    set("NumiFluxMuparpz", &NumiFluxMuparpz);
    set("NumiFluxMupare", &NumiFluxMupare);
    set("NumiFluxNecm", &NumiFluxNecm);
    set("NumiFluxNimpwt", &NumiFluxNimpwt);
    set("NumiFluxXpoint", &NumiFluxXpoint);
    set("NumiFluxYpoint", &NumiFluxYpoint);
    set("NumiFluxZpoint", &NumiFluxZpoint);
    set("NumiFluxTvx", &NumiFluxTvx);
    set("NumiFluxTvy", &NumiFluxTvy);
    set("NumiFluxTvz", &NumiFluxTvz);
    set("NumiFluxTpx", &NumiFluxTpx);
    set("NumiFluxTpy", &NumiFluxTpy);
    set("NumiFluxTpz", &NumiFluxTpz);
    set("NumiFluxTptype", &NumiFluxTptype);
    set("NumiFluxTgen", &NumiFluxTgen);
    set("NumiFluxTgptype", &NumiFluxTgptype);
    set("NumiFluxTgppx", &NumiFluxTgppx);
    set("NumiFluxTgppy", &NumiFluxTgppy);
    set("NumiFluxTgppz", &NumiFluxTgppz);
    set("NumiFluxTprivx", &NumiFluxTprivx);
    set("NumiFluxTprivy", &NumiFluxTprivy);
    set("NumiFluxTprivz", &NumiFluxTprivz);
    set("NumiFluxBeamx", &NumiFluxBeamx);
    set("NumiFluxBeamy", &NumiFluxBeamy);
    set("NumiFluxBeamz", &NumiFluxBeamz);
    set("NumiFluxBeampx", &NumiFluxBeampx);
    set("NumiFluxBeampy", &NumiFluxBeampy);
    set("NumiFluxBeampz", &NumiFluxBeampz);
  } else {
    UFW_DEBUG("NumiFlux branches missing — skipping NumiFlux setup.");
  }
}

ufw::data::factory<sand::genie_reader>::~factory() = default;

sand::genie_reader& ufw::data::factory<sand::genie_reader>::instance(ufw::context_id i) {
  if (m_id != i) {
    input_tree->GetEntry(i);

    reader.event_ = {EvtNum, EvtXSec, EvtDXSec, 
                     EvtKPS, EvtWght, EvtProb,
                     EvtVtx, EvtCode, EvtFlags};

    reader.stdHep_ = StdHep(StdHepN, StdHepPdg, StdHepStatus, 
                            StdHepRescat, StdHepX4, StdHepP4, 
                            StdHepPolz, StdHepFd, StdHepLd, 
                            StdHepFm, StdHepLm);
    
    if (hasNuParent) {
      reader.nuParent_ = NuParent(NuParentPdg, NuParentDecMode,
                                  NuParentDecP4, NuParentDecX4,
                                  NuParentProP4,  NuParentProX4,
                                  NuParentProNVtx);
    }

    if (hasNumiFlux) {
      reader.numiFlux_ = {NumiFluxRun, NumiFluxEvtno, NumiFluxNdxdz, 
                          NumiFluxNdydz, NumiFluxNpz, NumiFluxNenergy, 
                          NumiFluxNdxdznea, NumiFluxNdydznea, NumiFluxNenergyn, 
                          NumiFluxNwtnear, NumiFluxNdxdzfar, NumiFluxNdydzfar, 
                          NumiFluxNenergyf, NumiFluxNwtfar, NumiFluxNorig, 
                          NumiFluxNdecay, NumiFluxNtype, NumiFluxVx, NumiFluxVy, 
                          NumiFluxVz, NumiFluxPdpx, NumiFluxPdpy, NumiFluxPdpz, 
                          NumiFluxPpdxdz, NumiFluxPpdydz, NumiFluxPppz, 
                          NumiFluxPpenergy, NumiFluxPpmedium, NumiFluxPtype, 
                          NumiFluxPpvx, NumiFluxPpvy, NumiFluxPpvz, NumiFluxMuparpx, 
                          NumiFluxMuparpy, NumiFluxMuparpz, NumiFluxMupare, 
                          NumiFluxNecm, NumiFluxNimpwt, NumiFluxXpoint, 
                          NumiFluxYpoint, NumiFluxZpoint, NumiFluxTvx, 
                          NumiFluxTvy, NumiFluxTvz, NumiFluxTpx, NumiFluxTpy, 
                          NumiFluxTpz, NumiFluxTptype, NumiFluxTgen, 
                          NumiFluxTgptype, NumiFluxTgppx, NumiFluxTgppy, 
                          NumiFluxTgppz, NumiFluxTprivx, NumiFluxTprivy, 
                          NumiFluxTprivz, NumiFluxBeamx, NumiFluxBeamy, 
                          NumiFluxBeamz, NumiFluxBeampx, NumiFluxBeampy, 
                          NumiFluxBeampz, true};
    }
    m_id = i;
  }
  return reader;
}

