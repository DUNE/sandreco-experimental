#include "caf_filler.hpp"

#include "../genie_helpers/EvtCode_parser.hpp"

#include <cmath>

namespace sand {

  namespace {
    void fill_momentum(::caf::SRLorentzVector& p, double px, double py, double pz, double E) {
      p.px = static_cast<float>(px);
      p.py = static_cast<float>(py);
      p.pz = static_cast<float>(pz);
      p.E  = static_cast<float>(E);
    }

    void fill_position(::caf::SRVector3D& pos, double x, double y, double z) {
      pos.x = static_cast<float>(x);
      pos.y = static_cast<float>(y);
      pos.z = static_cast<float>(z);
    }

    struct Kinematics {
      float Q2;
      float q0;
      float modq;
      float W;
      float bjorkenX;
      float inelasticity;

      static Kinematics calculate(const ROOT::Math::PxPyPzEVector& nu_p4, const ROOT::Math::PxPyPzEVector& lep_p4) {
        const auto q = nu_p4 - lep_p4;

        Kinematics k{};
        k.Q2   = static_cast<float>(-q.M2());
        k.q0   = static_cast<float>(q.E());
        k.modq = static_cast<float>(q.P());
        k.W =
            static_cast<float>(std::sqrt(kNucleonMass_GeV * kNucleonMass_GeV + 2.0 * k.q0 * kNucleonMass_GeV + q.M2()));

        const float Enu = static_cast<float>(nu_p4.E());
        k.bjorkenX      = k.Q2 / (2.0f * kNucleonMass_GeV * k.q0);
        k.inelasticity  = k.q0 / Enu;

        return k;
      }
    };

    void fill_vertex(::caf::SRTrueInteraction& ixn, const GRooTrackerEvent& event) {
      ixn.vtx.x     = static_cast<float>(event.EvtVtx_[0]);
      ixn.vtx.y     = static_cast<float>(event.EvtVtx_[1]);
      ixn.vtx.z     = static_cast<float>(event.EvtVtx_[2]);
      ixn.time      = static_cast<float>(event.EvtVtx_[3]);
      ixn.isvtxcont = true;
    }

    void fill_neutrino_momentum(::caf::SRTrueInteraction& ixn, const ROOT::Math::PxPyPzEVector& nu_p4) {
      ixn.E          = static_cast<float>(nu_p4.E());
      ixn.momentum.x = static_cast<float>(nu_p4.Px());
      ixn.momentum.y = static_cast<float>(nu_p4.Py());
      ixn.momentum.z = static_cast<float>(nu_p4.Pz());
    }

    int find_final_lepton(const StdHep& stdhep) {
      const auto daughters = stdhep.daughters_indexes_of_part(static_cast<int>(StdHepIndex::nu));
      if (daughters.size() != 1) {
        UFW_ERROR("Nu produced {} leptons, expected 1", daughters.size());
      }
      if (!is_lepton_pdg(stdhep.Pdg_[daughters[0]])) {
        UFW_ERROR("Nu didn't produce a lepton, PDG: {}", stdhep.Pdg_[daughters[0]]);
      }
      return daughters[0];
    }
  } // namespace

  ::caf::SRTrueParticle CAFFiller<::caf::SRTrueParticle>::from_edep(const EDEPTrajectory& traj, long int interaction_id,
                                                                    const ::caf::TrueParticleID& ancestor_id) {
    ::caf::SRTrueParticle p{};

    p.pdg            = traj.GetPDGCode();
    p.G4ID           = traj.GetId();
    p.interaction_id = interaction_id;
    p.ancestor_id    = ancestor_id;
    p.parent         = traj.GetParentId();

    const auto mom = traj.GetInitialMomentum();
    fill_momentum(p.p, mom.Px(), mom.Py(), mom.Pz(), mom.E());

    if (const auto points = traj.GetTrajectoryPointsVect(); !points.empty()) {
      const auto& first = points.front();
      const auto& last  = points.back();

      fill_position(p.start_pos, first.GetPosition().X(), first.GetPosition().Y(), first.GetPosition().Z());
      fill_position(p.end_pos, last.GetPosition().X(), last.GetPosition().Y(), last.GetPosition().Z());

      p.time = first.GetPosition().T();

      p.first_process    = static_cast<unsigned int>(first.GetProcess());
      p.first_subprocess = static_cast<unsigned int>(first.GetSubprocess());
      p.end_process      = static_cast<unsigned int>(last.GetProcess());
      p.end_subprocess   = static_cast<unsigned int>(last.GetSubprocess());
    }

    return p;
  }

