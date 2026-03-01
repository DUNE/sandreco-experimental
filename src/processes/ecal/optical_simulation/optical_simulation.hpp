#include <ufw/factory.hpp>
#include <ufw/process.hpp>

namespace sand::ecal {

  class optical_simulation : public ufw::process {
   public:
    optical_simulation();
    void configure(const ufw::config& cfg) override;
    void run() override;

   private:
    /// @brief Convert deposited energy to number of scintillation photons
    /// @param de Deposited energy
    /// @param attenuation Attenuation factor for the medium
    /// @return Number of photons produced
    int de_to_nphotons(double de, double attenuation);

    /// @brief Calculate scintillation photon emission time
    /// @param rise_time Scintillation rise time constant
    /// @param decay_time Scintillation decay time constant
    /// @return Emission time of the photon
    double scintillation_time(double rise_time, double decay_time);

    /// @brief Calculate light propagation time through medium
    /// @param pathlentgh Path length traveled by photon
    /// @param velocity Velocity of light in the medium
    /// @return Time for photon to travel the path
    inline double propagation_time(double pathlentgh, double velocity) const { return pathlentgh / velocity; };

   private:
    /// @brief Scintillation light yield (photons per MeV)
    double m_light_yield;
  };
} // namespace sand::ecal

UFW_REGISTER_PROCESS(sand::ecal::optical_simulation)
UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::ecal::optical_simulation)
