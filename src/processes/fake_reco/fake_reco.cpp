#include "fake_reco.hpp"

#include "caf_handlers/interactions.hpp"
#include "caf_handlers/particles.hpp"
#include "caf_handlers/tracks_showers.hpp"
#include "caf_handlers/utils.hpp"

#include <edep_reader/edep_reader.hpp>
#include <genie_reader/genie_reader.hpp>

#include <ufw/factory.hpp>

#include <duneanaobj/StandardRecord/StandardRecord.h>

#include <utility>
#include <vector>

namespace sand::fake_reco {

  fake_reco::fake_reco() : process{{}, {{"output_caf", "sand::caf::caf_wrapper"}}} {}

  void fake_reco::configure(const ufw::config& cfg) { process::configure(cfg); }

  void fake_reco::run() {
    // Bind input and output
    edep_            = &get<edep_reader>();
    genie_           = &get<genie_reader>();
    standard_record_ = &set<sand::caf::caf_wrapper>("output_caf");

    const auto edep_map = make_edep_interaction_map();

    // Initialize spill structures
    set_branches_capacities_();

    // Loop over interactions (nu vertices)
    for (std::size_t interaction_index = 0; interaction_index < edep_map.size(); interaction_index++) {
      const auto [edep_first_index, edep_size] = edep_map[interaction_index];

      // Create empty interactions
      ::caf::SRTrueInteraction& true_interaction = standard_record_->mc.nu.emplace_back();

      // Fill the interactions with nu data
      initialize_SRTrueInteraction(true_interaction, genie_->events_[interaction_index],
                                   genie_->stdHeps_[interaction_index], genie_->events_[interaction_index].EvtVtx_);
      true_interaction.id = genie_->events_[interaction_index].EvtNum_; // Each ND experiment does whatever it wants
      true_interaction.genieIdx = genie_->events_[interaction_index].EvtNum_;

      fill_true_interaction_with_preFSI_hadrons_(true_interaction, interaction_index);

      set_true_interaction_vectors_capacities_(true_interaction, edep_first_index, edep_size);

      // Create fake reconstructed interaction (common branch)
      ::caf::SRInteraction& reco_interaction = standard_record_->common.ixn.sandreco.emplace_back();
      standard_record_->common.ixn.nsandreco++;

      // Create SAND-specific reconstructed interaction
      ::caf::SRSANDInt& sand_interaction = standard_record_->nd.sand.ixn.emplace_back();
      standard_record_->nd.sand.nixn++;

      // Initialize reco interaction from true interaction
      reco_interaction.id  = true_interaction.id;
      reco_interaction.vtx = true_interaction.vtx;

      // Link to true interaction (index in mc.nu vector)
      reco_interaction.truth.push_back(interaction_index);
      reco_interaction.truthOverlap.push_back(1.0f);

      // Fake reco: use true neutrino energy
      reco_interaction.Enu.calo = true_interaction.E;

      // Process all particles for this interaction
      process_interaction_particles_(true_interaction, reco_interaction, sand_interaction,
                                     interaction_index, edep_first_index, edep_size);

      // Compute direction from sum of particle momenta
      float sum_px = 0, sum_py = 0, sum_pz = 0;
      for (const auto& part : reco_interaction.part.sandreco) {
        sum_px += part.p.x;
        sum_py += part.p.y;
        sum_pz += part.p.z;
      }
      reco_interaction.dir.part_mom_sum = normalize_to_direction(sum_px, sum_py, sum_pz);
    }
    assert_sizes();
  }

  // Returns a vector of pairs (interactions_begin_index, interaction_size) referred to elements of tree
  // children_trajectories_
  std::vector<std::pair<std::size_t, std::size_t>> fake_reco::make_edep_interaction_map() const {
    std::vector<std::pair<std::size_t, std::size_t>> output{};
    output.reserve(genie_->events_.size());

    std::size_t current_event_index       = edep_->GetChildrenTrajectories()[0].GetInteractionNumber();
    std::size_t current_event_begin_index = 0;
    for (std::size_t i = 0; i < edep_->GetChildrenTrajectories().size(); i++) {
      if (edep_->GetChildrenTrajectories()[i].GetInteractionNumber() == current_event_index) {
        continue;
      }

      output.emplace_back(current_event_begin_index, i - current_event_begin_index);
      current_event_index       = edep_->GetChildrenTrajectories()[i].GetInteractionNumber();
      current_event_begin_index = i;
    }
    output.emplace_back(current_event_begin_index, edep_->GetChildrenTrajectories().size() - current_event_begin_index);
    UFW_ASSERT(output.size() == genie_->events_.size(),
               "Could not find the same number of spills in gRooTracker and EDepSimEvents");
    return output;
  }

