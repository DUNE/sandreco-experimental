
#pragma once

#include <ufw/data.hpp>
#include <common/digi.h>
#include <common/sand.h>
#include <common/truth.h>
#include <common/cluster.h>

namespace sand::tracker {

  struct cluster_container : ufw::data::base<ufw::data::managed_tag, ufw::data::instanced_tag, ufw::data::context_tag> {
    /**
     * @brief A signal recorded by a tracker channel.
     */
    struct cluster : reco::cluster {

      
    };

    using cluster_collection = std::vector<cluster>;
    cluster_collection clusters;
  };
} // namespace sand::tracker

UFW_DECLARE_MANAGED_DATA(sand::tracker::cluster_container)
