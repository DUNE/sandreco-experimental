#ifndef SANDRECO_FAKE_RECO_HPP
#define SANDRECO_FAKE_RECO_HPP

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

  class fake_reco : public ufw::process {
    const edep_reader* m_edep;     ///< Pointer to the edep reader providing MC truth data
    const genie_reader* m_genie;   ///< Pointer to the genie reader providing MC truth data
    sand::caf::caf_wrapper* m_caf; ///< Pointer to the CAF output wrapper (non-const for writing)

    std::string m_sec_fill_mode; ///< Mode for secondary particle filling (default or recursive)
    std::optional<int> m_sec_fill_depth; ///< Depth level down to which save particles  in the interaction tree (0 for secondaries)
    /// @brief Build map of edep-sim primaries grouped by interaction
    [[nodiscard]] std::vector<EdepInteractionRange> make_edep_interaction_map() const;

    /// @brief Reserve capacity for spill-level vectors
    void initialize_spill_capacities();

    /// @brief Recursive helper starting from the secondaries iterators
    std::size_t count_secondaries_recursive(EDEPTree::const_iterator begin, EDEPTree::const_iterator end, const int depth) const;

    /// @brief Recursive helper for filling the vector of secondary particles
    void add_secondaries_recursive(::caf::SRTrueInteraction& ixn, EDEPTree::const_iterator begin,
                                   EDEPTree::const_iterator end, const ::caf::TrueParticleID& ancestor_id,
                                   const int depth) const;

    /// @brief Fill the reco caf objects for a particle
    void fill_reco_objects(const ::caf::SRTrueParticle &true_part, const ::caf::TrueParticleID &part_id, 
                            const bool is_primary, ::caf::SRInteraction& reco_ixn, ::caf::SRSANDInt& sand_ixn) const;

    /// @brief Process all particles for one interaction
    void process_interaction_particles(::caf::SRTrueInteraction& true_ixn, ::caf::SRInteraction& reco_ixn,
                                       ::caf::SRSANDInt& sand_ixn) const;

    /// @brief Verify all size counters match vector sizes
    void assert_sizes() const;

   public:
    fake_reco();

    void configure(const ufw::config& cfg) override;

    void run() override;
  };

} // namespace sand

UFW_REGISTER_PROCESS(sand::fake_reco);

#endif // SANDRECO_FAKE_RECO_HPP
