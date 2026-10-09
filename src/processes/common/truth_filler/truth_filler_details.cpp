#include "truth_filler_details.hpp"
#include "evtcode_parser.hpp"

#include <sand.h>

#include <edep_reader_refactor/EDEPTrajectory.h>
#include <edep_reader_refactor/EDEPTree.h>
#include <genie_reader/GenieWrapper.h>

#include <duneanaobj/StandardRecord/SRTrueInteraction.h>
#include <duneanaobj/StandardRecord/SRTrueParticle.h>

#include <TDatabasePDG.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
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
    };

    [[nodiscard]] Kinematics calculate_kinematics(sand::mom_4d const& nu_p4, sand::mom_4d const& lep_p4) {
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

      return k;
    }
  } // namespace

  bool is_darkneutrino_pdg(int pdg) {
    const int abs_pdg = std::abs(pdg);
    return abs_pdg == 2000030000;
  }

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

  bool is_lepton_pdg(int pdg) {
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

    ixn.id        = event.EvtIdx_;
    ixn.genieIdx  = event.EvtNum_;
    ixn.pdg       = summary.probe_pdg;
    ixn.pdgorig   = ixn.pdg;
    ixn.iscc      = summary.interaction_type == "Weak[CC]";
    ixn.mode      = summary.scattering_type;
    ixn.targetPDG = summary.target_pdg;
    ixn.hitnuc    = summary.hit_nucleon_pdg.value_or(0);

    constexpr double m_to_cm = 100.;
    ixn.vtx.x     = static_cast<float>(event.EvtVtx_[0] * m_to_cm);
    ixn.vtx.y     = static_cast<float>(event.EvtVtx_[1] * m_to_cm);
    ixn.vtx.z     = static_cast<float>(event.EvtVtx_[2] * m_to_cm);
    ixn.time      = static_cast<float>(event.EvtVtx_[3]);
    ixn.isvtxcont = false;

    const auto& nu_p4 = stdhep.P4_[static_cast<int>(StdHepIndex::nu)];
    UFW_DEBUG("nu_p4.E(): {}", nu_p4.E());
    
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
    // gRooTracker stores the cross section in 1e-38 cm2; CAF (GENIE XSec()) wants GeV^-2
    constexpr double xsec_1e38cm2_to_GeVm2 = 1e-38 / 3.893793e-28;   // 1 GeV^-2 = 0.3893793 mb
    ixn.xsec       = static_cast<float>(event.EvtXSec_ * xsec_1e38cm2_to_GeVm2);
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
                                                TrueParticleTree& out) {
      auto const ordered_slot = static_cast<int>(out.sec.size());
      ::caf::TrueParticleID const my_id{sr_ixn, caf::TrueParticleID::kSecondary, ordered_slot};

      out.sec.emplace_back(true_particle_from_edep(traj, interaction_id, ancestor_id));
      out.sec[ordered_slot].parentID = parent_id;

      auto const& children = traj.GetChildrenTrajectories();
      std::vector<caf::TrueParticleID> dids;
      std::vector<unsigned int> g4ids;
      dids.reserve(children.size());
      g4ids.reserve(children.size());

      std::transform(children.begin(), children.end(), std::back_inserter(dids), [&](auto const& child) {
        g4ids.push_back(static_cast<unsigned int>(child.GetId()));
        return add_secondary_subtree(child, my_id, ancestor_id, sr_ixn, interaction_id, out);
      });

      out.sec[ordered_slot].daughters   = std::move(g4ids);
      out.sec[ordered_slot].daughtersID = std::move(dids);
      return my_id;
    }
  } // namespace

  [[nodiscard]] TrueParticleTree build_true_particle_tree(Primaries const& primaries, std::size_t first_idx,
                                                          std::size_t count, int sr_ixn, long int interaction_id) {
    TrueParticleTree out_tree;

    out_tree.prim.reserve(count);
    auto begin = primaries.begin() + first_idx;
    auto end   = primaries.begin() + first_idx + count;
    auto total = std::accumulate(begin, end, 0, [&](int acc, auto const& p) { return acc + subtree_node_count(p); });

    out_tree.sec.reserve(static_cast<std::size_t>(total) - count);

    for (std::size_t i{}; i != count; ++i) {
      auto const& prim = primaries[first_idx + i];
      ::caf::TrueParticleID const prim_id{sr_ixn, caf::TrueParticleID::kPrimary, static_cast<int>(i)};

      out_tree.prim.emplace_back(true_particle_from_edep(prim, interaction_id, prim_id));

      auto const& children = prim.GetChildrenTrajectories();
      std::vector<caf::TrueParticleID> dids;
      std::vector<unsigned int> g4ids;
      dids.reserve(children.size());
      g4ids.reserve(children.size());

      std::transform(children.begin(), children.end(), std::back_inserter(dids), [&](auto const& child) {
        g4ids.push_back(static_cast<unsigned int>(child.GetId()));
        return add_secondary_subtree(child, prim_id, prim_id, sr_ixn, interaction_id, out_tree);
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

    void to_local_track_ids(TrueParticleTree& tree) {
    if (tree.prim.empty())
      return;

    auto const by_g4id = [](auto const& a, auto const& b) { return a.G4ID < b.G4ID; };

    // primaries and secondaries of one interaction are two contiguous blocks of spill-wide ids
    auto const nprim   = static_cast<int>(tree.prim.size());
    auto const prim_lo = std::min_element(tree.prim.begin(), tree.prim.end(), by_g4id)->G4ID;
    auto const prim_hi = std::max_element(tree.prim.begin(), tree.prim.end(), by_g4id)->G4ID;
    if (prim_hi - prim_lo + 1 != nprim)
      UFW_ERROR("Primary track ids [{}, {}] are not contiguous for {} primaries", prim_lo, prim_hi, nprim);

    auto const nsec = static_cast<int>(tree.sec.size());
    int sec_lo      = 0;
    if (nsec > 0) {
      sec_lo            = std::min_element(tree.sec.begin(), tree.sec.end(), by_g4id)->G4ID;
      auto const sec_hi = std::max_element(tree.sec.begin(), tree.sec.end(), by_g4id)->G4ID;
      if (sec_hi - sec_lo + 1 != nsec)
        UFW_ERROR("Secondary track ids [{}, {}] are not contiguous for {} secondaries", sec_lo, sec_hi, nsec);
    }

    // spill-wide id -> per-interaction id (primaries 0..nprim-1, then secondaries), as in ND_CAFMaker
    auto const to_local = [=](int g) {
      if (g < 0)
        return g;
      if (g >= prim_lo && g < prim_lo + nprim)
        return g - prim_lo;
      if (nsec > 0 && g >= sec_lo && g < sec_lo + nsec)
        return nprim + g - sec_lo;
      UFW_ERROR("Track id {} does not belong to this interaction", g);
    };

    for (auto* particles : {&tree.prim, &tree.sec}) {
      for (auto& p : *particles) {
        p.G4ID   = to_local(p.G4ID);
        p.parent = to_local(p.parent);
        for (auto& d : p.daughters)
          d = static_cast<unsigned int>(to_local(static_cast<int>(d)));
      }
    }
  }


} // namespace sand::common::filler_details
