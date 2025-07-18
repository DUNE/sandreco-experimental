#include <ufw/context.hpp>
#include <ufw/config.hpp>
#include <ufw/data.hpp>
#include <ufw/factory.hpp>
#include <ufw/process.hpp>

#include <tracker/digi.h>

namespace sand::tracker {

  class fast_digi : public ufw::process {

  public:
    fast_digi();
    void configure (const ufw::config& cfg) override;
    void run() override;

  private:
    double m_drift_velocity; //[mm/ns]
    double m_wire_velocity; //[mm/ns]
    double m_sigma_tdc; //[ns]

  };

}

UFW_REGISTER_PROCESS(sand::tracker::fast_digi)