  void fake_reco::set_branches_capacities_() const {
    auto& truth_branch = standard_record_->mc;
    truth_branch.nnu   = genie_->events_.size();
    truth_branch.nu.reserve(genie_->events_.size());

    // Don't set nsandreco yet - will be set when sandreco is actually populated
    standard_record_->common.ixn.sandreco.reserve(genie_->events_.size());

    // Don't set nixn yet - will be set when ixn is actually populated
    standard_record_->nd.sand.ixn.reserve(genie_->events_.size());
  }

  void fake_reco::fill_true_interaction_with_preFSI_hadrons_(::caf::SRTrueInteraction& true_interaction,
                                                             const std::size_t interaction_index) const {
    // Loop over GENIE particles
    for (std::size_t particle_index = 0; particle_index < genie_->stdHeps_[interaction_index].N_; particle_index++) {
      if (genie_->stdHeps_[interaction_index].Status_[particle_index] != ::genie::EGHepStatus::kIStHadronInTheNucleus) {
        continue;
      }
      if (genie_->stdHeps_[interaction_index].Pdg_[particle_index] == kBindinoPdg) {
        continue;
      }
      ::caf::SRTrueParticle true_particle =
          SRTrueParticle_from_genie(particle_index, genie_->stdHeps_[interaction_index]);
      true_particle.interaction_id = true_interaction.id;
      true_interaction.prefsi.push_back(true_particle);
      true_interaction.nprefsi++;
    }
  }

  void fake_reco::set_true_interaction_vectors_capacities_(::caf::SRTrueInteraction& true_interaction,
                                                           const std::size_t edep_first_index,
                                                           const std::size_t edep_size) const {
    true_interaction.prim.reserve(edep_size);

    const auto first_element_ptr = edep_->GetChildrenTrajectories().data() + edep_first_index;
    const auto last_element_ptr  = edep_->GetChildrenTrajectories().data() + edep_first_index + edep_size;

    const std::size_t secondary_n =
        std::accumulate(first_element_ptr, last_element_ptr, 0, [&](std::size_t acc, const auto& primary_particle) {
          auto subtree_vertex             = edep_->GetTrajectory(primary_particle.GetId());
          const auto last_subtree_element = edep_->GetTrajectoryEnd(subtree_vertex);
          // Start to count from the first subtree element
          return acc + std::distance(++subtree_vertex, last_subtree_element);
        });
    true_interaction.sec.reserve(secondary_n);
  }

  ::caf::SRTrueParticle fake_reco::SRTrueParticle_from_edep(const EDEPTrajectory& particle) const {
    const auto trajectory_points = particle.GetTrajectoryPointsVect();
    const auto mom = particle.GetInitialMomentum();

    // Handle case where trajectory has no points inside SAND
    // when the events are generated with the whole ND geometry
    if (trajectory_points.empty()) {

      UFW_DEBUG("Particle {} has no trajectory points inside SAND", particle.GetId());
      return {.pdg              = particle.GetPDGCode(),
              .G4ID             = particle.GetId(),
              .interaction_id   = {},
              .time             = 0.0f,
              .ancestor_id      = {},
              .p                = TLorentzVector{mom.Px(), mom.Py(), mom.Pz(), mom.E()},
              .start_pos        = {},
              .end_pos          = {},
              .parent           = particle.GetParentId(),
              .daughters        = {},
              .first_process    = 0,
              .first_subprocess = 0,
              .end_process      = 0,
              .end_subprocess   = 0};
    }
    auto output = ::caf::SRTrueParticle{
        .pdg  = particle.GetPDGCode(),
        .G4ID = particle.GetId(), // This is the GEANT trajectory id. The CafMaker reset this index at each interaction
        .interaction_id = {},     // This will be filled by fake_reco loop
        .time           = static_cast<float>(trajectory_points.front().GetPosition().T()), // ?
        .ancestor_id    = {}, // This will be filled by fake_reco loop
        .p              = TLorentzVector{mom.Px(), mom.Py(), mom.Pz(), mom.E()},
        .start_pos =
            ::caf::SRVector3D{
                static_cast<float>(trajectory_points.front().GetPosition().Vect().X()),
                static_cast<float>(trajectory_points.front().GetPosition().Vect().Y()),
                static_cast<float>(trajectory_points.front().GetPosition().Vect().Z())},
        .end_pos          = ::caf::SRVector3D{
            static_cast<float>(trajectory_points.back().GetPosition().Vect().X()),
            static_cast<float>(trajectory_points.back().GetPosition().Vect().Y()),
            static_cast<float>(trajectory_points.back().GetPosition().Vect().Z())},
        .parent           = particle.GetParentId(),
        .daughters        = {},
        .first_process    = static_cast<unsigned int>(trajectory_points.front().GetProcess()),
        .first_subprocess = static_cast<unsigned int>(trajectory_points.front().GetSubprocess()),
        .end_process      = static_cast<unsigned int>(trajectory_points.back().GetProcess()),
        .end_subprocess   = static_cast<unsigned int>(trajectory_points.back().GetSubprocess())};

    const auto first_daughter_it = ++edep_->GetTrajectory(particle.GetId());
    const auto last_daughter_it  = edep_->GetTrajectoryEnd(edep_->GetTrajectory(particle.GetId()));
    output.daughters.reserve(std::distance(first_daughter_it, last_daughter_it));
    for (auto it = first_daughter_it; it != last_daughter_it; ++it) {
      output.daughters.push_back(it->GetId());
    }

    return output;
  }

