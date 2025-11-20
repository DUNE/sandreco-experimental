#include <drift_info.hpp>
#include <root_tgeomanager/root_tgeomanager.hpp>
#include <ufw/context.hpp>

#include <TGeoBBox.h>
#include <TGeoMatrix.h>
#include <TGeoTube.h>

namespace sand {

  static constexpr char s_drift_path[] = "sand_inner_volume_PV_0/SANDtracker_PV_0";

  /**
   * @brief Construct a new geoinfo::drift info::drift info object
   *
   * @param gi
   */
  geoinfo::drift_info::drift_info(const geoinfo& gi) : tracker_info(gi, s_drift_path) {
    // UFW_FATAL("drift");
    auto& tgm      = ufw::context::current()->instance<root_tgeomanager>();
    auto nav       = tgm.navigator();
    auto driftpath = gi.root_path() / path();

    nav->cd(driftpath);
    nav->for_each_node([&](auto supermod) {
      std::string smodname = supermod->GetName();
      nav->cd(driftpath / smodname);
      // UFW_INFO("Supermodule name: {}", smodname);
      nav->for_each_node([&](auto mod) {
        std::string modname = mod->GetName();
        nav->cd(driftpath / smodname / modname);
        auto stat = std::make_unique<station>();
        target_material tgt;
        if (modname.find("Frame") == 0) {
          return; // skip the frame, it has no wires
        } else if (modname.find("TrkDrift_") == 0) {
          tgt = TRKONLY;
        } else if (modname.find("C3H6Mod_") == 0) {
          tgt = C3H6;
        } else if (modname.find("CMod_") == 0) {
          tgt = CARBON;
        } else {
          UFW_ERROR("Drift module '{}' has unrecognized material.", modname);
        }
        // UFW_INFO("Module name: {}", modname);
        stat->target          = tgt;
        TGeoBBox* plane_shape = dynamic_cast<TGeoBBox*>(mod->GetVolume()->GetShape());
        if (!plane_shape) {
          UFW_ERROR("Drift module '{}' has invalid shape.", modname);
        }
        auto matrix  = nav->get_hmatrix();
        double* tran = matrix.GetTranslation();
        double* rot  = matrix.GetRotationMatrix();
        pos_3d centre;
        centre.SetCoordinates(tran);
        dir_3d halfsize(plane_shape->GetDX(), plane_shape->GetDY(), plane_shape->GetDZ());
        dir_3d boxcorner = nav->to_master(halfsize);
        boxcorner.SetZ(0); // we ignore the thickness here.
        stat->top_north    = centre + boxcorner;
        stat->bottom_south = centre - boxcorner;
        boxcorner.SetX(-boxcorner.x());
        stat->top_south    = centre + boxcorner;
        stat->bottom_north = centre - boxcorner;

        auto process_wire = [&](auto* wire_shape) {
          auto matrix  = nav->get_hmatrix();
          double* tran = matrix.GetTranslation();
          double* rot  = matrix.GetRotationMatrix();
          pos_3d centre;
          centre.SetCoordinates(tran);
          dir_3d halfsize(0, 0, wire_shape->GetDZ());
          dir_3d globalhalfsize = nav->to_master(halfsize);
          auto w                = std::make_unique<wire>();
          w->parent             = stat.get();
          w->head               = centre + globalhalfsize;
          w->tail               = centre - globalhalfsize;
          w->max_radius         = wire_shape->GetRmax();
          stat->wires.emplace_back(std::move(w));
        };

        if (tgt == TRKONLY) {
          nav->for_each_node([&](auto driftmod) {
            std::string driftmodname = driftmod->GetName();
            if (driftmodname.find("DriftMod") == std::string::npos) { // other stuff, targets, ...
              return;
            }
            // UFW_INFO("DriftMod name: {}", driftmodname);
            nav->cd(driftpath / smodname / modname / driftmodname);
            nav->for_each_node([&](auto wire) {
              std::string wname = wire->GetName();
              // UFW_INFO("Wire name: {}", wname);
              auto* wire_shape = dynamic_cast<TGeoTube*>(wire->GetVolume()->GetShape());
              if (!wire_shape) {
                UFW_ERROR("Wire '{}' has invalid shape.", wname);
              }
              process_wire(wire_shape);
            });
          });
        } else {
          nav->for_each_node([&](auto driftchamber) {
            std::string driftchambername = driftchamber->GetName();
            if (driftchambername.find("DriftChamber") == std::string::npos) { // other stuff, targets, ...
              return;
            }
            // UFW_INFO("DriftChamber name: {}", driftchambername);
            nav->cd(driftpath / smodname / modname / driftchambername);
            nav->for_each_node([&](auto driftmodule) {
              std::string driftmodname = driftmodule->GetName();
              if (driftmodname.find("DriftMod") == std::string::npos) { // other stuff, targets, ...
                return;
              }
              // UFW_INFO("DriftMod name: {}", driftmodname);
              nav->cd(driftpath / smodname / modname / driftchambername / driftmodname);
              nav->for_each_node([&](auto wire) {
                std::string wname = wire->GetName();
                // UFW_INFO("Wire name: {}", wname);
                auto* wire_shape = dynamic_cast<TGeoTube*>(wire->GetVolume()->GetShape());
                if (!wire_shape) {
                  UFW_ERROR("Wire '{}' has invalid shape.", wname);
                }
                process_wire(wire_shape);
              });
            });
          });
        }
        add_station(station_ptr(std::move(stat)));
      });
    });
  }

