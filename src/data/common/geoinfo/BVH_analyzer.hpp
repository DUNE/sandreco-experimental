/**
 * @file BVH_analyzer.hpp
 * @author Federico Battisti
 * @brief Utility functions for BVH tree analysis.
 */
#ifndef BVH_ANALYZER_HPP
#define BVH_ANALYZER_HPP

#include "BVH.hpp"
#include <cmath>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <queue>
#include <vector>

namespace sand {

  // Forward declaration
  template <typename WireT>
  class BVH;

  /** 
   * @brief Class containing utility functions for BVH tree analysis.
   */
  template <typename WireT>
  class BVH_Analyzer {
   public:
    // Simple function to check tree depth
    static size_t getTreeDepth(const BVH<WireT>& bvh) {
      if (!bvh.root_)
        return 0;
      return getNodeDepth(bvh.root_);
    }

    /**
     * @brief Print information about the BVH tree.
     * 
     * @param bvh The BVH tree to analyze.
     */
    static void printTreeInfo(const BVH<WireT>& bvh) {
      if (!bvh.root_) {
        UFW_ERROR("Tree is empty!");
        return;
      }

      size_t depth  = getNodeDepth(bvh.root_);
      size_t leaves = countLeaves(bvh.root_);
      size_t total  = countNodes(bvh.root_);

      UFW_DEBUG("========== BVH TREE ANALYSIS ==========");
      UFW_DEBUG("Depth: {} levels", depth);
      UFW_DEBUG("Total nodes: {}",  total);
      UFW_DEBUG("Leaf nodes: {}", leaves);
      UFW_DEBUG("Internal nodes: {}", (total - leaves));
      UFW_DEBUG("Balance: {}", (isBalanced(bvh.root_) ? "Balanced" : "Unbalanced"));

      if(!isBalanced(bvh.root_)) UFW_ERROR("Tree is not balanced!");

      // Print depth distribution
      UFW_DEBUG("--- Depth Distribution ---");
      std::vector<size_t> depth_counts(depth + 1, 0);
      countNodesAtDepth(bvh.root_, 0, depth_counts);

      for (size_t i = 0; i <= depth; ++i) {
        if (depth_counts[i] > 0) {
          UFW_DEBUG("Depth {} : {} nodes",i , depth_counts[i]);
        }
      }

      // Find deepest leaf
      size_t deepest_depth       = 0;
      const Node<WireT>* deepest = findDeepestLeaf(bvh.root_, 0, deepest_depth);
      if (deepest && deepest->wire_) {
        UFW_DEBUG("--- Deepest Leaf Node ---");
        UFW_DEBUG("Depth: {}", deepest_depth);
        UFW_DEBUG("Wire channel: ({},{},{})", int(deepest->wire_->daq_channel.subdetector), 
                  int(deepest->wire_->daq_channel.link), int(deepest->wire_->daq_channel.channel));
      }

      UFW_DEBUG("=====================================");
    }

    /**
     * @brief Print information about the leaf nodes of the BVH tree.
     * 
     * @param bvh The BVH tree to analyze.
     */
    static void printLeafChannelInfo(const BVH<WireT>& bvh) {
      if (!bvh.root_) {
        UFW_ERROR("Tree is empty!");
        return;
      }

      UFW_DEBUG("========== LEAF NODE DAQ CHANNELS ==========");
      size_t leaf_count = 0;

      std::function<void(const std::unique_ptr<Node<WireT>>&)> traverse =
          [&](const std::unique_ptr<Node<WireT>>& node) {
            if (!node)
              return;

            // Check if this is a leaf node
            if (!node->left_ && !node->right_) {
              leaf_count++;
              uint8_t plane = static_cast<uint8_t>(node->daq_channel_.channel >> 16);
              uint16_t tube = static_cast<uint16_t>(node->daq_channel_.channel & 0xFFFF);
              UFW_DEBUG("Leaf {}: subdetector= {}, link= {}, plane= {}, tube= {}, z= {}, x= {}, y= {} ", 
                          leaf_count, int(node->daq_channel_.subdetector), int(node->daq_channel_.link), 
                          int(plane), int(tube), node->wire_->head.z(), node->wire_->head.x(), node->wire_->head.y());
              return;
            }

            // Traverse subtrees
            traverse(node->left_);
            traverse(node->right_);
          };

      traverse(bvh.root_);

      UFW_DEBUG("==========================================");
      UFW_DEBUG("Total leaf nodes: {}", leaf_count);
    }