  ::caf::SRTrueParticle CAFFiller<::caf::SRTrueParticle>::from_genie(std::size_t index, const StdHep& stdhep,
                                                                     long int interaction_id) {
    ::caf::SRTrueParticle p{};

    p.pdg            = stdhep.Pdg_[index];
    p.G4ID           = -1;
    p.interaction_id = interaction_id;

    const auto& p4 = stdhep.P4_[index];
    fill_momentum(p.p, p4.Px(), p4.Py(), p4.Pz(), p4.E());

    return p;
  }

  ::caf::SRTrueInteraction CAFFiller<::caf::SRTrueInteraction>::from_genie(const GRooTrackerEvent& event,
                                                                           const StdHep& stdhep) {
    ::caf::SRTrueInteraction ixn{};

    const EventSummary summary{std::string_view{event.EvtCode_}};

    ixn.id        = event.EvtNum_;
    ixn.genieIdx  = event.EvtNum_;
    ixn.pdg       = summary.probe_pdg;
    ixn.pdgorig   = ixn.pdg;
    ixn.iscc      = summary.interaction_type == "Weak[CC]";
    ixn.mode      = summary.scattering_type;
    ixn.targetPDG = summary.target_pdg;
    ixn.hitnuc    = summary.hit_nucleon_pdg.value_or(0);

    fill_vertex(ixn, event);

    const auto& nu_p4 = stdhep.P4_[static_cast<int>(StdHepIndex::nu)];
    fill_neutrino_momentum(ixn, nu_p4);

    const auto kinematics = Kinematics::calculate(nu_p4, stdhep.P4_[find_final_lepton(stdhep)]);
    ixn.Q2                = kinematics.Q2;
    ixn.q0                = kinematics.q0;
    ixn.modq              = kinematics.modq;
    ixn.W                 = kinematics.W;
    ixn.bjorkenX          = kinematics.bjorkenX;
    ixn.inelasticity      = kinematics.inelasticity;

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

  std::vector<::caf::TrueParticleID>
  CAFFiller<::caf::SRTrueInteraction>::add_primaries(::caf::SRTrueInteraction& ixn,
                                                     const std::vector<EDEPTrajectory>& primaries,
                                                     std::size_t first_idx, std::size_t count) {
    std::vector<::caf::TrueParticleID> ancestor_ids;
    ancestor_ids.reserve(count);

    for (std::size_t i{}; i != count; ++i) {
      const auto& traj   = primaries[first_idx + i];
      const auto part_id = make_primary_id(ixn.id, static_cast<int>(ixn.prim.size()));

      ancestor_ids.push_back(part_id);

      // Pass part_id as ancestor_id so primaries know their own identity
      auto particle = CAFFiller<::caf::SRTrueParticle>::from_edep(traj, ixn.id, part_id);
      increment_particle_counter(ixn, particle.pdg);
      ixn.prim.push_back(std::move(particle));
    }

    ixn.nprim = static_cast<int>(ixn.prim.size());
    return ancestor_ids;
  }

  void CAFFiller<::caf::SRTrueInteraction>::add_secondaries(::caf::SRTrueInteraction& ixn,
                                                            EDEPTree::const_iterator begin,
                                                            EDEPTree::const_iterator end,
                                                            const ::caf::TrueParticleID& ancestor_id) {
    for (auto it = begin; it != end; ++it) {
      auto particle = CAFFiller<::caf::SRTrueParticle>::from_edep(*it, ixn.id, ancestor_id);
      increment_particle_counter(ixn, particle.pdg);
      ixn.sec.push_back(std::move(particle));
    }
    ixn.nsec = static_cast<int>(ixn.sec.size());
  }

  void CAFFiller<::caf::SRTrueInteraction>::add_prefsi(::caf::SRTrueInteraction& ixn, const StdHep& stdhep) {
    for (int i{}; i != stdhep.N_; ++i) {
      if (stdhep.Status_[i] != ::genie::kIStHadronInTheNucleus) {
        continue;
      }
      if (stdhep.Pdg_[i] == kBindinoPdg) {
        continue;
      }

      ixn.prefsi.push_back(CAFFiller<::caf::SRTrueParticle>::from_genie(static_cast<std::size_t>(i), stdhep, ixn.id));
    }
    ixn.nprefsi = static_cast<int>(ixn.prefsi.size());
  }

} // namespace sand
