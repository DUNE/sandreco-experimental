#pragma once

#include <geoinfo/subdetector_info.hpp>
#include <regex>

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

    // face
    struct shape_element_face {
     public:
      shape_element_face() = delete;
      shape_element_face(const pos_3d& p1, const pos_3d& p2, const pos_3d& p3, const pos_3d& p4);
      bool operator== (const shape_element_face& other) const;
      inline bool operator!= (const shape_element_face& other) const { return !(*this == other); };
      inline const shape_element_face& operator* (const xform_3d& transf) const {
        return *(new shape_element_face(transf * vtx(0), transf * vtx(1), transf * vtx(2), transf * vtx(3)));
      };
      const shape_element_face& transform(const xform_3d& transf);

      inline dir_3d side(size_t idx) const { return vtx(idx) - vtx(idx + 1); };
      inline const std::vector<pos_3d>& vtx() const { return v_; };
      inline const pos_3d& vtx(std::size_t i) const { return v_[i % 4]; };
      inline const dir_3d& normal() const { return normal_; };
      inline const pos_3d& centroid() const { return centroid_; };

     private:
      std::vector<pos_3d> v_;
      dir_3d normal_;
      pos_3d centroid_;

     private:
      bool are_points_coplanar() const;
    };

    // shape element
    enum class shape_element_type { straight, curved };

    class module;
    class shape_element_collection;

    struct shape_element_base {
      shape_element_face face1_;
      shape_element_face face2_;
      pos_3d axis_pos_;
      dir_3d axis_dir_;
      // consider to remove
      shape_element_type type_;

     public:
      shape_element_base() = delete;
      shape_element_base(const shape_element_face& f1, const shape_element_face& f2) : face1_(f1), face2_(f2) {};

      void transform(const xform_3d& transf);
      inline const pos_3d& axis_pos() const { return axis_pos_; };
      inline const dir_3d& axis_dir() const { return axis_dir_; };
      inline const shape_element_face& face1() const { return face1_; };
      inline const shape_element_face& face2() const { return face2_; };
      inline const shape_element_face& face(size_t face_id) const { return face_id % 2 == 0 ? face2() : face1(); };
      inline const shape_element_type type() const { return type_; };

     private:
      virtual double to_face(const pos_3d& p, size_t face_id, pos_3d& p2) const             = 0;
      virtual pos_3d to_face(const pos_3d& p, size_t face_id) const                         = 0;
      virtual shape_element_face to_face(const shape_element_face& f, size_t face_id) const = 0;

      void swap_faces() {
        std::swap(face1_, face2_);
        axis_dir_ *= -1;
      };

      friend class shape_element_collection;
      friend class module;
    };

    struct shape_element_straight : public shape_element_base {
     public:
      shape_element_straight(const shape_element_face& f1, const shape_element_face& f2);

     private:
      double to_face(const pos_3d& p, size_t face_id, pos_3d& p2) const override;
      pos_3d to_face(const pos_3d& p, size_t face_id) const override;
      shape_element_face to_face(const shape_element_face& f, size_t face_id) const override {
        return shape_element_face(to_face(f.vtx(0), face_id), to_face(f.vtx(1), face_id), to_face(f.vtx(2), face_id),
                                  to_face(f.vtx(3), face_id));
      };

      friend class module;
    };

    struct shape_element_curved : public shape_element_base {
     public:
      shape_element_curved(const shape_element_face& f1, const shape_element_face& f2);

     private:
      double to_face(const pos_3d& p, size_t face_id, pos_3d& p2) const override;
      pos_3d to_face(const pos_3d& p, size_t face_id) const override;
      shape_element_face to_face(const shape_element_face& f, size_t face_id) const override {
        return shape_element_face(to_face(f.vtx(0), face_id), to_face(f.vtx(1), face_id), to_face(f.vtx(2), face_id),
                                  to_face(f.vtx(3), face_id));
      };

      friend class module;
    };

    struct shape_element_collection {
     private:
      std::vector<std::shared_ptr<shape_element_base>> elements_;

     public:
      void add(const std::shared_ptr<shape_element_base>& el) { elements_.push_back(el); };
      inline const shape_element_base& at(size_t idx) const { return *(elements_.at(idx)); };
      inline size_t size() const { return elements_.size(); };
      void order_elements();
    };

    enum subdetector_t : uint8_t { BARREL = 0, ENDCAP_A = 1, ENDCAP_B = 2, UNKNOWN = 255 };
    using module_t = uint8_t;
    using row_t    = uint8_t;
    using column_t = uint8_t;

    struct module_id {
      subdetector_t subdetector;
      module_t module;
    };

    struct cell_id {
      module_id module;
      row_t row;
      column_t column;
    };

    struct cell {
     public:
      cell() = delete;
      cell(cell_id id, const fiber& f) : id_(id), fib_(f) {};
      void add(const std::shared_ptr<shape_element_base>& el) { el_collection_.add(el); };
      inline const shape_element_collection& elements() const { return el_collection_; };
      inline const fiber& get_fiber() const { return fib_; };
      inline cell_id id() const { return id_; };

     private:
      cell_id id_;
      fiber fib_;
      shape_element_collection el_collection_;
    };

    using cell_ref = std::vector<cell>::const_iterator;

    struct module {
      struct grid {
       private:
        std::vector<pos_3d> nodes_;
        size_t nrow_;
        size_t ncol_;

       public:
        grid() = delete;
        grid(const pos_3d& p1, const pos_3d& p2, const pos_3d& p3, const pos_3d& p4, const std::vector<double>& div12,
             const std::vector<double>& div14);
        shape_element_face face(size_t irow, size_t icol);
        inline size_t nrow() { return nrow_; };
        inline size_t ncol() { return ncol_; };

       private:
        inline pos_3d& node(size_t irow, size_t icol) { return nodes_[irow + nrow_ * icol]; };
      };

     public:
      module() = delete;
      module(module_id id) :id_(id) {};
      inline module_id id() const { return id_; };
      void add(const std::shared_ptr<shape_element_base>& el) { el_collection_.add(el); };
      inline const shape_element_collection& elements() const { return el_collection_; };
      void construct_cells(std::vector<geoinfo::ecal_info::cell>& cells);

     private:
      cell construct_cell(const shape_element_face& f, cell_id id, const fiber& fib) const;
      grid construct_grid() const;

     private:
      module_id id_;
      shape_element_collection el_collection_;
    };

   public:
    ecal_info(const geoinfo&);

    virtual ~ecal_info();

    const cell& at(pos_3d);

    using subdetector_info::path;

    geo_id id(const geo_path&) const override;
    geo_path path(geo_id) const override;

   private:
    std::map<geo_id, std::vector<cell_ref>> m_cell_map;
    std::vector<cell> m_cells;
    static inline const std::regex re_ecal_barrel_module{"/ECAL_lv_PV_(\\d+)$"};
    static inline const std::regex re_ecal_endcap_module{"/ECAL_endcap_lv_PV_(\\d+)/ECAL_ec_mod_(\\d+)_lv_PV_(\\d+)$"};
    static inline const std::regex re_ecal_barrel_sensible_volume{"/ECAL_lv_PV_(\\d+)/volECALActiveSlab_(\\d+)_PV_0$"};
    static inline const std::regex re_ecal_endcap_sensible_volume{
        "/ECAL_endcap_lv_PV_(\\d+)/ECAL_ec_mod_(\\d+)_lv_PV_(\\d+)/ECAL_ec_mod_(curv|vert|hor)_(\\d+)_lv_PV_(\\d+)/"
        "endvolECAL(curv|straight|)ActiveSlab_(\\d+)(_|)(\\d+)_PV_(\\d+)$"};

   private:
    static module_id to_module_id(const geo_path& path);
    void find_pattern(const geo_path& path);
    void endcap_module_cells(const geo_path& path);
    void barrel_module_cells(const geo_path& path);
  };

} // namespace sand
