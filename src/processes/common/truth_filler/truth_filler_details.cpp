#include "truth_filler_details.hpp"
#include "evtcode_parser.hpp"

#include <edep_reader/EDEPTrajectory.h>

#include <TDatabasePDG.h>

#include <algorithm>
#include <cmath>
#include <string_view>

namespace sand::common::filler_details {

  namespace {
    [[nodiscard]] ::caf::TrueParticleID make_primary_id(long int ixn_id, int index) {
      return {static_cast<int>(ixn_id), ::caf::TrueParticleID::kPrimary, index};
    }

    struct Kinematics {
      float Q2{};
      float q0{};
      float modq{};
      float W{};
      float bjorkenX{};
      float inelasticity{};
    };

    [[nodiscard]] Kinematics calculate_kinematics(sand::mom_4d const& nu_p4, sand::mom_4d const& lep_p4) {
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
  } // namespace

  std::vector<InteractionRange> make_interaction_ranges(Primaries const& primaries) {
    std::vector<InteractionRange> output;

    for (auto it = primaries.begin(); it != primaries.end();) {
      auto group_end = std::find_if_not(it, primaries.end(), [ixn = it->GetInteractionNumber()](auto const& p) {
        return p.GetInteractionNumber() == ixn;
      });
      output.push_back({static_cast<std::size_t>(it - primaries.begin()), static_cast<std::size_t>(group_end - it)});
      it = group_end;
    }

    return output;
  }

  [[nodiscard]] bool is_lepton_pdg(int pdg) {
    if (auto* p = TDatabasePDG::Instance()->GetParticle(pdg)) {
      char const* pclass = p->ParticleClass();
      return pclass && std::string_view{pclass} == "Lepton";
    }
    return false;
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

  ::caf::SRTrueParticle true_particle_from_genie(std::size_t index, StdHep const& stdhep, long int ixn_id) {
    ::caf::SRTrueParticle p{};

    p.pdg            = stdhep.Pdg_[index];
    p.G4ID           = -1;
    p.interaction_id = ixn_id;

    const auto& p4 = stdhep.P4_[index];
    p.p.px         = p4.Px();
    p.p.py         = p4.Py();
    p.p.pz         = p4.Pz();
    p.p.E          = p4.E();

    return p;
  }

  std::vector<::caf::SRTrueParticle> make_prefsi_particles(StdHep const& stdhep, long int ixn_id) {
    std::vector<::caf::SRTrueParticle> particles;

    for (int i{}; i != stdhep.N_; ++i) {
      if (stdhep.Status_[i] != genie::kIStHadronInTheNucleus) {
        continue;
      }
      if (is_bindino_pdg(stdhep.Pdg_[i])) {
        continue;
      }
      particles.push_back(true_particle_from_genie(static_cast<std::size_t>(i), stdhep, ixn_id));
    }

    return particles;
  }

  ::caf::SRTrueParticle true_particle_from_edep(EDEPTrajectory const& traj, long int ixn_id,
                                                ::caf::TrueParticleID const& ancestor_id) {
    ::caf::SRTrueParticle p{};

    p.pdg            = traj.GetPDGCode();
    p.G4ID           = traj.GetId();
    p.interaction_id = ixn_id;
    p.ancestor_id    = ancestor_id;
    p.parent         = traj.GetParentId();

    auto const mom = traj.GetInitialMomentum();
    p.p.px         = static_cast<float>(mom.Px() * 1e-3); // MeV → GeV
    p.p.py         = static_cast<float>(mom.Py() * 1e-3);
    p.p.pz         = static_cast<float>(mom.Pz() * 1e-3);
    p.p.E          = static_cast<float>(mom.E() * 1e-3);

    if (auto const points = traj.GetTrajectoryPointsVect(); !points.empty()) {
      auto const& first = points.front();
      auto const& last  = points.back();

      p.start_pos.x = static_cast<float>(first.GetPosition().X() * 0.1); // mm → cm
      p.start_pos.y = static_cast<float>(first.GetPosition().Y() * 0.1);
      p.start_pos.z = static_cast<float>(first.GetPosition().Z() * 0.1);
      p.end_pos.x   = static_cast<float>(last.GetPosition().X() * 0.1);
      p.end_pos.y   = static_cast<float>(last.GetPosition().Y() * 0.1);
      p.end_pos.z   = static_cast<float>(last.GetPosition().Z() * 0.1);

      p.time = first.GetPosition().T();

      p.first_process    = static_cast<unsigned int>(first.GetProcess());
      p.first_subprocess = static_cast<unsigned int>(first.GetSubprocess());
      p.end_process      = static_cast<unsigned int>(last.GetProcess());
      p.end_subprocess   = static_cast<unsigned int>(last.GetSubprocess());
    }

    return p;
  }

  PrimariesResult make_primaries(Primaries const& primaries, std::size_t first_idx, std::size_t count,
                                 long int ixn_id) {
    PrimariesResult result;
    result.particles.reserve(count);
    result.ancestor_ids.reserve(count);

    for (std::size_t i{}; i != count; ++i) {
      const auto part_id = make_primary_id(ixn_id, static_cast<int>(i));
      result.ancestor_ids.push_back(part_id);
      auto const& p = result.particles.emplace_back(true_particle_from_edep(primaries[first_idx + i], ixn_id, part_id));
      switch (p.pdg) {
      case 2212:
        ++result.nproton;
        break;
      case 2112:
        ++result.nneutron;
        break;
      case 211:
        ++result.npip;
        break;
      case -211:
        ++result.npim;
        break;
      case 111:
        ++result.npi0;
        break;
      default:
        break;
      }
    }

    return result;
  }

} // namespace sand::common::filler_details
