#include <full_digi.hpp>
#include <geoinfo/ecal_info.hpp>
#include <ecal/digit.h>
#include <ecal/photo_electron.h>

#include <ufw/factory.hpp>

namespace sand::ecal {
  void full_digi::configure(const ufw::config& cfg) {
    process::configure(cfg);
    // Time window for integrating photo-electrons into a single pulse
    m_int_time_window = cfg.at("int_time_window");
    // Dead time window during which no new pulses can be detected
    m_dead_time_window = cfg.at("dead_time_window");
    // Minimum number of photo-electrons required to trigger a digitized signal
    m_pe_threshold = cfg.at("pe_threshold");
    // Length of the PMT signal
    m_pmt_signal_length = cfg.at("pmt_signal_length");
    //Number of samples of the PMT waveform
    m_samples = cfg.at("samples");
    // Standard deviation of the gaussian response of the PMT (~ transit time spread)
    m_pmt_sigma = cfg.at("pmt_sigma");
    // Decay time of the PMT signal
    m_pmt_tau = cfg.at("pmt_tau");
    // Time offset for the integration of the PMT signal
    m_time_offset = cfg.at("time_offset");
    // Saturation level of the PMT signal (3.2 V)
    m_saturation_level = cfg.at("saturation_level");
  }

  full_digi::full_digi() : process({{"pes", "sand::ecal::pes_container"}}, {{"digi", "sand::ecal::digits_container"}, {"waveforms", "sand::reco::waveforms"}}) {
    UFW_DEBUG("Creating a ecal full digitization process at {}", fmt::ptr(this));
  }

  void full_digi::run() {
    UFW_DEBUG("Running a ecal full digitization process at {}", fmt::ptr(this));
    // Get ECAL geometry information
    const auto& gecal = instance<geoinfo>().ecal();
    // Get input photo-electron collection
    auto& pes = get<sand::ecal::pes_container>("pes");
    // Get output digitized signal collection
    auto& digi = set<sand::ecal::digits_container>("digi");
    auto& waveforms = set<sand::reco::waveforms>("waveforms");

    // Process photo-electrons for each PMT channel
    for (auto [pmt, pe_collection] : pes.collection) {
      // Sort photo-electrons by arrival time for temporal processing
      std::sort(pe_collection.begin(), pe_collection.end(),
                [](const auto& a, const auto& b) { return a.arrival_time < b.arrival_time; });

      // TODO: implement full digitization
            // Initialize sliding window starting with first photo-electron
      auto start_pe         = pe_collection.begin();
      auto start_int_window = start_pe->arrival_time;
      auto this_pe          = start_pe;

      // Sliding window loop: collect PEs within integration window
      while (true) {
        // Find all photo-electrons within the integration time window
        while (this_pe->arrival_time < start_int_window + m_int_time_window && this_pe != pe_collection.end()) {
          this_pe++;
        }
        // Count photo-electrons in current window
        auto pe_count = std::distance(start_pe, this_pe) + 1; // +1 to include the boundary PE

        // Check if pulse meets minimum threshold for digitization
        if (pe_count >= m_pe_threshold) {
        // Simulate PMT waveform, to get tdc and ToT
	  
    double t_start = start_int_window - m_time_offset;
    reco::waveform waveform(pmt, t_start, t_start + m_pmt_signal_length, reco::waveform::source::sim, m_samples);
	  
    double timeStep = m_pmt_signal_length/m_samples;
	  double actTime = start_int_window - m_time_offset;
	  for (double& sample : waveform) {
	    actTime += timeStep;
	    double pmtSignal = 0.0;
	    auto ipe = start_pe;
	    while (ipe != this_pe) {
	      pmtSignal += single_pe_response(actTime, ipe->arrival_time, start_int_window - m_time_offset);
	      ipe++;
	    }
	    if (this_pe != pe_collection.end()) {
	      pmtSignal += single_pe_response(actTime, this_pe->arrival_time, start_int_window - m_time_offset);
	    }
	    sample = pmtSignal;
	  }

	  // Normalize waveform, and check saturation

	  norm_waveform(waveform, timeStep);

	  // Calculate tdc and ToT with fixed threshold discriminator
	  double tdc = NAN;

	  for (int ip = 1; ip < waveform.size(); ++ip){
	    if( waveform.at(ip-1) < m_pe_threshold && waveform.at(ip) > m_pe_threshold){
	      tdc = ip * timeStep;
	  // Only the first threshold crossing
	      break;
	    }
	  }
	  double tot = std::count_if(waveform.begin(), waveform.end(), [this](double value) {
	    return value > m_pe_threshold;});
	  tot *= timeStep;

	  // Create digitized signal with PMT channel, timing window, and measurements
          digits_container::digit signal(pmt, tdc,
          // Calculate ADC value proportional to collected photo-electrons
          // for now, we just use the number of PEs as the ADC value.
          // This can be improved by using a more realistic response function.
                                         static_cast<double>(pe_count), tot);

          // Collect all truth hits from photo-electrons in this pulse
          auto it = start_pe;

          // Add truth hit information from all contributing photo-electrons
          while (it != this_pe) {
            signal.insert(*it);
            it++;
          }
          // Include the boundary photo-electron if it exists
          if (this_pe != pe_collection.end())
            signal.insert(*this_pe);

          // Store the digitized signal in output collection
          digi.digits.push_back(signal);
          // Store the waveform in output collection
          
          
          UFW_INFO("waveform sum: {}", std::accumulate(waveform.begin(), waveform.end(), 0.));
          waveforms.emplace_back(std::move(waveform));

          // Skip photo-electrons in the dead time window after signal detection
          while (this_pe->arrival_time < start_int_window + m_int_time_window + m_dead_time_window
                 && this_pe != pe_collection.end()) {
            this_pe++;
          }
          // Check if we've processed all photo-electrons
          if (this_pe == pe_collection.end())
            break;
          // Restart search from after dead time
          start_pe = std::next(this_pe);
	} else {
         // Pulse below threshold: advance starting point and continue searching
          if (start_pe == pe_collection.end())
            break;
          start_pe = std::next(start_pe);
        }
        // Update integration window with next starting photo-electron
        start_int_window = start_pe->arrival_time;
        this_pe          = start_pe;
      }
    }
  }
  
