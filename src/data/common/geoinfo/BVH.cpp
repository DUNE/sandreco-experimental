/**
 * @file BVH.hpp
 * @author Federico Battisti
 * @brief Contains the classes needed to implement the bounding volume hierarchy method.
 *
 */

#include <tracker_info.hpp>
#include <map>
#include <memory>
#include <BVH.hpp>

namespace sand {


  // Template implementations

  /**
   * @brief Constructor for the BVH class.
   *
   * @param wires The vector of unique pointers to WireT objects.
   * @param max_distance The maximum distance between wires to be considered adjacent.
   * @param overlap_tolerance The tolerance for overlap between wires.
   */
  BVH::BVH(wire_list & wires, double max_distance, double overlap_tolerance) : wires_(wires){
      createTree(*root_ ,wires_.begin(), wires_.end());
      searchAdjacentCells(root_.get(), root_.get(), max_distance, overlap_tolerance);
    };


 /**
  * @brief Creates a bounding volume hierarchy (BVH) for a set of wires.
  *
  * @param node The root node of the BVH tree.
  * @param begin Iterator to the beginning of the range of wires.
  * @param end Iterator to the end of the range of wires.
  */
  void BVH::createTree( Node & node,
                        typename wire_list::iterator begin,
                        typename wire_list::iterator end) {
    node.aabb_ = (*begin)->aabb;
    for (auto it = begin + 1; it != end; ++it) {
      node.aabb_.expand((*it)->aabb);
    }

    if (std::distance(begin, end) == 1) {
      node.wire_ = *begin;
      return;
    }

    auto sorting_by_z_then_id = [](const wire* a, const wire* b) {
        const double za = a->head.Z();
        const double zb = b->head.Z();

        if (za < zb) return true;
        if (za > zb) return false;

        return a->daq_channel.channel < b->daq_channel.channel;
    };

    auto sorting_by_id = [](const wire* a, const wire* b) {
        return a->daq_channel.channel < b->daq_channel.channel;
    };

    double deltaZ    = node.aabb_.max_.Z() - node.aabb_.min_.Z();
    int middle_point = std::distance(begin, end) / 2;
    const double w   = (*begin)->max_radius;
    if (w != deltaZ) {
      std::nth_element(begin, begin + middle_point, end, sorting_by_z_then_id);
    } else {
      std::nth_element(begin, begin + middle_point, end, sorting_by_id);
    }

    node.left_ = std::make_unique<Node>();
    createTree(*node.left_, begin, begin + middle_point);

    node.right_ = std::make_unique<Node>();
    createTree(*node.right_, begin + middle_point, end);

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
  void BVH::searchAdjacentCells(const Node * node, const Node * other_node,
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
        node->wire_->adjacent_wires.emplace_back(other_node->wire_);
        other_node->wire_->adjacent_wires.emplace_back(node->wire_);
      }
      return;
    }

    if (!other_node->wire_ && !node->wire_) {
      searchAdjacentCells(node->left_.get(), other_node->left_.get(), max_distance, overlap_tolerance);
      searchAdjacentCells(node->left_.get(), other_node->right_.get(), max_distance, overlap_tolerance);
      searchAdjacentCells(node->right_.get(), other_node->right_.get(), max_distance, overlap_tolerance);
      searchAdjacentCells(node->right_.get(), other_node->left_.get(), max_distance, overlap_tolerance);
    } else if (!node->wire_) {
      searchAdjacentCells(node->left_.get(), other_node, max_distance, overlap_tolerance);
      searchAdjacentCells(node->right_.get(), other_node, max_distance, overlap_tolerance);
    } else if (!other_node->wire_) {
      searchAdjacentCells(node, other_node->left_.get(), max_distance, overlap_tolerance);
      searchAdjacentCells(node, other_node->right_.get(), max_distance, overlap_tolerance);
    }
  }

} // namespace sand
