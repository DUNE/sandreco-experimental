#include <ufw/context.hpp>
#include <ufw/config.hpp>
#include <ufw/data.hpp>
#include <ufw/process.hpp>

namespace sand::tracker {

  class tracklet_finder : public ufw::process {

  public:
    tracklet_finder();
    void configure (const ufw::config& cfg) override;
    void run() override;

  };
    
}

UFW_REGISTER_PROCESS(sand::tracker::tracklet_finder)
