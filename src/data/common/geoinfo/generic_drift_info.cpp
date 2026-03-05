#include <generic_drift_info.hpp>
#include <root_tgeomanager/root_tgeomanager.hpp>
#include <ufw/context.hpp>

#include <TGeoBBox.h>
#include <TGeoMatrix.h>
#include <TGeoTube.h>

/*
Standard geometry:
Supermodule contains 1 C module and 9 C3H6 modules. Each of these modules contains a target and a drift chamber. 
The drift chamber is made of planes: mylar and drift modules (which have field and sense wires).
This is the structure: 
• Supermod
  • Frame
  • CMod
    • CTarget
    • CDriftChamber
      • CMylar
      • CDriftModule
        • list of wires
  • C3H6Mod x9
    same structure

Generic geometry:
No supermodules. The "modules" are called stations. Each station has a frame, a target (P or C) and a chamber. 
The chamber is composed of mylar planes and views (no wires). Number of views (and mylar planes) depends on the geometry.
This is the structure:
• Station (s)
  • Frame (fr)
  • Target (t_C)
  • DriftChamber (ch)
    • Mylar planes (m)
    • Views (v)
*/

namespace sand {

  static constexpr char s_drift_path[] = "sand_inner_volume_PV_0/SANDtracker_PV_0";

