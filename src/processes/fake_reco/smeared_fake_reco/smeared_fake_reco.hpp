#ifndef SANDRECO_SMEARED_FAKE_RECO_HPP
#define SANDRECO_SMEARED_FAKE_RECO_HPP

#include <caf/caf_wrapper.hpp>
#include <edep_reader/edep_reader.hpp>
#include <genie_reader/genie_reader.hpp>

#include <ufw/process.hpp>

#include <duneanaobj/StandardRecord/SRInteraction.h>
#include <duneanaobj/StandardRecord/SRSAND.h>
#include <duneanaobj/StandardRecord/SRTrueInteraction.h>

#include <vector>

namespace sand {

  struct EdepInteractionRange {
    std::size_t first_primary_index; ///< Index of the first primary particle in the interaction
    std::size_t primary_count;       ///< Number of primary particles in the interaction
  };

  class smeared_fake_reco : public ufw::process {
    const edep_reader* m_edep;     ///< Pointer to the edep reader providing MC truth data
    const genie_reader* m_genie;   ///< Pointer to the genie reader providing MC truth data
    sand::caf::caf_wrapper* m_caf; ///< Pointer to the CAF output wrapper (non-const for writing)

    /// @brief Build map of edep-sim primaries grouped by interaction
    [[nodiscard]] std::vector<EdepInteractionRange> make_edep_interaction_map() const;

    /// @brief Reserve capacity for spill-level vectors
    void initialize_spill_capacities();

    /// @brief Process all particles for one interaction
    void process_interaction_particles(::caf::SRTrueInteraction& true_ixn, ::caf::SRInteraction& reco_ixn,
                                       ::caf::SRSANDInt& sand_ixn, std::size_t interaction_index,
                                       std::size_t edep_first_index, std::size_t edep_count) const;

    /// @brief Verify all size counters match vector sizes
    void assert_sizes() const;

    /// @brief Wrapper for filling caf::SRRecoParticle according to the particle type (muon vs. all else, for now)
    sand::caf::SRRecoParticle fill_reco_particle(const sand::caf::SRTrueParticle &true_prim, const sand::caf::TrueParticleID &prim_id) const;

    /// @brief Apply bending-plane momentum smearing to muon tracks only (for the moment)
    void smear_muon_momentum(caf::SRRecoParticle &reco_mu, const sand::caf::TrueParticleID &mu_id) const;

   public:
    smeared_fake_reco();

    void configure(const ufw::config& cfg) override;

    void run() override;
  };

} // namespace sand

UFW_REGISTER_PROCESS(sand::smeared_fake_reco);

#endif // SANDRECO_SMEARED_FAKE_RECO_HPP
