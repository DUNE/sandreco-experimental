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

    inline static /*constexpr*/ fiber fiber_plane12{430.0};
    inline static /*constexpr*/ fiber fiber_plane3{380.0};
    inline static /*constexpr*/ fiber fiber_plane45{330.0};

    // cell

    struct cell_element_face  {
      pos_3d p1;
      pos_3d p2;
      pos_3d p3;
      pos_3d p4;

      dir_3d normal_dir;

      cell_element_face() = delete;
      cell_element_face(const pos_3d& p1, const pos_3d& p2, const pos_3d& p3, const pos_3d& p4);

      dir_3d normal() const;
    };
    
    enum class cell_element_type { straight, curved };

    struct cell_element {
      cell_element_face face1;
      cell_element_face face2;

      cell_element_type type;
  
      cell_element() = delete;
      cell_element(const cell_element_face& f1, const cell_element_face& f2);
    };
    
    enum class subdetector { barrel, endcapA, endcapB };

    struct cell {
      geo_id id;
      pos_3d centre;
      double length;
      fiber fib;
      std::vector<cell_element> elements;
      subdetector subdect;
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
