#include "fake_reco.hpp"

#include "caf_handlers/caf_filler.hpp"
#include "utils/smearer.hpp"

#include <ufw/factory.hpp>

#include <numeric>
#include <vector>

namespace sand {

  fake_reco::fake_reco()
    : process{{}, {{"output_caf", "sand::caf::caf_wrapper"}}}, m_edep{nullptr}, m_genie{nullptr}, m_caf{nullptr} {}

  void fake_reco::configure(const ufw::config& cfg) {
    process::configure(cfg);
    m_reco_mode = cfg.value("mode", "truth");
    m_intrinsic_pos_res_t = cfg.value("intrinsic_pos_res_t", 0.);
    m_intrinsic_pos_res_l = cfg.value("intrinsic_pos_res_l", 0.);
    m_hit_energy_thr = cfg.value("hit_energy_thr", 0.);
    m_b_field_magnitude = cfg.value("b_field_magnitude", 0.);
  }

  void fake_reco::run() {
    // Bind input readers and output writer
    m_edep = &get<edep_reader>();
    if (m_edep == nullptr) {
      UFW_ERROR("EDepSim reader not found");
      return;
    }

    m_genie = &get<genie_reader>();
    if (m_genie == nullptr) {
      UFW_ERROR("GENIE reader not found");
      return;
    }

    m_caf = &set<sand::caf::caf_wrapper>("output_caf");

    const auto& primaries = m_edep->GetChildrenTrajectories();
    const auto edep_map   = make_edep_interaction_map();

    // Initialize spill-level structures
    initialize_spill_capacities();

    // Loop over interactions (neutrino vertices)
    for (std::size_t ixn_idx{}, edep_map_size = edep_map.size(); ixn_idx != edep_map_size; ++ixn_idx) {
      const auto& [first_prim_idx, prim_count] = edep_map[ixn_idx];
      const auto& event                        = m_genie->events_[ixn_idx];
      const auto& stdhep                       = m_genie->stdHeps_[ixn_idx];

      // TRUTH
      // Create and fill SRTrueInteraction from GENIE
      auto& true_ixn = m_caf->mc.nu.emplace_back(CAFFiller<::caf::SRTrueInteraction>::from_genie(event, stdhep));

      // Add pre-FSI hadrons from GENIE StdHep
      CAFFiller<::caf::SRTrueInteraction>::add_prefsi(true_ixn, stdhep);

      // Reserve capacity for particles based on edep-sim data
      true_ixn.prim.reserve(prim_count);

      // Count secondaries for reservation
      const auto* first_prim_ptr = primaries.data() + first_prim_idx;
      const auto* last_prim_ptr  = primaries.data() + first_prim_idx + prim_count;
      const std::size_t sec_count =
          std::accumulate(first_prim_ptr, last_prim_ptr, std::size_t{0}, [this](std::size_t acc, const auto& prim) {
            auto prim_it   = m_edep->GetTrajectory(prim.GetId());
            auto sec_begin = std::next(prim_it);
            auto sec_end   = m_edep->GetTrajectoryEnd(prim_it);
            return acc + static_cast<std::size_t>(std::distance(sec_begin, sec_end));
          });
      true_ixn.sec.reserve(sec_count);

      // Add primaries from edep-sim (returns ancestor IDs for secondaries)
      auto ancestor_ids =
          CAFFiller<::caf::SRTrueInteraction>::add_primaries(true_ixn, primaries, first_prim_idx, prim_count);

      // Add secondaries for each primary
      for (std::size_t i{}; i != prim_count; ++i) {
        const auto& prim = primaries[first_prim_idx + i];
        auto prim_it     = m_edep->GetTrajectory(prim.GetId());
        auto sec_begin   = std::next(prim_it);
        auto sec_end     = m_edep->GetTrajectoryEnd(prim_it);

        CAFFiller<::caf::SRTrueInteraction>::add_secondaries(true_ixn, sec_begin, sec_end, ancestor_ids[i]);
      }

      // FAKE RECONSTRUCTION
      // Create fake reco interaction (common branch)
      auto& reco_ixn =
          m_caf->common.ixn.sandreco.emplace_back(CAFFiller<::caf::SRInteraction>::from_true(true_ixn, ixn_idx));
      m_caf->common.ixn.nsandreco++;

      // Create SAND-specific reco interaction
      auto& sand_ixn = m_caf->nd.sand.ixn.emplace_back();
      m_caf->nd.sand.nixn++;

      // Process particles: create SRRecoParticle, SRTrack, SRShower
      process_interaction_particles(true_ixn, reco_ixn, sand_ixn);

      // Compute direction from sum of particle momenta
      auto sum_mom = std::accumulate(reco_ixn.part.sandreco.begin(), reco_ixn.part.sandreco.end(),
                                     std::make_tuple(0.f, 0.f, 0.f), [](auto acc, const auto& part) {
                                       auto [px, py, pz] = acc;
                                       return std::make_tuple(px + part.p.x, py + part.p.y, pz + part.p.z);
                                     });

      auto [sum_px, sum_py, sum_pz] = sum_mom;

      reco_ixn.dir.part_mom_sum = normalize_to_direction(sum_px, sum_py, sum_pz);
    }

    assert_sizes();
  }

