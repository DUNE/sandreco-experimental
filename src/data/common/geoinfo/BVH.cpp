#include <tracker_info.hpp>
#include <map>
#include <memory>
#include <BVH.hpp>

namespace sand {

  /**
   * @brief Constructor for the BVH class.
   *
   * @param wires The vector of pointers to tracker wire objects.
   * @param max_distance The maximum distance between wires to be considered adjacent.
   * @param overlap_tolerance The tolerance for overlap between wires.
   */
  BVH::BVH(wire_list & wires, double max_distance, double overlap_tolerance) : wires_(wires){
      createTree(*root_ ,wires_.begin(), wires_.end());
      searchAdjacentCells(root_.get(), root_.get(), max_distance, overlap_tolerance);
    };


 /**
  * @brief Creates a BVH tree for a set of wires.
  *
  * @param node The root node of the BVH tree.
  * @param begin Iterator to the beginning of the range of wires.
  * @param end Iterator to the end of the range of wires.
  */
  void BVH::createTree( Node & node,
                        typename wire_list::iterator begin,
                        typename wire_list::iterator end) {

    if(begin == end) {
      UFW_ERROR("Empty wire list. Geoinfo building failed.");
    }
    
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

void BVH::printTreeInfo() {
      if (!root_) {
        UFW_ERROR("Tree is empty!");
        return;
      }

      size_t depth  = root_->getDepth();
      size_t leaves = root_->countLeaves();
      size_t total  = root_->countNodes();

      UFW_DEBUG("========== BVH TREE ANALYSIS ==========");
      UFW_DEBUG("Depth: {} levels", depth);
      UFW_DEBUG("Total nodes: {}",  total);
      UFW_DEBUG("Leaf nodes: {}", leaves);
      UFW_DEBUG("Internal nodes: {}", (total - leaves));
      UFW_DEBUG("Balance: {}", (root_->isBalanced() ? "Balanced" : "Unbalanced"));

      if(!root_->isBalanced()) UFW_ERROR("Tree is not balanced!");

      // Print depth distribution
      UFW_DEBUG("--- Depth Distribution ---");
      std::vector<size_t> depth_counts(depth + 1, 0);
      root_->countNodesAtDepth(0, depth_counts);

      for (size_t i = 0; i <= depth; ++i) {
        if (depth_counts[i] > 0) {
          UFW_DEBUG("Depth {} : {} nodes",i , depth_counts[i]);
        }
      }

      // Find deepest leaf
      size_t deepest_depth       = 0;
      const Node * deepest = root_->findDeepestLeaf(0, deepest_depth);
      if (deepest && deepest->wire_) {
        UFW_DEBUG("--- Deepest Leaf Node ---");
        UFW_DEBUG("Depth: {}", deepest_depth);
        uint8_t plane = static_cast<uint8_t>(deepest->wire_->daq_channel.channel >> 16);
        uint16_t tube = static_cast<uint16_t>(deepest->wire_->daq_channel.channel & 0xFFFF);
        UFW_DEBUG("Wire channel: ({},{},{},{})", int(deepest->wire_->daq_channel.subdetector), 
                  int(deepest->wire_->daq_channel.link), plane, tube);
      }

      UFW_DEBUG("=====================================");
    }

  size_t Node::countLeaves() const {
      if (!left_ && !right_)
          return 1;
      size_t count = 0;
      if (left_)  count += left_->countLeaves();
      if (right_) count += right_->countLeaves();
      return count;
  }

  size_t Node::getDepth() const {
      if (!left_ && !right_)
          return 1;
      return 1 + std::max(
          left_  ? left_->getDepth()  : 0,
          right_ ? right_->getDepth() : 0
      );
  }

    size_t Node::countNodes() const {
      return 1
          + (left_  ? left_->countNodes()  : 0)
          + (right_ ? right_->countNodes() : 0);
    }

    bool Node::isBalanced() const {
        size_t left_depth  = left_  ? left_->getDepth()  : 0;
        size_t right_depth = right_ ? right_->getDepth() : 0;

        int diff = std::abs(static_cast<int>(left_depth) - static_cast<int>(right_depth));

        return diff <= 1
            && (left_  ? left_->isBalanced()  : true)
            && (right_ ? right_->isBalanced() : true);
    }

  void Node::countNodesAtDepth(size_t current_depth, std::vector<size_t>& depth_counts) const {
      if (current_depth >= depth_counts.size())
          depth_counts.resize(current_depth + 1, 0);

      depth_counts[current_depth]++;

      if (left_)  left_->countNodesAtDepth(current_depth + 1, depth_counts);
      if (right_) right_->countNodesAtDepth(current_depth + 1, depth_counts);
  }

const Node* Node::findDeepestLeaf(size_t current_depth, size_t& max_depth) const {
    if (!left_ && !right_) {
        if (current_depth > max_depth) {
            max_depth = current_depth;
            return this;
        }
        return nullptr;
    }

    const Node* left_deepest  = left_  ? left_->findDeepestLeaf(current_depth + 1, max_depth)  : nullptr;
    const Node* right_deepest = right_ ? right_->findDeepestLeaf(current_depth + 1, max_depth) : nullptr;

    if (left_deepest) return left_deepest;
    return right_deepest;
}

} // namespace sand
