#include <edep_reader/edep_reader.hpp>
#include <geoinfo/ecal_info.hpp>
#include <optical_simulation.hpp>
#include <ufw/process.hpp>
#include <ecal/pes.h>

namespace sand::ecal {

  /// Configure the optical simulation process by loading parameters from config
  void optical_simulation::configure(const ufw::config& cfg) {
    process::configure(cfg);
    // Load the light yield (photons per MeV) from configuration
    m_light_yield = cfg.at("light_yield");
  }

  /// Constructor: Initialize the optical simulation process with output PES data
  optical_simulation::optical_simulation() : process({}, {{"pes", "sand::ecal::pes"}}) {
    UFW_DEBUG("Creating ECAL optical simulation process at {}", fmt::ptr(this));
  }

  /// Main simulation loop: Convert energy deposits to photo-electrons with arrival times
  void optical_simulation::run() {
    UFW_DEBUG("Running ECAL optical simulation process at {}", fmt::ptr(this));
    // Get input energy deposit data and ECAL geometry information
    const auto& tree  = get<sand::edep_reader>();
    const auto& gecal = get<geoinfo>().ecal();
    // Get output photo-electron collection
    auto& pes = set<sand::ecal::pes>("pes");

    // Process each trajectory from the energy deposit record
    for (const auto& trj : tree) {
      const auto& hit_map = trj.GetHitMap();
      // Skip trajectories with no ECAL hits
      if (!trj.HasHitInDetector(component::ECAL))
        continue;

      // Pointer to the current ECAL cell being processed
      sand::geoinfo::ecal_info::cell const* pcell = nullptr;

      // Process each energy deposit hit in the ECAL
      for (const auto& hit : hit_map.at(component::ECAL)) {
        // Calculate hit position (midpoint between start and stop)
        auto phit = 0.5 * (hit.GetStart() + hit.GetStop());
        pos_3d h_pos(phit.X(), phit.Y(), phit.Z());
        // Get hit time and energy deposit
        auto h_t  = phit.T();
        auto h_de = hit.GetEnergyDeposit();
        // Find the ECAL cell containing this hit position
        try {
          auto& cell = gecal.at(h_pos);
          pcell      = &cell;
        } catch (const std::invalid_argument& e) {
          // Skip hits outside the ECAL geometry
          continue;
        }

        // Get fiber properties for this cell
        auto& fiber = pcell->get_fiber();
        // Get the cell ID for channel identification
        auto cid = pcell->id();

        using face_location = geoinfo::ecal_info::face_location;

        for (auto fl : std::array{face_location::begin, face_location::end}) {
          // Calculate path lengths from hit position to both ends of the fiber
          auto l = pcell->pathlength(h_pos, fl);
          // Calculate light attenuation for each path
          auto at = pcell->attenuation(l);
          // Generate number of scintillation photons reaching each PMT end
          auto nph = de_to_nphotons(h_de, at);
          // Generate photon-electrons
          for (int i = 0; i < nph; i++) {
            // Calculate total arrival time: initial time + scintillation + propagation
            auto arrival_time = h_t + scintillation_time(fiber.scintillation_rise_time, fiber.scintillation_decay_time)
                              + propagation_time(l, fiber.light_velocity);
            // Create photo-electron with truth hit ID and calculated arrival time
            pes::pe pe_(hit.GetId(), arrival_time);
            // Build complete channel ID from cell geometry
            channel_id chid = gecal.channel({cid, fl});
            // Store photo-electron in output collection
            pes.collection[chid].emplace_back(std::move(pe_));
          }
        }
      }
    }
  }

  /// Generate number of photons from energy deposit considering light yield and attenuation
  /// Uses Poisson statistics for realistic photon production fluctuations
  int optical_simulation::de_to_nphotons(double de, double attenuation) {
    // Expected number of photons: light_yield * energy * attenuation factor
    std::poisson_distribution<int> poisson(m_light_yield * de * attenuation);
    return poisson(random_engine());
  };

  /// Generate scintillation emission time using rise and decay time constants
  /// Implements acceptance-rejection method based on Geant4 scintillation model
  /// See:
  /// https://github.com/Geant4/geant4/blob/e58e650b32b961c8093f3dd6a2c3bc917b2552be/source/processes/electromagnetic/xrays/src/G4Scintillation.cc#L638
  double optical_simulation::scintillation_time(double rise_time, double decay_time) {
    double t;
    static std::uniform_real_distribution<> rnd_unif(0.0, 1.0);

    // Use acceptance-rejection method to sample from scintillation time distribution
    do {
      // Generate time from exponential distribution (envelope function)
      t = -1.0 * decay_time * std::log(1.0 - rnd_unif(random_engine()));
    } while (rnd_unif(random_engine()) > (1.0 - std::exp(-t / rise_time)));

    return t;
  }

} // namespace sand::ecal