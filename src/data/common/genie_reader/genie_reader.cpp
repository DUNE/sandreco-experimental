#include <Geant4/CLHEP/Units/SystemOfUnits.h>
#include <TFile.h>
#include <TTree.h>

#include <algorithm>
#include <array>
#include <climits>

#include <EDepSim/TG4Event.h>

#include <ufw/config.hpp>
#include <ufw/context.hpp>
#include <ufw/data.hpp>

#include <genie_reader/genie_reader.hpp>

namespace sand {
  genie_reader::genie_reader() {}

  truth_adapter::value_type& truth_adapter::at(const index_type&) { UFW_FATAL("Not yet implemented"); }

  bool truth_adapter::valid(const index_type&) { UFW_FATAL("Not yet implemented"); }

} // namespace sand

namespace ufw::data {
  factory<sand::genie_reader>::factory(const ufw::config& cfg) : input_file{nullptr} {
    const auto path = cfg.path_at("uri");
    input_file.reset(TFile::Open(path.c_str()));
    input_tree = input_file->Get<TTree>("gRooTracker");
    if (input_tree == nullptr) {
      UFW_DEBUG("gRooTracker tree not found in the root directory of file '{}'.", path.c_str());
      UFW_DEBUG("Trying the edepsim path: {}/DetSimPassThru/gRooTracker.", path.c_str());

      input_tree = input_file->Get<TTree>("DetSimPassThru/gRooTracker");
      if (input_tree == nullptr) {
        UFW_ERROR("gRooTracker tree not found in file '{}'.", path.c_str());
      }
      UFW_DEBUG("Found");
    }

    check_gRooTracker_format();
    populate_spills_boundaries();
    attach_branches();
  }

  factory<sand::genie_reader>::~factory() = default;

  sand::genie_reader& factory<sand::genie_reader>::instance(ufw::context_id i) {
    if (m_id == i) {
      return reader;
    }

    m_id = i;
    clear_reader(i);

    for (Long64_t j = spills_boundaries[i].first; j < spills_boundaries[i].second; j++) {
      input_tree->GetEntry(j);

      // Copy EvtVtx array to std::array
      std::array<double, 4> evtVtxCopy{};
      std::copy(std::begin(EvtVtx), std::end(EvtVtx), evtVtxCopy.begin());

      // Copy EvtCode string (handle null pointer)
      std::string evtCodeStr = EvtCode ? EvtCode->GetString().Data() : "";

      reader.events_.push_back({EvtNum, EvtXSec, EvtDXSec, EvtKPS, EvtWght, EvtProb, evtVtxCopy, evtCodeStr, EvtFlags});

      reader.stdHeps_.emplace_back(StdHepN, StdHepPdg, StdHepStatus, StdHepRescat, StdHepX4, StdHepP4, StdHepPolz,
                                   StdHepFd, StdHepLd, StdHepFm, StdHepLm);

      if (reader.nuParents_.has_value()) {
        reader.nuParents_->emplace_back(NuParentPdg, NuParentDecMode, NuParentDecP4, NuParentDecX4, NuParentProP4,
                                        NuParentProX4, NuParentProNVtx);
      }

      if (reader.numiFluxes_.has_value()) {
        reader.numiFluxes_->push_back(
            {NumiFluxRun,      NumiFluxEvtno,    NumiFluxNdxdz,    NumiFluxNdydz,    NumiFluxNpz,      NumiFluxNenergy,
             NumiFluxNdxdznea, NumiFluxNdydznea, NumiFluxNenergyn, NumiFluxNwtnear,  NumiFluxNdxdzfar, NumiFluxNdydzfar,
             NumiFluxNenergyf, NumiFluxNwtfar,   NumiFluxNorig,    NumiFluxNdecay,   NumiFluxNtype,    NumiFluxVx,
             NumiFluxVy,       NumiFluxVz,       NumiFluxPdpx,     NumiFluxPdpy,     NumiFluxPdpz,     NumiFluxPpdxdz,
             NumiFluxPpdydz,   NumiFluxPppz,     NumiFluxPpenergy, NumiFluxPpmedium, NumiFluxPtype,    NumiFluxPpvx,
             NumiFluxPpvy,     NumiFluxPpvz,     NumiFluxMuparpx,  NumiFluxMuparpy,  NumiFluxMuparpz,  NumiFluxMupare,
             NumiFluxNecm,     NumiFluxNimpwt,   NumiFluxXpoint,   NumiFluxYpoint,   NumiFluxZpoint,   NumiFluxTvx,
             NumiFluxTvy,      NumiFluxTvz,      NumiFluxTpx,      NumiFluxTpy,      NumiFluxTpz,      NumiFluxTptype,
             NumiFluxTgen,     NumiFluxTgptype,  NumiFluxTgppx,    NumiFluxTgppy,    NumiFluxTgppz,    NumiFluxTprivx,
             NumiFluxTprivy,   NumiFluxTprivz,   NumiFluxBeamx,    NumiFluxBeamy,    NumiFluxBeamz,    NumiFluxBeampx,
             NumiFluxBeampy,   NumiFluxBeampz});
      }
    }
    return reader;
  }

