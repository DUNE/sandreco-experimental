#include <ufw/config.hpp>
#include <ufw/context.hpp>
#include <ufw/data.hpp>
#include <ufw/factory.hpp>
#include <ufw/process.hpp>

#include <grain/digi.h>
#include <grain/image.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace sand::grain {

  class spill_slicer : public ufw::process {
   public:
    spill_slicer();
    void configure(const ufw::config& cfg) override;
    void run() override;

   private:
    double m_min_response_signal;
    double m_delta_ns_for_comparison;
    uint64_t m_stat_photons_processed;
    uint64_t m_stat_photons_accepted;
    uint64_t m_stat_photons_discarded;
    std::vector<double> m_slice_times;
    int m_seed = 0;
    bool m_use_algo;

    void compute_slice_times();
  };

  void spill_slicer::configure(const ufw::config& cfg) {
    process::configure(cfg);
    m_slice_times.clear();
    for (auto time : cfg.value("slice_times", m_slice_times)) {
      m_slice_times.push_back(time);
    }
    if (m_slice_times.size() > 1) { // At least 2 edges for 1 slice
      UFW_DEBUG("Using slice times from parameters");
      m_use_algo = false;
    } else {
      UFW_DEBUG("Using slicing algorithm");
      m_use_algo                = true;
      m_min_response_signal     = cfg.at("min_response_signal");
      m_delta_ns_for_comparison = cfg.at("delta_ns_for_comparison");
    }
  }

  void spill_slicer::compute_slice_times() {
    // Place times into bins
    const int n_bins{100};
    const double min_time{0.0};
    const double max_time{20000.0}; // ns
    const double bin_width{(max_time - min_time) / n_bins};

    std::array<double, n_bins> binned_times;
    binned_times.fill(0.0);

    const auto& digis_in = get<digi>("digi");
    for (auto& signal : digis_in.signals) {
      double time{signal.time_rising_edge};
      if (time >= min_time && time < max_time) {
        size_t bin_index = static_cast<size_t>(std::floor((time - min_time) / bin_width));
        binned_times[bin_index] += signal.npe;
      } else {
        UFW_WARN("Digi in channel {} is out of time window for slicing (t = {} ns)", signal.channel().raw, time);
      }
    }

    // Go through bins to find slice_edges
    const size_t n_close_bins = static_cast<size_t>(m_delta_ns_for_comparison / bin_width);
    m_slice_times.clear();
    m_slice_times.push_back(min_time);
    for (size_t i = n_close_bins; i < n_bins - n_close_bins; ++i) {
      auto center        = binned_times.begin() + i;
      uint64_t left_max  = *std::max_element(center - n_close_bins, center);
      uint64_t right_max = *std::max_element(center + 1, center + n_close_bins + 1);
      if (binned_times[i] > left_max && binned_times[i] > right_max && binned_times[i] >= m_min_response_signal) {
        m_slice_times.push_back(static_cast<double>(i) * bin_width);
      }
    }
    m_slice_times.push_back(max_time);
  }

  spill_slicer::spill_slicer() : process({{"digi", "sand::grain::digi"}}, {{"images", "sand::grain::images"}}) {
    UFW_INFO("Creating a spill_slicer process at {}", fmt::ptr(this));
  }

  void spill_slicer::run() {
    m_stat_photons_processed = 0;
    m_stat_photons_accepted  = 0;
    m_stat_photons_discarded = 0;
    const auto& digis_in     = get<digi>("digi");
    auto& images_out         = set<images>("images").images;
    if (m_use_algo) {
      m_slice_times.clear();
      compute_slice_times();
    }
    for (int img_idx = 0; img_idx < m_slice_times.size() - 1; img_idx++) {
      UFW_INFO("Building images in time interval [{} - {}] ns", m_slice_times[img_idx], m_slice_times[img_idx + 1]);
      size_t offset = images_out.size();
      for (auto& signal : digis_in.signals) {
        auto id = signal.channel().link;
        auto it = std::find_if(images_out.begin() + offset, images_out.end(),
                               [id](auto& img) { return img.camera_id == id; });
        if (it == images_out.end()) {
          images::image img{id, m_slice_times[img_idx], m_slice_times[img_idx + 1]};
          images_out.emplace_back(img);
          it = images_out.end() - 1;
          it->blank();
          UFW_DEBUG("Created image for camera id: {}, starting at time: {}", id, m_slice_times[img_idx]);
        }
        m_stat_photons_processed++;
        if (signal.time_rising_edge >= m_slice_times[img_idx] && signal.time_rising_edge < m_slice_times[img_idx + 1]) {
          // UFW_DEBUG("signal to be assigned to image {}", img_idx);
          // FIXME this assumes that channel ids and the pixel array are indexed consistently
          auto& pixel = it->pixels.Array()[signal.channel().channel];
          pixel.insert(signal.true_hits());
          pixel.amplitude += signal.npe;
          if (std::isnan(pixel.time_first) || (pixel.time_first > signal.time_rising_edge)) {
            pixel.time_first = signal.time_rising_edge;
          }
          m_stat_photons_accepted++;
        } else {
          m_stat_photons_discarded++;
        }
      }
      for (const auto& img :images_out) {
        size_t maxhits = 0;
        double npe = 0.;
        for (int x = 0; x != camera_width; ++x) {
          for (int y = 0; y != camera_height; ++y) {
            maxhits = std::max(maxhits, img.pixels[x][y].true_hits().size());
            npe += img.pixels[x][y].amplitude;
          }
        }
        UFW_DEBUG("Camera {} recorded a total of {} photons from {} different MC true hits", img.camera_id, npe, maxhits );
      }
    }
    UFW_INFO("Processed {} photons; {} were accepted, {} discarded.", m_stat_photons_processed, m_stat_photons_accepted,
             m_stat_photons_discarded);
  }

} // namespace sand::grain

UFW_REGISTER_PROCESS(sand::grain::spill_slicer)
UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::grain::spill_slicer)
