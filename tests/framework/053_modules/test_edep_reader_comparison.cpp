#include <edep_reader/edep_reader.hpp>
#include <edep_reader_refactor/edep_reader_refactor.hpp>

#include <ufw/config.hpp>
#include <ufw/context.hpp>
#include <ufw/factory.hpp>
#include <ufw/process.hpp>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <vector>

namespace sand::test {

  /// Reads the same event with sand::edep_reader and sand::edep_reader_refactor and verifies that the two
  /// trees are equivalent: same trajectories in the same order, same relations, hits, trajectory points,
  /// entering/exiting flags and hit-to-trajectory lookups.
  ///
  /// The first/last points per subdetector are not compared: for points in volumes matching no known name,
  /// the legacy reader uses an uninitialised subdetector, so its result is undefined there.
  class edep_reader_comparison : public ufw::process {
   public:
    edep_reader_comparison();
    void run() override;
  };

  edep_reader_comparison::edep_reader_comparison() : process({}, {}) {}

  namespace {
    template <typename Hits>
    std::vector<int> sorted_hit_ids(Hits const& hits) {
      std::vector<int> ids;
      ids.reserve(hits.size());
      for (auto const& h : hits) {
        ids.push_back(h.GetId());
      }
      std::sort(ids.begin(), ids.end());
      return ids;
    }

    void compare_hits(EDEPTrajectory const& o, edep_refactor::EDEPTrajectory const& n) {
      UFW_ASSERT(o.GetHitMap().size() == n.GetHitMap().size(), "Trajectory {}: different number of hit detectors",
                 o.GetId());
      for (auto const& [comp, o_hits] : o.GetHitMap()) {
        auto n_it = n.GetHitMap().find(comp);
        UFW_ASSERT(n_it != n.GetHitMap().end(), "Trajectory {}: hits missing for detector {}", o.GetId(),
                   static_cast<int>(comp));
        auto const& n_hits = n_it->second;
        UFW_ASSERT(o_hits.size() == n_hits.size(), "Trajectory {}: {} vs {} hits in detector {}", o.GetId(),
                   o_hits.size(), n_hits.size(), static_cast<int>(comp));
        // equal times may be ordered differently, so ids are compared as sets and times in order
        UFW_ASSERT(sorted_hit_ids(o_hits) == sorted_hit_ids(n_hits), "Trajectory {}: different hit ids", o.GetId());
        for (std::size_t k = 0; k != o_hits.size(); ++k) {
          UFW_ASSERT(o_hits[k].GetStart().T() == n_hits[k].GetStart().T(), "Trajectory {}: hits not time ordered",
                     o.GetId());
        }
        // the hit with a given id must carry the same data
        for (auto const& oh : o_hits) {
          auto nh = std::find_if(n_hits.begin(), n_hits.end(), [&](auto const& h) { return h.GetId() == oh.GetId(); });
          UFW_ASSERT(nh != n_hits.end(), "Hit {} missing", oh.GetId());
          UFW_ASSERT(oh.GetEnergyDeposit() == nh->GetEnergyDeposit() && oh.GetSecondaryDeposit() == nh->GetSecondaryDeposit()
                         && oh.GetTrackLength() == nh->GetTrackLength() && oh.GetContrib() == nh->GetContrib()
                         && oh.GetPrimaryId() == nh->GetPrimaryId() && oh.GetStart() == nh->GetStart()
                         && oh.GetStop() == nh->GetStop(),
                     "Hit {}: different content", oh.GetId());
        }
      }
    }

    void compare_points(EDEPTrajectory const& o, edep_refactor::EDEPTrajectory const& n) {
      UFW_ASSERT(o.GetTrajectoryPoints().size() == n.GetTrajectoryPoints().size(),
                 "Trajectory {}: different number of point detectors", o.GetId());
      for (auto const& [comp, o_pts] : o.GetTrajectoryPoints()) {
        auto n_it = n.GetTrajectoryPoints().find(comp);
        UFW_ASSERT(n_it != n.GetTrajectoryPoints().end(), "Trajectory {}: points missing for detector {}", o.GetId(),
                   static_cast<int>(comp));
        auto const& n_pts = n_it->second;
        UFW_ASSERT(o_pts.size() == n_pts.size(), "Trajectory {}: {} vs {} points in detector {}", o.GetId(),
                   o_pts.size(), n_pts.size(), static_cast<int>(comp));
        for (std::size_t k = 0; k != o_pts.size(); ++k) {
          UFW_ASSERT(o_pts[k].GetPosition() == n_pts[k].GetPosition() && o_pts[k].GetMomentum() == n_pts[k].GetMomentum()
                         && o_pts[k].GetProcess() == n_pts[k].GetProcess()
                         && o_pts[k].GetSubprocess() == n_pts[k].GetSubprocess(),
                     "Trajectory {}: point {} differs", o.GetId(), k);
        }
      }
      for (auto const& [comp, name] : sand::component_to_string) {
        UFW_ASSERT(o.IsEntering(comp) == n.IsEntering(comp), "Trajectory {}: entering flag differs for {}", o.GetId(),
                   name);
        UFW_ASSERT(o.IsExiting(comp) == n.IsExiting(comp), "Trajectory {}: exiting flag differs for {}", o.GetId(),
                   name);
      }
    }

