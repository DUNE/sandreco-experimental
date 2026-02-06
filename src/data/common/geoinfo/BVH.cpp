#include "BVH.hpp"
#include <TVector2.h>


namespace sand {

    AABB::AABB(const std::unique_ptr<geoinfo::tracker_info::wire> & wire, geoinfo::tracker_info * geo){
        min_ = pos_3d(1E9, 1E9, 1E9);
        max_ = pos_3d(-1E9, -1E9, -1E9);
        std::vector<pos_3d> vertices;
        auto p = wire->head;
        const auto & transform = wire->wire_plane_transform();
        // auto& plane = *geo->getPlaneInfo(cell->first);
        auto p_rotated = transform.Inverse() * p; 
        
        auto h_2 = wire->length() /2.;
        auto w_2 = wire->max_radius /2.;

        pos_3d v1 = p_rotated + dir_3d(0, h_2, 0);
        pos_3d v2 = p_rotated + dir_3d(0, -h_2, 0);
        
        auto p1 = transform * v1; 
        auto p2 = transform * v2;

        vertices.push_back(pos_3d(p1.X(), p1.Y(), p.Z() + w_2));
        vertices.push_back(pos_3d(p2.X(), p2.Y(), p.Z() + w_2));   

        p = wire->tail;
        p_rotated = transform.Inverse() * p; 
        pos_3d v3 = p_rotated + dir_3d(0, h_2, 0);
        pos_3d v4 = p_rotated + dir_3d(0, -h_2, 0);

        auto p3 = transform * v3; 
        auto p4 = transform * v4;

        vertices.push_back(pos_3d(p3.X(), p3.Y(), p.Z() - w_2));
        vertices.push_back(pos_3d(p4.X(), p4.Y(), p.Z() - w_2));

        for(const auto &v : vertices){
            min_.SetX(std::min(min_.X(), v.X()));
            max_.SetX(std::max(max_.X(), v.X()));
            min_.SetY(std::min(min_.Y(), v.Y()));
            max_.SetY(std::max(max_.Y(), v.Y()));
            min_.SetZ(std::min(min_.Z(), v.Z()));
            max_.SetZ(std::max(max_.Z(), v.Z()));
        }
    }

    void AABB::expand(const AABB& second_aabb){
        min_.SetX(std::min(min_.X(), second_aabb.min_.X()));
        max_.SetX(std::max(max_.X(), second_aabb.max_.X()));
        min_.SetY(std::min(min_.Y(), second_aabb.min_.Y()));
        max_.SetY(std::max(max_.Y(), second_aabb.max_.Y()));
        min_.SetZ(std::min(min_.Z(), second_aabb.min_.Z()));
        max_.SetZ(std::max(max_.Z(), second_aabb.max_.Z()));
    }

    bool AABB::isOverlapping(const AABB& second_aabb, double epsilon) const {
        if(max_.X() + epsilon >= second_aabb.min_.X() && min_.X() - epsilon <= second_aabb.max_.X() && 
            max_.Y() + epsilon >= second_aabb.min_.Y() && min_.Y() - epsilon <= second_aabb.max_.Y() && 
            max_.Z() + epsilon >= second_aabb.min_.Z() && min_.Z() - epsilon <= second_aabb.max_.Z()){
            return true;
        }
        return false;
    }

