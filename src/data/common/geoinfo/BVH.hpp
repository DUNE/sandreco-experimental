/**
 * @file BVH.hpp
 * @author Federico Battisti
 * @brief Contains the classes needed to implement the bounding volume hierarchy method.
 *
 */

#pragma once
#include "tracker_info.hpp"
#include <map>
#include <memory>

namespace sand {

  // Forward declaration
  template <typename WireT>
  class BVH_Analyzer;
  
  /**
   * @struct AABB
   * @brief Defines the AABB structure for axis-aligned bounding box.
   *
   * This structure represents an axis-aligned bounding box (AABB) and 
   * provides methods for expanding the AABB and checking for overlap.
   *
   * @template WireT The type of wire objects used in the BVH tree.
   */

  /**
   * @struct Node
   * @brief Defines the Node structure for the BVH tree.
   *
   * This structure represents a node in the BVH tree and stores information 
   * about the wire object, left child, right child, and the AABB.
   *
   * @template WireT The type of wire objects used in the BVH tree.
   */
  template <typename WireT>
  struct AABB {
    AABB() {};
    AABB(const std::unique_ptr<WireT>& wire);
    void expand(const AABB& second_aabb);
    bool isOverlapping(const AABB& second_aabb, double epsilon = 0) const;
    pos_3d min_;
    pos_3d max_;
  };

  /**
   * @brief Node structure for the BVH tree.
   *
   * This structure represents a node in the BVH tree and stores 
   * information about the wire object, left child, right child, 
   * and the AABB.
   *
   * @template WireT The type of wire objects used in the BVH tree.
   */
  template <typename WireT>
  struct Node {
    WireT* wire_ = nullptr;
    std::unique_ptr<Node<WireT>> left_;
    std::unique_ptr<Node<WireT>> right_;
    AABB<WireT> aabb_;
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
  template <typename WireT>
  class BVH {
    public:
      BVH() {};

      BVH(std::vector<std::unique_ptr<WireT>>& wires, double max_distance, double overlap_tolerance);

      const std::unique_ptr<Node<WireT>>& getRoot() const { return root_; }

      // Add friend declaration for BVH_Analyzer
      template <typename T>
      friend class BVH_Analyzer;

    private:
      void createTree(std::unique_ptr<Node<WireT>>& node, typename std::vector<std::unique_ptr<WireT>>::iterator begin,
                      typename std::vector<std::unique_ptr<WireT>>::iterator end);

      void fillCellAABBMap(const std::vector<std::unique_ptr<WireT>>& wires);
      void searchAdjacentCells(std::unique_ptr<Node<WireT>>& node, std::unique_ptr<Node<WireT>>& other_node,
                              double max_distance, double overlap_tolerance);
      std::map<const WireT*, AABB<WireT>> cellAABBs_;
      std::unique_ptr<Node<WireT>> root_ = std::make_unique<Node<WireT>>();
  };

  // Template implementations

  /**
   * @brief Constructor for the BVH class.
   *
   * @param wires The vector of unique pointers to WireT objects.
   * @param max_distance The maximum distance between wires to be considered adjacent.
   * @param overlap_tolerance The tolerance for overlap between wires.
   */
  template <typename WireT>
  BVH<WireT>::BVH(std::vector<std::unique_ptr<WireT>>& wires, double max_distance, double overlap_tolerance) {
    fillCellAABBMap(wires);
    createTree(root_, wires.begin(), wires.end());
    searchAdjacentCells(root_, root_, max_distance, overlap_tolerance);
  };

