#pragma once

#include <array>

/**
 * @file field.hpp
 * @brief Magnetic field interface used by the propagator.
 *
 * The field is given in T at a position in mm, in the global frame. The first implementation is a uniform field;
 * the field read from the BField auxiliary tags of the GDML will be another implementation of the same interface.
 * The engine owns the field of a job through a pointer to this interface.
 */

namespace sand::tracker::kalman {

  using field_vector = std::array<double, 3>; ///< (Bx, By, Bz) [T]

  /// Magnetic field map, queried by the propagator at each Runge-Kutta stage.
  class field {
   public:
    virtual ~field();
    /// Field [T] at the global position (x, y, z) [mm]
    virtual field_vector at(double x, double y, double z) const = 0;
  };

  /// Field with the same value everywhere. Useful for tests and as a first approximation inside the magnet.
  class uniform_field : public field {
   public:
    /// @param b Field value (Bx, By, Bz) [T]
    explicit uniform_field(field_vector b) : m_b(b) {}
    field_vector at(double, double, double) const override { return m_b; }

   private:
    field_vector m_b;
  };

} // namespace sand::tracker::kalman