  void factory<sand::genie_reader>::check_gRooTracker_format() {
    if (input_tree->GetBranch("NuParentPdg")) {
      reader.nuParents_ = std::vector<NuParent>{};
    } else {
      UFW_DEBUG("NuParent branches missing — skipping NuParent setup.");
    }

    if (input_tree->GetBranch("NumiFluxRun")) {
      reader.numiFluxes_ = std::vector<NumiFlux>{};
    } else {
      UFW_DEBUG("NumiFlux branches missing — skipping NumiFlux setup.");
    }
  }

  void factory<sand::genie_reader>::populate_spills_boundaries() {
    // Get spill boundaries from EDepSimEvents tree using InteractionNumber from TG4PrimaryVertex.
    // Each spill in EDepSimEvents contains multiple primary vertices, and each vertex has an
    // InteractionNumber that corresponds to the entry index in gRooTracker.

    UFW_DEBUG("Looking for EDepSimEvents tree...");
    auto* edep_tree = input_file->Get<TTree>("EDepSimEvents");
    if (edep_tree == nullptr) {
      UFW_ERROR("EDepSimEvents tree not found in file. Cannot determine spill boundaries.");
      return;
    }
    UFW_DEBUG("Found EDepSimEvents with {} entries", edep_tree->GetEntries());

    TG4Event* event = new TG4Event();
    edep_tree->SetBranchAddress("Event", &event);
    UFW_DEBUG("Branch address set");

    const Long64_t n_spills = edep_tree->GetEntries();
    spills_boundaries.reserve(n_spills);

    for (Long64_t spill = 0; spill < n_spills; spill++) {
      edep_tree->GetEntry(spill);

      if (event->Primaries.empty()) {
        UFW_WARN("Spill {} has no primary vertices", spill);
        continue;
      }

      // Get first and last InteractionNumber for this spill
      const Long64_t first_idx = event->Primaries.front().GetInteractionNumber();
      const Long64_t last_idx  = event->Primaries.back().GetInteractionNumber();

      // Boundaries are [first, last+1) to match the original convention
      spills_boundaries.emplace_back(first_idx, last_idx + 1);
    }

    delete event;
    edep_tree->ResetBranchAddresses();

    UFW_DEBUG("Found {} spills from EDepSimEvents", spills_boundaries.size());
  }

  void factory<sand::genie_reader>::attach_branches() {
    auto set = [&](const char* b, auto* addr) {
      if (const auto br = input_tree->GetBranch(b)) {
        br->SetAddress(addr);
      } else
        UFW_WARN("Missing expected branch '{}'", b);
    };

    set("EvtNum", &EvtNum);
    set("EvtXSec", &EvtXSec);
    set("EvtDXSec", &EvtDXSec);
    set("EvtKPS", &EvtKPS);
    set("EvtWght", &EvtWght);
    set("EvtProb", &EvtProb);
    set("EvtVtx", &EvtVtx);
    set("EvtCode", &EvtCode);

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

    if (reader.nuParents_.has_value()) {
      set("NuParentPdg", &NuParentPdg);
      set("NuParentDecMode", &NuParentDecMode);
      set("NuParentDecP4", &NuParentDecP4);
      set("NuParentDecX4", &NuParentDecX4);
      set("NuParentProP4", &NuParentProP4);
      set("NuParentProX4", &NuParentProX4);
      set("NuParentProNVtx", &NuParentProNVtx);
    }

    if (reader.numiFluxes_.has_value()) {
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
    }
  }

  void factory<sand::genie_reader>::clear_reader(const ufw::context_id i) {
    reader.events_.clear();
    reader.stdHeps_.clear();
    if (reader.nuParents_.has_value()) {
      reader.nuParents_->clear();
    }
    if (reader.numiFluxes_.has_value()) {
      reader.numiFluxes_->clear();
    }

    const auto spill_size = spills_boundaries[i].second - spills_boundaries[i].first;
    reader.events_.reserve(spill_size);
    reader.stdHeps_.reserve(spill_size);
    if (reader.nuParents_.has_value()) {
      reader.nuParents_->reserve(spill_size);
    }
    if (reader.numiFluxes_.has_value()) {
      reader.numiFluxes_->reserve(spill_size);
    }
  }

} // namespace ufw::data
