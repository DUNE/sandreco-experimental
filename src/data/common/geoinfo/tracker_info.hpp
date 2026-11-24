#pragma once

#include <geoinfo/subdetector_info.hpp>

#include <optional>
#include <utility>
#include <string>
#include <vector>

class TGeoNavigator;
class TGeoNode;

namespace sand {

  /**
   * All positions of wires and stations are in global ND coordinates.
   * North-south refers to the global orientation of SAND.
   * North is towards negative X, or the side of the cavern towards the shaft.
   * Positive Y is up, Positive Z goes with the beam, towards the FD.
   *
   * Wire deflection follows the catenary curve y = a*cosh((x-x_min)/a)-a+y_min
   * x_min and y_min can be determined by direct observation or fit of the lowest measured point.
   * a is by definition the length of wire whose weight is equal in magnitude to the tension at x_min.
   * It must be determined by fit after x,y_min are known, fixing the head and tail coordinates.
   *
   * Wire deflection is a separate catenary between each fixed point, up to s_max_wire_spacers + 1.
   * Fixed arrays are used instead of vectors because the number is small and likely identical for all wires.
   * Should there be wires with fewer spacers, the excess entries are guaranteed to be nan.
   *
   * There is a one to one correspondance between a wire and a readout channel.
   */
  class geoinfo::tracker_info : public subdetector_info {
   public:
    static const std::size_t s_max_wire_spacers = 0;

    struct station;
    struct wire;

    using wire_ptr  = std::unique_ptr<const wire>;
    using wire_list = std::vector<const wire*>;

    struct wire {
      struct catenary {
        pos_3d minimum;
        double a;
      };

      struct AABB {
        AABB() {};
        AABB(const wire& wire);
        void expand(const AABB& second_aabb);
        bool isOverlapping(const AABB& second_aabb, double epsilon = 0) const;
        pos_3d min_;
        pos_3d max_;
      };

      AABB aabb;

      using catenary_array = std::array<catenary, s_max_wire_spacers + 1>;
      using spacer_array   = std::array<double, s_max_wire_spacers>; ///< The position of each spacer in local X
                                                                     ///< coordinate, starting from north.
      const station* parent;                                         ///< The parent station
      mutable wire_list adjacent_wires;                              ///< The list of adjacent wires.
      ///< The mutable attribute makes the element of a const object modifiable.
      ///< In this case needed because the adjacency can be attributed only once all
      ///< wires are constructed.
      bool is_adjacent(const wire* w) const; ///< Check if the wire is adjecent
      channel_id daq_channel;                ///< The unique daq identifier
      pos_3d head;                           ///< The readout end of the wire
      pos_3d tail;                           ///< The termination end of the wire
      double hv;                             ///< The bias voltage
      double max_radius;                     ///< The maximum drift distance
      catenary_array
          catenaries; ///< Maximum deflection downwards at the centre of the segment between two spacers, north to south
      spacer_array spacers; ///< Horizontal coordinates of wire spacers, north to south
      double angle() const {
        return std::atan2(direction().y(), direction().x());
      } // signed angle w.r.t. horizontal north to south direction
      dir_3d direction() const { return tail - head; } ///< direction pointing towards from head to tail, not normalized
      double length() const { return std::sqrt(direction().Mag2()); } ///< length of the line between
      pos_3d actual(pos_3d) const;
      std::size_t segment(pos_3d x) const;
      std::pair<double, double> closest_approach_segment(const pos_3d&, const pos_3d&) const;
      double closest_approach_segment_distance(const pos_3d&, const pos_3d&) const;
      double closest_approach_point(const pos_3d&) const;
      xform_3d wire_plane_transform() const; /// transform from local wire plane to global coordinates
    };

    enum target_material : uint8_t {
      TRKONLY = 0,
      C3H6    = 1,
      CARBON  = 2,
      NONE    = 255,
    };

    /**
     * @brief Sternheimer parameters of the density-effect correction to the Bethe formula.
     *
     * Notation of Sternheimer, Berger and Seltzer, At. Data Nucl. Data Tables 30 (1984) 261:
     * with x = log10(beta gamma), delta(x) = 2 ln(10) x - C + a (x1 - x)^k for x0 <= x < x1,
     * 2 ln(10) x - C for x >= x1, and delta0 10^(2 (x - x0)) for x < x0 (conductors).
     * The parameters are valid at @ref reference_density; they are not rescaled to the density in the geometry.
     */
    struct sternheimer_parameters {
      double a;                 ///< Coefficient a (dimensionless)
      double k;                 ///< Exponent k, also called m_s (dimensionless)
      double x0;                ///< Lower limit x0 of the intermediate region, x = log10(beta gamma)
      double x1;                ///< Upper limit x1 of the intermediate region, x = log10(beta gamma)
      double C;                 ///< Constant C, written as -C in the tables (C is positive)
      double delta0;            ///< Value of delta at x0 for conductors, 0 for insulators
      double reference_density; ///< Density at which the parameters were computed [g/cm3]
    };

