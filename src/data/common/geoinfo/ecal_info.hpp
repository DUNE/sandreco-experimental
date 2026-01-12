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
          light_velocity(1. / 5.85) {}
    };

    inline static /*constexpr*/ fiber kfiber_plane12{430.0};
    inline static /*constexpr*/ fiber kfiber_plane3{380.0};
    inline static /*constexpr*/ fiber kfiber_plane45{330.0};

    // shape element

    enum class shape_element_type { straight, curved };

    struct shape_element {
     public:
      struct face {
       public:
        pos_3d p1;
        pos_3d p2;
        pos_3d p3;
        pos_3d p4;

        dir_3d normal_dir;

        face() = delete;
        face(const pos_3d& p1, const pos_3d& p2, const pos_3d& p3, const pos_3d& p4);

       private:
        dir_3d normal() const;
        bool are_points_coplanar() const;
      };

      face face1;
      face face2;

      shape_element_type type;

      shape_element() = delete;
      shape_element(const face& f1, const face& f2);

     private:
      bool are_faces_parallel() const;
      bool are_faces_perpendicular() const;
    };

    // cell

    // enum class cell_element_type { straight, curved };

    // struct cell_element {
    //  public:
    //   struct face {
    //    public:
    //     pos_3d p1;
    //     pos_3d p2;
    //     pos_3d p3;
    //     pos_3d p4;

    //     dir_3d normal_dir;

    //     face() = delete;
    //     face(const pos_3d& p1, const pos_3d& p2, const pos_3d& p3, const pos_3d& p4);

    //    private:
    //     dir_3d normal() const;
    //     bool are_points_coplanar() const;
    //   };

    //   face face1;
    //   face face2;

    //   cell_element_type type;

    //   cell_element() = delete;
    //   cell_element(const face& f1, const face& f2);

    //  private:
    //   bool are_faces_parallel() const;
    //   bool are_faces_perpendicular() const;
    // };

    enum class subdetector { barrel, endcapA, endcapB };

    struct cell {
     public:
      geo_id id;
      pos_3d centre;
      double length;
      fiber fib;
      std::vector<shape_element> elements;
      subdetector subdect;
    };

    struct module {
      std::vector<shape_element> elements;
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
    std::map<geo_id, cell> cells_;
    static constexpr double ktolerance = 1e-10;

   private:
    static inline bool is_zero_within_tolerance(double value) { return std::abs(value) < ktolerance; };
    static inline bool string_begins_with(const char* str, const char* prefix) {
      return std::string(str).rfind(prefix) == 0;
    }
  };

} // namespace sand
