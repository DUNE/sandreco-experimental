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
  // children_trajectories_
  std::vector<std::pair<std::size_t, std::size_t>> make_edep_interaction_map(const EDEPTree& tree,
                                                                             std::size_t spills_number) {
    std::vector<std::pair<std::size_t, std::size_t>> output{};
    output.reserve(spills_number);

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
    output.emplace_back(current_spill_begin_index, tree.GetChildrenTrajectories().size() - current_spill_begin_index);
    UFW_ASSERT(output.size() == spills_number,
               "Could not find the same number of spills in gRooTracker and EDepSimEvents");
    return output;
  }

  fake_reco::fake_reco() : process{{}, {}} {}

  void fake_reco::configure(const ufw::config& cfg) { process::configure(cfg); }

  void fake_reco::run() {
    // Read input
    const auto& edep  = get<edep_reader>();
    const auto& genie = get<genie_reader>();

    const auto edep_map = make_edep_interaction_map(edep, genie.events_.size());

    // Initialize spill structures
    caf::StandardRecord standard_record{};

    auto& truth_branch             = standard_record.mc;
    auto& true_interactions_vector = truth_branch.nu;
    true_interactions_vector.reserve(edep_map.size());
    truth_branch.nnu = edep_map.size();

    auto& reco_branch                       = standard_record.nd.sand;
    auto& reconstructed_interactions_vector = reco_branch.ixn;
    reconstructed_interactions_vector.reserve(edep_map.size());
    reco_branch.nixn = edep_map.size();

    // Loop over interactions (nu vertices)
    for (std::size_t interaction_index = 0; interaction_index < edep_map.size(); interaction_index++) {
      const auto [edep_first_index, edep_size] = edep_map[interaction_index];
      // Create empty interactions
      caf::SRTrueInteraction& true_interaction = true_interactions_vector.emplace_back();
      caf::SRInteraction reco_interaction      = empty_interaction_from_vertex(edep);

      // Fill the interactions with nu data
      initialize_SRTrueInteraction(true_interaction, genie.events_[interaction_index],
                                   genie.stdHeps_[interaction_index]);
      // true_interaction.id =
      true_interaction.genieIdx = genie.events_[interaction_index].EvtNum_;

      // Fill the interaction with the pre-FSI hadrons
      // Loop over GENIE particles
      for (std::size_t particle_index = 0; particle_index < genie.stdHeps_[interaction_index].N_; particle_index++) {
        if (genie.stdHeps_[interaction_index].Status_[particle_index] != ::genie::EGHepStatus::kIStHadronInTheNucleus) {
          continue;
        }
        if (genie.stdHeps_[interaction_index].Pdg_[particle_index] == 2000000101) {
          // Skip GENIE "bindino"
          // https://github.com/DUNE/ND_CAFMaker/blob/972f11bc5b69ea1f595e14ed16e09162f512011e/src/truth/FillTruth.cxx#L223
          continue;
        }
        caf::SRTrueParticle true_particle =
            SRTrueParticle_from_genie(particle_index, genie.stdHeps_[interaction_index]);
        true_particle.interaction_id = true_interaction.id;
        true_interaction.prefsi.push_back(true_particle);
        true_interaction.nprefsi++;
      }

      // Set the true_interaction vectors capacities
      true_interaction.prim.reserve(edep_size);
      const std::size_t secondary_n =
          std::accumulate(edep.GetChildrenTrajectories().data() + edep_first_index,
                          edep.GetChildrenTrajectories().data() + edep_first_index + edep_size, 0,
                          [&](std::size_t acc, const auto primary_particle) -> std::size_t {
                            return acc
                                 + std::distance(++edep.GetTrajectory(primary_particle.GetId()),
                                                 edep.GetTrajectoryEnd(edep.GetTrajectory(primary_particle.GetId())));
                          });
      true_interaction.sec.reserve(secondary_n);
      // Fill the interaction with particles propagated by GEANT
      // Loop over edep tree particles
      for (std::size_t edep_index = edep_first_index; edep_index < edep_first_index + edep_size; edep_index++) {
        // Create the primary true particle
        const auto& primary_particle = edep.GetChildrenTrajectories()[edep_index];

        auto true_primary_particle           = SRTrueParticle_from_edep(primary_particle);
        true_primary_particle.interaction_id = true_interaction.id;
        true_primary_particle.ancestor_id    = {static_cast<int>(interaction_index), caf::TrueParticleID::kPrimary,
                                                true_interaction.nprim};

        for (auto it = ++edep.GetTrajectory(primary_particle.GetId());
             it != edep.GetTrajectoryEnd(edep.GetTrajectory(primary_particle.GetId())); ++it) {
          // Create the secondary true particles from this primary
          const auto& secondary_particle = *it;

          auto true_secondary_particle           = SRTrueParticle_from_edep(secondary_particle);
          true_secondary_particle.interaction_id = true_interaction.id;
          true_secondary_particle.ancestor_id    = {static_cast<int>(interaction_index), caf::TrueParticleID::kPrimary,
                                                    true_interaction.nprim};
          true_interaction.sec.push_back(true_secondary_particle);
          true_interaction.nsec++;
        }
        // Now add the primary to its vector, just to use nprim in the upper loop
        true_interaction.prim.push_back(true_primary_particle);
        true_interaction.nprim++;
      }
    }

    UFW_ASSERT(true_interactions_vector.size() == truth_branch.nnu,
               "truth_branch.nnu doesn't match truth_branch.nu size");
    UFW_ASSERT(reconstructed_interactions_vector.size() == reco_branch.nixn,
               "reco_branch.nixn doesn't match reco_branch.ixn size");
  }
} // namespace sand::fake_reco

UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::fake_reco::fake_reco);
