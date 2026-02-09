#include <ufw/factory.hpp>
#include <ufw/process.hpp>

namespace sand::ecal {

  class optical_simulation : public ufw::process {
   public:
    optical_simulation();
    void configure(const ufw::config& cfg) override;
    void run() override;

   private:
    inline double de_to_nphotons(double de, double attenuation) const { return m_light_yield * de * attenuation; };
    double scintillation_time(double rise_time, double decay_time) const;
    inline double propagation_time(double pathlentgh, double velocity) const { return pathlentgh / velocity; };

   private:
    double m_light_yield;
  };
} // namespace sand::ecal

UFW_REGISTER_PROCESS(sand::ecal::optical_simulation)
UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::ecal::optical_simulation)