  /**
   * @brief Constructor for the AABB class.
   *
   * @param wire The unique pointer to the WireT object.
   */
  template <typename WireT>
  AABB<WireT>::AABB(const std::unique_ptr<WireT>& wire) {
    min_ = pos_3d(1E9, 1E9, 1E9);
    max_ = pos_3d(-1E9, -1E9, -1E9);
    std::vector<pos_3d> vertices;
    auto p                = wire->head;
    const auto& transform = wire->wire_plane_transform();
    auto p_rotated        = transform.Inverse() * p;

    // auto h_2 = wire->length() /2.;
    auto w_2 = wire->max_radius / 2.;

    pos_3d v1 = p_rotated + dir_3d(0, w_2, 0);
    pos_3d v2 = p_rotated + dir_3d(0, -w_2, 0);

    auto p1 = transform * v1;
    auto p2 = transform * v2;

    vertices.push_back(pos_3d(p1.X(), p1.Y(), p.Z() + w_2));
    vertices.push_back(pos_3d(p2.X(), p2.Y(), p.Z() + w_2));

    p         = wire->tail;
    p_rotated = transform.Inverse() * p;
    pos_3d v3 = p_rotated + dir_3d(0, w_2, 0);
    pos_3d v4 = p_rotated + dir_3d(0, -w_2, 0);

    auto p3 = transform * v3;
    auto p4 = transform * v4;

    vertices.push_back(pos_3d(p3.X(), p3.Y(), p.Z() - w_2));
    vertices.push_back(pos_3d(p4.X(), p4.Y(), p.Z() - w_2));

    for (const auto& v : vertices) {
      min_.SetX(std::min(min_.X(), v.X()));
      max_.SetX(std::max(max_.X(), v.X()));
      min_.SetY(std::min(min_.Y(), v.Y()));
      max_.SetY(std::max(max_.Y(), v.Y()));
      min_.SetZ(std::min(min_.Z(), v.Z()));
      max_.SetZ(std::max(max_.Z(), v.Z()));
      // UFW_DEBUG("Vertex: ({}, {}, {})", v.X(), v.Y(), v.Z());
    }
  }

  template <typename WireT>
  void AABB<WireT>::expand(const AABB& second_aabb) {
    min_.SetX(std::min(min_.X(), second_aabb.min_.X()));
    max_.SetX(std::max(max_.X(), second_aabb.max_.X()));
    min_.SetY(std::min(min_.Y(), second_aabb.min_.Y()));
    max_.SetY(std::max(max_.Y(), second_aabb.max_.Y()));
    min_.SetZ(std::min(min_.Z(), second_aabb.min_.Z()));
    max_.SetZ(std::max(max_.Z(), second_aabb.max_.Z()));
  }

 /**
  * @brief Checks if two AABBs are overlapping.
  *
  * @param second_aabb The second AABB to check for overlap.
  * @param epsilon The epsilon value for the overlap check.
  * @return True if the AABBs are overlapping, false otherwise.
  */
  template <typename WireT>
  bool AABB<WireT>::isOverlapping(const AABB& second_aabb, double epsilon) const {
    if (max_.X() + epsilon >= second_aabb.min_.X() && min_.X() - epsilon <= second_aabb.max_.X()
        && max_.Y() + epsilon >= second_aabb.min_.Y() && min_.Y() - epsilon <= second_aabb.max_.Y()
        && max_.Z() + epsilon >= second_aabb.min_.Z() && min_.Z() - epsilon <= second_aabb.max_.Z()) {
      return true;
    }
    return false;
  }

 /**
  * @brief Fills the cellAABBs_ map with AABBs for each WireT object.
  *
  * @param wires The vector of unique pointers to WireT objects.
  */
  template <typename WireT>
  void BVH<WireT>::fillCellAABBMap(const std::vector<std::unique_ptr<WireT>>& wires) {
    for (const auto& wire : wires) {
      // UFW_DEBUG("Set up AABB for Wire: head ({}, {}, {}), tail ({}, {}, {})", wire->head.X(), wire->head.Y(),
      // wire->head.Z(), wire->tail.X(), wire->tail.Y(), wire->tail.Z());
      cellAABBs_[wire.get()] = AABB<WireT>(wire);
    }
  }

