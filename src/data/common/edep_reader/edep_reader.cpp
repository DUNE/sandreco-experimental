#include <TFile.h>
#include <TTree.h>

#include <edep_reader/edep_reader.hpp>

#include <ufw/config.hpp>
#include <ufw/context.hpp>

namespace sand {
  truth_adapter::value_type& truth_adapter::at(const index_type& id) {
    edep_reader& edep = ufw::context::current()->instance<edep_reader>();
    auto trajectory   = edep.GetTrajectoryWithHitId(id);
    UFW_ASSERT(trajectory != edep.end(), "Trajectory not found");
    UFW_DEBUG("Accessing hit with id {}, produced by particle with PDG: {}", id, trajectory->GetPDGCode());
    auto& hit = trajectory->GetHitWithId(id);
    UFW_ASSERT(hit.GetContrib() == trajectory->GetId(), "Hit does not belong to trajectory");
    UFW_ASSERT(trajectory == edep.GetTrajectory(hit.GetContrib()), "Trajectory has identity issues");
    return hit;
  }

  bool truth_adapter::valid(const index_type& id) {
    edep_reader& edep = ufw::context::current()->instance<edep_reader>();
    auto trajectory   = edep.GetTrajectoryWithHitId(id);
    return trajectory != edep.end();
  }

} // namespace sand

ufw::data::factory<sand::edep_reader>::factory(const ufw::config& cfg) : input_file(nullptr), event(new TG4Event()) {
  auto path = cfg.path_at("uri");
  input_file.reset(TFile::Open(path.c_str()));
  input_tree = input_file->Get<TTree>("EDepSimEvents");
  if (!input_tree) {
    UFW_ERROR("EDepSim tree not found in file '{}'.", path.c_str());
  }
  TBranch* br = input_tree->GetBranch("Event");
  if (!br) {
    UFW_ERROR("EDepSim branch \"Event\" not found in file '{}'.", path.c_str());
  }
  br->SetAddress(&event);
}

ufw::data::factory<sand::edep_reader>::~factory() = default;

sand::edep_reader& ufw::data::factory<sand::edep_reader>::instance(ufw::context_id i) {
  if (m_id != i) {
    input_tree->GetEntry(i);
    reader.InizializeFromEdep(*event);
    reader.m_event = event;
    m_id           = i;
  }
  return reader;
}
