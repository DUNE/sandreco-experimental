// BVH_Analyzer.hpp
#ifndef BVH_ANALYZER_HPP
#define BVH_ANALYZER_HPP

#include <iostream>
#include <queue>
#include <memory>
#include <vector>
#include <limits>
#include <cmath>
#include <iomanip>
#include <functional>
#include "BVH.hpp"

namespace sand {

// Forward declaration
template <typename WireT>
class BVH;

template <typename WireT>
class BVH_Analyzer {
public:
    // Simple function to check tree depth
    static size_t getTreeDepth(const BVH<WireT>& bvh) {
        if (!bvh.root_) return 0;
        return getNodeDepth(bvh.root_);
    }
    
    // Print basic tree info
    static void printTreeInfo(const BVH<WireT>& bvh) {
        if (!bvh.root_) {
            std::cout << "Tree is empty!\n";
            return;
        }
        
        size_t depth = getNodeDepth(bvh.root_);
        size_t leaves = countLeaves(bvh.root_);
        size_t total = countNodes(bvh.root_);
        
        std::cout << "\n========== BVH TREE ANALYSIS ==========\n";
        std::cout << "Depth: " << depth << " levels\n";
        std::cout << "Total nodes: " << total << "\n";
        std::cout << "Leaf nodes: " << leaves << "\n";
        std::cout << "Internal nodes: " << (total - leaves) << "\n";
        std::cout << "Balance: " << (isBalanced(bvh.root_) ? "Balanced" : "Unbalanced") << "\n";
        
        // Print depth distribution
        std::cout << "\n--- Depth Distribution ---\n";
        std::vector<size_t> depth_counts(depth + 1, 0);
        countNodesAtDepth(bvh.root_, 0, depth_counts);
        
        for (size_t i = 0; i <= depth; ++i) {
            if (depth_counts[i] > 0) {
                std::cout << "Depth " << i << ": " << depth_counts[i] << " nodes\n";
            }
        }
        
        // Find deepest leaf
        size_t deepest_depth = 0;
        const Node<WireT>* deepest = findDeepestLeaf(bvh.root_, 0, deepest_depth);
        if (deepest && deepest->wire_) {
            std::cout << "\n--- Deepest Leaf Node ---\n";
            std::cout << "Depth: " << deepest_depth << "\n";
            std::cout << "Wire channel: (" << int(deepest->wire_->daq_channel.subdetector) << ", " << int(deepest->wire_->daq_channel.link) << ", " << int(deepest->wire_->daq_channel.channel) << ")\n";
        }
        
        std::cout << "=====================================\n\n";
    }
    
    // Detailed analysis
    static void analyzeTree(const BVH<WireT>& bvh) {
        if (!bvh.root_) {
            std::cout << "Tree is empty!\n";
            return;
        }
        
        struct Stats {
            size_t depth = 0;
            size_t nodes = 0;
            size_t leaves = 0;
            size_t max_leaf_depth = 0;
            size_t min_leaf_depth = std::numeric_limits<size_t>::max();
            size_t total_leaf_depth = 0;
        };
        
        std::function<Stats(const std::unique_ptr<Node<WireT>>&, size_t)> traverse = 
            [&](const std::unique_ptr<Node<WireT>>& node, size_t current_depth) -> Stats {
            if (!node) return {0, 0, 0, 0, std::numeric_limits<size_t>::max(), 0};
            
            if (!node->left_ && !node->right_) {
                // Leaf node
                return {
                    current_depth,  // depth
                    1,              // nodes
                    1,              // leaves
                    current_depth,  // max_leaf_depth
                    current_depth,  // min_leaf_depth
                    current_depth   // total_leaf_depth
                };
            }
            
            Stats left = traverse(node->left_, current_depth + 1);
            Stats right = traverse(node->right_, current_depth + 1);
            
            return {
                std::max({current_depth, left.depth, right.depth}),
                1 + left.nodes + right.nodes,
                left.leaves + right.leaves,
                std::max(left.max_leaf_depth, right.max_leaf_depth),
                std::min(left.min_leaf_depth, right.min_leaf_depth),
                left.total_leaf_depth + right.total_leaf_depth
            };
        };
        
        Stats stats = traverse(bvh.root_, 0);
        
        std::cout << "\n========== BVH DETAILED ANALYSIS ==========\n";
        std::cout << "Max depth: " << stats.depth << " levels\n";
        std::cout << "Total nodes: " << stats.nodes << "\n";
        std::cout << "Leaf nodes: " << stats.leaves << "\n";
        std::cout << "Internal nodes: " << (stats.nodes - stats.leaves) << "\n";
        std::cout << "Max leaf depth: " << stats.max_leaf_depth << "\n";
        std::cout << "Min leaf depth: " << stats.min_leaf_depth << "\n";
        
        if (stats.leaves > 0) {
            double avg_depth = static_cast<double>(stats.total_leaf_depth) / stats.leaves;
            std::cout << "Average leaf depth: " << std::fixed << std::setprecision(2) << avg_depth << "\n";
        }
        
        // Check balance
        std::function<bool(const std::unique_ptr<Node<WireT>>&, int&)> checkBalance = 
            [&](const std::unique_ptr<Node<WireT>>& node, int& height) -> bool {
            if (!node) {
                height = 0;
                return true;
            }
            
            int left_height = 0, right_height = 0;
            bool left_balanced = checkBalance(node->left_, left_height);
            bool right_balanced = checkBalance(node->right_, right_height);
            
            height = 1 + std::max(left_height, right_height);
            
            if (std::abs(left_height - right_height) > 1) return false;
            
            return left_balanced && right_balanced;
        };
        
        int height = 0;
        bool balanced = checkBalance(bvh.root_, height);
        std::cout << "Height-balanced: " << (balanced ? "Yes" : "No") << "\n";
        
        std::cout << "============================================\n\n";
    }
    
