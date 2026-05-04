
#pragma once

#include <ufw/data.hpp>
#include <common/digi.h>
#include <common/sand.h>
#include <common/truth.h>
#include <common/cluster.h>
#include <tracker/digi.h>

namespace sand::tracker {

/**
 * @class cluster_container
 * @brief Container for clusters in the tracker.
 */

struct cluster_container :
    ufw::data::base<ufw::data::managed_tag, ufw::data::instanced_tag, ufw::data::context_tag> {

    template <typename T = sand::truth_index>    
    struct cluster : reco::cluster<T> {

        cluster() = default;

        explicit cluster(size_t idx)
                : reco::cluster<T>(idx)  
            {}
    };

/**
 * @brief Collection of clusters in the tracker. Each cluster contains a vector of digit indices that belong to that cluster.
 */
    using cluster_collection = std::vector<cluster<>>;
    cluster_collection clusters;
};

} // namespace sand::tracker

UFW_DECLARE_MANAGED_DATA(sand::tracker::cluster_container)