   private:
    /**
     * @brief Get the depth of a node in the BVH tree.
     * 
     * @param node The node to get the depth of.
     */
    static size_t getNodeDepth(const std::unique_ptr<Node<WireT>>& node) {
      if (!node)
        return 0;
      if (!node->left_ && !node->right_)
        return 1;

      size_t left_depth = 0, right_depth = 0;
      if (node->left_)
        left_depth = getNodeDepth(node->left_);
      if (node->right_)
        right_depth = getNodeDepth(node->right_);

      return 1 + std::max(left_depth, right_depth);
    }

    /**
     * @brief Count the number of leaf nodes in the BVH tree.
     * 
     * @param node The node to start counting from.
     */
    static size_t countLeaves(const std::unique_ptr<Node<WireT>>& node) {
      if (!node)
        return 0;
      if (!node->left_ && !node->right_)
        return 1;
      return countLeaves(node->left_) + countLeaves(node->right_);
    }

    /** @brief Count the number of nodes in the BVH tree
     * 
     * @param node The node to start counting from
     */
    static size_t countNodes(const std::unique_ptr<Node<WireT>>& node) {
      if (!node)
        return 0;
      return 1 + countNodes(node->left_) + countNodes(node->right_);
    }

    /** @brief Check if the tree is balanced
     * 
     * @param node The node to start checking from
     */
    static bool isBalanced(const std::unique_ptr<Node<WireT>>& node) {
      if (!node)
        return true;

      size_t left_depth  = getNodeDepth(node->left_);
      size_t right_depth = getNodeDepth(node->right_);

      int diff = std::abs(static_cast<int>(left_depth) - static_cast<int>(right_depth));

      return diff <= 1 && isBalanced(node->left_) && isBalanced(node->right_);
    }

    /**
     * @brief Count the number of nodes at each depth in the BVH tree.
     * 
     * @param node The node to start counting from.
     * @param current_depth The current depth of the node.
     * @param depth_counts The vector to store the counts of nodes at each depth.
     */
    static void countNodesAtDepth(const std::unique_ptr<Node<WireT>>& node, size_t current_depth,
                                  std::vector<size_t>& depth_counts) {
      if (!node)
        return;

      if (current_depth >= depth_counts.size()) {
        depth_counts.resize(current_depth + 1, 0);
      }
      depth_counts[current_depth]++;

      countNodesAtDepth(node->left_, current_depth + 1, depth_counts);
      countNodesAtDepth(node->right_, current_depth + 1, depth_counts);
    }

    /**
     * @brief Find the deepest leaf node in the BVH tree.
     * 
     * @param node The node to start searching from.
     * @param current_depth The current depth of the node.
     * @param max_depth The maximum depth found so far.
     */
    static const Node<WireT>* findDeepestLeaf(const std::unique_ptr<Node<WireT>>& node, size_t current_depth,
                                              size_t& max_depth) {
      if (!node)
        return nullptr;

      if (!node->left_ && !node->right_) {
        // Leaf node
        if (current_depth > max_depth) {
          max_depth = current_depth;
          return node.get();
        }
        return nullptr;
      }

      const Node<WireT>* left_deepest  = findDeepestLeaf(node->left_, current_depth + 1, max_depth);
      const Node<WireT>* right_deepest = findDeepestLeaf(node->right_, current_depth + 1, max_depth);

      // Return the deeper one
      if (left_deepest && right_deepest) {
        // Both found, return one (doesn't matter which)
        return left_deepest;
      } else if (left_deepest) {
        return left_deepest;
      } else {
        return right_deepest;
      }
    }
  };

} // namespace sand

#endif // BVH_ANALYZER_HPP