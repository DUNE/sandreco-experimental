#pragma once

#include <geoinfo/subdetector_info.hpp>

namespace sand {

  /**
   * All positions of wires and planes are in global ND coordinates.
   * North-south refers to the global orientation of SAND.
   * North is towards negative X, or the side of the cavern towards the shaft.
   * Positive Y is up, Positive Z goes with the beam, towards the FD.
   * 
   * Wire deflection follows the catenary curve y = a*cosh((x-x_min)/a)-a+y_min
   * x_min and y_min can be determined by direct observation or fit if the lowest measured point.
   * a is by definition the length of wire whose weight is equal in magnitude to the tension at x_min.
   * It must be determined by fit after x,y_min are known, fixing the head and tail coordinates.
   * 
   * Wire deflection is a separate catenary between each fixed point, up to s_max_wire_spacers + 1.
   * Fixed arrays are used instead of vectors because the number is small and likely identical for all wires.
   * Should there be wires with fewer spacers, the excess entries are guaranteed to be nan.
   */
  class geoinfo::tracker_info : public subdetector_info {

  public:
    static const std::size_t s_max_wire_spacers = 0;

    struct plane;

    struct wire {
      struct catenary {
        pos_3d minimum;
        double a;
      };
      using catenary_array = std::array<catenary, s_max_wire_spacers + 1>;
      const plane* const parent; ///< The parent plane
      geo_id geo; ///< The unique geometry identifier
      channel_id channel; ///< The unique daq identifier
      pos_3d head; ///< The readout end of the wire
      pos_3d tail; ///< The termination end of the wire
      catenary_array catenaries; ///< Maximum deflection downwards at the centre of the segment between two spacers, north to south
      double angle() const { return std::atan2(direction().y(), direction().x()); } //signed angle w.r.t. horizontal north to south direction
      // ROOT went through the trouble of defining a separate position and direction vector, only to f***up the only operator where the difference matters....
      dir_3d direction() const { return dir_3d(head - tail); } ///< direction pointing towards the readout end, not normalized
      double length() const { return std::sqrt(direction().Mag2()); } ///< length of the line between 
      pos_3d actual(pos_3d) const;
      std::size_t segment(pos_3d x) const;
    };

    using wire_ptr = std::unique_ptr<const wire>;
    using wire_list = std::vector<const wire*>;

    struct plane {
      geo_id geo; ///< The unique geometry identifier
      // There is no daq identifier for a plane
      // These points lie on the nominal plane of the wire
      pos_3d top_north; ///< Top right (looking towards FD) of the sensitive volume
      pos_3d top_south; ///< Top left (looking towards FD) of the sensitive volume
      pos_3d bottom_south;
      pos_3d bottom_north;
      std::vector<wire_ptr> wires; ///< all the wires in this plane, sorted top down, north to south
      std::array<double, s_max_wire_spacers> spacers; ///< Horizontal coordinates of wire spacers, north to south
      pos_3d centre() const { return (top_north + top_south + bottom_south + bottom_north) / 4.0; }
      template <typename Func> wire_list select(Func&& f) const {
        wire_list wl;
        for (const auto& wp : wires) {
          if (std::forward<Func>(f)(*wp)) {
            wl.push_back(wp.get());
          }
        }
        return wl;
      }
    };

    using plane_ptr = std::unique_ptr<const plane>;

  public:
    tracker_info(const geoinfo&, const geo_path&);

  protected:
    void add_plane(plane_ptr&&);

  private:
    std::vector<plane_ptr> m_planes;

  };

}
