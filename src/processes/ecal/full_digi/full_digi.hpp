#include <common/data.h>
#include <common/waveform.h>

#include <ufw/process.hpp>

namespace sand::ecal {
  class full_digi : public ufw::process {
   public:
    full_digi();
    void configure(const ufw::config& cfg) override;
    void run() override;

   private:

   double single_pe_response(double, double, double);

   void norm_waveform(reco::waveform, double);
   
  /// @brief Integration time window for signal accumulation
    double m_int_time_window;

  /// @brief Dead time window preventing pulse pile-up detection
    double m_dead_time_window;

  /// @brief Threshold for photo-electron detection
    double m_pe_threshold;

  /// @brief Integration interval for pmt signal 
    double m_pmt_signal_length;

  /// @brief Number of samples of the waveform  
    int m_samples;
    
  /// @brief Standard deviation of the gaussian response of the PMT
    double m_pmt_sigma;
    
  /// @brief Decay time of the PMT signal
    double m_pmt_tau;

 /// @brief Time offset for calculation of the waveform
    double m_time_offset;

 /// @brief Saturation of PMT
    double m_saturation_level;

    };
} // namespace sand::ecal

UFW_REGISTER_PROCESS(sand::ecal::full_digi)
