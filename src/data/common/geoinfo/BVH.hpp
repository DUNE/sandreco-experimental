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
    const wire * wire_ = nullptr;
    std::unique_ptr<Node> left_;
    std::unique_ptr<Node> right_;
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

      // // Add friend declaration for BVH_Analyzer
      // template <typename T>
      // friend class BVH_Analyzer;

    private:
      void createTree(Node & node,
                      typename wire_list::iterator begin,
                      typename wire_list::iterator end);

      void searchAdjacentCells(const Node * node, const Node * other_node,
                              double max_distance, double overlap_tolerance);
      std::unique_ptr<Node> root_ = std::make_unique<Node>();
      wire_list & wires_;

    private:
      size_t getNodeDepth(const Node * node);
      bool isBalanced(const Node * node);
      size_t countNodes(const Node * node);
      size_t countLeaves(const Node * node);
      void countNodesAtDepth(const Node * node, size_t current_depth,
                               std::vector<size_t> & depth_counts);
      const Node * findDeepestLeaf(const Node * node, size_t current_depth,
                                   size_t & max_depth);
  };

} // namespace sand
