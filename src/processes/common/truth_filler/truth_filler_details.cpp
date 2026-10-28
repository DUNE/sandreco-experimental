#include "truth_filler_details.hpp"

#include "evtcode_parser.hpp"
#include "g4_id_map.hpp"

#include <sand.h>

#include <edep_reader/EDEPTrajectory.h>
#include <edep_reader/EDEPTree.h>
#include <genie_reader/GenieWrapper.h>

#include <duneanaobj/StandardRecord/SRTrueInteraction.h>
#include <duneanaobj/StandardRecord/SRTrueParticle.h>

#include <TDatabasePDG.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <numeric>
#include <string_view>
#include <vector>

namespace sand::common::filler_details {

  namespace {
    struct Kinematics {
      float Q2{};
      float q0{};
      float modq{};
      float W{};
      float bjorkenX{};
      float inelasticity{};
      float t{std::numeric_limits<float>::quiet_NaN()}; ///< only for coherent events, see calculate_kinematics
    };

    /// The t GENIE stores as the selected t of a coherent pion event (what ND_CAFMaker reads with Kine().t(true)).
    ///
    /// In the Berger-Sehgal and Rein-Sehgal COH models GENIE samples Q2, y and t, computes x = Q2 / (2 M_A nu) with
    /// the nucleus mass M_A, and then builds the pion (E_pi = nu) at the angle to q given by t and x, but using the
    /// nucleon mass m_N in place of M_A. That is, it uses Q2' = Q2 m_N / M_A instead of the real Q2, so the final state
    /// has |(q - p_pi)^2| != t. Inverting that construction gives back the sampled t from the final state.
    /// In the other COH models (Berger-Sehgal FM, Alvarez-Ruso) E_pi != nu and GENIE stores t = |(q - p_pi)^2|.
    [[nodiscard]] double genie_coh_t(sand::mom_4d const& q, sand::mom_4d const& pion_p4, double target_mass) {
      const double t_final_state = std::abs((q - pion_p4).M2());
      const double nu            = q.E();
      if (std::abs(nu - pion_p4.E()) > 1e-6 * nu) {
        return t_final_state;
      }

      constexpr double nucleon_mass = 0.9389187; // genie::constants::kNucleonMass, average of proton and neutron
      const double nu2              = nu * nu;
      const double Q2               = -q.M2();
      const double Q2p              = Q2 * nucleon_mass / target_mass;
      const double mpi2             = pion_p4.M2();
      return 2 * nu2 + Q2p - mpi2 - std::sqrt((nu2 + Q2p) / (nu2 + Q2)) * (2 * nu2 + Q2 - mpi2 - t_final_state);
    }

    /// @param pion_p4      final-state pion of a coherent event. When given, also computes t, the squared
    ///                     four-momentum transfer to the nucleus, as GENIE stores it (see genie_coh_t).
    /// @param target_mass  mass of the target nucleus, needed for t.
    [[nodiscard]] Kinematics calculate_kinematics(sand::mom_4d const& nu_p4, sand::mom_4d const& lep_p4,
                                                  sand::mom_4d const* pion_p4 = nullptr, double target_mass = 0) {
      // auto const nucleon_mass = static_cast<float>(TDatabasePDG::Instance()->GetParticle(2212)->Mass());
      auto const nucleon_mass = 0.939;
      auto q                  = nu_p4 - lep_p4;

      Kinematics k{};
      k.Q2   = static_cast<float>(-q.M2());
      k.q0   = static_cast<float>(q.E());
      k.modq = static_cast<float>(q.P());
      k.W    = std::sqrt(nucleon_mass * nucleon_mass + 2.0f * k.q0 * nucleon_mass + static_cast<float>(q.M2()));

      const auto Enu = static_cast<float>(nu_p4.E());
      k.bjorkenX     = k.Q2 / (2.0f * nucleon_mass * k.q0);
      k.inelasticity = k.q0 / Enu;

      if (pion_p4 != nullptr) {
        k.t = static_cast<float>(genie_coh_t(q, *pion_p4, target_mass));
      }

      return k;
    }
  } // namespace

  bool is_darkneutrino_pdg(int pdg) {
    const int abs_pdg = std::abs(pdg);
    return abs_pdg == 2000030000;
  }

