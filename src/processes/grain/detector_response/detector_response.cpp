#include <ufw/context.hpp>
#include <ufw/config.hpp>
#include <ufw/data.hpp>
#include <ufw/factory.hpp>
#include <ufw/process.hpp>

#include <grain/photons.h>

#include "Randomize.hh"
#include "TSystem.h"


class detector_response : public ufw::process {

  public:
  detector_response();
  void configure (const ufw::config& cfg) override;
  // const ufw::var_type_map& products() const override;
  // const ufw::var_type_map& requirements() const override;
  void run(const ufw::var_id_map& inputs, const ufw::var_id_map& outputs) override;
};
  
  UFW_REGISTER_PROCESS(detector_response)
  UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(detector_response)

  void detector_response::configure (const ufw::config& cfg) {
    process::configure(cfg);
    
  }
  
  // ufw::data_list detector_response::products() const {
  //   return ufw::data_list{{"output", "sand::example"}};
  // }
  
  // ufw::data_list detector_response::requirements() const {
  //   return ufw::data_list{{"input", "sand::example"}};
  // }
  
  detector_response::detector_response() : process({{"hits", "sand::grain::hits"}}, {}) {
    UFW_INFO("Creating a detector_response process at {}", fmt::ptr(this));
  }


  void detector_response::run(const ufw::var_id_map& inputs, const ufw::var_id_map& outputs) {
    auto& hits = ufw::context::instance<sand::grain::hits>(inputs.at("hits"));
    UFW_INFO("QUI: {}", hits.cameras.size());
    
    UFW_INFO("Executing test_process::run at {} with parameters:", fmt::ptr(this));
    for (const auto& [var, id]: inputs) {
      UFW_INFO("  in : {{{}, {}}}", var, id);
    }
    for (auto& i : hits.cameras) {
      UFW_INFO("Image {}", i.camera_name);
      for (auto& p : i.photons) {
        // UFW_INFO("Position: {}", p.pos.X());
      }
    }

}  