  geoinfo::drift_info::~drift_info() = default;

  geo_id geoinfo::drift_info::id(const geo_path& gp) const {
    geo_id gi;
    auto path      = gp;
    gi.subdetector = DRIFT;
    // abuse the bad notation here, module/plane/straw
    std::string supermodpath(path.token(0));
    // UFW_INFO("supermodule path: '{}'", supermodpath);

    auto tgt_ct = 0;
    auto trk_ct = 0;
    bool is_trk = 0;

    if (supermodpath.find("X0") != std::string::npos) {
      tgt_ct = 0;
    } else if (supermodpath.find("X1") != std::string::npos) {
      tgt_ct = 1;
    } else if (supermodpath.find("C") != std::string::npos && supermodpath.find("1") == std::string::npos) {
      tgt_ct = 2;
    } else if (supermodpath.find("C") != std::string::npos && supermodpath.find("1") != std::string::npos) {
      tgt_ct = 3;
    } else if (supermodpath.find("Trk") != std::string::npos) {
      tgt_ct = 4;
      is_trk = true;
    } else if (supermodpath.find("B") != std::string::npos && supermodpath.find("1") == std::string::npos) {
      tgt_ct = 4;
      trk_ct = 1; // this is a tracker only module
    } else if (supermodpath.find("B") != std::string::npos && supermodpath.find("1") != std::string::npos) {
      tgt_ct = 5;
      trk_ct = 1;
    } else if (supermodpath.find("A") != std::string::npos && supermodpath.find("1") == std::string::npos) {
      tgt_ct = 6;
      trk_ct = 1;
    } else if (supermodpath.find("A") != std::string::npos && supermodpath.find("1") != std::string::npos) {
      tgt_ct = 7;
      trk_ct = 1;
    } else {
      UFW_ERROR("Supermodule path '{}' is not recognized.", supermodpath);
    }

    std::string modpath(path.token(1));
    auto mod_ct = 0;
    if (modpath.find("CMod") != std::string::npos || modpath.find("TrkDrift") != std::string::npos) {
      mod_ct = 0; // Carbon
    } else if (modpath.find("C3H6Mod") != std::string::npos) {
      mod_ct = 1; // C3H6
      if (modpath.find('#') != std::string::npos) {
        size_t pos = modpath.find('#');
        auto a     = std::stoi(modpath.substr(pos + 1));
        mod_ct += a; // C3H6
      } else if (modpath.find("PV_") != std::string::npos) {
        size_t pos = modpath.find("PV_");
        auto a     = std::stoi(modpath.substr(pos + 3));
        mod_ct += a; // C3H6
      }
    } else {
      UFW_ERROR("Drift module path '{}' is not recognized.", modpath);
    }
    gi.drift.supermodule = tgt_ct * 10 + trk_ct + mod_ct; // supermodule is a combination of target and module

    std::string wire_path(is_trk ? path.token(3) : path.token(4));
    // UFW_INFO("Wire path: '{}'", wire_path);
    auto i1        = wire_path.find('_');
    auto i2        = wire_path.find('_', i1 + 1);
    auto plane     = std::stoi(wire_path.substr(i1 + 1, i2 - i1 - 1));
    auto dir       = (wire_path.find("Fwire") == std::string::npos) ? 0 : 1;
    gi.drift.plane = 2 * plane + dir;

    return gi;
  }