  /**
   * @brief Construct a new geoinfo::generic_drift info::generic_drift info object
   *
   * @param gi
   */
  geoinfo::generic_drift_info::generic_drift_info(const geoinfo& gi, 
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
    nav->for_each_node([&](auto station_node) {
      std::string stname = station_node->GetName();
      nav->cd(driftpath / stname);
      // UFW_DEBUG("Station name: {}", stname);
      auto stat = std::make_unique<station>();
      target_material tgt;

      nav->for_each_node([&](auto subunit) {
        std::string subuname = subunit->GetName();
        // UFW_DEBUG("Subunit name: {}", subuname);
        nav->cd(driftpath / stname / subuname);
        if (subuname.find("fr") != std::string::npos) {
          return; // skip the frame
        } else if (subuname.find("t_P") != std::string::npos) {
          tgt = C3H6;
        } else if (subuname.find("t_C") != std::string::npos) {
          tgt = CARBON;
        } else if (subuname.find("ch") != std::string::npos) {
          tgt = TRKONLY;
        } else {
          UFW_ERROR("Drift station '{}' has unrecognized material.", subuname);
        }
      });
        
      stat->target          = tgt;
      TGeoBBox* station_shape = dynamic_cast<TGeoBBox*>(station_node->GetVolume()->GetShape());
      if (!station_shape) {
        UFW_ERROR("Drift station '{}' has invalid shape.", stname);
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

      nav->cd(driftpath / stname); 
      nav->for_each_node([&](auto subunit){
        std::string subuname = subunit->GetName();
        if (subuname.find("ch") == std::string::npos){
          return; // since they are subunits with only targets
        }
        geo_path chamber_path = driftpath / stname / subuname; 
        nav->cd(chamber_path);
        nav->for_each_node([&](auto plane){
          std::string planename = plane->GetName();
          if (planename.find("m") != std::string::npos){
            return; // mylar plane
          }
          geo_path full_path = driftpath / stname / subuname / planename;
          nav->cd(full_path);
          auto ID = id(partial_path(full_path, gi));
          stat->daq_link = ID.drift.supermodule;
          stat->set_drift_view(full_path, ID);
        });
      });

      // UFW_DEBUG("Adding station {} ", stname);
      add_station(station_ptr(std::move(stat)));
    });
  }

  geoinfo::generic_drift_info::~generic_drift_info() = default;

  geo_id geoinfo::generic_drift_info::id(const geo_path& gp) const {
    UFW_INFO("Searching for path {}.", gp);
    geo_id gi;
    auto path      = gp;
    gi.subdetector = DRIFT;

    std::string stationpath(path.token(0));
    // UFW_INFO("Station path: '{}'", stationpath);

    auto st_ct = 0;

    // count the index of the station
    if (stationpath.find("s") == 0){
      auto i1        = stationpath.find('_');
      auto i2        = stationpath.find('_', i1 + 1);
      st_ct  = std::stoi(stationpath.substr(i1 + 1, i2 - i1 - 1));
    }
    else {
      UFW_ERROR("Station path '{}' is not recognized.", stationpath);
    }
    // called supermodule just to be consistent with the base struct, 
    // but actually here we have no supermodules. The id is just the station id
    gi.drift.supermodule = st_ct; 

    // I ignore the chamber path since there is 1-1 correspondence 
    // between station and its chamber
    std::string plane_path(path.token(2));
    UFW_INFO("Plane path: '{}'", plane_path);
    if(plane_path.find("m") != std::string::npos ) {
      UFW_WARN("Plane path '{}' corresponds to a mylar foil. This is not a sensitive detector", plane_path);
      gi.drift.plane = 255; // invalid plane
      return gi;
    } else {
      auto v_index   = plane_path.find('v');
      auto i1        = plane_path.find('_', v_index);
      auto i2        = plane_path.find('_', i1 + 1);
      auto plane     = std::stoi(plane_path.substr(i1 + 1, i2 - i1 - 1));
      gi.drift.plane = plane;
      return gi;
    }
  }

  geo_path geoinfo::generic_drift_info::path(geo_id gi) const {
    UFW_ASSERT(gi.subdetector == DRIFT, "Subdetector must be DRIFT");
    geo_path gp           = path();
    auto stat             = at(gi.drift.supermodule);
    std::string placement = "_PV";

    int st_ct             = gi.drift.supermodule;
    int plane             = gi.drift.plane;

    std::string station_basename;
    std::string station_fullname;
    UFW_INFO("station: {}, plane: {}", st_ct, plane);
    
    station_basename = "s_" + std::to_string(st_ct);
    station_fullname = station_basename + placement + "_0";
    gp /= station_fullname;
    
    std::string chamber_name;
    chamber_name = station_basename + "_ch" + placement + "_0";
    gp /= chamber_name;

    std::string view_name;
    view_name = station_basename + "_v" + std::to_string(plane) + placement + "_0";
    gp /= view_name;

    return gp;
  }


  void geoinfo::generic_drift_info::station::set_drift_view(const geo_path & ch_path, const geo_id& id) {

    auto& tgm = ufw::context::current()->instance<root_tgeomanager>();
    auto nav = tgm.navigator();
    std::string ch_name(nav->GetCurrentNode()->GetName());

    UFW_DEBUG("Setting drift view for path: {}", ch_path);
    auto v_index = ch_name.find('v');
    auto i1      = ch_name.find('_', v_index);
    auto i2      = ch_name.find('_', i1 + 1);
    auto plane_ID = std::stoi(ch_name.substr(i1 + 1, i2 - i1 - 1));

    if ((plane_ID % 3) == 0) {
        geo_x = id;        
    } else if ((plane_ID % 3) == 1) {
        geo_u = id;
    } else if ((plane_ID % 3) == 2) {
        geo_v = id;
    } else {
        UFW_ERROR("DriftChamber '{}' has unrecognized plane '{}'.", ch_name, plane_ID);
    }
    set_wire_list(plane_ID % 3);

  }

  void geoinfo::generic_drift_info::station::set_wire_list(const size_t & view_ID) {

    UFW_DEBUG("Set wire list ");

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
    auto drift = dynamic_cast<const sand::geoinfo::generic_drift_info*>(this->parent);
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

  geoinfo::generic_drift_info::wire_list geoinfo::generic_drift_info::station::x_view() const {
    auto drift = dynamic_cast<const sand::geoinfo::generic_drift_info*>(this->parent);
    wire_list wl = select([&](const wire& w) {
      return std::abs(w.angle() - drift->view_angle()[geo_x.drift.plane]) < 1e-6;
    });
    return wl;
  }

  geoinfo::generic_drift_info::wire_list geoinfo::generic_drift_info::station::u_view() const {
    auto drift = dynamic_cast<const sand::geoinfo::generic_drift_info*>(this->parent);
    wire_list wl = select([&](const wire& w) {
      return std::abs(w.angle() - drift->view_angle()[geo_u.drift.plane]) < 1e-6;
    });
    return wl;
  }

  geoinfo::generic_drift_info::wire_list geoinfo::generic_drift_info::station::v_view() const {
    auto drift = dynamic_cast<const sand::geoinfo::generic_drift_info*>(this->parent);
    wire_list wl = select([&](const wire& w) {
      return std::abs(w.angle() - drift->view_angle()[geo_v.drift.plane]) < 1e-6;
    });
    return wl;
  }
    

} // namespace sand