  void fake_reco::process_interaction_particles_(::caf::SRTrueInteraction& true_interaction,
                                                   ::caf::SRInteraction& reco_interaction,
                                                   ::caf::SRSANDInt& sand_interaction,
                                                   const std::size_t interaction_index,
                                                   const std::size_t edep_first_index,
                                                   const std::size_t edep_size) const {
    // Loop over edep tree particles (primaries from this interaction)
    for (std::size_t edep_index = edep_first_index; edep_index < edep_first_index + edep_size; edep_index++) {
      const auto& primary_particle = edep_->GetChildrenTrajectories()[edep_index];

      // Create the primary true particle
      auto true_primary_particle             = SRTrueParticle_from_edep(primary_particle);
      true_primary_particle.interaction_id   = true_interaction.id;
      true_primary_particle.ancestor_id.ixn  = static_cast<int>(interaction_index);
      true_primary_particle.ancestor_id.type = ::caf::TrueParticleID::kPrimary;
      true_primary_particle.ancestor_id.part = true_interaction.nprim;

      // Create secondary true particles from this primary
      for (auto it = ++edep_->GetTrajectory(primary_particle.GetId());
           it != edep_->GetTrajectoryEnd(edep_->GetTrajectory(primary_particle.GetId())); ++it) {
        const auto& secondary_particle = *it;

        auto true_secondary_particle             = SRTrueParticle_from_edep(secondary_particle);
        true_secondary_particle.interaction_id   = true_interaction.id;
        true_secondary_particle.ancestor_id.ixn  = static_cast<int>(interaction_index);
        true_secondary_particle.ancestor_id.type = ::caf::TrueParticleID::kPrimary;
        true_secondary_particle.ancestor_id.part = true_interaction.nprim;

        true_interaction.sec.push_back(true_secondary_particle);
        true_interaction.nsec++;
        update_true_interaction_pdg_counters(true_interaction, true_secondary_particle.pdg);
      }

      // Add the primary (after secondaries to use nprim in ancestor_id)
      true_interaction.prim.push_back(true_primary_particle);
      true_interaction.nprim++;
      update_true_interaction_pdg_counters(true_interaction, true_primary_particle.pdg);

      // === FAKE RECONSTRUCTION ===
      // Create SRRecoParticle and add to reco interaction
      auto reco_particle = SRRecoParticle_from_true_particle(true_primary_particle, true_primary_particle.ancestor_id);
      reco_interaction.part.sandreco.push_back(reco_particle);
      reco_interaction.part.nsandreco++;

      // Create SRTrack or SRShower based on particle type
      if (is_track_like(true_primary_particle.pdg)) {
        auto track = SRTrack_from_true_particle(true_primary_particle, true_primary_particle.ancestor_id);
        sand_interaction.tracks.push_back(track);
        sand_interaction.ntracks++;
      } else if (is_shower_like(true_primary_particle.pdg)) {
        auto shower = SRShower_from_true_particle(true_primary_particle, true_primary_particle.ancestor_id);
        sand_interaction.showers.push_back(shower);
        sand_interaction.nshowers++;
      }
    }
  }

  void fake_reco::assert_sizes() const {
    const auto& truth_branch = standard_record_->mc;
    const auto& [nu, nnu]    = truth_branch;

    const auto& common_branch = standard_record_->common.ixn;

    const auto& sand_branch = standard_record_->nd.sand;
    const auto& [nixn, ixn] = sand_branch;

    UFW_ASSERT(nu.size() == nnu, "truth_branch.nnu doesn't match truth_branch.nu size");
    UFW_ASSERT(common_branch.sandreco.size() == common_branch.nsandreco,
               "common_branch.nsandreco doesn't match common_branch.sandreco size");
    UFW_ASSERT(ixn.size() == sand_branch.nixn, "sand_branch.nixn doesn't match sand_branch.ixn size");
  }

} // namespace sand::fake_reco

UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::fake_reco::fake_reco);
