//
// Created by Paolo Forni on 27/09/2025.
//

#ifndef SR_INTERACTIONS_HANDLERS_HPP
#define SR_INTERACTIONS_HANDLERS_HPP

#include "../genie_helpers/EvtCode_parser.hpp"

#include <edep_reader/EDEPTree.h>
#include <genie_reader/GenieWrapper.h>

#include <duneanaobj/StandardRecord/SRInteraction.h>
#include <duneanaobj/StandardRecord/SRTrueInteraction.h>

#include <Math/Vector4D.h>

#include <cmath>

namespace sand::fake_reco {
  inline caf::SRInteraction empty_interaction_from_vertex(const EDEPTree& vertex) {
    caf::SRInteraction interaction{};
    // TODO: get the id from genie
    // interaction.id = ...;
    // FIXME: waiting for the GeoManager
    // interaction.vtx =
    // caf::SRVector3D{vertex.GetTrajectoryPointsVect()[0].GetPosition().Vect()};
    return interaction;
  }

  inline void fill_sr_interaction(caf::SRInteraction& interaction, const caf::SRRecoParticle& particle) {
    interaction.part.sandreco.push_back(particle);
    interaction.part.nsandreco++;
  }

  inline void initialize_SRTrueInteraction(caf::SRTrueInteraction& interaction, const GRooTrackerEvent& genie_event,
                                           const StdHep& genie_stdhep) {
    // data got via  genie_stdhep.(...)[0].(...) are relative to the nu
    // data got via  genie_stdhep.(...)[1].(...) are relative to the target

    const std::string_view interaction_string = genie_event.EvtCode_->GetString().Data();
    const EventSummary summary{interaction_string};

    // interaction.id = ? // TODO: understand what goes here

    interaction.pdg = summary.probe_pdg;
    // fill this for similarity with FD, but no oscillations
    //  (comment from
    //  https://github.com/DUNE/ND_CAFMaker/blob/972f11bc5b69ea1f595e14ed16e09162f512011e/src/truth/FillTruth.cxx#L171C46-L171C102)
    interaction.pdgorig = interaction.pdg;

    interaction.iscc      = summary.interaction_type == "Weak[CC]";
    interaction.mode      = summary.scattering_type;
    interaction.targetPDG = summary.target_pdg;
    interaction.hitnuc    = summary.hit_nucleon_pdg.value_or(0);

    // todo: get this from Hugh G or somebody who will get it right
    //  (comment from
    //  https://github.com/DUNE/ND_CAFMaker/blob/972f11bc5b69ea1f595e14ed16e09162f512011e/src/truth/FillTruth.cxx#L178C5-L178C67)
    // interaction.removalE = ?

    const auto& nu_p4 = genie_stdhep.P4_[static_cast<int>(StdHepIndex::nu)];

    // both in GeV
    interaction.E          = static_cast<float>(nu_p4.E());
    interaction.momentum.x = static_cast<float>(nu_p4.Px());
    interaction.momentum.y = static_cast<float>(nu_p4.Py());
    interaction.momentum.z = static_cast<float>(nu_p4.Pz());

    const auto nu_daughters_indexes = genie_stdhep.daughters_indexes_of_part(static_cast<int>(StdHepIndex::nu));
    if (nu_daughters_indexes.size() != 1) {
      // not sure if this is the right way to address this problem (is this even possible?)
      UFW_ERROR("Nu produced more than one lepton, it produced {} leptons", nu_daughters_indexes.size());
    }
    if ((genie_stdhep.Pdg_[nu_daughters_indexes[0]] < 11 || genie_stdhep.Pdg_[nu_daughters_indexes[0]] > 16)
        && (genie_stdhep.Pdg_[nu_daughters_indexes[0]] > -11 || genie_stdhep.Pdg_[nu_daughters_indexes[0]] < -16)) {
      UFW_ERROR("Nu didn't produced a lepton, PDG code produced: {}", genie_stdhep.Pdg_[nu_daughters_indexes[0]]);
    }
    const auto& final_lepton_p4 = genie_stdhep.P4_[nu_daughters_indexes[0]];

    // true 4-momentum transfer
    const auto q         = nu_p4 - final_lepton_p4;
    constexpr float Mnuc = 0.939; // average nucleon mass

    interaction.Q2       = static_cast<float>(-q.M2());
    interaction.q0       = static_cast<float>(q.E());
    interaction.modq     = static_cast<float>(q.P());
    interaction.W        = static_cast<float>(std::sqrt(Mnuc * Mnuc + 2. * interaction.q0 * Mnuc + q.M2())); // "Wexp"
    interaction.bjorkenX = interaction.Q2 / (2 * Mnuc * interaction.q0);
    interaction.inelasticity = interaction.q0 / interaction.E;

    if (interaction.mode == caf::kCoh || interaction.mode == caf::kDiffractive) {
      // returns the running or selected value of invariant hadronic mass W
      // when not set returns -99999
      // https://github.com/GENIE-MC/Generator/blob/2084cc6b8f25a460ebf4afd6a4658143fa9ce2ff/src/Framework/Interaction/Kinematics.cxx#L170

      // interaction.t = in->Kine().t();
    }

    interaction.ischarm    = summary.is_charm_event;
    interaction.isseaquark = summary.scattering_type == caf::kDIS && summary.hit_sea_quark;
    if (interaction.mode == caf::kRes) {
      interaction.resnum = static_cast<int>(summary.resonance_type.value());
    }
    interaction.xsec      = static_cast<float>(genie_event.EvtXSec_);
    interaction.genweight = static_cast<float>(genie_event.EvtWght_);

    // Add DUNErw weights to the CAF
    // (comment from
    //  https://github.com/DUNE/ND_CAFMaker/blob/972f11bc5b69ea1f595e14ed16e09162f512011e/src/truth/FillTruth.cxx#L289)
    interaction.xsec_cvwgt = 1;
  }

  inline void fill_sr_true_interaction(caf::SRTrueInteraction& interaction, const caf::SRTrueParticle& particle) {
    if (particle.parent == -1) {
      interaction.prim.push_back(particle);
      interaction.nprim++;
    } else {
      interaction.sec.push_back(particle);
      interaction.nsec++;
    }
    // Should we consider e.g. pi0 100111, ... too?
    switch (particle.pdg) {
    case 2212:
      interaction.nproton++;
      break;
    case 2112:
      interaction.nneutron++;
      break;
    case 111:
      interaction.npi0++;
      break;
    case 211:
      interaction.npip++;
      break;
    default:
      break;
    }
  }
} // namespace sand::fake_reco
#endif // SR_INTERACTIONS_HANDLERS_HPP
