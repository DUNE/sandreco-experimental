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
    std::vector<double> m_slice_times;
    
  };

  void time_slicing::configure (const ufw::config& cfg) {
    process::configure(cfg);
    for (auto time: cfg.at("slice_times") ) {
      m_slice_times.push_back(time);
    }
  
  }


  time_slicing::time_slicing() : process({{"digi", "sand::grain::digi"}}, {{"pictures", "sand::grain::pictures"}}) {
    UFW_INFO("Creating a time_slicing process at {}", fmt::ptr(this));
  }


  void time_slicing::run() {
    const auto& digis_in = get<digi>("digi");
    UFW_DEBUG("Camera images size: {}.", digis_in.cameras.size());
    auto& images_out = set<pictures>("pictures");
    

    for (int img_idx = 0; img_idx < m_slice_times.size()-1; img_idx++ ) {
      UFW_DEBUG("Building images in time interval [{} - {}] ns", m_slice_times[img_idx], m_slice_times[img_idx+1]);
      for (auto& cam : digis_in.cameras) {
        UFW_DEBUG("Camera {} {}", cam.camera_id, cam.camera_name);
        pictures::picture cam_image; 
        cam_image.camera_id = cam.camera_id;
        //cam_image.camera_name = cam.camera_name;
        cam_image.time_begin = m_slice_times[img_idx];
        cam_image.time_end = m_slice_times[img_idx+1];
        for (auto& pe : cam.photoelectrons) {
          UFW_DEBUG("channel id: {}, time: {}", pe.channel_id, pe.time_rising_edge);
          if (pe.time_rising_edge >= m_slice_times[img_idx] && pe.time_rising_edge < m_slice_times[img_idx+1]){
              UFW_DEBUG("pe to be assigned to image {}", img_idx);
              cam_image.channels[pe.channel_id].amplitude += pe.charge;
          }  
        }
        images_out.pictures.emplace_back(cam_image);
      }
    }
  }

}
  
UFW_REGISTER_PROCESS(sand::grain::time_slicing)
UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::grain::time_slicing)
