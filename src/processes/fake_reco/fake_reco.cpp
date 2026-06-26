#include "fake_reco.hpp"

#include "caf_handlers/caf_filler.hpp"

#include <ufw/factory.hpp>

#include <numeric>

namespace sand {

  fake_reco::fake_reco()
    : process{{{"in_truth", "sand::caf::truth_branch_wrapper"}}, {{"output_caf", "sand::caf::standard_record_wrapper"}}} {}

  void fake_reco::configure(const ufw::config& cfg) { process::configure(cfg); }

  void fake_reco::run() {
    auto const& truth_branch = get<sand::caf::truth_branch_wrapper>("in_truth");
    m_caf                    = &set<sand::caf::standard_record_wrapper>("output_caf");

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
      auto sum_mom =
          std::accumulate(reco_ixn.part.sandreco.begin(), reco_ixn.part.sandreco.end(), std::make_tuple(0.f, 0.f, 0.f),
                          [](auto acc, const auto& part) {
                            auto [px, py, pz] = acc;
                            return std::make_tuple(px + part.p.x, py + part.p.y, pz + part.p.z);
                          });

      auto [sum_px, sum_py, sum_pz] = sum_mom;
      reco_ixn.dir.part_mom_sum     = normalize_to_direction(sum_px, sum_py, sum_pz);
    }

    assert_sizes();
  }

  void fake_reco::process_interaction_particles(::caf::SRTrueInteraction& true_ixn, ::caf::SRInteraction& reco_ixn,
                                                ::caf::SRSANDInt& sand_ixn) const {
    const auto nprim = static_cast<std::size_t>(true_ixn.nprim);
    reco_ixn.part.sandreco.reserve(nprim);

    for (std::size_t i{}; i != nprim; ++i) {
      const auto& true_prim = true_ixn.prim[i];
      const auto& prim_id   = true_prim.ancestor_id;

      // Create SRRecoParticle from truth
      auto reco_part = CAFFiller<::caf::SRRecoParticle>::from_true(true_prim, prim_id);
      reco_ixn.part.sandreco.push_back(std::move(reco_part));
      reco_ixn.part.nsandreco++;

      // Create SRTrack or SRShower based on particle type
      if (is_track_like(true_prim.pdg)) {
        auto track = CAFFiller<::caf::SRTrack>::from_true(true_prim, prim_id);
        sand_ixn.tracks.push_back(std::move(track));
        sand_ixn.ntracks++;
      } else if (is_shower_like(true_prim.pdg)) {
        auto shower = CAFFiller<::caf::SRShower>::from_true(true_prim, prim_id);
        sand_ixn.showers.push_back(std::move(shower));
        sand_ixn.nshowers++;
      } else {
        UFW_DEBUG("Particle PDG {} is neither track-like nor shower-like, skipping reco object", true_prim.pdg);
      }
    }
  }

  void fake_reco::assert_sizes() const {
    // Truth branch
    UFW_ASSERT(m_caf->mc.nu.size() == m_caf->mc.nnu,
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
