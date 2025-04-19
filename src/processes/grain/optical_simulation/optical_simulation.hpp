#include <filesystem>
#include <deque>

#include <ufw/context.hpp>
#include <ufw/config.hpp>
#include <ufw/data.hpp>
#include <ufw/factory.hpp>
#include <ufw/process.hpp>

#include <grain/photons.h>

#include <TH1D.h>

namespace sand::grain {

class geant_run_manager;

  class optical_simulation : public ufw::process {

  public:
    enum OpticsType {
      MASK,
      LENS,
      LENS_DOPED
    };

    struct properties_t {
      std::unique_ptr<TH1D> m_fast_component_distribution;
      std::unique_ptr<TH1D> m_slow_component_distribution;
      double m_tau_fast;
      double m_tau_slow;
      double m_scintillation_yield;
    };

    optical_simulation();

    void configure (const ufw::config& cfg) override;
    void run() override;

    OpticsType opticsType() const {return m_optics_type;}

    const properties_t& properties();

    double energySplitThreshold() const {return m_energy_split_threshold;}
    int GetEventsNumber();
    bool getStartRun() const {return m_run_start;}
    void setStartRun(bool run_start) {m_run_start = run_start;}
    bool getNewIteration() const {return m_new_iteration;}
    void setNewIteration(bool new_iteration) {m_new_iteration = new_iteration;}

  mutable std::deque<int> track_ids;
  mutable std::deque<double> track_times;

  private:
    void init_properties();

  private:
    OpticsType m_optics_type;
    std::filesystem::path m_geometry;
    double m_energy_split_threshold = 100; // MeV
    bool m_new_iteration;
    bool m_run_start;
    std::unique_ptr<properties_t> m_properties;

  };

}

UFW_REGISTER_PROCESS(sand::grain::optical_simulation)
