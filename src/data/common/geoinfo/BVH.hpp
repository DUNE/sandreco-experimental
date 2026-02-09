#pragma once
#include <memory>
#include <map>
#include "tracker_info.hpp"


namespace sand {
  template <typename WireT>
  struct AABB{
    AABB(){};
    AABB(const std::unique_ptr<WireT> & wire);
    void expand(const AABB& second_aabb);
    bool isOverlapping(const AABB& second_aabb, double epsilon = 0) const;
    pos_3d min_;
    pos_3d max_;
  };

  template <typename WireT>
  struct Node{
    WireT* wire_ = nullptr;
    // std::map<const WireT*, AABB>::iterator wire_iterator_;
    std::unique_ptr<Node<WireT>> left_;
    std::unique_ptr<Node<WireT>> right_;
    AABB<WireT> aabb_;
  };
  template <typename WireT>
  class BVH{
    public:
      BVH(){};

      BVH(std::vector<std::unique_ptr<WireT>>& wires,  
          double max_distance, 
          double overlap_tolerance);
      
      private:
      void createTree(std::unique_ptr<Node<WireT>>& node, 
                      typename std::vector<std::unique_ptr<WireT>>::iterator begin, 
                      typename std::vector<std::unique_ptr<WireT>>::iterator end);
      
      void fillCellAABBMap(const std::vector<std::unique_ptr<WireT>>& wires);
      void searchAdjacentCells(std::unique_ptr<Node<WireT>>& node, std::unique_ptr<Node<WireT>>& other_node, double max_distance, double overlap_tolerance);
      std::map<const WireT*, AABB<WireT>> cellAABBs_;
      std::unique_ptr<Node<WireT>> root_ = std::make_unique<Node<WireT>>();
  };

  // Template implementations
  template <typename WireT>
  BVH<WireT>::BVH(std::vector<std::unique_ptr<WireT>>& wires, 
          double max_distance, 
          double overlap_tolerance) {
          fillCellAABBMap(wires); 
          createTree(root_, wires.begin(), wires.end()); 
          searchAdjacentCells(root_, root_, max_distance, overlap_tolerance);
      };

