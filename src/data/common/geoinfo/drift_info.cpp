#include <drift_info.hpp>
#include <ufw/context.hpp>
#include <root_tgeomanager/root_tgeomanager.hpp>

#include <TGeoMatrix.h>
#include <TGeoBBox.h>
#include <TGeoTube.h>

namespace sand {

  static constexpr char s_drift_path[] = "sand_inner_volume_0/SANDtracker_0";

  geoinfo::drift_info::drift_info(const geoinfo& gi) : tracker_info(gi, s_drift_path) {
    //UFW_FATAL("drift");
    auto& tgm = ufw::context::current()->instance<root_tgeomanager>();
    auto nav = tgm.navigator();
    auto driftpath =  gi.root_path() / path();

    nav->cd(driftpath);
    nav->for_each_node([&](auto supermod) {
      std::string smodname = supermod->GetName();
      nav->cd(driftpath / smodname);
      //UFW_INFO("Supermodule name: {}", smodname);
      nav->for_each_node([&](auto mod) {
        std::string modname = mod->GetName();
        nav->cd(driftpath / smodname / modname);
        auto stat = std::make_unique<station>();
        target_material tgt;
        if (modname.find("Frame") == 0) {
          return; //skip the frame, it has no wires
        } else if (modname.find("TrkDrift_") == 0) {
          tgt = TRKONLY;
        } else if (modname.find("C3H6Mod_") == 0) {
          tgt = C3H6;
        } else if (modname.find("CMod_") == 0) {
          tgt = CARBON;
        } else {
          UFW_ERROR("Drift module '{}' has unrecognized material.", modname);
        }
        //UFW_INFO("Module name: {}", modname);
        stat->target = tgt;
        TGeoBBox* plane_shape = dynamic_cast<TGeoBBox*>(mod->GetVolume()->GetShape());
        if (!plane_shape) {
          UFW_ERROR("Drift module '{}' has invalid shape.", modname);
        }
        auto matrix = nav->get_hmatrix();
        double* tran = matrix.GetTranslation();
        double* rot = matrix.GetRotationMatrix();
        pos_3d centre;
        centre.SetCoordinates(tran);
        dir_3d halfsize(plane_shape->GetDX(), plane_shape->GetDY(), plane_shape->GetDZ());
        dir_3d boxcorner = nav->to_master(halfsize);
        boxcorner.SetZ(0); //we ignore the thickness here.
        stat->top_north = centre + boxcorner;
        stat->bottom_south = centre - boxcorner;
        boxcorner.SetX(-boxcorner.x());
        stat->top_south = centre + boxcorner;
        stat->bottom_north = centre - boxcorner;

        auto process_wire = [&](auto* wire_shape) {
              auto matrix = nav->get_hmatrix();
              double* tran = matrix.GetTranslation();
              double* rot = matrix.GetRotationMatrix();
              pos_3d centre;
              centre.SetCoordinates(tran);
              dir_3d halfsize(0, 0, wire_shape->GetDZ());
              dir_3d globalhalfsize = nav->to_master(halfsize);
              auto w = std::make_unique<wire>();
              w->parent = stat.get();
              w->head = centre + globalhalfsize;
              w->tail = centre - globalhalfsize;
              w->max_radius = wire_shape->GetRmax();
              stat->wires.emplace_back(std::move(w));
        };


        if(tgt == TRKONLY){
          nav->for_each_node([&](auto driftmod){
            std::string driftmodname = driftmod->GetName();
            if (driftmodname.find("DriftMod") == std::string::npos) { //other stuff, targets, ...
              return;
            }
            //UFW_INFO("DriftMod name: {}", driftmodname);
            nav->cd(driftpath / smodname / modname / driftmodname);
            nav->for_each_node([&](auto wire) {
              std::string wname = wire->GetName();
              //UFW_INFO("Wire name: {}", wname);
              auto* wire_shape = dynamic_cast<TGeoTube*>(wire->GetVolume()->GetShape());
              if (!wire_shape) {
                UFW_ERROR("Wire '{}' has invalid shape.", wname);
              }
              process_wire(wire_shape);
            });

          });
        } else {
          nav->for_each_node([&](auto driftchamber){
            std::string driftchambername = driftchamber->GetName();
            if (driftchambername.find("DriftChamber") == std::string::npos) { //other stuff, targets, ...
              return;
            }
            //UFW_INFO("DriftChamber name: {}", driftchambername);
            nav->cd(driftpath / smodname / modname / driftchambername);
            nav->for_each_node([&](auto driftmodule) {
              std::string driftmodname = driftmodule->GetName();
              if (driftmodname.find("DriftMod") == std::string::npos) { //other stuff, targets, ...
                return;
              }
              //UFW_INFO("DriftMod name: {}", driftmodname);
              nav->cd(driftpath / smodname / modname / driftchambername / driftmodname);
              nav->for_each_node([&](auto wire) {
                std::string wname = wire->GetName();
                //UFW_INFO("Wire name: {}", wname);
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

  geo_id geoinfo::drift_info::id(const geo_path&) const {
    geo_id gi;
    return gi;
  }

  geo_path geoinfo::drift_info::path(geo_id) const {
    geo_path gp;
    return gp;
  }

}
