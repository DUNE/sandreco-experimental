#include <common/data.h>

#include <ufw/process.hpp>

namespace sand::ecal {
  class full_digi : public ufw::process {
   public:
    full_digi();
    void configure(const ufw::config& cfg) override;
    void run() override;

   private:
    double m_config_param;
  };
} // namespace sand::ecal

UFW_REGISTER_PROCESS(sand::ecal::full_digi)
