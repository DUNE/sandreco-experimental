#pragma once

#include <geoinfo/subdetector_info.hpp>

namespace sand {

  class geoinfo::ecal_info : public subdetector_info {
   public:
    
    // fiber
    struct fiber {
      // attenuation
      double attenuation_length_1; // cm
      double attenuation_length_2; // cm
      double fraction;

      // scintillation
      double scintillation_constant_1; // ns
      double scintillation_constant_2;

      // light velocity
      double light_velocity; // m/ns

      constexpr fiber(double atl2)
          : attenuation_length_1(50.0),
            attenuation_length_2(atl2),
            fraction(0.35),
            scintillation_constant_1(3.8),
            scintillation_constant_2(0.588),
            light_velocity(1./5.85) {}
    };

    inline constexpr fiber_plane12 = fiber(430.0);
    inline constexpr fiber_plane3  = fiber(380.0);
    inline constexpr fiber_plane45 = fiber(330.0);    
    
    enum class cell_type { barrel, endcap };

    struct cell {
      geo_id id;
      pos_3d centre;
      double length;
      cell_type type;
    };

   public:
    ecal_info(const geoinfo&);

    virtual ~ecal_info();

    const cell& at(pos_3d);

    const cell& at(geo_id);

    using subdetector_info::path;

    geo_id id(const geo_path&) const override;

    geo_path path(geo_id) const override;

   private:
    std::map<geo_id, cell> m_cells;
  };

} // namespace sand