    /**
     * @brief A slab of uniform material crossed by a track going along z through a station.
     *
     * Layers are obtained by walking the geometry along z at one (x, y) point of the station: the centre for the
     * drift chambers, halfway between two adjacent wires for the STT, see docs/geoinfo_tracker_materials.md.
     * They describe the material along that line only.
     */
    struct material_layer {
      double z_min;                                      ///< Upstream face of the layer, global z [mm]
      double z_max;                                      ///< Downstream face of the layer, global z [mm]
      double radiation_length;                           ///< Radiation length X0 [mm]
      double density;                                    ///< Density [g/cm3]
      double z_over_a;                                   ///< Mean ratio <Z/A> of atomic number to atomic mass [mol/g]
      double mean_excitation;                            ///< Mean excitation energy I [MeV]
      std::string material;                              ///< Name of the material in the ROOT geometry
      std::optional<sternheimer_parameters> sternheimer; ///< Density-effect parameters, empty when unknown

      /// Thickness of the layer along z [mm].
      double thickness() const { return z_max - z_min; }

      /// Thickness of the layer in units of radiation length (dimensionless).
      double x_over_x0() const { return thickness() / radiation_length; }
    };

    struct station {
      pos_3d top_north; ///< Top right (looking towards FD) of the sensitive volume
      pos_3d top_south; ///< Top left (looking towards FD) of the sensitive volume
      pos_3d bottom_south;
      pos_3d bottom_north;
      double thickness;
      std::vector<wire_ptr> wires; ///< all the wires in this station, sorted top down, north to south
      target_material target;
      dir_3d target_box;     ///< Full size of the target box along global x, y, z [mm], all components positive
      double target_density; ///< Density of the target material [g/cm3]
      std::vector<material_layer> layers; ///< Material crossed along z at the station centre, ordered by z
      tracker_info* parent;
      channel_id::link_t daq_link; ///< the daq link number for this station (not 1-1)
      template <typename Func>
      wire_list select(Func&& f) const {
        wire_list wl;
        for (const auto& wp : wires) {
          if (std::forward<Func>(f)(*wp)) {
            wl.push_back(wp.get());
          }
        }
        return wl;
      }
      void set_wire_adjacency();

      /// Thickness of the station material in radiation lengths: the sum of x/X0 over @ref layers (dimensionless).
      double radiation_lengths() const;
    };

    using target_mass_t = std::map<target_material, double>; ///< Target mass [kg] per target material

   protected:
    using station_ptr = std::unique_ptr<const station>;

    struct gas_volume {
      const wire* w = nullptr;
      const station* p;
      std::string gas;
      double gas_pressure;
    };

   public:
    tracker_info(const geoinfo&, const geo_path&, const ufw::config&);

    virtual ~tracker_info();

    using subdetector_info::path;

    const std::vector<station_ptr>& stations() const { return m_stations; }

    const station* get_station_by_ID(std::size_t i) const { return m_stations.at(i).get(); }

    virtual target_mass_t target_masses() const;

    virtual const wire& wire_at(channel_id) const;

    std::pair<const wire*, size_t> closest_wire_in_list(wire_list, pos_3d) const;

   protected:
    void add_station(station_ptr&&);

    static std::vector<material_layer> sample_material_layers(TGeoNavigator&, const geo_path&,
                                                              std::optional<std::pair<double, double>> xy = {});

    static void set_target(station&, TGeoNavigator&, const TGeoNode&);

    void add_volume(const geo_path&, const gas_volume&);

    const station* at(std::size_t i) const { return m_stations.at(i).get(); }

    template <typename Func>
    void for_each_station(Func&& f) const {
      for (auto& sptr : m_stations) {
        std::forward<Func>(f)(*sptr);
      }
    }

   private:
    std::vector<station_ptr> m_stations;
    std::map<geo_path, gas_volume> m_volumes;
  };

} // namespace sand