  template <typename WireT>
  AABB<WireT>::AABB(const std::unique_ptr<WireT> & wire){
      min_ = pos_3d(1E9, 1E9, 1E9);
      max_ = pos_3d(-1E9, -1E9, -1E9);
      std::vector<pos_3d> vertices;
      auto p = wire->head;
      const auto & transform = wire->wire_plane_transform();
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

  template <typename WireT>
  void AABB<WireT>::expand(const AABB& second_aabb){
      min_.SetX(std::min(min_.X(), second_aabb.min_.X()));
      max_.SetX(std::max(max_.X(), second_aabb.max_.X()));
      min_.SetY(std::min(min_.Y(), second_aabb.min_.Y()));
      max_.SetY(std::max(max_.Y(), second_aabb.max_.Y()));
      min_.SetZ(std::min(min_.Z(), second_aabb.min_.Z()));
      max_.SetZ(std::max(max_.Z(), second_aabb.max_.Z()));
  }

  template <typename WireT>
  bool AABB<WireT>::isOverlapping(const AABB& second_aabb, double epsilon) const {
      if(max_.X() + epsilon >= second_aabb.min_.X() && min_.X() - epsilon <= second_aabb.max_.X() && 
          max_.Y() + epsilon >= second_aabb.min_.Y() && min_.Y() - epsilon <= second_aabb.max_.Y() && 
          max_.Z() + epsilon >= second_aabb.min_.Z() && min_.Z() - epsilon <= second_aabb.max_.Z()){
          return true;
      }
      return false;
  }

  template <typename WireT>   
  void BVH<WireT>::fillCellAABBMap(const std::vector<std::unique_ptr<WireT>>& wires){
      for(const auto &wire : wires){
          cellAABBs_[wire.get()] = AABB(wire);
      }
  }

  template <typename WireT>
  void BVH<WireT>::createTree(std::unique_ptr<Node<WireT>>& node, typename std::vector<std::unique_ptr<WireT>>::iterator begin, typename std::vector<std::unique_ptr<WireT>>::iterator end){
      node->aabb_ = cellAABBs_[begin->get()];  
      for (auto it = begin + 1; it != end; ++it) {
          node->aabb_.expand(cellAABBs_[it->get()]);  
      }

      node->wire_ = begin->get();
      
      if(std::distance(begin, end) == 1){
          node->wire_ = begin->get();
          // node->cell_iterator_ = (*begin);
          return;
      }
      
      // auto sorting_by_z = [](const typename std::vector<std::unique_ptr<WireT>>::iterator c1, const typename std::vector<std::unique_ptr<WireT>>::iterator c2){
      //     const auto& center_1 = (c1->get()->head + dir_3d(c1->get()->tail)) * 0.5;
      //     const auto& center_2 = (c2->get()->head + dir_3d(c2->get()->tail)) * 0.5;
          
      //     return center_1.Z() < center_2.Z();
      // };

    auto sorting_by_z = [](const std::unique_ptr<WireT>& a,
                   const std::unique_ptr<WireT>& b) {
        const auto& center_a = (a->head + dir_3d(a->tail)) * 0.5;
        const auto& center_b = (b->head + dir_3d(b->tail)) * 0.5;
        return center_a.Z() < center_b.Z();
    };
      
      // auto sorting_by_id = [geo](const sand_geometry::tracker::cell_map_iterator c1, const sand_geometry::tracker::cell_map_iterator c2){
      //     const auto& id1 = c1->second.getId();
      //     const auto& id2 = c2->second.getId();
          
      //     return id1 < id2;
      // };
      
      double deltaZ = node->aabb_.max_.Z() - node->aabb_.min_.Z();
      int middle_point= std::distance(begin, end) / 2;
      // // const double w = (*begin)->second.getSize().w;
      // // if (w != deltaZ) {
      //     // std::sort(begin, end, sorting_by_z);
      std::nth_element(begin, begin + middle_point, end, sorting_by_z);
      // // } else {
      // //     // std::sort(begin, end, sorting_by_id);
      // //     std::nth_element(begin, begin + middle_point, end, sorting_by_id);
      // // }
      node->left_ = std::make_unique<Node<WireT>>();
      createTree(node->left_, begin, begin + middle_point);

      node->right_ = std::make_unique<Node<WireT>>();
      createTree(node->right_, begin + middle_point, end);
   
      return;
  }

  template <typename WireT>
  void BVH<WireT>::searchAdjacentCells(std::unique_ptr<Node<WireT>>& node, std::unique_ptr<Node<WireT>>& other_node, double max_distance, double overlap_tolerance){
      if(!node || !other_node) return;
      
      if(!node->aabb_.isOverlapping(other_node->aabb_, overlap_tolerance)) return;

      if(other_node->wire_ && node->wire_) {
          if(other_node->wire_ == node->wire_) {
              return;
          }

          if (node->wire_ < other_node->wire_) {
              return;
          }
          const auto& wire = node->wire_;
          const auto& other_wire = other_node->wire_;
          
          double distance = wire->closest_approach_segment_distance(other_wire->head, other_wire->tail);
          if(distance < max_distance){
              node->wire_->adjecent_wires.emplace_back(other_node->wire_); 
              other_node->wire_->adjecent_wires.emplace_back(node->wire_);
          }
          return;
      } 
      
      if (!other_node->wire_ && !node->wire_){
          searchAdjacentCells(node->left_,  other_node->left_,  max_distance, overlap_tolerance);
          searchAdjacentCells(node->left_,  other_node->right_,  max_distance, overlap_tolerance);
          searchAdjacentCells(node->right_, other_node->right_,  max_distance, overlap_tolerance);
          searchAdjacentCells(node->right_, other_node->left_,  max_distance, overlap_tolerance);
      } else if (!node->wire_) {
          searchAdjacentCells(node->left_,  other_node,  max_distance, overlap_tolerance);
          searchAdjacentCells(node->right_, other_node,  max_distance, overlap_tolerance);
      } else if (!other_node->wire_) {
          searchAdjacentCells(node, other_node->left_,  max_distance, overlap_tolerance);
          searchAdjacentCells(node, other_node->right_,  max_distance, overlap_tolerance);
      }
  }



}
