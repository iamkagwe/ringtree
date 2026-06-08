#pragma once

#include "types.hpp"
#include <cstdint>

namespace ringtree {

// Forward declaration
class RingTree;

/**
 * RingTreeEval: Statistics and evaluation module
 * Provides query operations and performance metrics for ring state
 */
class RingTreeEval {
public:
    explicit RingTreeEval(const RingTree* ring_tree);
    
    // Query operations
    size_t get_ring_size() const;
    int64_t get_total_keys() const;
    int64_t get_total_load() const;
    
    // Statistics structure
    struct Stats {
        size_t total_nodes = 0;
        size_t total_vnodes = 0;
        int64_t total_keys = 0;
        int64_t total_load = 0;
        size_t arena_used = 0;
        size_t arena_capacity = 0;
        double avg_load = 0.0;
    };
    
    Stats get_stats() const;
    
private:
    const RingTree* ring_tree_;
    
    // Helper to extract load from Node or Ring member
    int64_t get_member_load(const Ring::Member& member) const;
};

} // namespace ringtree
