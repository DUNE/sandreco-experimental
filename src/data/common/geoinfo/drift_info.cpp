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
  geoinfo::drift_info::drift_info(const geoinfo& gi, 
                                  const std::array<double, 3>& view_angle, 
                                  const std::array<double, 3>& view_offset, 
                                  const std::array<double, 3>& view_spacing) : tracker_info(gi, s_drift_path) {
    // UFW_FATAL("drift");
    auto& tgm      = ufw::context::current()->instance<root_tgeomanager>();
    auto nav       = tgm.navigator();
    auto driftpath = gi.root_path() / path();

    m_view_angle = view_angle;
    m_view_offset = view_offset;
    m_view_spacing =  view_spacing;

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
        TGeoBBox* station_shape = dynamic_cast<TGeoBBox*>(mod->GetVolume()->GetShape());
        if (!station_shape) {
          UFW_ERROR("Drift module '{}' has invalid shape.", modname);
        }
        auto matrix  = nav->get_hmatrix();
        double* tran = matrix.GetTranslation();
        double* rot  = matrix.GetRotationMatrix();
        pos_3d centre;
        centre.SetCoordinates(tran);
        dir_3d halfsize(station_shape->GetDX(), station_shape->GetDY(), station_shape->GetDZ());
        dir_3d boxcorner = nav->to_master(halfsize);
        boxcorner.SetZ(0); // we ignore the thickness here.
        stat->top_north    = centre + boxcorner;
        stat->bottom_south = centre - boxcorner;
        boxcorner.SetX(-boxcorner.x());
        stat->top_south    = centre + boxcorner;
        stat->bottom_north = centre - boxcorner;
        stat->parent = this;


        if (tgt == TRKONLY) {
          nav->for_each_node([&](auto driftmod) {
            std::string driftmodname = driftmod->GetName();
            geo_path full_path = driftpath / smodname / modname / driftmodname;
            nav->cd(full_path);
            auto ID = id(partial_path(full_path, gi));
            stat->daq_link = ID.drift.supermodule;
            stat->set_drift_view(full_path, ID);
          });
        } else {
          nav->for_each_node([&](auto driftchamber) {
            std::string driftchambername = driftchamber->GetName();
            if (driftchambername.find("DriftChamber") == std::string::npos) { // other stuff, targets, ...
              return;
            }
            UFW_INFO("DriftChamber name: {}", driftchambername);
            nav->cd(driftpath / smodname / modname / driftchambername);
            nav->for_each_node([&](auto driftmodule) {
              std::string driftmodname = driftmodule->GetName();
              geo_path full_path = driftpath / smodname / modname / driftchambername / driftmodname;
              nav->cd(full_path);
              auto ID = id(partial_path(full_path, gi));
              stat->daq_link = ID.drift.supermodule;
              stat->set_drift_view(full_path, ID);
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
      size_t pos = modpath.find("PV_");
      auto a     = std::stoi(modpath.substr(pos + 3));
      mod_ct += a; // C3H6
    } else {
      UFW_ERROR("Drift module path '{}' is not recognized.", modpath);
    }
    gi.drift.supermodule = tgt_ct * 10 + trk_ct + mod_ct; // supermodule is a combination of target and module

    std::string plane_path(is_trk ? path.token(2) : path.token(3));
    // UFW_INFO("Wire path: '{}'", plane_path);
    if(plane_path.find("Mylar_") != std::string::npos ) {
      UFW_DEBUG("Plane path '{}' corresponds to a mylar foil. This is not a sensitive detector", plane_path);
      gi.drift.plane = 255; // invalid plane
      return gi;
    } else {
      auto i1        = plane_path.find('_');
      auto i2        = plane_path.find('_', i1 + 1);
      auto plane     = std::stoi(plane_path.substr(i1 + 1, i2 - i1 - 1));
      gi.drift.plane = plane;
      return gi;
    }
  }

  geo_path geoinfo::drift_info::path(geo_id gi) const {
    UFW_ASSERT(gi.subdetector == DRIFT, "Subdetector must be DRIFT");
    geo_path gp           = path();
    auto stat             = at(gi.drift.supermodule);
    std::string placement = "_PV";

    int val     = gi.drift.supermodule;
    bool is_trk = (val == 40) ? true : false; // tracker-only supermodule
    if (val >= 40)
      val -= 1;
    int tgt_ct = val / 10;
    int mod_ct = val - tgt_ct * 10;

    int plane             = gi.drift.plane;

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
        supermod_name = "SuperMod_C" + placement + "_1";
      break;
    case 4:
      supermod_name = "SuperMod_B" + placement + "_0";
      break;
    case 5:
      supermod_name = "SuperMod_B" + placement + "_1";
      break;
    case 6:
      supermod_name = "SuperMod_A" + placement + "_0";
      break;
    case 7:
      supermod_name = "SuperMod_A" + placement + "_1";
      break;
    default:
      UFW_ERROR("Supermodule index '{}' is not recognized.", val);
    }

    gp /= supermod_name;

    if (supermod_name.find("Trk") != std::string::npos) {
      gp /= "TrkDrift" + placement + "_0";
      gp /= "CDriftModule_" + std::to_string(plane) + placement + "_0";
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
      gp /= module_name + "Mod_" + sm_ID + placement + "_" + std::to_string(mod_ct - 1);
      gp /= module_name + "DriftChamber_" + sm_ID + placement + "_0";
      gp /= module_name + "DriftModule_" + std::to_string(plane) + "_" + sm_ID + placement + "_0";
    }
    return gp;
  }


  void geoinfo::drift_info::station::set_drift_view(const geo_path & driftmod_path, const geo_id& id) {

    auto& tgm = ufw::context::current()->instance<root_tgeomanager>();
    auto nav = tgm.navigator();
    std::string driftmod_name(nav->GetCurrentNode()->GetName());
    
    if (driftmod_name.find("DriftMod") == std::string::npos) {
        return;  // not a drift module
    }

    UFW_DEBUG("Setting drift view for path: {}", driftmod_path);
    auto i1 = driftmod_name.find('_');
    auto i2 = driftmod_name.find('_', i1 + 1);
    auto plane_ID = std::stoi(driftmod_name.substr(i1 + 1, i2 - i1 - 1));

    if (plane_ID == 0) {
        geo_x = id;        
    } else if (plane_ID == 1) {
        geo_u = id;
    } else if (plane_ID == 2) {
        geo_v = id;
    } else {
        UFW_ERROR("DriftMod '{}' has unrecognized plane '{}'.", driftmod_name, plane_ID);
    }
    set_wire_list(plane_ID);

  }

  void geoinfo::drift_info::station::set_wire_list(const size_t & view_ID) {

    /////Get gas volume properties
    auto& tgm = ufw::context::current()->instance<root_tgeomanager>();
    auto nav = tgm.navigator();

    dir_3d view_translation;
    view_translation.SetCoordinates(nav->get_hmatrix().GetTranslation());

    /// Set global vs local view_corners
    std::vector<pos_3d> view_corners_global(4);
    view_corners_global[0] = pos_3d(top_north.x(), top_north.y(), view_translation.z()); // top_north
    view_corners_global[1] = pos_3d(top_south.x(), top_south.y(), view_translation.z()); // top_south
    view_corners_global[2] = pos_3d(bottom_south.x(), bottom_south.y(), view_translation.z()); // bottom_south
    view_corners_global[3] = pos_3d(bottom_north.x(), bottom_north.y(), view_translation.z()); // bottom_north

    std::vector<pos_3d> view_corners_local(4);
    for (size_t i = 0; i < view_corners_global.size(); ++i) {
      view_corners_local[i] = view_corners_global[i] - view_translation;
    }

    // Setting up rotation matrix for wires
    auto drift = dynamic_cast<const sand::geoinfo::drift_info*>(this->parent);
    double c = std::cos(drift->view_angle()[view_ID]);
    double s = std::sin(drift->view_angle()[view_ID]);
    xform_3d wire_rot(c, -s, 0., 0.,
                      s,  c, 0., 0.,
                      0., 0., 1, 0.);   
               
    // Find max y in wire plane (rotated frame)
    double max_wire_plane_y = 0.0;
    for (auto v:view_corners_local) {
      pos_3d v_rot = wire_rot.Inverse() * v;  //Local to rotated frame
      if (v_rot.Y() > max_wire_plane_y) {
        max_wire_plane_y = v_rot.Y();
      }
    }
  

    /// Find intersections of wires with frame
    double transverse_position = max_wire_plane_y - drift->view_offset()[view_ID];
    while (transverse_position > -max_wire_plane_y) {
      pos_3d wire_centre_rot(0., transverse_position, 0.); // origin of the vector in rotated frame

      pos_3d wire_centre_local = wire_rot * wire_centre_rot; // origin of the vector in local frame from rotated frame
      dir_3d local_x_axis = wire_rot * dir_3d(1., 0., 0.); // direction of the vector in local frame from rotated frame

      std::vector<pos_3d> intersections = getXYLinePolygonIntersections(
        view_corners_local,
        wire_centre_local,
        local_x_axis);

      std::vector<pos_3d> intersections_global;
      for (const auto &inter : intersections) intersections_global.push_back(inter + view_translation);

      // Need exactly two intersections to define a wire
      if (intersections_global.size() == 2) {
        auto w                = std::make_unique<wire>();
        w->parent             = this;
        w->head               = (intersections_global[1].x()<intersections_global[0].x()) ? intersections_global[1] : intersections_global[0];
        w->tail               = (intersections_global[1].x()<intersections_global[0].x()) ? intersections_global[0] : intersections_global[1];
        w->max_radius         = drift->view_spacing()[view_ID] / 2.0;
        w->daq_channel.subdetector = DRIFT;
        w->daq_channel.link = daq_link;
        w->daq_channel.channel = (view_ID << 16) | wires.size();
        wires.emplace_back(std::move(w));
      } else {
        UFW_DEBUG("Transverse position {} has {} intersections, skipping.", transverse_position, intersections_global.size());
      }

      transverse_position -= drift->view_spacing()[view_ID];

    }
  }

  geoinfo::drift_info::wire_list geoinfo::drift_info::station::x_view() const {
    auto drift = dynamic_cast<const sand::geoinfo::drift_info*>(this->parent);
    wire_list wl = select([&](const wire& w) {
      return std::abs(w.angle() - drift->view_angle()[geo_x.drift.plane]) < 1e-6;
    });
    return wl;
  }

  geoinfo::drift_info::wire_list geoinfo::drift_info::station::u_view() const {
    auto drift = dynamic_cast<const sand::geoinfo::drift_info*>(this->parent);
    wire_list wl = select([&](const wire& w) {
      return std::abs(w.angle() - drift->view_angle()[geo_u.drift.plane]) < 1e-6;
    });
    return wl;
  }

  geoinfo::drift_info::wire_list geoinfo::drift_info::station::v_view() const {
    auto drift = dynamic_cast<const sand::geoinfo::drift_info*>(this->parent);
    wire_list wl = select([&](const wire& w) {
      return std::abs(w.angle() - drift->view_angle()[geo_v.drift.plane]) < 1e-6;
    });
    return wl;
  }
    

} // namespace sand
