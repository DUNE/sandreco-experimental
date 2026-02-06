#include <root_tgeomanager/root_tgeomanager.hpp>
#include <stt_info.hpp>
#include <ufw/context.hpp>

#include <TGeoBBox.h>
#include <TGeoMatrix.h>
#include <TGeoTube.h>

namespace sand {

  static constexpr char s_stt_path[] = "sand_inner_volume_PV_0/STTtracker_PV_0";

  geoinfo::stt_info::stt_info(const geoinfo& gi) : tracker_info(gi, s_stt_path) {
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
      boxcorner.SetZ(0); // we ignore the thickness here.
      stat->top_north    = centre + boxcorner;
      stat->bottom_south = centre - boxcorner;
      boxcorner.SetX(-boxcorner.x());
      stat->top_south    = centre + boxcorner;
      stat->bottom_north = centre - boxcorner;
      stat->parent = this;
      std::vector<std::unique_ptr<wire>> wires_for_stat;
      
      nav->for_each_node([&](auto plane) {
        // The plane does not carry useful information for us.
        std::string plname = plane->GetName();
        if (plname.find("plane") == std::string::npos) { // other stuff, targets, ...
          return;
        }
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
            w->daq_channel.link = w->geo.stt.supermodule;
            w->daq_channel.channel = (w->geo.stt.plane << 16) | w->geo.stt.tube;
            wires_for_stat.emplace_back(std::move(w));
            stat->daq_link = w->geo.stt.supermodule;
            stat->wires.emplace_back(std::move(w));
          } else {
            UFW_ERROR("STT tube '{}' has unsupported shape type.", tname);
          }
        });
      });

      for (auto &w : wires_for_stat) {
          stat->wires.emplace_back(std::move(w));
      }
      add_station(station_ptr(std::move(stat)));
    });
  }

  geoinfo::stt_info::~stt_info() = default;


  void geoinfo::stt_info::set_wire_adjecency(std::vector<std::unique_ptr<wire>> & ws){
    double dz; 
    double dy;
    dy = ws[0]->length();
    dz = ws[0]->max_radius * sqrt(3) / 2.;

    double max_distance = sqrt(dy*dy + dz*dz) + 0.1;
    std::cout << "max_distance " << dy << " " << dz << " " << max_distance << std::endl;

    auto start = std::chrono::system_clock::now();
    //TO DO : implement BVH search algorithm
    auto end_build = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end_build - start);
    std::cout << "Time to build, search, and fill adj_cells: " << elapsed.count() << " ms" << std::endl;

  }

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
