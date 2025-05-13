#include <ufw/context.hpp>
#include <ufw/config.hpp>
#include <ufw/data.hpp>
#include <ufw/factory.hpp>
#include <ufw/process.hpp>

#include <grain/digi.h>
#include <grain/image.h>

#include <array>
#include <vector>
#include <algorithm>

namespace sand::grain {

  class spill_slicer : public ufw::process {

  public:
    spill_slicer();
    void configure (const ufw::config& cfg) override;
    void run() override;
    std::vector<double> compute_slice_times();

  private:
    int m_seed = 0;
    uint32_t m_min_response_signal;
    double m_delta_ns_for_comparison;
    //std::vector<double> m_slice_times;
    uint64_t m_stat_photons_processed;
    uint64_t m_stat_photons_accepted;
    uint64_t m_stat_photons_discarded;
    
  };

  void spill_slicer::configure (const ufw::config& cfg) {
    process::configure(cfg);
    // for (auto time: cfg.at("slice_times") ) {
    //   m_slice_times.push_back(time);
    // }
    m_min_response_signal = cfg.at("min_response_signal");
    m_delta_ns_for_comparison = cfg.at("delta_ns_for_comparison");
  }

  std::vector<double> spill_slicer::compute_slice_times() {
    // Place times into bins
    const int n_bins{100};
    const double min_time{0.0};
    const double max_time{20000.0}; //ns
    const double bin_width{(max_time - min_time)/n_bins};
    
    std::array<double, n_bins> binned_times;
    binned_times.fill(0.0);

    const auto& digis_in = get<digi>("digi");
    for (auto& cam : digis_in.cameras) {
      for (auto& pe : cam.photoelectrons) {
        double time{pe.time_rising_edge};
        if (time >= min_time && time < max_time) {
          size_t bin_index = static_cast<size_t>((time - min_time) / bin_width);
          binned_times[bin_index] += pe.charge;
        }
        else {
          UFW_DEBUG("Digi in camera {} {} is out of time window for slicing (t = {} ns)", cam.camera_id, cam.camera_name, time);
        }
      }
    }

    // Go through bins to find slice_edges
    const size_t n_close_bins = static_cast<size_t>(m_delta_ns_for_comparison / bin_width);
    std::vector<double> slice_edges;
    slice_edges.push_back(min_time);
    for (size_t i = n_close_bins; i < n_bins - n_close_bins; ++i) {
        uint64_t left_max = *std::max_element(binned_times.begin() + i - n_close_bins, binned_times.begin() + i);
        uint64_t right_max = *std::max_element(binned_times.begin() + i + 1, binned_times.begin() + i + n_close_bins + 1);
        if (binned_times[i] > left_max && binned_times[i] > right_max && binned_times[i] >= m_min_response_signal) {
            slice_edges.push_back(static_cast<double>(i) * bin_width);
        }
    }
    slice_edges.push_back(max_time);
    return slice_edges;
  }

  spill_slicer::spill_slicer() : process({{"digi", "sand::grain::digi"}}, {{"images", "sand::grain::images"}}) {
    UFW_INFO("Creating a spill_slicer process at {}", fmt::ptr(this));
  }

  void spill_slicer::run() {
    m_stat_photons_processed = 0;
    m_stat_photons_accepted = 0;
    m_stat_photons_discarded = 0;
    const auto& digis_in = get<digi>("digi");
    auto& images_out = set<images>("images");
    std::vector<double> slice_times = compute_slice_times();
    for (int img_idx = 0; img_idx < slice_times.size() - 1; img_idx++) {
      UFW_INFO("Building images in time interval [{} - {}] ns", slice_times[img_idx], slice_times[img_idx + 1]);
      for (auto& cam : digis_in.cameras) {
        UFW_DEBUG("Camera {} {}", cam.camera_id, cam.camera_name);
        images::image cam_image;
        cam_image.blank();
        cam_image.camera_id = cam.camera_id;
        cam_image.camera_name = cam.camera_name;
        cam_image.time_begin = slice_times[img_idx];
        cam_image.time_end = slice_times[img_idx + 1];
        for (auto& pe : cam.photoelectrons) {
          //UFW_DEBUG("channel id: {}, time: {}", pe.channel_id, pe.time_rising_edge);
          m_stat_photons_processed++;
          if (pe.time_rising_edge >= slice_times[img_idx] && pe.time_rising_edge < slice_times[img_idx + 1]) {
              //UFW_DEBUG("pe to be assigned to image {}", img_idx);
              //consistent indexing: Row Major
              cam_image.pixels.Array()[pe.channel_id].add(pe.hits);
              cam_image.pixels.Array()[pe.channel_id].amplitude += pe.charge;
              if (std::isnan(cam_image.pixels.Array()[pe.channel_id].time_first)) {
                cam_image.pixels.Array()[pe.channel_id].time_first = pe.time_rising_edge;
              } else if (cam_image.pixels.Array()[pe.channel_id].time_first > pe.time_rising_edge) {
                cam_image.pixels.Array()[pe.channel_id].time_first = pe.time_rising_edge;
              }
              m_stat_photons_accepted++;
          } else {
            m_stat_photons_discarded++;
          }
        }
        images_out.images.emplace_back(cam_image);
      }
    }
    UFW_INFO("Processed {} photons; {} were accepted, {} discarded.", m_stat_photons_processed, m_stat_photons_accepted, m_stat_photons_discarded);
  }

}

UFW_REGISTER_PROCESS(sand::grain::spill_slicer)
UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::grain::spill_slicer)
