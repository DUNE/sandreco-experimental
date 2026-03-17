#pragma once
#include <tracker_info.hpp>
#include <map>
#include <memory>

namespace sand {

  using wire_list = geoinfo::tracker_info::wire_list;
  using wire = geoinfo::tracker_info::wire;


  /**
   * @brief Node structure for the BVH tree.
   *
   * This structure represents a node in the BVH tree and stores 
   * information about the wire object, left child, right child, 
   * and the AABB enveloping the tree starting from the node.
   *
   */
  struct Node {
      wire::AABB aabb_;
      const wire* wire_ = nullptr;

      const Node* left()  const { return left_.get(); }
      const Node* right() const { return right_.get(); }

      size_t getDepth() const;
      bool   isBalanced() const;
      
      size_t countNodes() const;
      size_t countLeaves() const; 

      void   countNodesAtDepth(size_t current_depth,
                              std::vector<size_t> & depth_counts) const;

      const Node * findDeepestLeaf(size_t current_depth,
                                  size_t & max_depth) const;
      

  private:
      std::unique_ptr<Node> left_;
      std::unique_ptr<Node> right_;

      friend class BVH;  // BVH::createTree can access left_ and right_ directly
  };

  /**
   * @class BVH
   * @brief Defines the BVH class for bounding volume hierarchy.
   *
   * This class provides methods for creating a bounding volume hierarchy (BVH) for a set of wires.
   *
   */
  class BVH {
    public:

      BVH(wire_list & wires, double max_distance, double overlap_tolerance);

      void printTreeInfo();

      const Node* root() const { return root_.get(); }

    private:
      void createTree(Node & node,
                      typename wire_list::iterator begin,
                      typename wire_list::iterator end);

      void searchAdjacentCells(const Node * node, const Node * other_node,
                              double max_distance, double overlap_tolerance);
      std::unique_ptr<Node> root_ = std::make_unique<Node>();
      wire_list & wires_;
  };

} // namespace sand