    void compare_trajectories(EDEPTrajectory const& o, edep_refactor::EDEPTrajectory const& n) {
      UFW_ASSERT(o.GetId() == n.GetId(), "Different trajectory id {} vs {}", o.GetId(), n.GetId());
      UFW_ASSERT(o.GetParentId() == n.GetParentId(), "Trajectory {}: different parent id", o.GetId());
      UFW_ASSERT(o.GetParent()->GetId() == n.GetParent()->GetId(), "Trajectory {}: different parent", o.GetId());
      UFW_ASSERT(o.GetDepth() == n.GetDepth(), "Trajectory {}: different depth", o.GetId());
      UFW_ASSERT(o.GetPDGCode() == n.GetPDGCode(), "Trajectory {}: different PDG code", o.GetId());
      UFW_ASSERT(o.GetInitialMomentum() == n.GetInitialMomentum(), "Trajectory {}: different initial momentum",
                 o.GetId());
      UFW_ASSERT(o.GetInteractionNumber() == n.GetInteractionNumber(), "Trajectory {}: different interaction number",
                 o.GetId());
      UFW_ASSERT(o.GetReaction() == n.GetReaction(), "Trajectory {}: different reaction", o.GetId());
      UFW_ASSERT(o.GetChildrenTrajectories().size() == n.GetChildrenTrajectories().size(),
                 "Trajectory {}: different number of children", o.GetId());
      for (std::size_t k = 0; k != o.GetChildrenTrajectories().size(); ++k) {
        UFW_ASSERT(o.GetChildrenTrajectories()[k].GetId() == n.GetChildrenTrajectories()[k].GetId(),
                   "Trajectory {}: children differ", o.GetId());
      }
      compare_hits(o, n);
      compare_points(o, n);
    }
  } // namespace

  void edep_reader_comparison::run() {
    using clock = std::chrono::steady_clock;
    // Cumulative over the contexts of the job. The first context of each reader also includes opening the file.
    static double total_legacy{}, total_novel{};

    // instance() reads the event and builds the tree: this is the time to compare
    auto const t0 = clock::now();
    auto const& novel = ufw::context::current()->instance<sand::edep_reader_refactor>();
    auto const t1 = clock::now();
    double const novel_s  = std::chrono::duration<double>(t1 - t1).count();
    
    auto& legacy  = ufw::context::current()->instance<sand::edep_reader>();
    auto const t2 = clock::now();
    double const legacy_s = std::chrono::duration<double>(t2 - t0).count();

    total_legacy += legacy_s;
    total_novel += novel_s;

    // same underlying event
    UFW_ASSERT(legacy.event().Trajectories.size() == novel.event().Trajectories.size(),
               "The two readers see different events");

    // whole tree, depth-first
    std::size_t n_trj{};
    auto o_it = legacy.begin();
    auto n_it = novel.begin();
    for (; o_it != legacy.end() && n_it != novel.end(); ++o_it, ++n_it, ++n_trj) {
      compare_trajectories(*o_it, *n_it);
    }
    UFW_ASSERT(o_it == legacy.end() && n_it == novel.end(), "Different number of trajectories in the tree");
    UFW_ASSERT(legacy.GetChildrenTrajectories().size() == novel.GetChildrenTrajectories().size(),
               "Different number of primaries");

    // lookups by trajectory id and by hit id
    std::size_t n_hits{};
    for (auto const& det : legacy.event().SegmentDetectors) {
      n_hits += det.second.size();
    }
    for (auto const& trj : novel) {
      UFW_ASSERT(novel.GetTrajectory(trj.GetId())->GetId() == trj.GetId(), "Lookup of trajectory {} failed",
                 trj.GetId());
    }
    UFW_ASSERT(novel.GetTrajectory(-12345) == novel.end(), "Unknown trajectory found");
    for (int id = 0; id != static_cast<int>(n_hits) + 1; ++id) { // one past the end is not a hit
      auto lt = legacy.GetTrajectoryWithHitId(id);
      auto nt = novel.GetTrajectoryWithHitId(id);
      UFW_ASSERT((lt == legacy.end()) == (nt == novel.end()), "Hit {}: only one reader finds its trajectory", id);
      if (nt != novel.end()) {
        UFW_ASSERT(lt->GetId() == nt->GetId(), "Hit {}: different trajectory", id);
        UFW_ASSERT(novel.GetHit(id) != nullptr && novel.GetHit(id)->GetId() == id, "GetHit({}) failed", id);
      } else {
        UFW_ASSERT(novel.GetHit(id) == nullptr, "GetHit({}) found a hit without trajectory", id);
      }
    }

    auto const t3 = clock::now();

    UFW_INFO("edep_reader_comparison: {} trajectories and {} hits are identical in both readers", n_trj, n_hits);
    UFW_INFO("edep_reader_comparison timing: tree build legacy {:.4f} s, refactor {:.4f} s (x{:.1f}); comparison {:.4f} s",
             legacy_s, novel_s, novel_s > 0. ? legacy_s / novel_s : 0., std::chrono::duration<double>(t3 - t2).count());
    UFW_INFO("edep_reader_comparison timing: cumulative tree build legacy {:.4f} s, refactor {:.4f} s (x{:.1f})",
             total_legacy, total_novel, total_novel > 0. ? total_legacy / total_novel : 0.);
  }

} // namespace sand::test

UFW_REGISTER_PROCESS(sand::test::edep_reader_comparison)
UFW_REGISTER_DYNAMIC_PROCESS_FACTORY(sand::test::edep_reader_comparison)
