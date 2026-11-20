#pragma once

#include <array>

/**
 * @file field.hpp
 * @brief Magnetic field interface used by the propagator.
 *
 * The field is given in T at a position in mm, in the global frame. The first implementation is a uniform field;
 * the field read from the GDML auxiliary BField tags (see the design document) will be another implementation of the
 * same interface.
 */

namespace sand::tracker::kalman {

  using field_vector = std::array<double, 3>; ///< (Bx, By, Bz) [T]

  class field {
   public:
    virtual ~field()                                            = default;
    virtual field_vector at(double x, double y, double z) const = 0;
  };

  class uniform_field : public field {
   public:
    explicit uniform_field(field_vector b) : m_b(b) {}
    field_vector at(double, double, double) const override { return m_b; }

   private:
    field_vector m_b;
  };

} // namespace sand::tracker::kalman
