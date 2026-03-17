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
      root_->createTree(wires_.begin(), wires_.end());
      root_->searchAdjacentCells(root(), max_distance, overlap_tolerance);
    };


 /**
  * @brief Creates a BVH tree for a set of wires.
  *
  * @param node The root node of the BVH tree.
  * @param begin Iterator to the beginning of the range of wires.
  * @param end Iterator to the end of the range of wires.
  */
  void Node::createTree( typename wire_list::iterator begin,
                        typename wire_list::iterator end) {

    if(begin == end) {
      UFW_ERROR("Empty wire list. Geoinfo building failed.");
    }
    
    aabb_ = (*begin)->aabb;
    for (auto it = begin + 1; it != end; ++it) {
      aabb_.expand((*it)->aabb);
    }

    if (std::distance(begin, end) == 1) {
      wire_ = *begin;
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

    double deltaZ    = aabb_.max_.Z() - aabb_.min_.Z();
    int middle_point = std::distance(begin, end) / 2;
    const double w   = (*begin)->max_radius;
    if (w != deltaZ) {
      std::nth_element(begin, begin + middle_point, end, sorting_by_z_then_id);
    } else {
      std::nth_element(begin, begin + middle_point, end, sorting_by_id);
    }

    left_ = std::make_unique<Node>();
    left_->createTree(begin, begin + middle_point);

    right_ = std::make_unique<Node>();
    right_->createTree(begin + middle_point, end);

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
void Node::searchAdjacentCells(const Node* other, double max_distance, double overlap_tolerance) const {
    if (!other)
        return;

    if (!aabb_.isOverlapping(other->aabb_, overlap_tolerance))
        return;

    if (wire_ && other->wire_) {
        if (wire_ == other->wire_)
            return;
        if (wire_->daq_channel.channel < other->wire_->daq_channel.channel)
            return;

        double distance = wire_->closest_approach_segment_distance(other->wire_->head, other->wire_->tail);
        if (distance < max_distance) {
            wire_->adjacent_wires.emplace_back(other->wire_);
            other->wire_->adjacent_wires.emplace_back(wire_);
        }
        return;
    }

    if (!wire_ && !other->wire_) {
        if (left_)  left_->searchAdjacentCells(other->left(),  max_distance, overlap_tolerance);
        if (left_)  left_->searchAdjacentCells(other->right(), max_distance, overlap_tolerance);
        if (right_) right_->searchAdjacentCells(other->right(), max_distance, overlap_tolerance);
        if (right_) right_->searchAdjacentCells(other->left(),  max_distance, overlap_tolerance);
    } else if (!wire_) {
        if (left_)  left_->searchAdjacentCells(other, max_distance, overlap_tolerance);
        if (right_) right_->searchAdjacentCells(other, max_distance, overlap_tolerance);
    } else if (!other->wire_) {
        searchAdjacentCells(other->left(),  max_distance, overlap_tolerance);
        searchAdjacentCells(other->right(), max_distance, overlap_tolerance);
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