  std::vector<EdepInteractionRange> fake_reco::make_edep_interaction_map() const {
    const auto& primaries = m_edep->GetChildrenTrajectories();
    if (primaries.empty()) {
      UFW_WARN("No primary trajectories found in EDepSim event");
      return {};
    }

    std::vector<EdepInteractionRange> output;
    output.reserve(m_genie->events_.size());

    std::size_t current_ixn = primaries[0].GetInteractionNumber();
    std::size_t range_begin{};

    for (std::size_t i{1}, primaries_size = primaries.size(); i != primaries_size; ++i) {
      if (primaries[i].GetInteractionNumber() != current_ixn) {
        output.push_back({range_begin, i - range_begin});
        current_ixn = primaries[i].GetInteractionNumber();
        range_begin = i;
      }
    }
    output.push_back({range_begin, primaries.size() - range_begin});

    UFW_ASSERT(output.size() == m_genie->events_.size(),
               "Mismatch between edep-sim interactions ({}) and GENIE events ({})", output.size(),
               m_genie->events_.size());

    return output;
  }

  void fake_reco::initialize_spill_capacities() {
    const std::size_t n_interactions = m_genie->events_.size();

    // Truth branch
    m_caf->mc.nnu = static_cast<int>(n_interactions);
    m_caf->mc.nu.reserve(n_interactions);

    // Common reco branch
    m_caf->common.ixn.sandreco.reserve(n_interactions);

    // SAND-specific reco branch
    m_caf->nd.sand.ixn.reserve(n_interactions);
  }


  void fake_reco::fill_reco_objects(const std::function<::caf::SRRecoParticle(const ::caf::SRTrueParticle, const ::caf::TrueParticleID)>& make_reco,
                                    const ::caf::SRTrueParticle &true_part, const ::caf::TrueParticleID &part_id, 
                                    const bool is_primary,::caf::SRInteraction& reco_ixn, ::caf::SRSANDInt& sand_ixn) const {
    // Create SRRecoParticle from truth
    auto reco_part = make_reco(true_part, part_id);
    reco_part.primary = is_primary;
    reco_ixn.part.sandreco.push_back(std::move(reco_part));
    reco_ixn.part.nsandreco++;

    // Create SRTrack or SRShower based on particle type
    if (is_track_like(true_part.pdg)) {
      auto track = CAFFiller<::caf::SRTrack>::from_true(true_part, part_id);
      sand_ixn.tracks.push_back(std::move(track));
      sand_ixn.ntracks++;
    } else if (is_shower_like(true_part.pdg)) {
      auto shower = CAFFiller<::caf::SRShower>::from_true(true_part, part_id);
      sand_ixn.showers.push_back(std::move(shower));
      sand_ixn.nshowers++;
    } else {
      UFW_DEBUG("Particle PDG {} is neither track-like nor shower-like, skipping reco object", true_part.pdg);
    }
  }

