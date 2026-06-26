#ifndef SANDRECO_FAKE_RECO_HPP
#define SANDRECO_FAKE_RECO_HPP

#include <caf/caf_wrapper.hpp>

#include <ufw/process.hpp>

#include <duneanaobj/StandardRecord/SRInteraction.h>
#include <duneanaobj/StandardRecord/SRSAND.h>

namespace sand {

  class fake_reco : public ufw::process {
    sand::caf::standard_record_wrapper* m_caf{nullptr};

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
