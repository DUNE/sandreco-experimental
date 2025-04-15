#include <ufw/context.hpp>
#include <ufw/config.hpp>
#include <ufw/data.hpp>
#include <ufw/factory.hpp>
#include <ufw/process.hpp>

#include <grain/digi.h>
#include <grain/picture.h>


class time_slicing : public ufw::process {

  public:
    time_slicing();
    void configure (const ufw::config& cfg) override;
    // const ufw::var_type_map& products() const override;
    // const ufw::var_type_map& requirements() const override;
    void run(const ufw::var_id_map& inputs, const ufw::var_id_map& outputs) override;

  
  private:
    int m_seed = 0;
};
  
  UFW_REGISTER_PROCESS(time_slicing)
  UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(time_slicing)

  void time_slicing::configure (const ufw::config& cfg) {
    process::configure(cfg);
  }
  
  // ufw::data_list detector_response::products() const {
  //   return ufw::data_list{{"output", "sand::example"}};
  // }
  
  // ufw::data_list detector_response::requirements() const {
  //   return ufw::data_list{{"input", "sand::example"}};
  // }
  // time_slicing::time_slicing() : process({{"cameras_in", "sand::grain::digi"}}, {{"sliced_pictures_out", "sand::grain::pictures"}}) {
  time_slicing::time_slicing() : process({{"cameras_in", "sand::grain::digi"}}, {}) {
    UFW_INFO("Creating a time_slicing process at {}", fmt::ptr(this));
  }


  void time_slicing::run(const ufw::var_id_map& inputs, const ufw::var_id_map& outputs) {
    auto& hits = ufw::context::instance<sand::grain::digi>(inputs.at("cameras_in"));
    UFW_INFO("QUI: {}", hits.images.size());
    
    UFW_INFO("Executing test_process::run at {} with parameters:", fmt::ptr(this));
    for (const auto& [var, id]: inputs) {
      UFW_INFO("  in : {{{}, {}}}", var, id);
    }
    for (auto& i : hits.images) {
      UFW_INFO("Image {}", i.camera_id);
      for (auto& c : i.channels) {
        // UFW_INFO("Position: {}", p.pos.X());
      }
    }
}  
