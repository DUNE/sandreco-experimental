#pragma once

#include <vector>
#include <cstdint>

#include <ufw/data.hpp>
#include <common/truth.h>

namespace sand::tracker {

  struct tracks : ufw::data::base<ufw::data::managed_tag, ufw::data::instanced_tag, ufw::data::context_tag> {

    struct dg_wire
    {
        std::string det;
        long did;
        double x;
        double y;
        double z;
        double t0;
        double de;
        double adc;
        double tdc = 1e9;
        bool hor;
        double wire_length;
        std::vector<int> hindex;
        /*
          ADDENDUM
          tdc = drift_time + signal_time + t_hit
          added to check validity of track fitting
          reconstruction method for drift chamber
        */
        // true quantities
        double t_hit = 1e9;
        double signal_time = 1e9;
        double drift_time = 1e9;
        // measured quantities
        double t_hit_measured = 1e9;        // via global trigger
        double signal_time_measured = 1e9;  // exploit different wire orientation
        double drift_time_measured =
            1e9;  // tdc - signal_time_measured - t_hit_measured

        double missing_coordinate = 1e9;
    };
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
      std::vector<dg_wire> clX;
      std::vector<dg_wire> clY;
    };

    using track_collection = std::vector<track>;
    track_collection tracks;
  };

}

UFW_DECLARE_MANAGED_DATA(sand::tracker::tracks)
