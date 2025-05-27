#pragma once

#include <geoinfo/subdetector_info.hpp>

namespace sand {

  /**
   * All positions of wires and planes are in global ND coordinates.
   */
  class geoinfo::tracker_info : public subdetector_info {

  public:
    static const std::size_t s_max_wire_spacers = 0;

    struct wire {
      geo_id geo; ///< The unique geometry identifier
      channel_id channel; ///< The unique daq identifier
      pos_3d head; ///< The readout end of the wire
      pos_3d tail; ///< The termination end of the wire
      std::array<double, s_max_wire_spacers + 1> sagittas; ///< Maximum deflection downwards at the centre of the segment between two spacers
      dir_3d direction() const { return dir_3d(head - tail); } // ROOT went through the trouble of defining a separate position and direction vector, only to f***up the only operator where the difference matters....
      double length() { return std::sqrt(direction().Mag2()); }
    };

    using wire_ptr = std::unique_ptr<const wire>;
    using wire_list = std::vector<const wire*>;

    struct plane {
      geo_id geo; ///< The unique geometry identifier
      // There is no daq identifier for a plane
      //These points lie on the nominal plane of the wire
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

  private:
    std::vector<plane_ptr> m_planes;

  };

}
