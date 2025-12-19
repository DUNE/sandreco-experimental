#include <ufw/config.hpp>
#include <ufw/context.hpp>
#include <ufw/data.hpp>
#include <ufw/factory.hpp>
#include <ufw/process.hpp>

#include <grain/digi.h>
#include <grain/photons.h>

namespace sand::grain {

  class detector_response_fast : public ufw::process {
   public:
    detector_response_fast();
    void configure(const ufw::config& cfg) override;
    void run() override;

   private:
    double m_pde;
    std::uniform_real_distribution<> m_uniform;
    uint64_t m_stat_photons_processed;
    uint64_t m_stat_photons_accepted;
    uint64_t m_stat_photons_discarded;
  };

} // namespace sand::grain

UFW_REGISTER_PROCESS(sand::grain::detector_response_fast)
