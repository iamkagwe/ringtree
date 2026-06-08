#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>
#include "arena_allocator.hpp"

namespace ringtree {

using VNodeHash = uint64_t;
using NodeID = std::string;
using KeyHash = uint64_t;

// Forward declarations
struct Node;
struct Ring;

/**
 * VirtualNodeEntry: Maps vnode hash to physical node/subring
 * Packed tightly for cache efficiency during lookups.
 */
struct VirtualNodeEntry {
    VNodeHash hash;
    NodeID node_id;
    
    bool operator<(const VirtualNodeEntry& other) const {
        return hash < other.hash;
    }
};

/**
 * Node: Represents a physical server
 */
struct Node {
    std::string id;
    std::atomic<int64_t> load{0};
    std::atomic<int64_t> key_count{0};
    int threshold = 0;
    
    // Map of key_string -> key_hash (simpler than nested map)
    std::unordered_map<std::string, KeyHash> keys;
    
    // Lock-free load tracking for HFT scenarios
    void add_load(int64_t delta) {
        load.fetch_add(delta, std::memory_order_release);
    }
    
    void add_key_count(int64_t delta) {
        key_count.fetch_add(delta, std::memory_order_release);
    }
    
    int64_t get_load_acquire() const {
        return load.load(std::memory_order_acquire);
    }
};

/**
 * Ring: Main hierarchical consistent hashing ring
 * Contains vnodes and physical nodes/subrings.
 * 
 * CRITICAL DESIGN: Each ring has its OWN ArenaAllocator to maintain
 * hierarchical locality. Changes to a subring's vnodes/keys affect ONLY
 * that subring's arena, enabling independent parallelization of sibling
 * subrings. When a subring is discarded, its entire arena is freed at once.
 */
struct Ring {
    std::string id;
    int level = 0;
    std::atomic<int> size{0};           // Number of members
    int max_count = 0;
    
    Ring* parent = nullptr;             // Pointer to parent ring (not owned)
    
    // Per-ring arena allocator: isolates this ring's vnode allocations
    // from siblings and parent. Subrings have independent arenas.
    ArenaAllocator arena{64 * 1024 * 1024};  // 64 MB per ring
    
    // VNode storage: keep sorted for binary search
    std::vector<VirtualNodeEntry> vnodes;
    
    // Member tracking: can be either Node or subring Ring
    // Using shared_ptr<Ring> (not raw Ring*) for proper lifecycle management
    using Member = std::variant<std::shared_ptr<Node>, std::shared_ptr<Ring>>;
    std::unordered_map<std::string, Member> members;
    
    // Per-ring synchronization (each ring has its own lock)
    mutable std::shared_mutex ring_mutex;  // RW lock for this ring's consistency
};

} // namespace ringtree