  double full_digi::single_pe_response(double act_time, double pe_time, double t0){
    double signal = 1./ (m_pmt_sigma * sqrt(2.0 * M_PI)) * exp(-0.5 * pow((act_time - pe_time) / m_pmt_sigma, 2)) * exp(-(act_time - pe_time)/m_pmt_tau);
    //double signal = 1./ (m_pmt_sigma * sqrt(2.0 * M_PI)) * exp(-0.5 * pow((act_time - pe_time) / m_pmt_sigma, 2)) * exp(-(act_time-t0)/m_pmt_tau);

    return signal;
  }

  void full_digi::norm_waveform(reco::waveform signal, double DeltaT){
	  //Normalize waveform assuming 1 pe ~ 1 mV, so peak in mV corresponds to the number of pe, and check signal saturation
    double maxwf = *std::max_element(signal.begin(), signal.end());
    int area = std::accumulate(signal.begin(), signal.end(), 0);
    double scaleFactor = maxwf/(area * DeltaT);
    std::transform(signal.begin(), signal.end(), signal.begin(), [scaleFactor](int x) {return x * scaleFactor;});
	  
    // Check saturation (at ~ 3.2 V)
    std::vector<double>diff;
    for (int is = 0; is < signal.size(); is++){
      if( signal.at(is) > m_saturation_level){
	signal.at(is) = m_saturation_level;
	diff.push_back(signal.at(is) - m_saturation_level);
      }
    }
    // for (int id = 0; id < diff.size(); id++){
      
    //} 
    
  }

} // namespace sand::ecal
UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::ecal::full_digi)