    // Print daq_channel info for all leaf nodes
    static void printLeafChannelInfo(const BVH<WireT>& bvh) {
        if (!bvh.root_) {
            std::cout << "Tree is empty!\n";
            return;
        }
        
        std::cout << "\n========== LEAF NODE DAQ CHANNELS ==========\n";
        size_t leaf_count = 0;
        
        std::function<void(const std::unique_ptr<Node<WireT>>&)> traverse = 
            [&](const std::unique_ptr<Node<WireT>>& node) {
            if (!node) return;
            
            // Check if this is a leaf node
            if (!node->left_ && !node->right_) {
                leaf_count++;
                uint8_t plane = static_cast<uint8_t>(node->daq_channel_.channel >> 16);
                uint16_t tube =static_cast<uint16_t>(node->daq_channel_.channel & 0xFFFF);
                std::cout << "Leaf " << leaf_count << ": ";
                std::cout << "subdetector=" << int(node->daq_channel_.subdetector) 
                         << ", link=" << int(node->daq_channel_.link);
                std::cout << " (plane=" << int(plane) << ", tube=" << int(tube) << ")";
                std::cout << " z = " << node->wire_->head.z() << " x = " << node->wire_->head.x() << " y = " << node->wire_->head.y() << "\n";
                
                std::cout << " (raw=0x" << std::hex << node->daq_channel_.raw << std::dec << ")\n";
                return;
            }
            
            // Traverse subtrees
            traverse(node->left_);
            traverse(node->right_);
        };
        
        traverse(bvh.root_);
        
        std::cout << "==========================================\n";
        std::cout << "Total leaf nodes: " << leaf_count << "\n\n";
    }
    
private:
    // Helper functions that need access to Node (friend class has access)
    static size_t getNodeDepth(const std::unique_ptr<Node<WireT>>& node) {
        if (!node) return 0;
        if (!node->left_ && !node->right_) return 1;
        
        size_t left_depth = 0, right_depth = 0;
        if (node->left_) left_depth = getNodeDepth(node->left_);
        if (node->right_) right_depth = getNodeDepth(node->right_);
        
        return 1 + std::max(left_depth, right_depth);
    }
    
    static size_t countLeaves(const std::unique_ptr<Node<WireT>>& node) {
        if (!node) return 0;
        if (!node->left_ && !node->right_) return 1;
        return countLeaves(node->left_) + countLeaves(node->right_);
    }
    
    static size_t countNodes(const std::unique_ptr<Node<WireT>>& node) {
        if (!node) return 0;
        return 1 + countNodes(node->left_) + countNodes(node->right_);
    }
    
    static bool isBalanced(const std::unique_ptr<Node<WireT>>& node) {
        if (!node) return true;
        
        size_t left_depth = getNodeDepth(node->left_);
        size_t right_depth = getNodeDepth(node->right_);
        
        int diff = std::abs(static_cast<int>(left_depth) - static_cast<int>(right_depth));
        
        return diff <= 1 && isBalanced(node->left_) && isBalanced(node->right_);
    }
    
    static void countNodesAtDepth(const std::unique_ptr<Node<WireT>>& node, 
                                 size_t current_depth, 
                                 std::vector<size_t>& depth_counts) {
        if (!node) return;
        
        if (current_depth >= depth_counts.size()) {
            depth_counts.resize(current_depth + 1, 0);
        }
        depth_counts[current_depth]++;
        
        countNodesAtDepth(node->left_, current_depth + 1, depth_counts);
        countNodesAtDepth(node->right_, current_depth + 1, depth_counts);
    }
    
    static const Node<WireT>* findDeepestLeaf(const std::unique_ptr<Node<WireT>>& node, 
                                             size_t current_depth, 
                                             size_t& max_depth) {
        if (!node) return nullptr;
        
        if (!node->left_ && !node->right_) {
            // Leaf node
            if (current_depth > max_depth) {
                max_depth = current_depth;
                return node.get();
            }
            return nullptr;
        }
        
        const Node<WireT>* left_deepest = findDeepestLeaf(node->left_, current_depth + 1, max_depth);
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