#ifndef SANDRECO_FAKE_RECO_HPP
#define SANDRECO_FAKE_RECO_HPP

#include <caf/caf_wrapper.hpp>
#include <edep_reader/edep_reader.hpp>

#include <ufw/process.hpp>

#include <duneanaobj/StandardRecord/SRInteraction.h>
#include <duneanaobj/StandardRecord/SRSAND.h>
#include <duneanaobj/StandardRecord/SRTrueInteraction.h>

#include <functional>
#include <string>

namespace sand {

  class fake_reco : public ufw::process {
    const edep_reader* m_edep{nullptr};
    sand::caf::standard_record_wrapper* m_caf{nullptr};
    std::string m_reco_mode;
    double m_intrinsic_pos_res_t{};
    double m_intrinsic_pos_res_l{};
    double m_hit_energy_thr{};
    double m_b_field_magnitude{};

    /// @brief Fill the reco caf objects for a particle
    void fill_reco_objects(const std::function<::caf::SRRecoParticle(const ::caf::SRTrueParticle, const ::caf::TrueParticleID)>& make_reco,
                           const ::caf::SRTrueParticle& true_part, const ::caf::TrueParticleID& part_id,
                           bool is_primary, ::caf::SRInteraction& reco_ixn, ::caf::SRSANDInt& sand_ixn) const;

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
