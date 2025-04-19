#include <ufw/context.hpp>
#include <ufw/config.hpp>
#include <ufw/data.hpp>
#include <ufw/factory.hpp>
#include <ufw/process.hpp>

#include <grain/photons.h>


class detector_response : public ufw::process {

  public:
  detector_response();
  void configure (const ufw::config& cfg) override;
  void run() override;
};
  
  UFW_REGISTER_PROCESS(detector_response)
  UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(detector_response)

  void detector_response::configure (const ufw::config& cfg) {
    process::configure(cfg);
    
  }

  detector_response::detector_response() : process({{"hits", "sand::grain::hits"}}, {}) {
    UFW_INFO("Creating a detector_response process at {}", fmt::ptr(this));
  }

  void detector_response::run() {
    const auto& hits = get<sand::grain::hits>("hits");
    UFW_INFO("Camera hits size: {}.", hits.cameras.size());
    for (auto& i : hits.cameras) {
      UFW_INFO("Reading Image for camera {}.", i.camera_name);
      for (auto& p : i.photons) {
        // UFW_INFO("Position: {}", p.pos.X());
      }
    }

}  
