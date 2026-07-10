/**
 * @file
 * @brief Implementation of @ref sand::geoinfo: top-level geometry, tracker-type
 *        detection and lazy construction of the subdetector descriptions.
 */

#include <common/sand.h>

#include <ufw/config.hpp>
#include <ufw/context.hpp>

#include <common/sand.h>

#include <drift_info.hpp>
#include <generic_drift_info.hpp>
#include <ecal_info.hpp>
#include <geoinfo.hpp>
#include <grain_info.hpp>
#include <stt_info.hpp>
#include <tracker_info.hpp>

#include <TFile.h>
#include <TGeoNavigator.h>

#include <root_tgeomanager/root_tgeomanager.hpp>

namespace sand {

  namespace {
    constexpr char root_path[]  = "/volWorld_PV_1/rockBox_lv_PV_0/volDetEnclosure_PV_0/volSAND_PV_0/MagIntVol_volume_PV_0/";
    constexpr char ecal_path[]  = "kloe_calo_volume_PV_0/";
    constexpr char inner_path[] = "sand_inner_volume_PV_0/";
    constexpr char grain_path[] = "GRAIN_lv_PV_0/GRAIN_LAr_lv_PV_0/";
    constexpr char stt_path[]   = "STTtracker_PV_0/";
    constexpr char drift_path[] = "SANDtracker_PV_0/";
  }

  /**
   * @brief Builds the top-level geometry description and detects the tracker type.
   *
   * Reads the per-subdetector configuration (each passed as its own JSON object,
   * with global parameters as top-level keys) and, by inspecting the geometry
   * tree, decides whether the tracker is an STT or a drift chamber, merging the
   * matching subdetector parameters into the tracker configuration.
   *
   * @param cfg The geoinfo configuration object.
   */
  geoinfo::geoinfo(const ufw::config& cfg) {
    m_root_path            = cfg.value("basepath", sand::root_path);
    m_grain_cfg            = cfg.value("grain", ufw::json::object());
    m_ecal_cfg             = cfg.value("ecal", ufw::json::object());
    m_tracker_cfg          = cfg.value("tracker", ufw::json::object());
    auto drift_cfg         = cfg.value("drift", ufw::json::object());
    auto stt_cfg           = cfg.value("stt", ufw::json::object());
    auto generic_drift_cfg = cfg.value("generic_drift", ufw::json::object());

    auto& tgm = ufw::context::current()->instance<root_tgeomanager>();
    auto nav  = tgm.navigator();

    UFW_DEBUG("Using root path '{}'.", m_root_path.c_str());
    nav->cd(m_root_path.c_str());

    auto inner = m_root_path / inner_path;

    nav->cd(inner);
    bool isSTT = false;
    nav->for_each_node([&isSTT](auto node){
      std::string name = node->GetName();
      isSTT |= (name.find("STTtracker") != std::string::npos);
    });

    bool isGenericDrift = false; /// CHECKME!!!
    for (int d = 0; d < nav->GetCurrentNode()->GetNdaughters(); ++d) {
      std::string daughter_tmp = nav->GetCurrentNode()->GetDaughter(d)->GetName(); // SANDtracker_PV_0
      if (daughter_tmp.find("SANDtracker") != std::string::npos) {
        UFW_DEBUG("DAUGHTER vol '{}' ", daughter_tmp.c_str());
        std::string daughter_path = inner / daughter_tmp.c_str();
        nav->cd(daughter_path.c_str());
        std::string granddaughter_tmp = nav->GetCurrentNode()->GetDaughter(0)->GetName(); // s_...
        UFW_DEBUG("GRANDDAUGHTER vol '{}' ", granddaughter_tmp.c_str());
        if (granddaughter_tmp.find("s_") != std::string::npos){
          isGenericDrift = true;
        }
        nav->CdUp();
        break;
      }
    }

    if (isSTT) {
      UFW_INFO("STT subdetector implementation detected.");
      m_tracker_cfg.update(stt_cfg);
      m_tracker_type = STT;
    } else if (isGenericDrift){ /// FIXME!!!
      UFW_INFO("Generic Drift subdetector implementation detected.");
      m_tracker_cfg.update(generic_drift_cfg);
      m_tracker_type = GENERIC_DRIFT;
    } else {
      UFW_INFO("Drift subdetector implementation detected.");
      m_tracker_type = DRIFT;
      m_tracker_cfg.update(drift_cfg);
    }
  }

  /// @brief Default destructor.
  geoinfo::~geoinfo() = default;

  /// @brief Lazily constructs the ECAL geometry description.
  void geoinfo::init_ecal() const {
    m_ecal.reset(new ecal_info(*this, ecal_path, m_ecal_cfg));
  }

