#pragma once

#include "circle.hpp"
#include "types.hpp"
#include "arena_allocator.hpp"
#include <atomic>
#include <functional>
#include <memory>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace ringtree {

// Forward declaration
class RingTreeEval;

/**
 * RingTree: High-performance hierarchical consistent hashing
 * 
 * CRITICAL DESIGN PRINCIPLE: Per-Ring Arenas for Hierarchical Locality
 * 
 * Each Ring (main ring and all subrings) allocates vnodes from its OWN ArenaAllocator.
 * This design maintains the core principle: changes to a subring affect ONLY that subring,
 * enabling independent parallelization of sibling subrings.
 * 
 * Key benefits:
 * - Subring A rebalancing doesn't fragment Subring B's arena
 * - Subrings can rebalance in parallel without contention
 * - When a subring is discarded, its entire arena is freed at once
 * - Locality of reference: vnodes stay cohesive per ring level
 * 
 * Core performance optimizations:
 * 
 * ARENA ALLOCATION (per-ring)
 *    - Each ring allocates vnodes from its own contiguous arena
 *    - Eliminates cross-ring fragmentation
 *    - Improves CPU prefetching within same ring
 * 
 * CACHE-LINE ALIGNMENT
 *    - Node and SubRing structs use alignas(64)
 *    - Eliminates false sharing between threads
 *    - Each thread access independent 64-byte lines
 * 
 * LOCK-FREE TRAVERSALS
 *    - Use std::atomic for load, key counts
 *    - Compare-And-Swap (CAS) patterns for updates
 *    - Readers don't block writers during lookups
 * 
 * CONTIGUOUS VNODES
 *    - ArrayCircle stores all vnodes in flat array per ring
 *    - Binary search for O(log n) cache-efficient lookup
 *    - No pointer chasing across heap
 */
class RingTree {
public:
    struct Config {
        Config() noexcept = default;
        
        size_t max_count = 100;              // Max members per ring
        bool use_array_circle = true;        // true=array, false=RB-tree
        int num_replicas = 1;                // Virtual nodes per physical node
        int branch_factor = 1;               // Multiplier for hierarchy levels
        size_t arena_capacity = 64 * 1024 * 1024; // 64 MB per-ring arena
    };
    
    RingTree();
    explicit RingTree(const Config& config);
    ~RingTree() = default;
    
    RingTree(const RingTree&) = delete;
    RingTree& operator=(const RingTree&) = delete;
    
    // Node operations (public API, match Go's InsertNode/RemoveNode)
    bool insert_node(std::shared_ptr<Node> node);  // Inserts into main_ring_
    bool remove_node(const NodeID& node_id);
    
    // Find operations
    // Returns whether a node was found for the key
    bool find_node(const std::string& key, 
                   Node*& out_node, Ring*& out_ring, VNodeHash& out_vnode_hash) const;
    
    // Key management
    bool insert_key(const std::string& key);
    bool remove_key(const std::string& key);
    bool lookup(const std::string& key, NodeID& out_owner) const;
    
    // Ring queries 
    std::vector<std::string> members() const;
    size_t size() const;
    bool is_empty() const;
    
    // Traversal
    void traversal(std::function<void(Node*)> operation) const;
    
    // Lookup operations (low-level, for performance)
    bool find_closest(const std::string& key, NodeID& out_node_id) const;
    bool find_next_closest(const std::string& key, NodeID& out_node_id) const;
    
    // Hierarchy management
    std::shared_ptr<Ring> create_subring(int level);
    
private:
    Config config_;
    
    // Main ring (level 0) - owns its own arena for all its vnodes
    Ring main_ring_;
    
    // Global counters (cache-line aligned for high-frequency updates)
    alignas(64) std::atomic<size_t> num_remaps_{0};
    alignas(64) std::atomic<size_t> num_nodes_{0};
    alignas(64) std::atomic<int64_t> num_keys_{0};
    
    // Friend class for statistics and evaluation
    friend class RingTreeEval;
    
    // Internal helpers - node and ring management
    void initialize_main_ring();
    bool insert_node_to_ring(Ring* ring, std::shared_ptr<Node> node);  // Inserts into specified ring
    
    // Recursive hierarchy helpers (internal implementation)
    bool find_node_recursive(const Ring* ring, const std::string& key,
                             Node*& out_node, Ring*& out_ring, VNodeHash& out_vnode_hash) const;
    bool insert_key_recursive(Ring* ring, const std::string& key, int level);
    
    // Node splitting and ring collapse (internal implementation)
    // Now returns shared_ptr<Ring> to manage lifecycle properly
    std::shared_ptr<Ring> split_node(Ring* parent_ring, Node* node);
    bool collapse_ring(std::shared_ptr<Ring> subring);
    bool should_collapse(Ring* ring) const;
    
    // Key redistribution helpers
    bool should_move_key(KeyHash key_hash, VNodeHash new_vnode_hash, VNodeHash next_vnode_hash) const;
    void move_key(const std::string& key, KeyHash key_hash,
                  Node* old_node, Node* new_node,
                  VNodeHash old_vnode_hash, VNodeHash new_vnode_hash);
    bool find_next_closest_internal(VNodeHash hash, VNodeHash& out_hash, NodeID& out_id) const;
    bool remap_keys_for_vnode(Node* node, VNodeHash vnode_hash);
    bool remap_subring_keys(Ring* subring, int level, Node* new_node, 
                            VNodeHash new_vnode_hash, VNodeHash next_vnode_hash);
    
    // Traversal helper
    void traversal_recursive(const Ring* ring, std::function<void(Node*)> operation) const;
    
    // Utilities
    std::unique_ptr<Circle> create_circle() const;
    std::string create_id();
};

} // namespace ringtree
