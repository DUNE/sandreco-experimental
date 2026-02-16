
#include <random>

#include <edep_reader/edep_reader.hpp>
#include <geoinfo/ecal_info.hpp>
#include <optical_simulation.hpp>
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
        // Calculate path lengths from hit position to both ends of the fiber
        auto l1 = pcell->pathlength(h_pos, sand::geoinfo::ecal_info::face_location::begin);
        auto l2 = pcell->pathlength(h_pos, sand::geoinfo::ecal_info::face_location::end);
        // Calculate light attenuation for each path
        auto at1 = pcell->attenuation(l1);
        auto at2 = pcell->attenuation(l2);
        // Get the cell ID for channel identification
        auto cid = pcell->id();
        // Generate number of scintillation photons reaching each PMT end
        auto nph1 = de_to_nphotons(h_de, at1);
        auto nph2 = de_to_nphotons(h_de, at2);
        // Generate photon-electrons for the first fiber end (X_PLUS side)
        for (int i = 0; i < nph1; i++) {
          // Calculate total arrival time: initial time + scintillation + propagation
          auto arrival_time = h_t + scintillation_time(fiber.scintillation_rise_time, fiber.scintillation_decay_time)
                            + propagation_time(l1, fiber.light_velocity);
          // Create photo-electron with truth hit ID and calculated arrival time
          pes::pe pe_(hit.GetId(), arrival_time);
          // Build complete channel ID from cell geometry
          channel_id chid;
          chid.ecal_pmt.subdetector   = sand::subdetector_t::ECAL;
          chid.ecal_pmt.region        = static_cast<sand::channel_id::region_t>(cid.region);
          chid.ecal_pmt.module_number = sand::channel_id::module_t(cid.module_number);
          chid.ecal_pmt.row           = sand::channel_id::row_t(cid.row);
          chid.ecal_pmt.column        = sand::channel_id::column_t(cid.column);
          chid.ecal_pmt.side          = sand::channel_id::side_t::X_PLUS;
          // Store photo-electron in output collection
          pes.collection[chid].emplace_back(std::move(pe_));
        }
        // Generate photon-electrons for the second fiber end (X_MINUS side)
        for (int i = 0; i < nph2; i++) {
          // Calculate total arrival time: initial time + scintillation + propagation
          auto arrival_time = h_t + scintillation_time(fiber.scintillation_rise_time, fiber.scintillation_decay_time)
                            + propagation_time(l2, fiber.light_velocity);
          // Create photo-electron with truth hit ID and calculated arrival time
          pes::pe pe_(hit.GetId(), arrival_time);
          // Build complete channel ID from cell geometry
          channel_id chid;
          chid.ecal_pmt.subdetector   = sand::subdetector_t::ECAL;
          chid.ecal_pmt.region        = static_cast<sand::channel_id::region_t>(cid.region);
          chid.ecal_pmt.module_number = sand::channel_id::module_t(cid.module_number);
          chid.ecal_pmt.row           = sand::channel_id::row_t(cid.row);
          chid.ecal_pmt.column        = sand::channel_id::column_t(cid.column);
          chid.ecal_pmt.side          = sand::channel_id::side_t::X_MINUS;
          // Store photo-electron in output collection
          pes.collection[chid].emplace_back(std::move(pe_));
        }
      }
    }
  }

  /// Generate number of photons from energy deposit considering light yield and attenuation
  /// Uses Poisson statistics for realistic photon production fluctuations
  int optical_simulation::de_to_nphotons(double de, double attenuation) const {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    // Expected number of photons: light_yield * energy * attenuation factor
    std::poisson_distribution<int> poisson(m_light_yield * de * attenuation);
    return poisson(gen);
  };

  /// Generate scintillation emission time using rise and decay time constants
  /// Implements acceptance-rejection method based on Geant4 scintillation model
  /// See:
  /// https://github.com/Geant4/geant4/blob/e58e650b32b961c8093f3dd6a2c3bc917b2552be/source/processes/electromagnetic/xrays/src/G4Scintillation.cc#L638
  double optical_simulation::scintillation_time(double rise_time, double decay_time) const {
    double t;
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_real_distribution<> rnd_unif(0.0, 1.0);

    // Use acceptance-rejection method to sample from scintillation time distribution
    do {
      // Generate time from exponential distribution (envelope function)
      t = -1.0 * decay_time * std::log(1.0 - rnd_unif(gen));
    } while (rnd_unif(gen) > (1.0 - std::exp(-t / rise_time)));

    return t;
  }

} // namespace sand::ecal