  /// @brief Lazily constructs the GRAIN geometry description.
  void geoinfo::init_grain() const {
    m_grain.reset(new grain_info(*this, geo_path(inner_path) / grain_path, m_grain_cfg));
  }

  /// @brief constructs the tracker geometry (STT or drift) for the detected type.
  void geoinfo::init_tracker() const {
    switch (m_tracker_type) {
      case DRIFT:
      m_tracker.reset(new drift_info(*this, geo_path(inner_path) / drift_path, m_tracker_cfg));
      return;
      case STT:
      m_tracker.reset(new stt_info(*this, geo_path(inner_path) / stt_path, m_tracker_cfg));
      return;
      case GENERIC_DRIFT:
      m_tracker.reset(new generic_drift_info(*this, geo_path(inner_path) / drift_path, m_tracker_cfg));
      return;
      default:
      UFW_ERROR("Unknown tracker type");
    }
  }

  /**
   * @brief Maps a geometry path to a geometry identifier.
   * @param gp The geometry path.
   * @return The corresponding geo_id (currently a default-constructed placeholder).
   */
  geo_id geoinfo::id(const geo_path& gp) const {
    geo_id gi;
    return gi;
  }

  /**
   * @brief Maps a geometry identifier back to its full geometry path.
   * @param gi The geometry identifier.
   * @return The full geometry path, dispatched to the relevant subdetector.
   */
  geo_path geoinfo::path(geo_id gi) const {
    geo_path gp(m_root_path);
    switch (gi.subdetector) {
    case DRIFT:
    case STT:
      gp /= m_tracker->path(gi);
      break;
    case ECAL:
      gp /= m_ecal->path(gi);
      break;
    case GRAIN:
      gp /= m_grain->path(gi);
      break;
    case MUON:
    case NONE:
    default:
      UFW_ERROR("Subdetector '{}' not supported.", gi.subdetector);
    }
    return gp;
  }

  // Solve  p+t*dir = v1 + s*(v2-v1)
  /**
   * @brief Intersection of an infinite line with a segment, in the XY plane.
   *
   * Solves p + t*dir = v1 + s*(v2 - v1) and accepts the solution only when it
   * lies on the segment (s in [0,1]); the Z of the result is taken from @p p.
   *
   * @param v1 First endpoint of the segment.
   * @param v2 Second endpoint of the segment.
   * @param p   A point on the line.
   * @param dir Direction of the line.
   * @param[out] intersection_point The intersection, set only when the function returns true.
   * @return true if the line meets the segment.
   */
  bool geoinfo::getXYLineSegmentIntersection(
    const pos_3d& v1,
    const pos_3d& v2,
    const pos_3d& p,
    const dir_3d& dir,
    pos_3d& intersection_point)
    {
        // Segment direction vector
        double delta_x = v1.X() - v2.X();
        double delta_y = v1.Y() - v2.Y();

        // Determinant for solving the linear system
        double det = dir.X() * delta_y - dir.Y() * delta_x;

        // If det is ~0, the lines are parallel or nearly parallel → no intersection
        if (std::fabs(det) < 1e-9) {
            return false;
        }

        // Solve parametric values t (for infinite line p + t*dir) and s (segment)
        double t = ((v1.X() - p.X()) * delta_y -
                    (v1.Y() - p.Y()) * delta_x) / det;

        double s = ((p.X() - v1.X()) * dir.Y() -
                    (p.Y() - v1.Y()) * dir.X()) / det;

        // Intersection must lie on the segment (s ∈ [0,1])
        if (s < 0.0 || s > 1.0) {
            return false;
        }

        // Compute intersection point 
        intersection_point = p;
        intersection_point.SetX(p.X() + t * dir.X());
        intersection_point.SetY(p.Y() + t * dir.Y());
        intersection_point.SetZ(p.Z());


        return true;
    }

  /**
   * @brief All intersections of an infinite line with a polygon, in the XY plane.
   * @param v   The polygon vertices, in order.
   * @param p   A point on the line.
   * @param dir Direction of the line.
   * @return The intersection points with the polygon edges.
   */
  std::vector<pos_3d> geoinfo::getXYLinePolygonIntersections(
    const std::vector<pos_3d>& v,
    const pos_3d& p,
    const dir_3d& dir){
      std::vector<pos_3d> intersections;
      for (int i = 0; i < v.size(); i++) {
        pos_3d intersection;
        const auto& current_v = v[i];
        const auto& next_v    = v[(i + 1) % v.size()]; // wraps to first
        if(getXYLineSegmentIntersection(current_v, next_v, p, dir, intersection)) {
          intersections.push_back(intersection);
        }
      }
      return intersections;
    }



} // namespace sand