  bool is_lepton_pdg(int pdg) {
    if (auto* p = TDatabasePDG::Instance()->GetParticle(pdg)) {
      char const* pclass = p->ParticleClass();
      return pclass && std::string_view{pclass} == "Lepton";
    }
    return false;
  }

  /// @return index (in `stdhep`) of the stable final-state pion of a coherent event, -1 if there is none.
  static int find_final_pion(StdHep const& stdhep) {
    for (int i{}; i != stdhep.N_; ++i) {
      const int abs_pdg = std::abs(stdhep.Pdg_[i]);
      if (stdhep.Status_[i] == genie::kIStStableFinalState && (abs_pdg == 211 || abs_pdg == 111)) {
        return i;
      }
    }
    return -1;
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

    ixn.id        = event.EvtIdx_;
    ixn.genieIdx  = event.EvtNum_;
    ixn.pdg       = summary.probe_pdg;
    ixn.pdgorig   = ixn.pdg;
    ixn.iscc      = summary.interaction_type == "Weak[CC]";
    ixn.mode      = summary.scattering_type;
    ixn.targetPDG = summary.target_pdg;
    ixn.hitnuc    = summary.hit_nucleon_pdg.value_or(0);

    constexpr double m_to_cm = 100.;
    ixn.vtx.x                = static_cast<float>(event.EvtVtx_[0] * m_to_cm);
    ixn.vtx.y                = static_cast<float>(event.EvtVtx_[1] * m_to_cm);
    ixn.vtx.z                = static_cast<float>(event.EvtVtx_[2] * m_to_cm);
    ixn.time                 = static_cast<float>(event.EvtVtx_[3]);
    ixn.isvtxcont            = false;

    const auto& nu_p4 = stdhep.P4_[static_cast<int>(StdHepIndex::nu)];
    UFW_DEBUG("nu_p4.E(): {}", nu_p4.E());

    ixn.E          = static_cast<float>(nu_p4.E());
    ixn.momentum.x = static_cast<float>(nu_p4.Px());
    ixn.momentum.y = static_cast<float>(nu_p4.Py());
    ixn.momentum.z = static_cast<float>(nu_p4.Pz());

    // t is defined for coherent events only (as in ND_CAFMaker); it needs the final-state pion
    const int pion_idx = ixn.mode == ::caf::kCoh ? find_final_pion(stdhep) : -1;
    if (ixn.mode == ::caf::kCoh && pion_idx < 0) {
      UFW_WARN("Coherent event {} has no final-state pion: t not filled", ixn.id);
    }
    const double target_mass = stdhep.P4_[static_cast<int>(StdHepIndex::tgt)].M();
    auto kinematics          = calculate_kinematics(nu_p4, stdhep.P4_.at(find_final_lepton(stdhep)),
                                           pion_idx >= 0 ? &stdhep.P4_.at(pion_idx) : nullptr, target_mass);
    ixn.Q2                   = kinematics.Q2;
    ixn.q0                   = kinematics.q0;
    ixn.modq                 = kinematics.modq;
    ixn.W                    = kinematics.W;
    ixn.bjorkenX             = kinematics.bjorkenX;
    ixn.inelasticity         = kinematics.inelasticity;
    if (pion_idx >= 0) {
      ixn.t = kinematics.t;
    }

    ixn.ischarm    = summary.is_charm_event;
    ixn.isseaquark = summary.scattering_type == ::caf::kDIS && summary.hit_sea_quark;
    if (ixn.mode == ::caf::kRes && summary.resonance_type) {
      ixn.resnum = static_cast<int>(summary.resonance_type.value());
    }

    ixn.xsec      = static_cast<float>(event.EvtXSec_);
    ixn.genweight = static_cast<float>(event.EvtWght_);
    // gRooTracker stores the cross section in 1e-38 cm2; CAF (GENIE XSec()) wants GeV^-2
    constexpr double xsec_1e38cm2_to_GeVm2 = 1e-38 / 3.893793e-28; // 1 GeV^-2 = 0.3893793 mb
    ixn.xsec                               = static_cast<float>(event.EvtXSec_ * xsec_1e38cm2_to_GeVm2);
    ixn.xsec_cvwgt                         = 1.0f;

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
                                                ::caf::TrueParticleID const& ancestor_id, G4IdMap const& ids) {
    ::caf::SRTrueParticle p{};

    p.pdg            = traj.GetPDGCode();
    p.G4ID           = ids.to_interaction_id(traj.GetId());
    p.interaction_id = ixn_id;
    p.ancestor_id    = ancestor_id;
    p.parent         = ids.to_interaction_id(traj.GetParentId());

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

  int subtree_node_count(EDEPTrajectory const& t) {
    return std::accumulate(t.GetChildrenTrajectories().begin(), t.GetChildrenTrajectories().end(), 1,
                           [](int acc, EDEPTrajectory const& c) { return acc + subtree_node_count(c); });
  }

  namespace {
    /// Recursively appends `traj`'s subtree to `out.sec` in pre-order, cross-linking each
    /// node to its parent (`parentID`), root primary (`ancestor_id`) and children (`daughtersID`).
    /// @return this node's own TrueParticleID, so the caller can link it as a parent/daughter.
    ::caf::TrueParticleID add_secondary_subtree(EDEPTrajectory const& traj, ::caf::TrueParticleID parent_id,
                                                ::caf::TrueParticleID ancestor_id, int sr_ixn, long int interaction_id,
                                                G4IdMap const& ids, TrueParticleTree& out) {
      auto const ordered_slot = static_cast<int>(out.sec.size());
      ::caf::TrueParticleID const my_id{sr_ixn, caf::TrueParticleID::kSecondary, ordered_slot};

      out.sec.emplace_back(true_particle_from_edep(traj, interaction_id, ancestor_id, ids));
      out.sec[ordered_slot].parentID = parent_id;

      auto const& children = traj.GetChildrenTrajectories();
      std::vector<caf::TrueParticleID> dids;
      std::vector<unsigned int> g4ids;
      dids.reserve(children.size());
      g4ids.reserve(children.size());

      std::transform(children.begin(), children.end(), std::back_inserter(dids), [&](auto const& child) {
        g4ids.push_back(static_cast<unsigned int>(ids.to_interaction_id(child.GetId())));
        return add_secondary_subtree(child, my_id, ancestor_id, sr_ixn, interaction_id, ids, out);
      });

      out.sec[ordered_slot].daughters   = std::move(g4ids);
      out.sec[ordered_slot].daughtersID = std::move(dids);
      return my_id;
    }
  } // namespace

  [[nodiscard]] TrueParticleTree build_true_particle_tree(Primaries const& primaries, std::size_t first_idx,
                                                          std::size_t count, int sr_ixn, long int interaction_id) {
    TrueParticleTree out_tree;
    G4IdMap const ids{primaries, {first_idx, count}};

    out_tree.prim.reserve(count);
    auto begin = primaries.begin() + first_idx;
    auto end   = primaries.begin() + first_idx + count;
    auto total = std::accumulate(begin, end, 0, [&](int acc, auto const& p) { return acc + subtree_node_count(p); });

    out_tree.sec.reserve(static_cast<std::size_t>(total) - count);

    for (std::size_t i{}; i != count; ++i) {
      auto const& prim = primaries[first_idx + i];
      ::caf::TrueParticleID const prim_id{sr_ixn, caf::TrueParticleID::kPrimary, static_cast<int>(i)};

      out_tree.prim.emplace_back(true_particle_from_edep(prim, interaction_id, prim_id, ids));

      auto const& children = prim.GetChildrenTrajectories();
      std::vector<caf::TrueParticleID> dids;
      std::vector<unsigned int> g4ids;
      dids.reserve(children.size());
      g4ids.reserve(children.size());

      std::transform(children.begin(), children.end(), std::back_inserter(dids), [&](auto const& child) {
        g4ids.push_back(static_cast<unsigned int>(ids.to_interaction_id(child.GetId())));
        return add_secondary_subtree(child, prim_id, prim_id, sr_ixn, interaction_id, ids, out_tree);
      });

      out_tree.prim[i].daughters   = std::move(g4ids);
      out_tree.prim[i].daughtersID = std::move(dids);

      // Count only primaries
      // clang-format off
      switch (out_tree.prim[i].pdg) {
        case  2212: ++out_tree.nproton;  break;
        case  2112: ++out_tree.nneutron; break;
        case   211: ++out_tree.npip;     break;
        case  -211: ++out_tree.npim;     break;
        case   111: ++out_tree.npi0;     break;
        default: break;
      }
      // clang-format on
    }

    return out_tree;
  }

} // namespace sand::common::filler_details
