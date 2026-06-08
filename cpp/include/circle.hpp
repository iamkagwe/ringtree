#pragma once

#include "types.hpp"
#include <algorithm>
#include <vector>

namespace ringtree {

/**
 * Circle: Interface for virtual node storage and lookup
 * Abstracts between array and tree implementations
 */
class Circle {
public:
    virtual ~Circle() = default;
    
    virtual bool insert(VNodeHash hash, const NodeID& node_id) = 0;
    virtual bool remove(VNodeHash hash) = 0;
    
    // Find closest vnode >= hash (or wrap to first)
    virtual bool find_closest(VNodeHash hash, VNodeHash& out_hash, NodeID& out_id) const = 0;
    
    // Find next closest vnode after hash
    virtual bool find_next_closest(VNodeHash hash, VNodeHash& out_hash, NodeID& out_id) const = 0;
    
    virtual size_t size() const = 0;
    virtual void sort() = 0;
};

/**
 * ArrayCircle: O(log n) lookup using binary search
 * Better cache locality than tree-based approach
 * Suitable for mostly static rings with infrequent changes
 */
class ArrayCircle : public Circle {
public:
    bool insert(VNodeHash hash, const NodeID& node_id) override;
    bool remove(VNodeHash hash) override;
    bool find_closest(VNodeHash hash, VNodeHash& out_hash, NodeID& out_id) const override;
    bool find_next_closest(VNodeHash hash, VNodeHash& out_hash, NodeID& out_id) const override;
    
    size_t size() const override { return vnodes_.size(); }
    void sort() override;
    
    // Direct access for iteration
    const std::vector<VirtualNodeEntry>& get_vnodes() const { return vnodes_; }
    
private:
    std::vector<VirtualNodeEntry> vnodes_;
};

/**
 * RBTreeCircle: Red-Black tree implementation
 * Maintains logarithmic operations with auto-balancing
 * Better for dynamic workloads with frequent insertion/deletion
 */
class RBTreeCircle : public Circle {
public:
    bool insert(VNodeHash hash, const NodeID& node_id) override;
    bool remove(VNodeHash hash) override;
    bool find_closest(VNodeHash hash, VNodeHash& out_hash, NodeID& out_id) const override;
    bool find_next_closest(VNodeHash hash, VNodeHash& out_hash, NodeID& out_id) const override;
    
    size_t size() const override;
    void sort() override {} // No-op for balanced tree
    
private:
    struct RBNode;
    using RBNodePtr = std::unique_ptr<RBNode>;
    
    struct RBNode {
        VNodeHash hash;
        NodeID node_id;
        RBNodePtr left;
        RBNodePtr right;
        bool red = true;
        
        RBNode(VNodeHash h, const NodeID& id) : hash(h), node_id(id) {}
    };
    
    RBNodePtr root_;
    size_t size_ = 0;
    
    // Helper functions
    static bool is_red(const RBNodePtr& node);
    static std::unique_ptr<RBNode> rotate_left(std::unique_ptr<RBNode>&& node);
    static std::unique_ptr<RBNode> rotate_right(std::unique_ptr<RBNode>&& node);
    
    std::unique_ptr<RBNode> insert_impl(std::unique_ptr<RBNode>&& node, 
                                         VNodeHash hash, const NodeID& node_id,
                                         bool& inserted);
                                         
    std::unique_ptr<RBNode> remove_impl(std::unique_ptr<RBNode>&& node, VNodeHash hash);
    
    bool find_closest_impl(const RBNodePtr& node, VNodeHash hash, 
                          VNodeHash& out_hash, NodeID& out_id) const;
};

} // namespace ringtree
