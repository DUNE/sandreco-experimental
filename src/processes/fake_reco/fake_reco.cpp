#include "fake_reco.hpp"

#include "caf_handlers/caf_filler.hpp"

#include <ufw/factory.hpp>

#include <numeric>

namespace sand {

  fake_reco::fake_reco()
    : process{{{"in_truth", "sand::caf::truth_branch_wrapper"}},
              {{"output_caf", "sand::caf::standard_record_wrapper"}}} {}

  void fake_reco::configure(const ufw::config& cfg) {
    process::configure(cfg);
    m_reco_mode           = cfg.value("mode", "truth");
    m_intrinsic_pos_res_t = cfg.value("intrinsic_pos_res_t", 0.);
    m_intrinsic_pos_res_l = cfg.value("intrinsic_pos_res_l", 0.);
    m_hit_energy_thr      = cfg.value("hit_energy_thr", 0.);
    m_b_field_magnitude   = cfg.value("b_field_magnitude", 0.);
  }

  void fake_reco::run() {
    auto const& truth_branch = get<sand::caf::truth_branch_wrapper>("in_truth");
    m_caf                    = &set<sand::caf::standard_record_wrapper>("output_caf");

    if (m_reco_mode == "smearing") {
      m_edep = &get<edep_reader>();
    }

    // Copy truth branch into output StandardRecord
    m_caf->mc = truth_branch;

    // Reserve reco capacities to match truth
    m_caf->common.ixn.sandreco.reserve(truth_branch.nu.size());
    m_caf->nd.sand.ixn.reserve(truth_branch.nu.size());

    for (std::size_t ixn_idx{}, n_nu = truth_branch.nu.size(); ixn_idx != n_nu; ++ixn_idx) {
      auto& true_ixn = m_caf->mc.nu[ixn_idx];

      // Create fake reco interaction (common branch)
      auto& reco_ixn = m_caf->common.ixn.sandreco.emplace_back(
          CAFFiller<::caf::SRInteraction>::from_true(true_ixn, static_cast<int>(ixn_idx)));
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
      reco_ixn.dir.part_mom_sum     = normalize_to_direction(sum_px, sum_py, sum_pz);
    }

    assert_sizes();
  }

  void fake_reco::fill_reco_objects(
      const std::function<::caf::SRRecoParticle(const ::caf::SRTrueParticle, const ::caf::TrueParticleID)>& make_reco,
      const ::caf::SRTrueParticle& true_part, const ::caf::TrueParticleID& part_id, bool is_primary,
      ::caf::SRInteraction& reco_ixn, ::caf::SRSANDInt& sand_ixn) const {
    auto reco_part    = make_reco(true_part, part_id);
    reco_part.primary = is_primary;
    reco_ixn.part.sandreco.push_back(std::move(reco_part));
    reco_ixn.part.nsandreco++;

    if (is_track_like(true_part.pdg)) {
      auto track = CAFFiller<::caf::SRTrack>::from_true(true_part, part_id);
      sand_ixn.tracker.tracks.push_back(std::move(track));
      sand_ixn.tracker.ntracks++;
    } else if (is_shower_like(true_part.pdg)) {
      auto shower = CAFFiller<::caf::SRShower>::from_true(true_part, part_id);
      sand_ixn.tracker.showers.push_back(std::move(shower));
      sand_ixn.tracker.nshowers++;
    } else {
      UFW_DEBUG("Particle PDG {} is neither track-like nor shower-like, skipping reco object", true_part.pdg);
    }
  }

  void fake_reco::process_interaction_particles(::caf::SRTrueInteraction& true_ixn, ::caf::SRInteraction& reco_ixn,
                                                ::caf::SRSANDInt& sand_ixn) const {
    const std::size_t prim_count = true_ixn.prim.size();
    const std::size_t sec_count  = true_ixn.sec.size();
    reco_ixn.part.sandreco.reserve(prim_count + sec_count);

    std::function<::caf::SRRecoParticle(const ::caf::SRTrueParticle&, const ::caf::TrueParticleID&)> make_reco;

    if (m_reco_mode == "truth") {
      UFW_INFO("Using reconstruction from truth");
      make_reco = [](const ::caf::SRTrueParticle& true_prim, const ::caf::TrueParticleID& prim_id) {
        return CAFFiller<::caf::SRRecoParticle>::from_true(true_prim, prim_id);
      };
    } else if (m_reco_mode == "smearing") {
      UFW_INFO("Using reconstruction from truth with smearing");
      make_reco = [this](const ::caf::SRTrueParticle& true_prim, const ::caf::TrueParticleID& prim_id) {
        const auto true_prim_trj = *m_edep->GetTrajectory(true_prim.G4ID);
        return CAFFiller<::caf::SRRecoParticle>::from_true_with_mu_smearing(
            true_prim, prim_id, true_prim_trj, m_intrinsic_pos_res_t, m_intrinsic_pos_res_l, m_hit_energy_thr,
            m_b_field_magnitude);
      };
    } else {
      UFW_ERROR("You need to specify which reco mode you want to use");
    }

    for (std::size_t i{}; i != prim_count; ++i) {
      const auto& true_prim = true_ixn.prim[i];
      fill_reco_objects(make_reco, true_prim, true_prim.ancestor_id, true, reco_ixn, sand_ixn);
    }

    for (std::size_t i{}; i != sec_count; ++i) {
      const auto& true_sec = true_ixn.sec[i];
      fill_reco_objects(make_reco, true_sec, true_sec.ancestor_id, false, reco_ixn, sand_ixn);
    }
  }

  void fake_reco::assert_sizes() const {
    UFW_ASSERT(m_caf->mc.nu.size() == m_caf->mc.nnu, "mc.nnu ({}) doesn't match mc.nu.size() ({})", m_caf->mc.nnu,
               m_caf->mc.nu.size());

    UFW_ASSERT(m_caf->common.ixn.sandreco.size() == static_cast<std::size_t>(m_caf->common.ixn.nsandreco),
               "common.ixn.nsandreco ({}) doesn't match common.ixn.sandreco.size() ({})", m_caf->common.ixn.nsandreco,
               m_caf->common.ixn.sandreco.size());

    UFW_ASSERT(m_caf->nd.sand.ixn.size() == static_cast<std::size_t>(m_caf->nd.sand.nixn),
               "nd.sand.nixn ({}) doesn't match nd.sand.ixn.size() ({})", m_caf->nd.sand.nixn,
               m_caf->nd.sand.ixn.size());

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
      UFW_ASSERT(sand_ixn.tracker.tracks.size() == sand_ixn.tracker.ntracks,
                 "Interaction {}: ntracks ({}) doesn't match tracks.size() ({})", i, sand_ixn.tracker.ntracks,
                 sand_ixn.tracker.tracks.size());
      UFW_ASSERT(sand_ixn.tracker.showers.size() == sand_ixn.tracker.nshowers,
                 "Interaction {}: nshowers ({}) doesn't match showers.size() ({})", i, sand_ixn.tracker.nshowers,
                 sand_ixn.tracker.showers.size());
    }
  }

} // namespace sand

UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::fake_reco);
