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
    m_id = i;
  }
  return reader;
}

