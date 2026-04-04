#ifndef DEBUG_INFO_HPP
#define DEBUG_INFO_HPP

#include <string>
#include <vector>

#include <ufw/data.hpp>
#include <edep_reader/EDEPHit.h>

namespace sand::debug {

struct debug_data : public ufw::data::base<ufw::data::managed_tag, ufw::data::instanced_tag, ufw::data::context_tag> {

    std::map<int, std::vector<EDEPHit>> track_hits_map;

    std::vector<EDEPHit>  hits{};
    std::vector<int>      hit_track_id{};

    void clear() {
        track_hits_map.clear();
        hits.clear();
        hit_track_id.clear();
    }

    void pack() {
        hits.clear();
        hit_track_id.clear();
        for (auto& [track_id, hit_vec] : track_hits_map)
            for (auto& h : hit_vec) {
                hits.push_back(h);
                hit_track_id.push_back(track_id);
            }
    }

    void unpack() {
        track_hits_map.clear();
        for (int i = 0; i < hits.size(); i++)
            track_hits_map[hit_track_id[i]].push_back(hits[i]);
    }
};

} // namespace sand::debug

UFW_REGISTER_TYPE_NAME(sand::debug::debug_data, "sand::debug::debug_data")
UFW_DECLARE_RTTI(sand::debug::debug_data)

#endif // DEBUG_INFO_HPP



// struct trajectory_debug {
//     int pdg{};
//     int trj_idx{};
//     std::vector<EDEPHit> hits{};
// };

// struct interaction_debug {
//     int interaction_idx{};
//     std::vector<trajectory_debug> trajectories{};
// };

// struct debug_data : public ufw::data::base<ufw::data::managed_tag, ufw::data::instanced_tag, ufw::data::context_tag> {
//   std::vector<interaction_debug> edep_interactions{};

//   void clear() {
//     edep_interactions.clear();
//   }
// };