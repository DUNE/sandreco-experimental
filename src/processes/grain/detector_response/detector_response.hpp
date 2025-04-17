#include <ufw/context.hpp>
#include <ufw/config.hpp>
#include <ufw/data.hpp>
#include <ufw/factory.hpp>
#include <ufw/process.hpp>

#include <grain/photons.h>


namespace sand::grain {

  class detector_response : public ufw::process {

  public:
    detector_response();
    void configure (const ufw::config& cfg) override;
    void run() override;
  
  private:
    double m_sipm_cell = 0.0;

  };

}

UFW_REGISTER_PROCESS(sand::grain::detector_response)
