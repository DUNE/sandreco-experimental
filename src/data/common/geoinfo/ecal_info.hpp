#pragma once

#include <geoinfo/subdetector_info.hpp>
#include <regex>

namespace sand {

  class geoinfo::ecal_info : public subdetector_info {
   public:
    class invalid_path : public std::exception {
     public:
      invalid_path(const std::string& msg) : message(msg) {}
      const char* what() const noexcept override { return message.c_str(); }

     private:
      std::string message;
    };
    // fiber
    struct fiber {
      ////////////////////////////////////////////////////////////////////////////
      //      dE/dx attenuation                                                 //
      ////////////////////////////////////////////////////////////////////////////
      // dE/dx attenuation - Ea=p1*exp(-d/atl1)+(1.-p1)*exp(-d/atl2)            //
      // with:                                                                  //
      //  - d              distance from photocatode - 2 PMTs/cell; d1 and d2   //
      //  - atl1  50. cm                                                        //
      //  - atl2  430 cm   planes 1-2  (innermost plane is 1)                   //
      //          380 cm   plane 3                                              //
      //          330 cm   planes 4-5                                           //
      //  - p1   0.35                                                           //
      ////////////////////////////////////////////////////////////////////////////

      // attenuation
      double attenuation_length_1; // mm
      double attenuation_length_2; // mm
      double fraction;

      ////////////////////////////////////////////////////////////////////////////
      //      Scintillation                                                     //
      ////////////////////////////////////////////////////////////////////////////
      //                                                                        //
      // C  PHOTOELECTRON TIME :  Particle TIME in the cell                     //
      // C                      + SCINTILLATION DECAY TIME +                    //
      // C                      + signal propagation to the PMT                 //
      // C                      + 1ns  uncertainty                              //
      //                                                                        //
      //                TPHE = Part_time+TSDEC+DPM1*VLFB+Gauss(1ns)             //
      //                                                                        //
      //       VLFB = 5.85 ns/m                                                 //
      // !!!! Input-TDC Scintillation time -                                    //
      //                TSDEC = TSCIN*(1./RNDMPH(1)-1)**TSCEX  (ns)             //
      //                                                                        //
      //       TSCIN  3.08  ns                                                  //
      //       TSCEX  0.588                                                     //
      ////////////////////////////////////////////////////////////////////////////

      // scintillation
      double scintillation_constant_1; // ns
      double scintillation_constant_2;
      double scintillation_rise_time;  // ns
      double scintillation_decay_time; // ns

      // light velocity
      double light_velocity; // mm/ns

      constexpr fiber(double atl2)
        : attenuation_length_1(500.0),
          attenuation_length_2(atl2),
          fraction(0.35),
          scintillation_constant_1(3.8),
          scintillation_constant_2(0.588),
          scintillation_rise_time(0.7),
          scintillation_decay_time(3.0),
          light_velocity(1000. / 5.85) {}
    };

    inline static /*constexpr*/ fiber kfiber_plane12{4300.0};
    inline static /*constexpr*/ fiber kfiber_plane3{3800.0};
    inline static /*constexpr*/ fiber kfiber_plane45{3300.0};

    // face
    struct shape_element_face {
     public:
      shape_element_face() = delete;
      // (p1, p2, p3, p4) are consecutive points along the perimeter; start point and direction are arbitrary.
      shape_element_face(const pos_3d& p1, const pos_3d& p2, const pos_3d& p3, const pos_3d& p4);
      const shape_element_face& transform(const xform_3d& transf);

      inline dir_3d side(size_t idx) const { return vtx(idx + 1) - vtx(idx); };
      inline const std::array<pos_3d, 4>& vtx() const { return v_; };
      inline const pos_3d& vtx(std::size_t i) const { return v_[((i % 4) + 4) % 4]; };
      inline const dir_3d& normal() const { return normal_; };
      inline const pos_3d& centroid() const { return centroid_; };
      size_t narrowest_trapezoid_basis() const;

