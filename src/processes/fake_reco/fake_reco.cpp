//
// Created by paolo on 08/05/2025.
//

#include "fake_reco.hpp"

#include "caf_handlers/interactions.hpp"
#include "caf_handlers/particles.hpp"

#include <edep_reader/edep_reader.hpp>
#include <genie_reader/genie_reader.hpp>

#include <ufw/factory.hpp>

#include <duneanaobj/StandardRecord/StandardRecord.h>

#include <utility>
#include <vector>

namespace sand::fake_reco {

  // Returns a vector of pairs (interactions_begin_index, interaction_size) referred to elements of tree
  // childen_trajectories_
  std::vector<std::pair<std::size_t, std::size_t>> make_edep_interaction_map(const EDEPTree& tree) {
    std::vector<std::pair<std::size_t, std::size_t>> output{};

    std::size_t current_spill_index       = tree.GetChildrenTrajectories()[0].GetInteractionNumber();
    std::size_t current_spill_begin_index = 0;
    for (std::size_t i = 0; i < tree.GetChildrenTrajectories().size(); i++) {
      if (tree.GetChildrenTrajectories()[i].GetInteractionNumber() == current_spill_index) {
        continue;
      }

      output.emplace_back(current_spill_begin_index, i - current_spill_begin_index);
      current_spill_index       = tree.GetChildrenTrajectories()[i].GetInteractionNumber();
      current_spill_begin_index = i;
    }
    return output;
  }

  fake_reco::fake_reco() : process{{}, {}} {}

  void fake_reco::configure(const ufw::config& cfg) { process::configure(cfg); }

  void fake_reco::run() {
    // Read input
    const auto& edep  = get<edep_reader>();
    const auto& genie = get<genie_reader>();

    const auto edep_map = make_edep_interaction_map(edep);

    const auto& event            = edep.event();
    const auto& primary_vertices = event.Primaries;

    // Initialize spill structures
    caf::StandardRecord standard_record{};

    auto& truth_branch             = standard_record.mc;
    auto& true_interactions_vector = truth_branch.nu;
    true_interactions_vector.reserve(primary_vertices.size());
    truth_branch.nnu = primary_vertices.size();

    auto& reco_branch                       = standard_record.nd.sand;
    auto& reconstructed_interactions_vector = reco_branch.ixn;
    reconstructed_interactions_vector.reserve(primary_vertices.size());
    reco_branch.nixn = primary_vertices.size();

    for (std::size_t event_index = 0; event_index < primary_vertices.size(); event_index++) {
      const auto& vertex = primary_vertices[event_index];
      UFW_DEBUG("Processing vertex number {} that points to gRooTracker index: {}", &vertex - primary_vertices.data(),
                vertex.InteractionNumber);
      // Fill the interactions with nu data
      caf::SRTrueInteraction& true_interaction = true_interactions_vector.emplace_back();
      caf::SRInteraction reco_interaction      = empty_interaction_from_vertex(edep);

      initialize_SRTrueInteraction(true_interaction, genie.events_[event_index], genie.stdHeps_[event_index]);

      // Fill the interactions with daughter particles data
      int stable_particle_index = 0; // This will be the G4ID of the true particles if they are a final state
      for (std::size_t particle_index = 0; particle_index < genie.stdHeps_[event_index].N_; particle_index++) {
        if (genie.stdHeps_[event_index].Status_[particle_index] != ::genie::EGHepStatus::kIStStableFinalState
            && genie.stdHeps_[event_index].Status_[particle_index] != ::genie::EGHepStatus::kIStHadronInTheNucleus) {
          continue;
        }

        if (genie.stdHeps_[event_index].Pdg_[particle_index] == 2000000101) {
          // Skip GENIE "bindino"
          // https://github.com/DUNE/ND_CAFMaker/blob/972f11bc5b69ea1f595e14ed16e09162f512011e/src/truth/FillTruth.cxx#L223
          continue;
        }

        if (genie.stdHeps_[event_index].Status_[particle_index] == ::genie::EGHepStatus::kIStHadronInTheNucleus) {
          caf::SRTrueParticle true_particle = SRTrueParticle_from_genie(particle_index, genie.stdHeps_[event_index]);
          true_particle.G4ID =
              genie.stdHeps_[event_index].Status_[particle_index] == ::genie::EGHepStatus::kIStStableFinalState
                  ? stable_particle_index++
                  : -1;
          true_particle.interaction_id = true_interaction.id;
        }
      }

      // for (const auto& particle : edep) {
      //   caf::SRRecoParticle recoParticle = reco_particle_from_edep_trajectory(particle);
      //   fill_sr_interaction(reco_interaction, recoParticle);
      //
      //   caf::SRTrueParticle true_particle = true_particle_from_edep_trajectory(particle);
      //   fill_sr_true_interaction(true_interaction, true_particle);
      //
      // }
    }

    UFW_ASSERT(true_interactions_vector.size() == truth_branch.nnu,
               "truth_branch.nnu doesn't match truth_branch.nu size");
    UFW_ASSERT(reconstructed_interactions_vector.size() == reco_branch.nixn,
               "reco_branch.nixn doesn't match reco_branch.ixn size");
  }
} // namespace sand::fake_reco

UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::fake_reco::fake_reco);
