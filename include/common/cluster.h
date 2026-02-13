#pragma once

#include <common/sand.h>
#include <common/timerange.h>
#include <common/truth.h>
#include <common/digi.h>
#include <cmath>

namespace sand::reco {

  /**
   * The cluster class represents a generic cluster in the detector.
   * Specialized classes for each detector must inherit from cluster.
   */
  class cluster : public sand::truth {
  //  public:
  //   cluster() {};

    // TODO: implement cluster class
  };


} // namespace sand::reco
