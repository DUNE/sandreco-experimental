#include <TFile.h>
#include <TTree.h>

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
    // Find gRooTracker tree
    const auto path = cfg.path_at("uri");
    input_file.reset(TFile::Open(path.c_str()));
    input_tree = input_file->Get<TTree>("gRooTracker");
    if (!input_tree) {
      UFW_DEBUG("gRooTracker tree not found in the root directory of file '{}'.", path.c_str());
      UFW_DEBUG("Trying the edepsim path: {}/DetSimPassThru/gRooTracker.", path.c_str());

      input_tree = input_file->Get<TTree>("DetSimPassThru/gRooTracker");
      if (!input_tree) {
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

      reader.events_.push_back({EvtNum, EvtXSec, EvtDXSec, EvtKPS, EvtWght, EvtProb, EvtVtx, EvtCode, EvtFlags});

      reader.stdHeps_.push_back({StdHepN, StdHepPdg, StdHepStatus, StdHepRescat, StdHepX4, StdHepP4, StdHepPolz,
                                 StdHepFd, StdHepLd, StdHepFm, StdHepLm});

      if (reader.nuParents_.has_value()) {
        reader.nuParents_->push_back({NuParentPdg, NuParentDecMode, NuParentDecP4, NuParentDecX4, NuParentProP4,
                                      NuParentProX4, NuParentProNVtx});
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
    input_tree->ResetBranchAddresses();

    // In the gRooTracker format, the end of a spill si marked by an event with just one invalid particle.
    const auto StdHepN_branch = input_tree->GetBranch("StdHepN");
    if (StdHepN_branch == nullptr) {
      UFW_ERROR("Missing required branch 'StdHepN' in gRooTracker");
    }
    StdHepN_branch->SetAddress(&StdHepN);

    std::vector<Long64_t> separator_indexes{};
    for (Long64_t i{0}; i < input_tree->GetEntries(); i++) {
      input_tree->GetEntry(i);
      if (StdHepN == 1) {
        separator_indexes.push_back(i);
      }
    }

    spills_boundaries.reserve(separator_indexes.size());
    spills_boundaries.emplace_back(0, separator_indexes[0]);
    for (std::size_t i = 1; i < separator_indexes.size(); i++) {
      spills_boundaries.emplace_back(separator_indexes[i - 1] + 1, separator_indexes[i]);
    }
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
    reader.nuParents_->clear();
    reader.numiFluxes_->clear();

    const auto spill_size = spills_boundaries[i].second - spills_boundaries[i].first;
    reader.events_.reserve(spill_size);
    reader.stdHeps_.reserve(spill_size);
    reader.nuParents_->reserve(spill_size);
    reader.numiFluxes_->reserve(spill_size);
  }

} // namespace ufw::data