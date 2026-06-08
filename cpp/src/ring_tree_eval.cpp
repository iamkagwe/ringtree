#include "ring_tree_eval.hpp"
#include "ring_tree.hpp"
#include <shared_mutex>

namespace ringtree {

RingTreeEval::RingTreeEval(const RingTree* ring_tree)
    : ring_tree_(ring_tree) {
}

// Query number of physical nodes. O(1) with shared_lock.
size_t RingTreeEval::get_ring_size() const {
    if (!ring_tree_) return 0;
    std::shared_lock<std::shared_mutex> lock(ring_tree_->main_ring_.ring_mutex);
    return ring_tree_->main_ring_.members.size();
}

// Lock-free atomic read of total keys using memory_order_acquire.
int64_t RingTreeEval::get_total_keys() const {
    if (!ring_tree_) return 0;
    return ring_tree_->num_keys_.load(std::memory_order_acquire);
}

// Sum node loads for load-balance analysis. O(N) iteration with shared_lock.
int64_t RingTreeEval::get_total_load() const {
    if (!ring_tree_) return 0;
    std::shared_lock<std::shared_mutex> lock(ring_tree_->main_ring_.ring_mutex);
    
    int64_t total = 0;
    for (const auto& [id, member] : ring_tree_->main_ring_.members) {
        total += get_member_load(member);
    }
    
    return total;
}

// Gather stats: nodes, vnodes, keys, load, arena usage, avg_load. O(N) iteration.
RingTreeEval::Stats RingTreeEval::get_stats() const {
    if (!ring_tree_) {
        return Stats();
    }
    
    std::shared_lock<std::shared_mutex> lock(ring_tree_->main_ring_.ring_mutex);
    
    Stats stats;
    stats.total_nodes = ring_tree_->num_nodes_.load(std::memory_order_acquire);
    stats.total_vnodes = ring_tree_->main_ring_.vnodes.size();
    stats.total_keys = ring_tree_->num_keys_.load(std::memory_order_acquire);
    stats.total_load = get_total_load();
    // Get arena stats from main ring (each ring has its own arena now)
    stats.arena_used = ring_tree_->main_ring_.arena.get_used();
    stats.arena_capacity = ring_tree_->main_ring_.arena.get_capacity();
    
    if (stats.total_nodes > 0) {
        stats.avg_load = static_cast<double>(stats.total_load) / stats.total_nodes;
    }
    
    return stats;
}

// Helper to get load from either a Node or Ring member.
int64_t RingTreeEval::get_member_load(const Ring::Member& member) const {
    if (std::holds_alternative<std::shared_ptr<Node>>(member)) {
        return std::get<std::shared_ptr<Node>>(member)->get_load_acquire();
    } else {
        auto subring = std::get<std::shared_ptr<Ring>>(member);
        int64_t total = 0;
        for (const auto& [id, submember] : subring->members) {
            total += get_member_load(submember);
        }
        return total;
    }
}

} // namespace ringtree
