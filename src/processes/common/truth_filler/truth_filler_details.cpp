#include "truth_filler_details.hpp"
#include "evtcode_parser.hpp"

#include <TDatabasePDG.h>

namespace sand::mctruth {

  [[nodiscard]] bool is_lepton_pdg(int pdg) {
    if (auto* p = TDatabasePDG::Instance()->GetParticle(pdg)) {
      char const* pclass = p->ParticleClass();
      return pclass && std::string_view{pclass} == "Lepton";
    }
    return false;
  }

  bool is_darkneutrino_pdg(int pdg) {
    const int abs_pdg = std::abs(pdg);
    return abs_pdg == 2000030000;
  }

  int find_final_lepton(StdHep const& stdhep) {
    const auto daughters = stdhep.daughters_indexes_of_part(static_cast<int>(StdHepIndex::nu));
    if (daughters.empty()) {
      UFW_ERROR("Nu produced no daughters");
      return -1;
    }
    if (daughters.size() != 1) {
      UFW_ERROR("Nu produced {} leptons, expected 1", daughters.size());
    }
    if (!is_lepton_pdg(stdhep.Pdg_[daughters[0]]) && !is_darkneutrino_pdg(stdhep.Pdg_[daughters[0]])) {
      UFW_ERROR("Nu didn't produce a lepton, PDG: {}", stdhep.Pdg_[daughters[0]]);
    }
    return daughters[0];
  }

  Kinematics calculate_kinematics(sand::mom_4d const& nu_p4, sand::mom_4d const& lep_p4) {
    auto const nucleon_mass = static_cast<float>(TDatabasePDG::Instance()->GetParticle(2212)->Mass());
    auto q                  = nu_p4 - lep_p4;

    Kinematics k{};
    k.Q2   = static_cast<float>(-q.M2());
    k.q0   = static_cast<float>(q.E());
    k.modq = static_cast<float>(q.P());
    k.W    = std::sqrt(nucleon_mass * nucleon_mass + 2.0f * k.q0 * nucleon_mass + static_cast<float>(q.M2()));

    const auto Enu = static_cast<float>(nu_p4.E());
    k.bjorkenX     = k.Q2 / (2.0f * nucleon_mass * k.q0);
    k.inelasticity = k.q0 / Enu;

    return k;
  }

  ::caf::SRTrueInteraction true_interaction_from_genie(GRooTrackerEvent const& event, StdHep const& stdhep) {
    ::caf::SRTrueInteraction ixn{};

    auto summary = parse_evt_code(event.EvtCode_);

    ixn.id        = event.EvtNum_;
    ixn.genieIdx  = event.EvtNum_;
    ixn.pdg       = summary.probe_pdg;
    ixn.pdgorig   = ixn.pdg;
    ixn.iscc      = summary.interaction_type == "Weak[CC]";
    ixn.mode      = summary.scattering_type;
    ixn.targetPDG = summary.target_pdg;
    ixn.hitnuc    = summary.hit_nucleon_pdg.value_or(0);

    ixn.vtx.x     = static_cast<float>(event.EvtVtx_[0]);
    ixn.vtx.y     = static_cast<float>(event.EvtVtx_[1]);
    ixn.vtx.z     = static_cast<float>(event.EvtVtx_[2]);
    ixn.time      = static_cast<float>(event.EvtVtx_[3]);
    ixn.isvtxcont = true;

    const auto& nu_p4 = stdhep.P4_[static_cast<int>(StdHepIndex::nu)];
    ixn.E             = static_cast<float>(nu_p4.E());
    ixn.momentum.x    = static_cast<float>(nu_p4.Px());
    ixn.momentum.y    = static_cast<float>(nu_p4.Py());
    ixn.momentum.z    = static_cast<float>(nu_p4.Pz());

    auto kinematics  = calculate_kinematics(nu_p4, stdhep.P4_.at(find_final_lepton(stdhep)));
    ixn.Q2           = kinematics.Q2;
    ixn.q0           = kinematics.q0;
    ixn.modq         = kinematics.modq;
    ixn.W            = kinematics.W;
    ixn.bjorkenX     = kinematics.bjorkenX;
    ixn.inelasticity = kinematics.inelasticity;

    ixn.ischarm    = summary.is_charm_event;
    ixn.isseaquark = summary.scattering_type == ::caf::kDIS && summary.hit_sea_quark;
    if (ixn.mode == ::caf::kRes && summary.resonance_type) {
      ixn.resnum = static_cast<int>(summary.resonance_type.value());
    }

    ixn.xsec       = static_cast<float>(event.EvtXSec_);
    ixn.genweight  = static_cast<float>(event.EvtWght_);
    ixn.generator  = ::caf::kGENIE;
    ixn.xsec_cvwgt = 1.0f;

    return ixn;
  }

} // namespace sand::mctruth
