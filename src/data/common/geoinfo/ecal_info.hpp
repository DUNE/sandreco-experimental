#pragma once

#include <geoinfo/subdetector_info.hpp>
#include <boost/range/combine.hpp>
#include <numeric>
#include <regex>

namespace sand {

  class geoinfo::ecal_info : public subdetector_info {
   private:
    static constexpr double ktolerance = 1e-10;
    static inline bool is_zero_within_tolerance(double value) { return std::abs(value) < ktolerance; };

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
          light_velocity(1. / 5.85) {}
    };

    inline static /*constexpr*/ fiber kfiber_plane12{430.0};
    inline static /*constexpr*/ fiber kfiber_plane3{380.0};
    inline static /*constexpr*/ fiber kfiber_plane45{330.0};

    // face
    struct shape_element_face {
     public:
      shape_element_face() = delete;
      shape_element_face(const pos_3d& p1, const pos_3d& p2, const pos_3d& p3, const pos_3d& p4);
      bool operator== (const shape_element_face& other) const;
      void transform(const xform_3d transf);
      inline bool is_parallel_to(const shape_element_face& other) const {
        return is_zero_within_tolerance(normal.Cross(other.normal).R());
      };

      inline bool is_perpendicular_to(const shape_element_face& other) const {
        return is_zero_within_tolerance(normal.Dot(other.normal));
      };

      inline dir_3d operator* (const shape_element_face& other) const {
        auto ax = normal.Cross(other.normal);
        return ax / ax.R();
      };

     private:
      std::vector<pos_3d> v;
      dir_3d normal;
      pos_3d centroid;

     private:
      bool are_points_coplanar() const;
    };

    // shape element
    enum class shape_element_type { straight, curved };

    struct shape_element {
     public:
      shape_element_face face1;
      shape_element_face face2;

      shape_element_type type;

      shape_element() = delete;
      shape_element(const shape_element_face& f1, const shape_element_face& f2);

      void transform(const xform_3d transf);
      pos_3d axis_pos() const;
      dir_3d axis_dir() const;
      double length() const;
      pos_3d to_face(int face_id) const;
    };

    enum subdetector_t : uint8_t { BARREL = 0, ENDCAP_A = 1, ENDCAP_B = 2, UNKNOWN = 255 };
    using module_t = uint8_t;
    using plane_t  = uint8_t;
    using column_t = uint8_t;

    struct cell_id {
      uint8_t reserved___0;
      subdetector_t subdetector;
      module_t region;
      plane_t supermodule;
      column_t element;
      uint8_t padding___1[3];
    };

    struct cell {
     public:
      cell_id id;
      fiber fib;
      std::vector<shape_element> elements;
    };

    struct module {
      std::vector<shape_element> elements;
    };

   public:
    ecal_info(const geoinfo&);

    virtual ~ecal_info();

    const cell& at(pos_3d);

    using subdetector_info::path;

    geo_id id(const geo_path&) const override;

    geo_path path(geo_id) const override;

   private:
    using cell_ref = std::vector<cell>::const_iterator;
    std::map<geo_id, std::vector<cell_ref>> m_cell_map;
    std::vector<cell> m_cells;
    static inline const std::regex re_ecal_barrel_module{"/ECAL_lv_PV_(\\d+)$"};
    static inline const std::regex re_ecal_endcap_module{"/ECAL_endcap_lv_PV_(\\d+)/ECAL_ec_mod_(\\d+)_lv_PV_(\\d+)$"};
    static inline const std::regex re_ecal_barrel_sensible_volume{"/ECAL_lv_PV_(\\d+)/volECALActiveSlab_(\\d+)_PV_0$"};
    static inline const std::regex re_ecal_endcap_sensible_volume{
        "/ECAL_endcap_lv_PV_(\\d+)/ECAL_ec_mod_(\\d+)_lv_PV_(\\d+)/ECAL_ec_mod_(curv|vert|hor)_(\\d+)_lv_PV_(\\d+)/"
        "endvolECAL(curv|straight|)ActiveSlab_(\\d+)(_|)(\\d+)_PV_(\\d+)$"};

   private:
    void find_pattern(const geo_path& path);
    void endcap_module_cells(const geo_path& path);
    void barrel_module_cells(const geo_path& path);
  };

} // namespace sand
