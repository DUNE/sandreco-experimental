#pragma once
#include <TVector3.h>
#include <memory>
#include <map>
#include "tracker_info.hpp"


namespace sand {
  struct AABB{
    AABB(){};
    AABB(const std::unique_ptr<geoinfo::tracker_info::wire> & wire, geoinfo::tracker_info * geo);
    void expand(const AABB& second_aabb);
    bool isOverlapping(const AABB& second_aabb, double epsilon = 0) const;
    pos_3d min_;
    pos_3d max_;
  };

  struct Node{
    const geoinfo::tracker_info::wire* wire_ = nullptr;
    std::map<const geoinfo::tracker_info::wire*, AABB>::iterator cell_iterator_;
    std::unique_ptr<Node> left_;
    std::unique_ptr<Node> right_;
    AABB aabb_;
  };

  class BVH{
    public:
      BVH(){};
      BVH(std::vector<std::unique_ptr<geoinfo::tracker_info::wire>>& wires, 
          geoinfo::tracker_info * geo, 
          double max_distance, 
          double overlap_tolerance) {
        fillCellAABBMap(wires, geo); 
        //createTree(root_, wires.begin(), wires.end(), geo); 
        // searchAdjacentCells(root_, root_, geo, max_distance, overlap_tolerance);
      };
      
      private:
      void createTree(std::unique_ptr<Node>& node,std::vector<std::unique_ptr<geoinfo::tracker_info::wire>>::iterator begin, std::vector<std::unique_ptr<geoinfo::tracker_info::wire>>::iterator end, geoinfo::tracker_info * geo);
      void fillCellAABBMap(const std::vector<std::unique_ptr<geoinfo::tracker_info::wire>>& wires, geoinfo::tracker_info * geo);
      // void searchAdjacentCells(std::unique_ptr<Node>& node, std::unique_ptr<Node>& other_node, geoinfo::tracker_info * geo, double max_distance, double overlap_tolerance);
      std::map<const geoinfo::tracker_info::wire*, AABB> cellAABBs_;
      std::unique_ptr<Node> root_ = std::make_unique<Node>();

      geoinfo::tracker_info* geo_ ; 
  };

}