 /**
  * @brief Creates a bounding volume hierarchy (BVH) for a set of wires.
  *
  * @param node The root node of the BVH tree.
  * @param begin Iterator to the beginning of the range of wires.
  * @param end Iterator to the end of the range of wires.
  */
  template <typename WireT>
  void BVH<WireT>::createTree(std::unique_ptr<Node<WireT>>& node,
                              typename std::vector<std::unique_ptr<WireT>>::iterator begin,
                              typename std::vector<std::unique_ptr<WireT>>::iterator end) {
    node->aabb_ = cellAABBs_[begin->get()];
    for (auto it = begin + 1; it != end; ++it) {
      node->aabb_.expand(cellAABBs_[it->get()]);
    }

    if (std::distance(begin, end) == 1) {
      node->wire_ = begin->get();
      return;
    }

    auto sorting_by_z_then_id = [](const std::unique_ptr<WireT>& a, const std::unique_ptr<WireT>& b) {
      const double za = a->head.Z();
      const double zb = b->head.Z();

      if (za < zb)
        return true;
      if (za > zb)
        return false;

      return a->daq_channel.channel < b->daq_channel.channel;
    };

    auto sorting_by_id = [](const std::unique_ptr<WireT>& a, const std::unique_ptr<WireT>& b) {
      const auto& id1 = a->daq_channel.channel;
      const auto& id2 = b->daq_channel.channel;

      return id1 < id2;
    };

    double deltaZ    = node->aabb_.max_.Z() - node->aabb_.min_.Z();
    int middle_point = std::distance(begin, end) / 2;
    const double w   = begin->get()->max_radius;
    if (w != deltaZ) {
      std::nth_element(begin, begin + middle_point, end, sorting_by_z_then_id);
    } else {
      std::nth_element(begin, begin + middle_point, end, sorting_by_id);
    }

    node->left_ = std::make_unique<Node<WireT>>();
    createTree(node->left_, begin, begin + middle_point);

    node->right_ = std::make_unique<Node<WireT>>();
    createTree(node->right_, begin + middle_point, end);

    return;
  }

  /**
   * @brief Searches recursively for adjacent cells in the BVH tree.
   *
   * @param node The current node in the BVH tree.
   * @param other_node The other node in the BVH tree to check for adjacency.
   * @param max_distance The maximum distance for adjacency.
   * @param overlap_tolerance The overlap tolerance for adjacency.
   */
  template <typename WireT>
  void BVH<WireT>::searchAdjacentCells(std::unique_ptr<Node<WireT>>& node, std::unique_ptr<Node<WireT>>& other_node,
                                      double max_distance, double overlap_tolerance) {
    if (!node || !other_node)
      return;

    if (!node->aabb_.isOverlapping(other_node->aabb_, overlap_tolerance))
      return;

    if (other_node->wire_ && node->wire_) {
      if (other_node->wire_ == node->wire_) {
        return;
      }
      if (node->wire_->daq_channel.channel < other_node->wire_->daq_channel.channel) {
        return;
      }
      const auto& wire       = node->wire_;
      const auto& other_wire = other_node->wire_;

      double distance = wire->closest_approach_segment_distance(other_wire->head, other_wire->tail);

      if (distance < max_distance) {
        node->wire_->adjecent_wires.emplace_back(other_node->wire_);
        other_node->wire_->adjecent_wires.emplace_back(node->wire_);
      }
      return;
    }

    if (!other_node->wire_ && !node->wire_) {
      searchAdjacentCells(node->left_, other_node->left_, max_distance, overlap_tolerance);
      searchAdjacentCells(node->left_, other_node->right_, max_distance, overlap_tolerance);
      searchAdjacentCells(node->right_, other_node->right_, max_distance, overlap_tolerance);
      searchAdjacentCells(node->right_, other_node->left_, max_distance, overlap_tolerance);
    } else if (!node->wire_) {
      searchAdjacentCells(node->left_, other_node, max_distance, overlap_tolerance);
      searchAdjacentCells(node->right_, other_node, max_distance, overlap_tolerance);
    } else if (!other_node->wire_) {
      searchAdjacentCells(node, other_node->left_, max_distance, overlap_tolerance);
      searchAdjacentCells(node, other_node->right_, max_distance, overlap_tolerance);
    }
  }

} // namespace sand
