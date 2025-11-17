#include <stt_info.hpp>
#include <ufw/context.hpp>
#include <root_tgeomanager/root_tgeomanager.hpp>

#include <TGeoMatrix.h>
#include <TGeoBBox.h>
#include <TGeoTube.h>

namespace sand {
  
  static constexpr char s_stt_path[] = "sand_inner_volume_PV_0/STTtracker_PV_0";
  
  geoinfo::stt_info::stt_info(const geoinfo& gi) : tracker_info(gi, s_stt_path) {
    auto& tgm = ufw::context::current()->instance<root_tgeomanager>();
    auto nav = tgm.navigator();
    auto sttpath =  gi.root_path() / path();
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
      stat->target = tgt;
      TGeoBBox* plane_shape = dynamic_cast<TGeoBBox*>(supermod->GetVolume()->GetShape());
      if (!plane_shape) {
        UFW_ERROR("STT module '{}' has invalid shape.", smodname);
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
      nav->for_each_node([&](auto plane) {
        //The plane does not carry useful information for us.
        std::string plname = plane->GetName();
        if (plname.find("plane") == std::string::npos) { //other stuff, targets, ...
          return;
        }
        nav->cd(sttpath / smodname / plname);
        nav->for_each_node([&](auto tube) {
          std::string tname = tube->GetName();
          nav->cd(sttpath / smodname / plname / tname);
          /*FIXED to support both TGeoTubeSeg and TGeoTube used in more recent geometries*/
          auto* generic_tube_shape = tube->GetVolume()->GetShape();
          
          auto process_tube = [&](auto* tube_shape) {
            auto matrix = nav->get_hmatrix();
            double* tran = matrix.GetTranslation();
            double* rot = matrix.GetRotationMatrix();
            pos_3d centre;
            centre.SetCoordinates(tran);
            dir_3d halfsize(0, 0, tube_shape->GetDZ());
            dir_3d globalhalfsize = nav->to_master(halfsize);
            auto w = std::make_unique<wire>();
            w->parent = stat.get();
            w->head = centre + globalhalfsize;
            w->tail = centre - globalhalfsize;
            w->max_radius = tube_shape->GetRmax();
            w->geo = id(geo_path(smodname.c_str()) / plname / tname);
            stat->wires.emplace_back(std::move(w));
          };
          
          if (auto* tube_shape = dynamic_cast<TGeoTube*>(generic_tube_shape)) {
            process_tube(tube_shape);
          } else {          
            UFW_ERROR("STT tube '{}' has unsupported shape type.", tname);
          }
          
        } );
      } );
      add_station(station_ptr(std::move(stat)));
    } );
  }
  
  geoinfo::stt_info::~stt_info() = default;
  
  geo_id geoinfo::stt_info::id(const geo_path& gp) const {
    geo_id gi;
    auto path = gp;
    gi.subdetector = STT;
    //abuse the bad notation here, module/plane/straw
    if(path.find("PV_") != std::string::npos) {
      std::string straw(path.token(2));
      auto i1 = straw.find('_');
      auto i2 = straw.find('_', i1 + 1);
      auto i3 = straw.find('_', i2 + 1);
      auto i4 = straw.find('_', i3 + 1);
      auto i5 = straw.find('_', i4 + 1);
      if (i5 != std::string::npos) {
        gi.stt.supermodule = std::stoi(straw.substr(i1 + 1, i2 - i1 - 1));
        gi.stt.plane = 0;
        if (straw.at(i3 - 1) == 'Y') {
          gi.stt.plane = 1;
        } else if (path.token(1).back() == '1') {
          gi.stt.plane = 2;
        }
        gi.stt.tube = std::stoi(straw.substr(i5 + 1));
      } else {
        UFW_ERROR("Path '{}' is incorrectly formatted for STT.", gp);
      }
    } else { // root geometry notation with no PV
      std::string straw(path.token(2));
      auto i1 = straw.find('_');
      auto i2 = straw.find('_', i1 + 1);
      auto i3 = straw.find('_', i2 + 1);
      if (i3 != std::string::npos) {
        gi.stt.supermodule = std::stoi(straw.substr(i1 + 1, i2 - i1 - 1));
        gi.stt.plane = 0;
        if (straw.at(i3 - 1) == 'Y') {
          gi.stt.plane = 1;
        } else if (path.token(0).back() == '1') {
          gi.stt.plane = 2;
        }
        size_t pos = straw.find('#');
        if (pos != std::string::npos) {       
          gi.stt.tube = std::stoi(straw.substr(pos+1));
        } else {
          gi.stt.tube = 0;
        }
      } else {
        UFW_ERROR("Path '{}' is incorrectly formatted for STT.", gp);
      }
    }
    return gi;
  }
  
  geo_path geoinfo::stt_info::path(geo_id gi) const {
    //TODO these path names are quite poor choices, heavy repetitions etc... they should be changed in gegede
    UFW_ASSERT(gi.subdetector == STT, "Subdetector must be STT");
    geo_path gp = path();
    std::string placement = (gp.find("_PV") != std::string::npos)? "_PV" : ""; // check if we are using the edepsim or ROOT geometry notation
    auto stat = at(gi.stt.supermodule);
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
      if(placement == "PV_") gp /= module_name + placement + "_1";
      else  gp /= module_name + "#1";
    } else {
      UFW_ERROR("Plane '{}' unsupported.", gi.stt.plane);
    }
    //TODO check max tube for this layer
    if(placement == "_PV") gp /= module_name + "_straw_PV_" + gi.stt.tube;
    else gp /= module_name + "_straw_0" + (gi.stt.tube == 0 ? "" : fmt::format("#{}", gi.stt.tube));
    return gp;
  }
  
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
  
  
}