  void fake_reco::process_interaction_particles(::caf::SRTrueInteraction& true_ixn, ::caf::SRInteraction& reco_ixn,
                                                ::caf::SRSANDInt& sand_ixn) const {
    // Reserve space for reco objects
    const std::size_t prim_count = true_ixn.prim.size();
    const std::size_t sec_count = true_ixn.sec.size();
    reco_ixn.part.sandreco.reserve((prim_count+sec_count));

    std::function<::caf::SRRecoParticle(const ::caf::SRTrueParticle&, const ::caf::TrueParticleID&)> make_reco;

    // Fill reco particle from truth or adding a smearing to muons momentum
    if (m_reco_mode == "truth") {
      UFW_INFO("Using reconstruction from truth");
      make_reco = [](const ::caf::SRTrueParticle& true_prim, const ::caf::TrueParticleID& prim_id) {
        return CAFFiller<::caf::SRRecoParticle>::from_true(true_prim, prim_id);
      };
    } else if (m_reco_mode == "smearing") {
      UFW_INFO("Using reconstruction from truth with smearing");
      make_reco = [this](const ::caf::SRTrueParticle& true_prim, const ::caf::TrueParticleID& prim_id) {
        const auto true_prim_trj = *m_edep->GetTrajectory(true_prim.G4ID);
        return CAFFiller<::caf::SRRecoParticle>::from_true_with_mu_smearing(true_prim, prim_id, true_prim_trj, m_intrinsic_pos_res_t, m_intrinsic_pos_res_l, m_hit_energy_thr, m_b_field_magnitude);
      };
    } else {
      UFW_ERROR("You need to specify which reco mode you want to use");
    }

    // Loop over primary particles
    for (std::size_t i{}; i != prim_count; ++i) {
      const auto& true_prim = true_ixn.prim[i];
      const auto& prim_id   = true_prim.ancestor_id;

      fill_reco_objects(make_reco, true_prim, prim_id, true, reco_ixn, sand_ixn);
    }

    // Loop over secondary particles
    for(std::size_t i{}; i !=sec_count; ++i){
      const auto& true_sec  = true_ixn.sec[i];
      const auto& sec_id    = true_sec.ancestor_id;
      
      fill_reco_objects(make_reco, true_sec, sec_id, false, reco_ixn, sand_ixn);
    }
  }

  void fake_reco::assert_sizes() const {
    // Truth branch
    UFW_ASSERT(m_caf->mc.nu.size() == static_cast<std::size_t>(m_caf->mc.nnu),
               "mc.nnu ({}) doesn't match mc.nu.size() ({})", m_caf->mc.nnu, m_caf->mc.nu.size());

    // Common reco branch
    UFW_ASSERT(m_caf->common.ixn.sandreco.size() == static_cast<std::size_t>(m_caf->common.ixn.nsandreco),
               "common.ixn.nsandreco ({}) doesn't match common.ixn.sandreco.size() ({})", m_caf->common.ixn.nsandreco,
               m_caf->common.ixn.sandreco.size());

    // SAND reco branch
    UFW_ASSERT(m_caf->nd.sand.ixn.size() == static_cast<std::size_t>(m_caf->nd.sand.nixn),
               "nd.sand.nixn ({}) doesn't match nd.sand.ixn.size() ({})", m_caf->nd.sand.nixn,
               m_caf->nd.sand.ixn.size());

    // Per-interaction checks
    for (std::size_t i{}, mc_nu_size = m_caf->mc.nu.size(); i != mc_nu_size; ++i) {
      const auto& true_ixn = m_caf->mc.nu[i];
      UFW_ASSERT(true_ixn.prim.size() == static_cast<std::size_t>(true_ixn.nprim),
                 "Interaction {}: nprim ({}) doesn't match prim.size() ({})", i, true_ixn.nprim, true_ixn.prim.size());
      UFW_ASSERT(true_ixn.sec.size() == static_cast<std::size_t>(true_ixn.nsec),
                 "Interaction {}: nsec ({}) doesn't match sec.size() ({})", i, true_ixn.nsec, true_ixn.sec.size());
      UFW_ASSERT(true_ixn.prefsi.size() == static_cast<std::size_t>(true_ixn.nprefsi),
                 "Interaction {}: nprefsi ({}) doesn't match prefsi.size() ({})", i, true_ixn.nprefsi,
                 true_ixn.prefsi.size());
    }

    for (std::size_t i{}, common_ixn_size = m_caf->common.ixn.sandreco.size(); i != common_ixn_size; ++i) {
      const auto& reco_ixn = m_caf->common.ixn.sandreco[i];
      UFW_ASSERT(reco_ixn.part.sandreco.size() == static_cast<std::size_t>(reco_ixn.part.nsandreco),
                 "Interaction {}: part.nsandreco ({}) doesn't match part.sandreco.size() ({})", i,
                 reco_ixn.part.nsandreco, reco_ixn.part.sandreco.size());
    }

    for (std::size_t i{}, nd_ixn_size = m_caf->nd.sand.ixn.size(); i != nd_ixn_size; ++i) {
      const auto& sand_ixn = m_caf->nd.sand.ixn[i];
      UFW_ASSERT(sand_ixn.tracks.size() == sand_ixn.ntracks,
                 "Interaction {}: ntracks ({}) doesn't match tracks.size() ({})", i, sand_ixn.ntracks,
                 sand_ixn.tracks.size());
      UFW_ASSERT(sand_ixn.showers.size() == sand_ixn.nshowers,
                 "Interaction {}: nshowers ({}) doesn't match showers.size() ({})", i, sand_ixn.nshowers,
                 sand_ixn.showers.size());
    }
  }

} // namespace sand

UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::fake_reco);