    void BVH::fillCellAABBMap(const std::vector<std::unique_ptr<geoinfo::tracker_info::wire>>& wires, geoinfo::tracker_info * geo){
        for(const auto &wire : wires){
            cellAABBs_[wire.get()] = AABB(wire, geo);
        }
    }

void BVH::createTree(std::unique_ptr<Node>& node,const std::vector<std::unique_ptr<geoinfo::tracker_info::wire>>::iterator begin, std::vector<std::unique_ptr<geoinfo::tracker_info::wire>>::iterator end, geoinfo::tracker_info * geo){
    node->aabb_ = cellAABBs_[begin->get()];  
    for (auto it = begin + 1; it != end; ++it) {
        node->aabb_.expand(cellAABBs_[it->get()]);  
    }

    // node->index_ = sand_geometry::tracker::CellID(std::nullopt);
    
    // if(std::distance(begin, end) == 1){
    //     node->index_ = (*begin)->first;
    //     node->cell_iterator_ = (*begin);
    //     return;
    // }
    
    // auto sorting_by_z = [geo](const sand_geometry::tracker::cell_map_iterator c1, const sand_geometry::tracker::cell_map_iterator c2){
    //     const auto& center_1 = c1->second.getWire().getCenter();
    //     const auto& center_2 = c2->second.getWire().getCenter();
        
    //     return center_1.Z() < center_2.Z();
    // };
    
    // auto sorting_by_id = [geo](const sand_geometry::tracker::cell_map_iterator c1, const sand_geometry::tracker::cell_map_iterator c2){
    //     const auto& id1 = c1->second.getId();
    //     const auto& id2 = c2->second.getId();
        
    //     return id1 < id2;
    // };
    
    // double deltaZ = node->aabb_.max_.Z() - node->aabb_.min_.Z();
    // int middle_point= std::distance(begin, end) / 2;
    // const double w = (*begin)->second.getSize().w;
    // if (w != deltaZ) {
    //     // std::sort(begin, end, sorting_by_z);
    //     std::nth_element(begin, begin + middle_point, end, sorting_by_z);
    // } else {
    //     // std::sort(begin, end, sorting_by_id);
    //     std::nth_element(begin, begin + middle_point, end, sorting_by_id);
    // }
    // node->left_ = std::make_unique<Node>();
    // createTree(node->left_, begin, begin + middle_point, geo);

    // node->right_ = std::make_unique<Node>();
    // createTree(node->right_, begin + middle_point, end, geo);
 
    return;
}


// void BVH::searchAdjacentCells(std::unique_ptr<Node>& node, std::unique_ptr<Node>& other_node, SANDGeoManager* geo, double max_distance, double overlap_tolerance){
//     if(!node || !other_node) return;
    
//     if(!node->aabb_.isOverlapping(other_node->aabb_, overlap_tolerance)) return;

//     if(other_node->index_() && node->index_()) {
//         if(other_node->index_ == node->index_) {
//             return;
//         }

//         if (node->index_ < other_node->index_) {
//             return;
//         }
//         const auto& wire = geo->getCellInfo(node->index_)->second.getWire();
//         const auto& other_wire = geo->getCellInfo(other_node->index_)->second.getWire();
        
//         double distance = geo->getMinDistanceBetweenSegments(wire.getFirstPoint(),
//                                                              wire.getSecondPoint(),
//                                                              other_wire.getFirstPoint(),
//                                                              other_wire.getSecondPoint());
//         if(distance < max_distance){
//             node->cell_iterator_->second.addAdjacentCell(&(other_node->cell_iterator_->second));
//             other_node->cell_iterator_->second.addAdjacentCell(&(node->cell_iterator_->second));
//         }
//         return;
//     } 
    
//     if (!other_node->index_() && !node->index_()){
//         searchAdjacentCells(node->left_,  other_node->left_, geo, max_distance, overlap_tolerance);
//         searchAdjacentCells(node->left_,  other_node->right_, geo, max_distance, overlap_tolerance);
//         searchAdjacentCells(node->right_, other_node->right_, geo, max_distance, overlap_tolerance);
//         searchAdjacentCells(node->right_, other_node->left_, geo, max_distance, overlap_tolerance);
//     } else if (!node->index_()) {
//         searchAdjacentCells(node->left_,  other_node, geo, max_distance, overlap_tolerance);
//         searchAdjacentCells(node->right_, other_node, geo, max_distance, overlap_tolerance);
//     } else if (!other_node->index_()) {
//         searchAdjacentCells(node, other_node->left_, geo, max_distance, overlap_tolerance);
//         searchAdjacentCells(node, other_node->right_, geo, max_distance, overlap_tolerance);
//     }
// }

}
