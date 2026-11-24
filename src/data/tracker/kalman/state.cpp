#include <kalman/state.hpp>

namespace sand::tracker::kalman {

  matrix_t identity() {
    matrix_t m;
    for (unsigned int i = 0; i < s_dim; ++i) {
      m(i, i) = 1.;
    }
    return m;
  }

} // namespace sand::tracker::kalman
