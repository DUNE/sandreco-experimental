#include "ufw/utils.hpp"
#include <grain/digi.h>
#include <grain/image.h>

#include <ufw/config.hpp>
#include <ufw/context.hpp>
#include <ufw/factory.hpp>
#include <ufw/process.hpp>

#include <algorithm>
#include <array>
#include <vector>

namespace sand::grain {

  /**
   * \class sand::grain::spill_slicer
   *
   * \brief Processes digitized signals into time-based images/slices for further reconstruction.
   *
   * This process accepts digitized signals (digi) and classifies time-of-flight photons into multiple time-based "slices".
   * Each slice represents a defined time window, and pixel-level data is assigned to the appropriate slice based on photon timing.
   * Outputs generated pixel images (`images`) for each given camera.
   *
   * \subsection Configuration
   * | Parameter Name            | Type             | Unit   | Required/Default                 | Description                                        |
   * |---------------------------|------------------|--------|----------------------------------|----------------------------------------------------|
   * | `slice_times`             | vector\<double\> | ns     | Default: []                      | Predefined time slices for photon assignment.      |
   * | `bin_width`               | double           | ns     | Default: 200.0                   | Bin size to use when histogramming arrival time    |
   * | `min_response_signal`     | double           | npe    | Required if slice_times is empty | Minimum photon response signal to trigger slicing. |
   * | `delta_ns_for_comparison` | double           | ns     | Required if slice_times is empty | **??**                                             |
   */

  class spill_slicer : public ufw::process {
   public:
    spill_slicer();
    void configure(const ufw::config& cfg) override;
    void run() override;

   private:
    double m_bin_width;
    double m_min_response_signal;
    double m_delta_ns_for_comparison;
    uint64_t m_stat_photons_processed;
    uint64_t m_stat_photons_accepted;
    uint64_t m_stat_photons_discarded;
    std::vector<double> m_slice_edges;
    int m_seed = 0;
    bool m_use_algo;

    void compute_slice_times();
  };

  void spill_slicer::configure(const ufw::config& cfg) {
    process::configure(cfg);
    m_slice_edges.clear();
    for (auto time : cfg.value("slice_times", m_slice_edges)) {
      m_slice_edges.push_back(time);
    }
    if (m_slice_edges.size() > 1) { // At least 2 edges for 1 slice
      UFW_DEBUG("Using slice times from parameters");
      m_use_algo = false;
    } else {
      UFW_DEBUG("Using slicing algorithm");
      m_use_algo                = true;
      m_bin_width               = cfg.value("bin_width", 200.0);
      m_min_response_signal     = cfg.at("min_response_signal");
      m_delta_ns_for_comparison = cfg.at("delta_ns_for_comparison");
    }
  }

  void spill_slicer::compute_slice_times() {
    // Place times into bins
    const double min_time{0.0};
    const double max_time{20000.0}; // ns
    const size_t n_bins = static_cast<size_t>(std::ceil((max_time - min_time) / m_bin_width));

    std::vector<double> binned_times(n_bins, 0.0);

    const auto& digis_in = get<digi>("digi");
    for (auto& signal : digis_in) {
      double time{signal.tdc()};
      if (time >= min_time && time < max_time) {
        size_t bin_index = static_cast<size_t>(std::floor((time - min_time) / m_bin_width));
        binned_times[bin_index] += signal.npe();
      } else {
        UFW_WARN("Signal from {} is out of time window (t = {} ns)", signal.channel(), time);
      }
    }

    // Go through bins to find slice_edges
    const size_t n_close_bins = static_cast<size_t>(m_delta_ns_for_comparison / m_bin_width);
    m_slice_edges.clear();
    m_slice_edges.push_back(min_time);
    for (size_t i = n_close_bins; i < n_bins - n_close_bins; ++i) {
      auto center        = binned_times.begin() + i;
      uint64_t left_max  = *std::max_element(center - n_close_bins, center);
      uint64_t right_max = *std::max_element(center + 1, center + n_close_bins + 1);
      if (binned_times[i] > left_max && binned_times[i] > right_max && binned_times[i] >= m_min_response_signal) {
        m_slice_edges.push_back(static_cast<double>(i) * m_bin_width);
      }
    }
    m_slice_edges.push_back(max_time);
  }