  geo_path geoinfo::drift_info::path(geo_id gi) const {
    UFW_ASSERT(gi.subdetector == DRIFT, "Subdetector must be DRIFT");
    geo_path gp           = path();
    auto stat             = at(gi.drift.supermodule);
    std::string placement = (gp.find("_PV") != std::string::npos) ? "_PV" : "";

    int val     = gi.drift.supermodule;
    bool is_trk = (val == 40) ? true : false; // tracker-only supermodule
    if (val >= 40)
      val -= 1;
    int tgt_ct = val / 10;
    int mod_ct = val - tgt_ct * 10;

    int plane             = gi.drift.plane / 2;
    int dir               = gi.drift.plane % 2; // 0 = Swire, 1 = Fwire
    std::string wire_type = (dir == 0) ? "Swire" : "Fwire";

    std::string supermod_name;
    // UFW_INFO("tgt_ct: {}, mod_ct: {}, plane: {}, dir: {}", tgt_ct, mod_ct, plane, dir);
    switch (tgt_ct) {
    case 0:
      supermod_name = "SuperMod_X0" + placement + "_0";
      break;
    case 1:
      supermod_name = "SuperMod_X1" + placement + "_0";
      break;
    case 2:
      supermod_name = "SuperMod_C" + placement + "_0";
      break;
    case 3:
      if (is_trk)
        supermod_name = "Trk" + placement + "_0";
      else
        supermod_name = "SuperMod_C" + ((placement == "") ? "_0#1" : placement + "_1");
      break;
    case 4:
      supermod_name = "SuperMod_B" + placement + "_0";
      break;
    case 5:
      supermod_name = "SuperMod_B" + ((placement == "") ? "_0#1" : placement + "_1");
      break;
    case 6:
      supermod_name = "SuperMod_A" + placement + "_0";
      break;
    case 7:
      supermod_name = "SuperMod_A" + ((placement == "") ? "_0#1" : placement + "_1");
      break;
    default:
      UFW_ERROR("Supermodule index '{}' is not recognized.", val);
    }

    gp /= supermod_name;

    if (supermod_name.find("Trk") != std::string::npos) {
      gp /= "TrkDrift" + placement + "_0";
      gp /= "CDriftModule_" + std::to_string(plane) + placement + "_0";
      gp /= "CDriftModule_" + std::to_string(plane) + "_" + wire_type + placement + "_0";
    } else {
      auto i1 = supermod_name.find('_');
      auto i2 = supermod_name.find('_', i1 + 1);
      std::string sm_ID(supermod_name.substr(i1 + 1, i2 - i1 - 1));
      std::string module_name;
      switch (stat->target) {
      case C3H6:
        module_name = "C3H6";
        break;
      case CARBON:
        module_name = "C";
        break;
      default:
        UFW_ERROR("Target material '{}' unsupported.", stat->target);
      }
      if (placement == "")
        gp /= module_name + "Mod_" + sm_ID + "_0" + (mod_ct > 1 ? ("#" + std::to_string(mod_ct - 1)) : "");
      else
        gp /= module_name + "Mod_" + sm_ID + placement + "_" + std::to_string(mod_ct - 1);
      gp /= module_name + "DriftChamber_" + sm_ID + placement + "_0";
      gp /= module_name + "DriftModule_" + std::to_string(plane) + "_" + sm_ID + placement + "_0";
      gp /= module_name + "DriftModule_" + std::to_string(plane) + "_" + sm_ID + "_" + wire_type + placement + "_0";
    }
    return gp;
  }

} // namespace sand