     private:
      std::array<pos_3d, 4> v_; // four vertices of the face
      dir_3d normal_;           // normal of the  face
      pos_3d centroid_;         // centroid of the face

     private:
      bool are_vtx_coplanar() const;
    };

    // shape element
    enum class shape_element_type { straight, curved };
    enum class face_location { begin, end };

    struct shape_element_base {
      shape_element_face begin_face_; // face of the shape
      shape_element_face end_face_;   // face of the shape
      pos_3d axis_pos_;               // position of the shape axis
      dir_3d axis_dir_;               // direction of the shape axis
      shape_element_type type_;       // shape type

     public:
      shape_element_base() = delete;

     protected:
      shape_element_base(const shape_element_face& f1, const shape_element_face& f2)
        : begin_face_(f1), end_face_(f2) {};

     public:
      void transform(const xform_3d& transf);
      inline const pos_3d& axis_pos() const { return axis_pos_; };
      inline const dir_3d& axis_dir() const { return axis_dir_; };
      inline const shape_element_face& begin_face() const { return begin_face_; };
      inline const shape_element_face& end_face() const { return end_face_; };
      inline const shape_element_face& face(face_location face_id) const {
        return face_id == face_location::end ? end_face() : begin_face();
      };
      inline const shape_element_type type() const { return type_; };
      double total_pathlength() const;
      bool is_inside(const pos_3d& p) const;
      virtual double pathlength(const pos_3d& p1, const pos_3d& p2) const  = 0;
      virtual pos_3d to_face(const pos_3d& p, face_location face_id) const = 0;
      virtual pos_3d offset2position(double offset_from_center) const      = 0;
      void swap_faces() {
        std::swap(begin_face_, end_face_);
        axis_dir_ *= -1;
      };
      shape_element_face to_face(const shape_element_face& f, face_location face_id) const;
    };

    using p_shape_element_base = std::unique_ptr<shape_element_base>;
    using el_vec               = std::vector<p_shape_element_base>;
    using el_vec_it            = std::vector<p_shape_element_base>::iterator;

    struct shape_element_straight : public shape_element_base {
     public:
      shape_element_straight(const shape_element_face& f1, const shape_element_face& f2);
      double pathlength(const pos_3d& p1, const pos_3d& p2) const override;
      pos_3d to_face(const pos_3d& p, face_location face_id) const override;
      pos_3d offset2position(double offset_from_center) const override;
    };

    struct shape_element_curved : public shape_element_base {
     public:
      shape_element_curved(const shape_element_face& f1, const shape_element_face& f2);
      double pathlength(const pos_3d& p1, const pos_3d& p2) const override;
      pos_3d to_face(const pos_3d& p, face_location face_id) const override;
      pos_3d offset2position(double offset_from_center) const override;
    };

    struct shape_element_collection {
     private:
      std::vector<p_shape_element_base> elements_;

     public:
      inline const std::vector<p_shape_element_base>& elements() const { return elements_; };
      void add(p_shape_element_base&& el) { elements_.emplace_back(std::move(el)); };
      inline const shape_element_base& at(size_t idx) const { return *(elements_.at(idx)); };
      inline size_t size() const { return elements_.size(); };
      void order_elements();
      double total_pathlength() const;
      bool is_inside(const pos_3d& p) const;
      size_t longest_side() const;
    };

    using module_t = uint8_t;
    using row_t    = uint8_t;
    using column_t = uint8_t;

    struct module_id {
      union {
        struct {
          geo_id::region_t region;
          module_t module_number;
        };
        uint16_t raw = -1;
      };
    };

    struct cell_id {
      union {
        struct {
          geo_id::region_t region;
          module_t module_number;
          row_t row;
          column_t column;
        };
        uint32_t raw = -1;
      };
    };

    struct pmt_id {
      cell_id cell_;
      face_location face_;
    };

    struct grid {
     private:
      std::vector<pos_3d> nodes_;
      size_t nrow_;
      size_t ncol_;

