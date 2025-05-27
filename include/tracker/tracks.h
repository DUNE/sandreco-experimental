#pragma once

#include <vector>
#include <cstdint>

#include <ufw/data.hpp>
#include <common/truth.h>

namespace sand::tracker {

  struct tracks : ufw::data::base<ufw::data::managed_tag, ufw::data::instanced_tag, ufw::data::context_tag> {
    struct track
    {
      int tid = -1;
      double yc = NAN;
      double zc = NAN;
      double r = NAN;
      double a = NAN;
      double b = NAN;
      double h = NAN;
      double ysig = NAN;
      double x0 = NAN;
      double y0 = NAN;
      double z0 = NAN;
      double t0 = NAN;
      int ret_ln = -1;
      double chi2_ln = NAN;
      int ret_cr = -1;
      double chi2_cr = NAN;
    };

    using track_collection = std::vector<track>;
    track_collection tracks;
  };

}

UFW_DECLARE_MANAGED_DATA(sand::tracker::tracks)
