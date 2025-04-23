#include <ufw/context.hpp>
#include <ufw/config.hpp>
#include <ufw/data.hpp>
#include <ufw/factory.hpp>
#include <ufw/process.hpp>

#include <detector_response_fast.hpp>
#include <grain/photons.h>
#include <grain/digi.h>


UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::grain::detector_response_fast)

namespace sand::grain {

  detector_response_fast::detector_response_fast() : process({{"hits", "sand::grain::hits"}}, {{"digi", "sand::grain::digi"}}), m_uniform(0.0, 1.0) {
    UFW_DEBUG("Creating a detector_response_fast process at {}", fmt::ptr(this));
  }
  


  void detector_response_fast::configure (const ufw::config& cfg) {
    process::configure(cfg); 
    m_sipm_cell = cfg.at("sipm_cell"); 
    m_matrix_rows = cfg.value("matrix_rows", 32);
    m_matrix_columns = cfg.value("matrix_columns", 32);
    m_pde = cfg.at("pde");
    m_rng_engine.seed(cfg.at("seed"));
    m_matrix_height = m_matrix_rows * m_sipm_cell;
    m_matrix_width = m_matrix_columns * m_sipm_cell; 
  }


  void detector_response_fast::run() {
    const auto& hits_in = get<hits>("hits");
    UFW_DEBUG("Hits size: {}.", hits_in.cameras.size());
    auto& digi_out = set<digi>("digi");

    for (auto& cam : hits_in.cameras) {
      UFW_DEBUG("Camera {} has {} photon hits.", cam.camera_name, cam.photons.size());
      digi_out.cameras.emplace_back(assign_to_pixel(cam));  
    }
  }



digi::camera detector_response_fast::assign_to_pixel(const hits::camera& h_cam) {
  digi::camera dg_cam;  
  dg_cam.camera_id = h_cam.camera_id;
  dg_cam.camera_name = h_cam.camera_name;
  for (auto& p: h_cam.photons) {
    true_hits t;
    double interaction_probability = m_uniform(m_rng_engine);
    if (interaction_probability < m_pde) {
      int col = static_cast<int>((p.pos.X() + m_matrix_width/2) / m_sipm_cell);
      int row = static_cast<int>((p.pos.Y() + m_matrix_height/2) / m_sipm_cell);
      uint16_t sipm_idx = row * m_matrix_columns + col;
      UFW_DEBUG("Position: {}, {}, assinged to channel {}", p.pos.X(), p.pos.Y(), sipm_idx);
      // check matrix boundaries 
      if (col >= 0 && col < m_matrix_columns && row >= 0 && row < m_matrix_rows) {
        t.add(p.hit);
        digi::photoelectron pe{t, sipm_idx, p.pos.T(), NAN, 1.0};
        dg_cam.photoelectrons.emplace_back(pe);
      } 
    }
  }
  return dg_cam;
}




}