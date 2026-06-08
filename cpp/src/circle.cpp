#include "circle.hpp"
#include <algorithm>
#include <cassert>
#include <functional>

namespace ringtree {

// ============================================================================
// ArrayCircle Implementation
// ============================================================================

bool ArrayCircle::insert(VNodeHash hash, const NodeID& node_id) {
    // Check for duplicates
    for (const auto& entry : vnodes_) {
        if (entry.hash == hash) {
            return false;
        }
    }
    
    vnodes_.emplace_back(VirtualNodeEntry{hash, node_id});
    return true;
}

bool ArrayCircle::remove(VNodeHash hash) {
    auto it = std::find_if(vnodes_.begin(), vnodes_.end(),
                           [hash](const VirtualNodeEntry& e) { return e.hash == hash; });
    
    if (it != vnodes_.end()) {
        vnodes_.erase(it);
        return true;
    }
    return false;
}

bool ArrayCircle::find_closest(VNodeHash hash, VNodeHash& out_hash, NodeID& out_id) const {
    if (vnodes_.empty()) {
        return false;
    }
    
    // Binary search for first vnode >= hash
    auto it = std::lower_bound(vnodes_.begin(), vnodes_.end(),
                               VirtualNodeEntry{hash, ""});
    
    if (it != vnodes_.end()) {
        out_hash = it->hash;
        out_id = it->node_id;
        return true;
    }
    
    // Wrap around to first vnode
    out_hash = vnodes_[0].hash;
    out_id = vnodes_[0].node_id;
    return true;
}

bool ArrayCircle::find_next_closest(VNodeHash hash, VNodeHash& out_hash, NodeID& out_id) const {
    if (vnodes_.empty()) {
        return false;
    }
    
    // Binary search for first vnode > hash
    auto it = std::upper_bound(vnodes_.begin(), vnodes_.end(),
                               VirtualNodeEntry{hash, ""});
    
    if (it != vnodes_.end()) {
        out_hash = it->hash;
        out_id = it->node_id;
        return true;
    }
    
    // Wrap around to first vnode
    out_hash = vnodes_[0].hash;
    out_id = vnodes_[0].node_id;
    return true;
}

void ArrayCircle::sort() {
    std::sort(vnodes_.begin(), vnodes_.end());
}

// ============================================================================
// RBTreeCircle Implementation
// ============================================================================

bool RBTreeCircle::is_red(const RBNodePtr& node) {
    return node && node->red;
}

std::unique_ptr<RBTreeCircle::RBNode> RBTreeCircle::rotate_left(std::unique_ptr<RBNode>&& node) {
    auto right = std::move(node->right);
    node->right = std::move(right->left);
    right->left = std::move(node);
    return right;
}

std::unique_ptr<RBTreeCircle::RBNode> RBTreeCircle::rotate_right(std::unique_ptr<RBNode>&& node) {
    auto left = std::move(node->left);
    node->left = std::move(left->right);
    left->right = std::move(node);
    return left;
}

bool RBTreeCircle::insert(VNodeHash hash, const NodeID& node_id) {
    bool inserted = false;
    root_ = insert_impl(std::move(root_), hash, node_id, inserted);
    
    if (inserted) {
        ++size_;
        root_->red = false; // Root is always black
    }
    
    return inserted;
}

std::unique_ptr<RBTreeCircle::RBNode> RBTreeCircle::insert_impl(
    std::unique_ptr<RBNode>&& node, VNodeHash hash, const NodeID& node_id, bool& inserted) {
    
    if (!node) {
        node = std::make_unique<RBNode>(hash, node_id);
        inserted = true;
        return node;
    }
    
    if (hash == node->hash) {
        inserted = false;
        return node;
    }
    
    if (hash < node->hash) {
        node->left = insert_impl(std::move(node->left), hash, node_id, inserted);
    } else {
        node->right = insert_impl(std::move(node->right), hash, node_id, inserted);
    }
    
    if (!inserted) return node;
    
    // RB rebalancing
    if (is_red(node->right) && !is_red(node->left)) {
        node = rotate_left(std::move(node));
    }
    
    if (is_red(node->left) && is_red(node->left->left)) {
        node = rotate_right(std::move(node));
    }
    
    if (is_red(node->left) && is_red(node->right)) {
        node->red = true;
        node->left->red = false;
        node->right->red = false;
    }
    
    return node;
}

bool RBTreeCircle::remove(VNodeHash hash) {
    size_t old_size = size_;
    root_ = remove_impl(std::move(root_), hash);
    return size_ < old_size;
}

std::unique_ptr<RBTreeCircle::RBNode> RBTreeCircle::remove_impl(
    std::unique_ptr<RBNode>&& node, VNodeHash hash) {
    
    if (!node) return nullptr;
    
    if (hash < node->hash) {
        node->left = remove_impl(std::move(node->left), hash);
    } else if (hash > node->hash) {
        node->right = remove_impl(std::move(node->right), hash);
    } else {
        --size_;
        
        // Node found
        if (!node->left) {
            return std::move(node->right);
        } else if (!node->right) {
            return std::move(node->left);
        } else {
            // Two children: find successor (max node in left subtree)
            auto temp = std::move(node->left);
            auto* successor = temp.get();
            
            // Find the rightmost node in the left subtree
            while (successor->right) {
                successor = successor->right.get();
            }
            
            node->hash = successor->hash;
            node->node_id = successor->node_id;
            node->left = remove_impl(std::move(node->left), successor->hash);
        }
    }
    
    return node;
}

bool RBTreeCircle::find_closest(VNodeHash hash, VNodeHash& out_hash, NodeID& out_id) const {
    if (!root_) return false;
    
    return find_closest_impl(root_, hash, out_hash, out_id);
}

bool RBTreeCircle::find_closest_impl(const RBNodePtr& node, VNodeHash hash,
                                      VNodeHash& out_hash, NodeID& out_id) const {
    if (!node) return false;
    
    if (hash <= node->hash) {
        out_hash = node->hash;
        out_id = node->node_id;
        return find_closest_impl(node->left, hash, out_hash, out_id) || true;
    } else {
        return find_closest_impl(node->right, hash, out_hash, out_id);
    }
}

bool RBTreeCircle::find_next_closest(VNodeHash hash, VNodeHash& out_hash, NodeID& out_id) const {
    if (!root_) return false;
    
    // Find smallest key > hash
    VNodeHash result_hash = 0;
    NodeID result_id;
    bool found = false;
    
    std::function<void(const RBNodePtr&)> traverse = [&](const RBNodePtr& node) {
        if (!node) return;
        
        if (node->hash > hash) {
            if (!found || node->hash < result_hash) {
                result_hash = node->hash;
                result_id = node->node_id;
                found = true;
            }
            traverse(node->left);
        } else {
            traverse(node->right);
        }
    };
    
    traverse(root_);
    
    if (found) {
        out_hash = result_hash;
        out_id = result_id;
        return true;
    }
    
    // Wrap around: find minimum
    if (root_) {
        auto temp = root_.get();
        while (temp->left) temp = temp->left.get();
        out_hash = temp->hash;
        out_id = temp->node_id;
        return true;
    }
    
    return false;
}

size_t RBTreeCircle::size() const {
    return size_;
}

} // namespace ringtree
