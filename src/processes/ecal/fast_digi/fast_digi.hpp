#include <ufw/factory.hpp>
#include <ufw/process.hpp>

namespace sand::ecal {

  class fast_digi : public ufw::process {
   public:
    fast_digi();
    void configure(const ufw::config& cfg) override;
    void run() override;

   private:
    /// @brief Integration time window for signal accumulation
    double m_int_time_window;

    /// @brief Dead time window preventing pulse pile-up detection
    double m_dead_time_window;

    /// @brief Threshold for photo-electron detection
    double m_pe_threshold;

    /// @brief Constant fraction for timing discrimination
    double m_costant_fraction;
  };
} // namespace sand::ecal

UFW_REGISTER_PROCESS(sand::ecal::fast_digi)
UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::ecal::fast_digi)
