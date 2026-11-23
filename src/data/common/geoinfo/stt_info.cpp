/**
 * @file
 * @brief Implementation of the STT geometry (@ref sand::geoinfo::stt_info):
 *        geometry-tree parsing and path/id mapping.
 */

#include <root_tgeomanager/root_tgeomanager.hpp>
#include <stt_info.hpp>
#include <ufw/context.hpp>

#include <TGeoBBox.h>
#include <TGeoMatrix.h>
#include <TGeoTube.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace sand {

  namespace {

    /**
     * @brief Midpoint of the gap between adjacent wire positions that is closest to a reference coordinate.
     * @param positions Transverse positions of the wires of one view [mm], in any order, possibly repeated.
     * @param reference The coordinate to stay close to [mm], usually the station centre.
     * @return The midpoint, or @p reference if there are fewer than two distinct positions.
     */
    double midpoint_between_wires(std::vector<double> positions, double reference) {
      constexpr double same = 1e-6; // mm
      std::sort(positions.begin(), positions.end());
      positions.erase(
          std::unique(positions.begin(), positions.end(), [](double a, double b) { return std::abs(a - b) < same; }),
          positions.end());
      double best      = reference;
      double best_dist = std::numeric_limits<double>::infinity();
      for (std::size_t i = 1; i < positions.size(); ++i) {
        const double mid = 0.5 * (positions[i - 1] + positions[i]);
        if (std::abs(mid - reference) < best_dist) {
          best      = mid;
          best_dist = std::abs(mid - reference);
        }
      }
      return best;
    }

    /**
     * @brief Point (x, y) where the material of an STT station is sampled.
     *
     * The straws have their wire on the axis: a line through the station centre can cross a wire head-on, and a
     * single wire (40 um of gold, 0.012 X0) weighs as much as a graphite target, while a track crosses one only
     * rarely. The line is moved, in each view, halfway between two adjacent wires (taken over all planes of that
     * view, so that staggered layers are included), at the gap closest to the station centre.
     *
     * @param stat The station, with its wires.
     * @return Global (x, y) of the sampling line [mm].
     */
    std::pair<double, double> stt_sampling_point(const geoinfo::tracker_info::station& stat) {
      const pos_3d centre = stat.top_north + 0.5 * (stat.bottom_south - stat.top_north);
      std::vector<double> xs; // positions of vertical wires (along y)
      std::vector<double> ys; // positions of horizontal wires (along x)
      for (const auto& w : stat.wires) {
        const dir_3d d   = w->direction();
        const pos_3d mid = w->head + 0.5 * d;
        if (std::abs(d.x()) > std::abs(d.y())) {
          ys.push_back(mid.y());
        } else {
          xs.push_back(mid.x());
        }
      }
      return {midpoint_between_wires(std::move(xs), centre.x()), midpoint_between_wires(std::move(ys), centre.y())};
    }

  } // namespace

  /**
   * @brief Builds the STT geometry by walking the ROOT geometry tree.
   *
   * Navigates each supermodule, classifies its target material, records the
   * station envelope and (for target planes) the target box and density, then
   * descends to every straw tube to create a wire with its endpoints, radius,
   * geometry id and DAQ channel. Wire adjacency is computed per station.
   *
   * @param gi  The parent geometry description.
   * @param gp  The geometry path of the STT volume.
   * @param cfg The tracker configuration (merged with STT-specific parameters).
   */
  geoinfo::stt_info::stt_info(const geoinfo& gi, const geo_path& gp, const ufw::config& cfg)
    : tracker_info(gi, gp, cfg) {
    auto& tgm    = ufw::context::current()->instance<root_tgeomanager>();
    auto nav     = tgm.navigator();
    auto sttpath = gi.root_path() / path();
    nav->cd(sttpath);
    nav->for_each_node([&](auto supermod) {
      std::string smodname = supermod->GetName();
      nav->cd(sttpath / smodname);
      auto stat = std::make_unique<station>();
      target_material tgt;
      if (smodname.find("TrkMod_") == 0) {
        tgt = TRKONLY;
      } else if (smodname.find("C3H6Mod_") == 0) {
        tgt = C3H6;
      } else if (smodname.find("CMod_") == 0) {
        tgt = CARBON;
      } else {
        UFW_ERROR("STT module '{}' has unrecognized material.", smodname);
      }
      stat->target          = tgt;
      TGeoBBox* plane_shape = dynamic_cast<TGeoBBox*>(supermod->GetVolume()->GetShape());
      if (!plane_shape) {
        UFW_ERROR("STT module '{}' has invalid shape.", smodname);
      }
      auto matrix  = nav->get_hmatrix();
      double* tran = matrix.GetTranslation();
      double* rot  = matrix.GetRotationMatrix();
      pos_3d centre;
      centre.SetCoordinates(tran);
      dir_3d halfsize(plane_shape->GetDX(), plane_shape->GetDY(), plane_shape->GetDZ());
      dir_3d boxcorner = nav->to_master(halfsize);
      stat->thickness  = 2.0 * boxcorner.Z();
      boxcorner.SetZ(0); // we ignore the thickness here.
      stat->top_north    = centre + boxcorner;
      stat->bottom_south = centre - boxcorner;
      boxcorner.SetX(-boxcorner.x());
      stat->top_south    = centre + boxcorner;
      stat->bottom_north = centre - boxcorner;
      stat->parent       = this;

      nav->for_each_node([&](auto plane) {
        // The plane does not carry useful information for us.
        std::string plname = plane->GetName();
        if (plname.find("target") != std::string::npos) {
          set_target(*stat, *nav, *plane);
        } else if (plname.find("plane") != std::string::npos) {
          nav->cd(sttpath / smodname / plname);
          nav->for_each_node([&](auto tube) {
            std::string tname = tube->GetName();
            nav->cd(sttpath / smodname / plname / tname);
            if (auto* tube_shape = dynamic_cast<TGeoTube*>(tube->GetVolume()->GetShape())) {
              auto matrix  = nav->get_hmatrix();
              double* tran = matrix.GetTranslation();
              double* rot  = matrix.GetRotationMatrix();
              pos_3d centre;
              centre.SetCoordinates(tran);
              dir_3d halfsize(0, 0, tube_shape->GetDZ());
              dir_3d globalhalfsize = nav->to_master(halfsize);
              auto w                = std::make_unique<wire>();
              w->parent             = stat.get();
              w->head               = centre + globalhalfsize;
              w->tail               = centre - globalhalfsize;
              w->max_radius         = tube_shape->GetRmax();
              w->geo                = id(geo_path(smodname.c_str()) / plname / tname);
              // FIXME temporary implementation of w->channel
              w->daq_channel.subdetector = STT;
              w->daq_channel.link        = w->geo.stt.supermodule;
              w->daq_channel.channel     = (uint32_t(w->geo.stt.plane) << 16) | uint32_t(w->geo.stt.tube);
              w->aabb                    = geoinfo::tracker_info::wire::AABB(*w);
              stat->daq_link             = w->geo.stt.supermodule;
              stat->wires.emplace_back(std::move(w));
            } else {
              UFW_ERROR("STT tube '{}' has unsupported shape type.", tname);
            }
          });
        }
      });

      stat->layers = sample_material_layers(*nav, sttpath / smodname, stt_sampling_point(*stat));
      stat->set_wire_adjacency();
      add_station(station_ptr(std::move(stat)));
    });
  }

  /// @brief Default destructor.
  geoinfo::stt_info::~stt_info() = default;

  /**
   * @brief Decodes an STT geometry path into a geometry identifier.
   *
   * Parses the supermodule, plane (X, Y, or the second X of tracker-only
   * modules) and tube indices out of the path tokens.
   *
   * @param gp The STT geometry path.
   * @return The decoded geo_id.
   */
  geo_id geoinfo::stt_info::id(const geo_path& gp) const {
    geo_id gi;
    auto path      = gp;
    gi.subdetector = STT;
    std::string straw(path.token(2));
    auto i1 = straw.find('_');
    auto i2 = straw.find('_', i1 + 1);
    auto i3 = straw.find('_', i2 + 1);
    auto i4 = straw.find('_', i3 + 1);
    auto i5 = straw.find('_', i4 + 1);
    if (i5 != std::string::npos) {
      gi.stt.supermodule = std::stoi(straw.substr(i1 + 1, i2 - i1 - 1));
      gi.stt.plane       = 0;
      if (straw.at(i3 - 1) == 'Y') {
        gi.stt.plane = 1;
      } else if (path.token(1).back() == '1') {
        gi.stt.plane = 2;
      }
      gi.stt.tube = std::stoi(straw.substr(i5 + 1));

    } else {
      UFW_ERROR("Path '{}' is incorrectly formatted for STT.", gp);
    }
    return gi;
  }

  /**
   * @brief Builds the STT geometry path for a geometry identifier.
   * @param gi The STT geometry identifier.
   * @return The corresponding geometry path.
   */
  geo_path geoinfo::stt_info::path(geo_id gi) const {
    // TODO these path names are quite poor choices, heavy repetitions etc... they should be changed in gegede
    UFW_ASSERT(gi.subdetector == STT, "Subdetector must be STT");
    geo_path gp           = path();
    std::string placement = "_PV";
    auto stat             = at(gi.stt.supermodule);
    std::string module_name;
    switch (stat->target) {
    case TRKONLY:
      module_name = "TrkMod_";
      break;
    case C3H6:
      module_name = "C3H6Mod_";
      break;
    case CARBON:
      module_name = "CMod_";
      break;
    default:
      UFW_ERROR("Target material '{}' unsupported.", stat->target);
    }
    module_name += fmt::format("{:02}", gi.stt.supermodule);

    gp /= module_name + placement + "_0";
    if (gi.stt.plane == 0) {
      module_name += "_planeXX";
      gp /= module_name + placement + "_0";
    } else if (gi.stt.plane == 1) {
      module_name += "_planeYY";
      gp /= module_name + placement + "_0";
    } else if (gi.stt.plane == 2 && stat->target == TRKONLY) {
      module_name += "_planeXX";
      gp /= module_name + placement + "_1";
    } else {
      UFW_ERROR("Plane '{}' unsupported.", gi.stt.plane);
    }
    // TODO check max tube for this layer
    gp /= module_name + "_straw_PV_" + gi.stt.tube;
    return gp;
  }

  /**
   * @brief Looks up the STT wire with a given geometry id.
   * @param id The wire geometry id.
   * @return Pointer to the matching wire, or nullptr if none is found.
   */
  const geoinfo::stt_info::wire* geoinfo::stt_info::get_wire_by_id(const geo_id& id) const {
    for (const auto& station_ptr : stations()) {
      for (const auto& wire_ptr : station_ptr->wires) {
        auto* stt_wire_ptr = static_cast<const sand::geoinfo::stt_info::wire*>(wire_ptr.get());
        if (stt_wire_ptr->geo == id) {
          return stt_wire_ptr;
        }
      }
    }
    return nullptr;
  }

} // namespace sand