  spill_slicer::spill_slicer() : process({{"digi", "sand::grain::digi"}},
                                         {{"timeranges", "sand::reco::timeranges"},
                                          {"images", "sand::grain::images"}}) {
    UFW_INFO("Creating a spill_slicer process at {}", fmt::ptr(this));
  }

  void spill_slicer::run() {
    const auto& digis_in   = get<digi>("digi");
    auto& trs_out          = set<reco::timeranges>("timeranges");
    auto& spill_images_out = set<images>("images");
    if (m_use_algo) {
      m_slice_edges.clear();
      compute_slice_times();
    }
    //convert edges to timeranges
    if (m_slice_edges.size() < 2) {
      return;
    }
    trs_out.reserve(m_slice_edges.size() - 1);
    for (auto it = m_slice_edges.begin(); it != m_slice_edges.end() - 1; ++it) {
      UFW_INFO("trs {}, {}, {}", it + m_bin_width * 0.5, *it, *(it + 1));
      trs_out.emplace_back(*it + m_bin_width * 0.5, *it, *(it + 1)); //best value in the centre of the first bin
      UFW_INFO("Found time interval {}", trs_out.back());
    }
    //use the timeranges
    for (auto tr: trs_out) {
      m_stat_photons_processed = 0;
      m_stat_photons_accepted  = 0;
      m_stat_photons_discarded = 0;
      UFW_INFO("Building images in time interval {} ns", tr);
      std::vector<image> event_images_out;
      for (auto& sig : digis_in) {
        if (tr.contains(sig.tdc())) {
          auto id = sig.channel().link;
          auto it = std::find_if(event_images_out.begin(), event_images_out.end(),
                                 [id](auto& img) { return img.camera_id == id; });
          if (it == event_images_out.end()) {
            //FIXME newer c++
            image img{id, tr};
            event_images_out.emplace_back(img);
            it = event_images_out.end() - 1;
            it->blank();
          }
          //UFW_DEBUG("signal to be assigned to camera id {}, image {}", id, img_idx);
          // FIXME this assumes that channel ids and the pixel array are indexed consistently
          pixel& pix = it->pixels.data()[sig.channel().channel];
          pix.insert(sig.true_hits());
          pix.amplitude += sig.npe();
          if (std::isnan(pix.time_first) || (pix.time_first > sig.tdc())) {
            pix.time_first = sig.tdc();
          }
          m_stat_photons_accepted++;
        } else {
          m_stat_photons_discarded++; //FIXME this is incorrect: should count only when discarded from all slices
        }
        m_stat_photons_processed++;
      }
      for (const auto& img : event_images_out) {
        size_t maxhits = 0;
        double npe     = 0.;
        for (int x = 0; x != camera_width; ++x) {
          for (int y = 0; y != camera_height; ++y) {
            maxhits = std::max(maxhits, img.pixels.at(x,y).true_hits().size());
            npe += img.pixels.at(x,y).amplitude;
          }
        }
        UFW_DEBUG("Camera {} recorded a total of {} photons from {} different MC true hits", img.camera_id, npe,
                  maxhits);
      }
      spill_images_out.insert(spill_images_out.end(), event_images_out.begin(), event_images_out.end());
      UFW_INFO("Processed {} photons; {} were accepted, {} discarded for this slice.", m_stat_photons_processed,
               m_stat_photons_accepted, m_stat_photons_discarded);
    }
  }

} // namespace sand::grain

UFW_REGISTER_PROCESS(sand::grain::spill_slicer)
UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::grain::spill_slicer)