     public:
      grid() = delete;
      grid(const pos_3d& p1, const pos_3d& p2, const pos_3d& p3, const pos_3d& p4, const std::vector<double>& div12,
           const std::array<double, 5>& div14);
      shape_element_face face(size_t irow, size_t icol) const;
      inline size_t nrow() const { return nrow_; };
      inline size_t ncol() const { return ncol_; };
      inline const pos_3d& get_node(size_t icol, size_t irow) const { return nodes_.at(irow + (nrow_ + 1) * icol); };

     private:
      inline pos_3d& node(size_t icol, size_t irow) { return nodes_[irow + (nrow_ + 1) * icol]; };
    };

    struct cell {
     public:
      cell() = delete;
      cell(cell_id id, fiber f) : id_(id), fib_(f) {};
      void add(p_shape_element_base&& el) { el_collection_.add(std::move(el)); };
      inline const shape_element_collection& element_collection() const { return el_collection_; };
      inline const fiber& get_fiber() const { return fib_; };
      inline cell_id id() const { return id_; };
      inline bool is_inside(const pos_3d& p) const { return element_collection().is_inside(p); }
      double pathlength(const pos_3d& p, face_location face_id) const;
      double attenuation(double d) const;
      pos_3d offset2position(double offset_from_center) const;
      inline double total_pathlength() const { return element_collection().total_pathlength(); };

     private:
      cell_id id_;
      fiber fib_;
      shape_element_collection el_collection_;
    };

    using cell_ref = std::map<cell_id, cell>::const_iterator;

    struct module {
     public:
      module() = delete;
      module(const geo_path& path);
      inline module_id id() const { return id_; };
      void add(p_shape_element_base&& el) { el_collection_.add(std::move(el)); };
      inline const shape_element_collection& element_collection() const { return el_collection_; };
      inline void order_elements() { el_collection_.order_elements(); };
      grid construct_grid(const std::vector<double>& col_widths) const;
      cell construct_cell(const shape_element_face& f, cell_id id, const fiber& fib) const;

     private:
      module_id id_;
      shape_element_collection el_collection_;
      shape_element_collection al_plate_;
      static constexpr std::array<double, 5> row_widths_{40., 40., 40., 40., 49.};
      void construct_al_plate(const geo_path& path);
      static module_id to_module_id(const geo_path& path);
    };

   public:
    ecal_info(const geoinfo&);
    virtual ~ecal_info();
    const cell& at(const pos_3d& p) const;
    const cell& at(cell_id cid) const;
    const std::vector<cell_ref>& cells(geo_id gid) const;
    inline pmt_id pmt(channel_id cid) const {
      pmt_id pid;
      pid.cell_.region        = static_cast<geo_id::region_t>(cid.link);
      pid.cell_.module_number = (cid.channel >> 16) & 0xFF;
      pid.cell_.row           = (cid.channel >> 8) & 0xFF;
      pid.cell_.column        = cid.channel & 0xFF;
      pid.face_               = static_cast<face_location>((cid.channel >> 24) & 0xFF);
      return pid;
    };
    inline channel_id channel(pmt_id pid) const {
      channel_id c;
      c.subdetector = subdetector_t::ECAL;
      c.channel     = (static_cast<uint32_t>(pid.face_) << 24) | (pid.cell_.module_number << 16) | (pid.cell_.row << 8)
                | pid.cell_.column;
      c.link = pid.cell_.region;
      return c;
    };

    using subdetector_info::path;

    geo_id id(const geo_path& path) const override;
    geo_path path(geo_id gid) const override;

   private:
    std::map<module_id, std::map<cell_id, cell>> m_modules_cells_maps;
    std::map<geo_id, std::vector<cell_ref>> m_cells_map;

   private:
    void find_modules(const geo_path& path);
    void find_active_volumes(const geo_path& path, const std::regex& re);
    void endcap_module_cells(const geo_path& path);
    void barrel_module_cells(const geo_path& path);
    void construct_module_cells(const module& m, const grid& g);
  };

} // namespace sand
