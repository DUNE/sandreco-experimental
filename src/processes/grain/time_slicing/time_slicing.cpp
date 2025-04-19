#include <ufw/context.hpp>
#include <ufw/config.hpp>
#include <ufw/data.hpp>
#include <ufw/factory.hpp>
#include <ufw/process.hpp>

#include <grain/digi.h>
#include <grain/picture.h>

namespace sand::grain {

  class time_slicing : public ufw::process {

  public:
    time_slicing();
    void configure (const ufw::config& cfg) override;
    void run() override;

  private:
    int m_seed = 0;
  };

  void time_slicing::configure (const ufw::config& cfg) {
    process::configure(cfg);
  }

  time_slicing::time_slicing() : process({{"digi", "sand::grain::digi"}}, {}) {
    UFW_INFO("Creating a time_slicing process at {}", fmt::ptr(this));
  }

  void time_slicing::run() {
    const auto& digis = get<sand::grain::digi>("digi");
    UFW_INFO("Camera images size: {}.", digis.images.size());

    for (auto& i : digis.images) {
      UFW_INFO("Image {}", i.camera_id);
      for (auto& c : i.channels) {
        // UFW_INFO("Position: {}", p.pos.X());
      }
    }
  }

}
  
UFW_REGISTER_PROCESS(sand::grain::time_slicing)
UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::grain::time_slicing)
