#include <ufw/context.hpp>
#include <ufw/config.hpp>
#include <ufw/data.hpp>
#include <ufw/factory.hpp>

#include <grain/grain.h>
#include <grain/photons.h>
#include <grain/digi.h>

#include <detector_response_fast.hpp>

UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::grain::detector_response_fast)

namespace sand::grain {

  detector_response_fast::detector_response_fast() : process({{"hits", "sand::grain::hits"}}, {{"digi", "sand::grain::digi"}}), m_uniform(0.0, 1.0) {
    UFW_DEBUG("Creating a detector_response_fast process at {}", fmt::ptr(this));
  }

  void detector_response_fast::configure (const ufw::config& cfg) {
    process::configure(cfg); 
    m_sipm_active_size = cfg.at("sipm_active_size");
    m_sipm_border = cfg.at("sipm_border");
    m_pde = cfg.at("pde");
    m_rng_engine.seed(cfg.at("seed"));
    m_sipm_size = m_sipm_active_size + 2 * m_sipm_border; 
    m_matrix_height = camera_height * m_sipm_size;
    m_matrix_width = camera_width * m_sipm_size; 
  }

  void detector_response_fast::run() {
    m_stat_photons_processed = 0;
    m_stat_photons_accepted = 0;
    m_stat_photons_discarded = 0;
    const auto& hits_in = get<hits>("hits");
    UFW_DEBUG("Processing {} photon hits.", hits_in.photons.size());
    auto& digi_out = set<digi>("digi");
    for (const auto& photon : hits_in.photons) {
      true_hits truth;
      double interaction_probability = m_uniform(m_rng_engine);
      m_stat_photons_processed++;
      if (interaction_probability < m_pde) {
        double shifted_pos_x = photon.pos.X() + m_matrix_width * 0.5;
        double shifted_pos_y = - photon.pos.Y() + m_matrix_height * 0.5;
        int col = static_cast<int>(shifted_pos_x / m_sipm_size);
        int row = static_cast<int>(shifted_pos_y / m_sipm_size);
        //UFW_DEBUG("Photon position: {}, {}, assigned to channel {}, {}", p.pos.X(), p.pos.Y(), row, col);
        // check matrix boundaries
        if (col >= 0 && col < camera_width && row >= 0 && row < camera_height) {
          // check sipm borders
          if (shifted_pos_x > col * m_sipm_size + m_sipm_border && shifted_pos_x < (col + 1) * m_sipm_size - m_sipm_border &&
              shifted_pos_y > row * m_sipm_size + m_sipm_border && shifted_pos_y < (row + 1) * m_sipm_size - m_sipm_border) {
            truth.add(photon.hit);
            //UFW_DEBUG("Photon is inside sipm");
            //consistent indexing: Row Major
            channel_id ch;
            ch.subdetector = GRAIN;
            ch.link = photon.camera;
            ch.channel = row * camera_width + col;
            digi::signal pe{truth, ch, photon.pos.T(), NAN, 1.0};
            digi_out.signals.emplace_back(pe);
            m_stat_photons_accepted++;
          } else {
            //UFW_DEBUG("Photon {}, {} is outside sipm {}, {}", shifted_pos_x, shifted_pos_y, col * m_sipm_size, row * m_sipm_size);
            m_stat_photons_discarded++;
          }
        } else {
          //UFW_DEBUG("Photon {}, {} is outside matrix", shifted_pos_x, shifted_pos_y);
          m_stat_photons_discarded++;
        }
      } else {
        m_stat_photons_discarded++;
      }
    }
    UFW_INFO("Processed {} photon hits; {} were accepted, {} discarded.", m_stat_photons_processed, m_stat_photons_accepted, m_stat_photons_discarded);
  }

}
