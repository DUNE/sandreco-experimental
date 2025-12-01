#pragma once

#include <ufw/data.hpp>

#include <common/truth.h>

#include <genie_reader/GenieWrapper.h>

#include <TBits.h>
#include <TObjString.h>

#include <vector>
#include <utility>

class TFile;
class TTree;

namespace sand {

  struct genie_reader
    : public GenieWrapper
    , public ufw::data::base<ufw::data::complex_tag, ufw::data::unique_tag, ufw::data::context_tag> {
   private:
    genie_reader();
    friend class ufw::data::factory<sand::genie_reader>;
  };

} // namespace sand

UFW_DECLARE_COMPLEX_DATA(sand::genie_reader);

template <>
class ufw::data::factory<sand::genie_reader> {
 public:
  factory(const ufw::config&);
  ~factory();
  sand::genie_reader& instance(ufw::context_id);

 private:
  sand::genie_reader reader;
  std::unique_ptr<TFile> input_file;
  TTree* input_tree;
  std::vector<std::pair<Long64_t, Long64_t>> spills_boundaries;
  ufw::context_id m_id;


  bool hasNuParent = false;
  bool hasNumiFlux = false;

  int EvtNum{};
  TBits* EvtFlags{};
  TObjString* EvtCode{};
  double EvtXSec{};
  double EvtDXSec{};
  double EvtKPS{};
  double EvtWght{};
  double EvtProb{};
  double EvtVtx[4]{};

  int StdHepN{};
  // TODO: check if also these fields are arrays
  //  https://github.com/GENIE-MC/Generator/blob/2084cc6b8f25a460ebf4afd6a4658143fa9ce2ff/src/contrib/t2k/read_t2k_rootracker.C#L54-L56
  constexpr static int kNPmax = 250;
  int StdHepPdg[kNPmax]{};
  int StdHepStatus[kNPmax]{};
  int StdHepRescat[kNPmax]{};
  double StdHepX4[kNPmax][4]{};
  double StdHepP4[kNPmax][4]{};
  double StdHepPolz[kNPmax][3]{};
  int StdHepFd[kNPmax]{};
  int StdHepLd[kNPmax]{};
  int StdHepFm[kNPmax]{};
  int StdHepLm[kNPmax]{};

  int NuParentPdg{};
  int NuParentDecMode{};
  double NuParentDecP4[4]{};
  double NuParentDecX4[4]{};
  double NuParentProP4[4]{};
  double NuParentProX4[4]{};
  int NuParentProNVtx{};

  int NumiFluxRun{};
  int NumiFluxEvtno{};
  double NumiFluxNdxdz{};
  double NumiFluxNdydz{};
  double NumiFluxNpz{};
  double NumiFluxNenergy{};
  double NumiFluxNdxdznea{};
  double NumiFluxNdydznea{};
  double NumiFluxNenergyn{};
  double NumiFluxNwtnear{};
  double NumiFluxNdxdzfar{};
  double NumiFluxNdydzfar{};
  double NumiFluxNenergyf{};
  double NumiFluxNwtfar{};
  int NumiFluxNorig{};
  int NumiFluxNdecay{};
  int NumiFluxNtype{};
  double NumiFluxVx{};
  double NumiFluxVy{};
  double NumiFluxVz{};
  double NumiFluxPdpx{};
  double NumiFluxPdpy{};
  double NumiFluxPdpz{};
  double NumiFluxPpdxdz{};
  double NumiFluxPpdydz{};
  double NumiFluxPppz{};
  double NumiFluxPpenergy{};
  int NumiFluxPpmedium{};
  int NumiFluxPtype{};
  double NumiFluxPpvx{};
  double NumiFluxPpvy{};
  double NumiFluxPpvz{};
  double NumiFluxMuparpx{};
  double NumiFluxMuparpy{};
  double NumiFluxMuparpz{};
  double NumiFluxMupare{};
  double NumiFluxNecm{};
  double NumiFluxNimpwt{};
  double NumiFluxXpoint{};
  double NumiFluxYpoint{};
  double NumiFluxZpoint{};
  double NumiFluxTvx{};
  double NumiFluxTvy{};
  double NumiFluxTvz{};
  double NumiFluxTpx{};
  double NumiFluxTpy{};
  double NumiFluxTpz{};
  double NumiFluxTptype{};
  double NumiFluxTgen{};
  double NumiFluxTgptype{};
  double NumiFluxTgppx{};
  double NumiFluxTgppy{};
  double NumiFluxTgppz{};
  double NumiFluxTprivx{};
  double NumiFluxTprivy{};
  double NumiFluxTprivz{};
  double NumiFluxBeamx{};
  double NumiFluxBeamy{};
  double NumiFluxBeamz{};
  double NumiFluxBeampx{};
  double NumiFluxBeampy{};
  double NumiFluxBeampz{};

  void attach_branches();
};
