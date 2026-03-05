
#pragma once

#include <ufw/data.hpp>
#include <common/digi.h>
#include <common/sand.h>
#include <common/truth.h>
#include <common/cluster.h>
#include <tracker/digi.h>

namespace sand::tracker {

struct cluster_container :
    ufw::data::base<ufw::data::managed_tag, ufw::data::instanced_tag, ufw::data::context_tag> {

    struct cluster : reco::cluster {

        cluster() = default;

        explicit cluster(size_t idx)
            : sand::reco::cluster(idx)
        {}
    };

    using cluster_collection = std::vector<cluster>;
    cluster_collection clusters;
};

} // namespace sand::tracker

UFW_DECLARE_MANAGED_DATA(sand::tracker::cluster_container)
