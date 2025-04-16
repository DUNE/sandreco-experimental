#include <filesystem>
#include <deque>

#include <ufw/context.hpp>
#include <ufw/config.hpp>
#include <ufw/data.hpp>
#include <ufw/factory.hpp>
#include <ufw/process.hpp>

#include <grain/photons.h>

#include "DetectorConstruction.hh"
#include "ActionInitialization.hh"
#include "AnalysisManager.hh"
#include "PhysicsList.hh"

#include <root/TTreeStreamer.hpp>

#include "Randomize.hh"
#include "TSystem.h"

class G4_optmen_runmanager;

class G4_optmen_edepsim : public ufw::process {

  public:
  enum OpticsType {
    MASK,
    LENS,
    LENS_DOPED
  };

  G4_optmen_edepsim();
  void configure (const ufw::config& cfg) override;
  // const ufw::var_type_map& products() const override;
  // const ufw::var_type_map& requirements() const override;
  void run(const ufw::var_id_map& inputs, const ufw::var_id_map& outputs) override;
  const ufw::public_id& outputVariableName() const {return m_output_variable_name;}
  OpticsType opticsType() const {return m_optics_type;}
  double energySplitThreshold() const {return m_energy_split_threshold;}
  int GetEventsNumber();
  bool getStartRun() const {return m_run_start;}
  void setStartRun(bool run_start) {m_run_start = run_start;}
  bool getNewIteration() const {return m_new_iteration;}
  void setNewIteration(bool new_iteration) {m_new_iteration = new_iteration;}

  mutable std::deque<int> track_ids;
  mutable std::deque<double> track_times;

  private:
    ufw::public_id m_output_variable_name;
    OpticsType m_optics_type;
    std::filesystem::path m_geometry;
    double m_energy_split_threshold = 100; // MeV
    bool m_new_iteration;
    bool m_run_start;
};
  
UFW_REGISTER_PROCESS(G4_optmen_edepsim)