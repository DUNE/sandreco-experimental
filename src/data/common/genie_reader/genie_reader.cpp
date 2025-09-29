#include <TFile.h>
#include <TTree.h>

#include <ufw/data.hpp>
#include <ufw/config.hpp>
#include <ufw/context.hpp>

#include <genie_reader/genie_reader.hpp>


namespace sand {
  genie_reader::genie_reader() : GenieWrapper() {}

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
    UFW_ERROR("gRooTracker tree not found in file '{}'.", path.c_str());
  }

  TBranch* brEvtNum   = input_tree->GetBranch("EvtNum");
  TBranch* brEvtXSec  = input_tree->GetBranch("EvtXSec");
  TBranch* brEvtDXSec = input_tree->GetBranch("EvtDXSec");
  TBranch* brEvtKPS = input_tree->GetBranch("EvtKPS");
  TBranch* brEvtWght = input_tree->GetBranch("EvtWght");
  TBranch* brEvtProb = input_tree->GetBranch("EvtProb");
  TBranch* brEvtVtx  = input_tree->GetBranch("EvtVtx");
  TBranch* brEvtCode  = input_tree->GetBranch("EvtCode");
  TBranch* brEvtFlags = input_tree->GetBranch("EvtFlags");
  brEvtNum->SetAddress(&EvtNum);
  brEvtXSec->SetAddress(&EvtXSec);
  brEvtDXSec->SetAddress(&EvtDXSec);
  brEvtKPS->SetAddress(&EvtKPS);
  brEvtWght->SetAddress(&EvtWght);
  brEvtProb->SetAddress(&EvtProb);
  brEvtVtx->SetAddress(&EvtVtx);
  brEvtCode->SetAddress(&EvtCode);
  brEvtFlags->SetAddress(&EvtFlags);
  

  TBranch* brStdHepN = input_tree->GetBranch("StdHepN");
  TBranch* brStdHepPdg = input_tree->GetBranch("StdHepPdg");
  TBranch* brStdHepStatus = input_tree->GetBranch("StdHepStatus");
  TBranch* brStdHepRescat = input_tree->GetBranch("StdHepRescat");
  TBranch* brStdHepX4 = input_tree->GetBranch("StdHepX4");
  TBranch* brStdHepP4 = input_tree->GetBranch("StdHepP4");
  TBranch* brStdHepPolz = input_tree->GetBranch("StdHepPolz");
  TBranch* brStdHepFd = input_tree->GetBranch("StdHepFd");
  TBranch* brStdHepLd = input_tree->GetBranch("StdHepLd");
  TBranch* brStdHepFm = input_tree->GetBranch("StdHepFm");
  TBranch* brStdHepLm = input_tree->GetBranch("StdHepLm");
  brStdHepN->SetAddress(&StdHepN);
  brStdHepPdg->SetAddress(&StdHepPdg);
  brStdHepStatus->SetAddress(&StdHepStatus);
  brStdHepRescat->SetAddress(&StdHepRescat);
  brStdHepX4->SetAddress(&StdHepX4);
  brStdHepP4->SetAddress(&StdHepP4);
  brStdHepPolz->SetAddress(&StdHepPolz);
  brStdHepFd->SetAddress(&StdHepFd);
  brStdHepLd->SetAddress(&StdHepLd);
  brStdHepFm->SetAddress(&StdHepFm);
  brStdHepLm->SetAddress(&StdHepLm);

  TBranch* brNumiFluxRun = input_tree->GetBranch("NumiFluxRun");
  TBranch* brNumiFluxEvtno = input_tree->GetBranch("NumiFluxEvtno");
  TBranch* brNumiFluxNdxdz = input_tree->GetBranch("NumiFluxNdxdz");
  TBranch* brNumiFluxNdydz = input_tree->GetBranch("NumiFluxNdydz");
  TBranch* brNumiFluxNpz = input_tree->GetBranch("NumiFluxNpz");
  TBranch* brNumiFluxNenergy = input_tree->GetBranch("NumiFluxNenergy");
  TBranch* brNumiFluxNdxdznea = input_tree->GetBranch("NumiFluxNdxdznea");
  TBranch* brNumiFluxNdydznea = input_tree->GetBranch("NumiFluxNdydznea");
  TBranch* brNumiFluxNenergyn = input_tree->GetBranch("NumiFluxNenergyn");
  TBranch* brNumiFluxNwtnear = input_tree->GetBranch("NumiFluxNwtnear");
  TBranch* brNumiFluxNdxdzfar = input_tree->GetBranch("NumiFluxNdxdzfar");
  TBranch* brNumiFluxNdydzfar = input_tree->GetBranch("NumiFluxNdydzfar");
  TBranch* brNumiFluxNenergyf = input_tree->GetBranch("NumiFluxNenergyf");
  TBranch* brNumiFluxNwtfar = input_tree->GetBranch("NumiFluxNwtfar");
  TBranch* brNumiFluxNorig = input_tree->GetBranch("NumiFluxNorig");
  TBranch* brNumiFluxNdecay = input_tree->GetBranch("NumiFluxNdecay");
  TBranch* brNumiFluxNtype = input_tree->GetBranch("NumiFluxNtype");
  TBranch* brNumiFluxVx = input_tree->GetBranch("NumiFluxVx");
  TBranch* brNumiFluxVy = input_tree->GetBranch("NumiFluxVy");
  TBranch* brNumiFluxVz = input_tree->GetBranch("NumiFluxVz");
  TBranch* brNumiFluxPdpx = input_tree->GetBranch("NumiFluxPdpx");
  TBranch* brNumiFluxPdpy = input_tree->GetBranch("NumiFluxPdpy");
  TBranch* brNumiFluxPdpz = input_tree->GetBranch("NumiFluxPdpz");
  TBranch* brNumiFluxPpdxdz = input_tree->GetBranch("NumiFluxPpdxdz");
  TBranch* brNumiFluxPpdydz = input_tree->GetBranch("NumiFluxPpdydz");
  TBranch* brNumiFluxPppz = input_tree->GetBranch("NumiFluxPppz");
  TBranch* brNumiFluxPpenergy = input_tree->GetBranch("NumiFluxPpenergy");
  TBranch* brNumiFluxPpmedium = input_tree->GetBranch("NumiFluxPpmedium");
  TBranch* brNumiFluxPtype = input_tree->GetBranch("NumiFluxPtype");
  TBranch* brNumiFluxPpvx = input_tree->GetBranch("NumiFluxPpvx");
  TBranch* brNumiFluxPpvy = input_tree->GetBranch("NumiFluxPpvy");
  TBranch* brNumiFluxPpvz = input_tree->GetBranch("NumiFluxPpvz");
  TBranch* brNumiFluxMuparpx = input_tree->GetBranch("NumiFluxMuparpx");
  TBranch* brNumiFluxMuparpy = input_tree->GetBranch("NumiFluxMuparpy");
  TBranch* brNumiFluxMuparpz = input_tree->GetBranch("NumiFluxMuparpz");
  TBranch* brNumiFluxMupare = input_tree->GetBranch("NumiFluxMupare");
  TBranch* brNumiFluxNecm = input_tree->GetBranch("NumiFluxNecm");
  TBranch* brNumiFluxNimpwt = input_tree->GetBranch("NumiFluxNimpwt");
  TBranch* brNumiFluxXpoint = input_tree->GetBranch("NumiFluxXpoint");
  TBranch* brNumiFluxYpoint = input_tree->GetBranch("NumiFluxYpoint");
  TBranch* brNumiFluxZpoint = input_tree->GetBranch("NumiFluxZpoint");
  TBranch* brNumiFluxTvx = input_tree->GetBranch("NumiFluxTvx");
  TBranch* brNumiFluxTvy = input_tree->GetBranch("NumiFluxTvy");
  TBranch* brNumiFluxTvz = input_tree->GetBranch("NumiFluxTvz");
  TBranch* brNumiFluxTpx = input_tree->GetBranch("NumiFluxTpx");
  TBranch* brNumiFluxTpy = input_tree->GetBranch("NumiFluxTpy");
  TBranch* brNumiFluxTpz = input_tree->GetBranch("NumiFluxTpz");
  TBranch* brNumiFluxTptype = input_tree->GetBranch("NumiFluxTptype");
  TBranch* brNumiFluxTgen = input_tree->GetBranch("NumiFluxTgen");
  TBranch* brNumiFluxTgptype = input_tree->GetBranch("NumiFluxTgptype");
  TBranch* brNumiFluxTgppx = input_tree->GetBranch("NumiFluxTgppx");
  TBranch* brNumiFluxTgppy = input_tree->GetBranch("NumiFluxTgppy");
  TBranch* brNumiFluxTgppz = input_tree->GetBranch("NumiFluxTgppz");
  TBranch* brNumiFluxTprivx = input_tree->GetBranch("NumiFluxTprivx");
  TBranch* brNumiFluxTprivy = input_tree->GetBranch("NumiFluxTprivy");
  TBranch* brNumiFluxTprivz = input_tree->GetBranch("NumiFluxTprivz");
  TBranch* brNumiFluxBeamx = input_tree->GetBranch("NumiFluxBeamx");
  TBranch* brNumiFluxBeamy = input_tree->GetBranch("NumiFluxBeamy");
  TBranch* brNumiFluxBeamz = input_tree->GetBranch("NumiFluxBeamz");
  TBranch* brNumiFluxBeampx = input_tree->GetBranch("NumiFluxBeampx");
  TBranch* brNumiFluxBeampy = input_tree->GetBranch("NumiFluxBeampy");
  TBranch* brNumiFluxBeampz = input_tree->GetBranch("NumiFluxBeampz");
  brNumiFluxRun->SetAddress(&NumiFluxRun);
  brNumiFluxEvtno->SetAddress(&NumiFluxEvtno);
  brNumiFluxNdxdz->SetAddress(&NumiFluxNdxdz);
  brNumiFluxNdydz->SetAddress(&NumiFluxNdydz);
  brNumiFluxNpz->SetAddress(&NumiFluxNpz);
  brNumiFluxNenergy->SetAddress(&NumiFluxNenergy);
  brNumiFluxNdxdznea->SetAddress(&NumiFluxNdxdznea);
  brNumiFluxNdydznea->SetAddress(&NumiFluxNdydznea);
  brNumiFluxNenergyn->SetAddress(&NumiFluxNenergyn);
  brNumiFluxNwtnear->SetAddress(&NumiFluxNwtnear);
  brNumiFluxNdxdzfar->SetAddress(&NumiFluxNdxdzfar);
  brNumiFluxNdydzfar->SetAddress(&NumiFluxNdydzfar);
  brNumiFluxNenergyf->SetAddress(&NumiFluxNenergyf);
  brNumiFluxNwtfar->SetAddress(&NumiFluxNwtfar);
  brNumiFluxNorig->SetAddress(&NumiFluxNorig);
  brNumiFluxNdecay->SetAddress(&NumiFluxNdecay);
  brNumiFluxNtype->SetAddress(&NumiFluxNtype);
  brNumiFluxVx->SetAddress(&NumiFluxVx);
  brNumiFluxVy->SetAddress(&NumiFluxVy);
  brNumiFluxVz->SetAddress(&NumiFluxVz);
  brNumiFluxPdpx->SetAddress(&NumiFluxPdpx);
  brNumiFluxPdpy->SetAddress(&NumiFluxPdpy);
  brNumiFluxPdpz->SetAddress(&NumiFluxPdpz);
  brNumiFluxPpdxdz->SetAddress(&NumiFluxPpdxdz);
  brNumiFluxPpdydz->SetAddress(&NumiFluxPpdydz);
  brNumiFluxPppz->SetAddress(&NumiFluxPppz);
  brNumiFluxPpenergy->SetAddress(&NumiFluxPpenergy);
  brNumiFluxPpmedium->SetAddress(&NumiFluxPpmedium);
  brNumiFluxPtype->SetAddress(&NumiFluxPtype);
  brNumiFluxPpvx->SetAddress(&NumiFluxPpvx);
  brNumiFluxPpvy->SetAddress(&NumiFluxPpvy);
  brNumiFluxPpvz->SetAddress(&NumiFluxPpvz);
  brNumiFluxMuparpx->SetAddress(&NumiFluxMuparpx);
  brNumiFluxMuparpy->SetAddress(&NumiFluxMuparpy);
  brNumiFluxMuparpz->SetAddress(&NumiFluxMuparpz);
  brNumiFluxMupare->SetAddress(&NumiFluxMupare);
  brNumiFluxNecm->SetAddress(&NumiFluxNecm);
  brNumiFluxNimpwt->SetAddress(&NumiFluxNimpwt);
  brNumiFluxXpoint->SetAddress(&NumiFluxXpoint);
  brNumiFluxYpoint->SetAddress(&NumiFluxYpoint);
  brNumiFluxZpoint->SetAddress(&NumiFluxZpoint);
  brNumiFluxTvx->SetAddress(&NumiFluxTvx);
  brNumiFluxTvy->SetAddress(&NumiFluxTvy);
  brNumiFluxTvz->SetAddress(&NumiFluxTvz);
  brNumiFluxTpx->SetAddress(&NumiFluxTpx);
  brNumiFluxTpy->SetAddress(&NumiFluxTpy);
  brNumiFluxTpz->SetAddress(&NumiFluxTpz);
  brNumiFluxTptype->SetAddress(&NumiFluxTptype);
  brNumiFluxTgen->SetAddress(&NumiFluxTgen);
  brNumiFluxTgptype->SetAddress(&NumiFluxTgptype);
  brNumiFluxTgppx->SetAddress(&NumiFluxTgppx);
  brNumiFluxTgppy->SetAddress(&NumiFluxTgppy);
  brNumiFluxTgppz->SetAddress(&NumiFluxTgppz);
  brNumiFluxTprivx->SetAddress(&NumiFluxTprivx);
  brNumiFluxTprivy->SetAddress(&NumiFluxTprivy);
  brNumiFluxTprivz->SetAddress(&NumiFluxTprivz);
  brNumiFluxBeamx->SetAddress(&NumiFluxBeamx);
  brNumiFluxBeamy->SetAddress(&NumiFluxBeamy);
  brNumiFluxBeamz->SetAddress(&NumiFluxBeamz);
  brNumiFluxBeampx->SetAddress(&NumiFluxBeampx);
  brNumiFluxBeampy->SetAddress(&NumiFluxBeampy);
  brNumiFluxBeampz->SetAddress(&NumiFluxBeampz);

}

ufw::data::factory<sand::genie_reader>::~factory() = default;

sand::genie_reader& ufw::data::factory<sand::genie_reader>::instance(ufw::context_id i) {
  if (m_id != i) {
    input_tree->GetEntry(i);
    reader.event(EvtNum, EvtXSec, EvtDXSec, 
                 EvtKPS, EvtWght, EvtProb,
                 EvtVtx, EvtCode, EvtFlags);
    reader.stdHep(StdHepN, StdHepPdg, StdHepStatus, 
                  StdHepRescat, StdHepX4, StdHepP4, 
                  StdHepPolz, StdHepFd, StdHepLd, 
                  StdHepFm, StdHepLm);
    reader.numiFlux(NumiFluxRun, NumiFluxEvtno, NumiFluxNdxdz, 
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
                    NumiFluxBeampz);
    m_id = i;
  }
  return reader;
}

