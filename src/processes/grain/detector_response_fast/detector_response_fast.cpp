#include <ufw/config.hpp>
#include <ufw/context.hpp>
#include <ufw/data.hpp>
#include <ufw/factory.hpp>

#include <geoinfo/geoinfo.hpp>
#include <geoinfo/grain_info.hpp>
#include <grain/digi.h>
#include <grain/grain.h>
#include <grain/photons.h>

#include <detector_response_fast.hpp>

UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::grain::detector_response_fast)

namespace sand::grain {

  detector_response_fast::detector_response_fast()
    : process({{"hits", "sand::grain::hits"}}, {{"digi", "sand::grain::digi"}}), m_uniform(0.0, 1.0) {
    UFW_DEBUG("Creating a detector_response_fast process at {}", fmt::ptr(this));
  }

  void detector_response_fast::configure(const ufw::config& cfg) {
    process::configure(cfg);
    m_pde = cfg.at("pde");
    m_gdml_geometry = cfg.at("geometry");
  }

  void detector_response_fast::run() {
    UFW_INFO("Running a detector_response_fast process at {}.", fmt::ptr(this));
    const auto& gi = instance<geoinfo>();
    UFW_INFO("GRAIN path: '{}'", gi.grain().path());
    m_stat_photons_processed = 0;
    m_stat_photons_accepted  = 0;
    m_stat_photons_discarded = 0;
    const auto& hits_in      = get<hits>("hits");
    UFW_DEBUG("Processing {} photon hits.", hits_in.photons.size());
    auto& digi_out                        = set<digi>("digi");
    geoinfo::grain_info::camera* pix_spam = nullptr;
    if (m_gdml_geometry == "gdml-masks") {
      pix_spam = &gi.grain().mask_cameras().front();
    } else if (m_gdml_geometry == "gdml-lenses") {
      pix_spam = &gi.grain().lens_cameras().front();
    } else {
      UFW_ERROR("GRAIN gdml-geometry type not found.");
    }
    for (const auto& photon : hits_in.photons) {
      true_hits truth;
      double interaction_probability = m_uniform(random_engine());
      m_stat_photons_processed++;
      if (interaction_probability < m_pde) {
        // UFW_DEBUG("processing photon with position: {}, {}", photon.pos.X(), photon.pos.Y());
        bool channel_found = false;
        for (int i = 0; i != camera_height && !channel_found; ++i) {
          for (int j = 0; j != camera_width; ++j) {
            if (photon.pos.X() > pix_spam->sipm_active_areas[i][j].left
                && photon.pos.X() < pix_spam->sipm_active_areas[i][j].right
                && photon.pos.Y() > pix_spam->sipm_active_areas[i][j].bottom
                && photon.pos.Y() < pix_spam->sipm_active_areas[i][j].top) {
              truth.add(photon.hit);
              channel_id ch;
              ch.subdetector = GRAIN;
              ch.link        = photon.camera_id;
              // consistent indexing: Row Major
              ch.channel = i * camera_width + j;
              digi::signal pe{truth, ch, photon.pos.T(), NAN, 1.0};
              digi_out.signals.emplace_back(pe);
              m_stat_photons_accepted++;
              // UFW_DEBUG("Added photon to SiPM {},{}", i, j);
              channel_found = true;
              break;
            }
          }
        }
        if (channel_found == false) {
          m_stat_photons_discarded++;
        }
      } else {
        m_stat_photons_discarded++;
      }
    }
    UFW_INFO("Processed {} photon hits; {} were accepted, {} discarded.", m_stat_photons_processed,
             m_stat_photons_accepted, m_stat_photons_discarded);
  }
} // namespace sand::grain
