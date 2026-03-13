/**
 * @file BVH.hpp
 * @author Federico Battisti
 * @brief Contains the classes needed to implement the bounding volume hierarchy method.
 *
 */

#pragma once
#include <tracker_info.hpp>
#include <map>
#include <memory>

namespace sand {

  using wire_list = geoinfo::tracker_info::wire_list;
  using wire = geoinfo::tracker_info::wire;

  // Forward declaration
  // template <typename WireT>
  // class BVH_Analyzer;


  /**
   * @brief Node structure for the BVH tree.
   *
   * This structure represents a node in the BVH tree and stores 
   * information about the wire object, left child, right child, 
   * and the AABB.
   *
   * @template WireT The type of wire objects used in the BVH tree.
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
   * It uses the Node and AABB structures to construct the BVH tree.
   *
   * @template WireT The type of wire objects used in the BVH tree.
   */
  class BVH {
    public:

      BVH(wire_list & wires, double max_distance, double overlap_tolerance);

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
  };

} // namespace sand
