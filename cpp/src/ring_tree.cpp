#include "ring_tree.hpp"
#include "hash.hpp"
#include <chrono>
#include <random>
#include <sstream>
#include <algorithm>
#include <shared_mutex>

namespace ringtree {

// High-performance consistent hash ring: arena allocation, lock-free atomics,
// shared_mutex for readers/writers, binary search (O(log N)) lookups.

RingTree::RingTree()
    : RingTree(Config()) {
}

RingTree::RingTree(const Config& config)
    : config_(config),
      main_ring_{.id = "main", .level = 0, .max_count = static_cast<int>(config.max_count)} {
    initialize_main_ring();
}

/**
 * Initializes the main ring to a clean state.
 * Called by constructor to ensure vnodes, subrings, and members are empty.
 */
void RingTree::initialize_main_ring() {
    main_ring_.vnodes.clear();
    main_ring_.members.clear();
    main_ring_.size.store(0, std::memory_order_release);
}

std::unique_ptr<Circle> RingTree::create_circle() const {
    if (config_.use_array_circle) {
        return std::make_unique<ArrayCircle>();
    } else {
        return std::make_unique<RBTreeCircle>();
    }
}

// Generate unique ID for subrings (random + timestamp)
std::string RingTree::create_id() {
    static std::mt19937_64 rng(
        std::chrono::steady_clock::now().time_since_epoch().count());
    
    std::stringstream ss;
    ss << "node_" << std::hex << rng();
    return ss.str();
}

// Inserts node into specified ring and its virtual nodes.
// Internal method used by insert_key_recursive for subrings.
bool RingTree::insert_node_to_ring(Ring* ring, std::shared_ptr<Node> node) {
    if (!ring) return false;
    
    if (static_cast<int>(ring->members.size()) >= ring->max_count) {
        return false; // Ring at capacity
    }
    
    if (ring->members.find(node->id) != ring->members.end()) {
        return false; // Node already exists
    }
    
    // Store shared_ptr to node
    ring->members[node->id] = node;
    
    // Add virtual nodes to circle
    for (int i = 0; i < config_.num_replicas; ++i) {
        VNodeHash vnode_hash = hash(node->id, i);
        
        VirtualNodeEntry entry{vnode_hash, node->id};
        ring->vnodes.push_back(entry);
    }
    
    // Sort vnodes for binary search
    std::sort(ring->vnodes.begin(), ring->vnodes.end());
    
    ring->size.fetch_add(1, std::memory_order_release);
    num_nodes_.fetch_add(1, std::memory_order_release);
    
    return true;
}

// Inserts node into main_ring_ (public API).
bool RingTree::insert_node(std::shared_ptr<Node> node) {
    std::unique_lock<std::shared_mutex> lock(main_ring_.ring_mutex);
    return insert_node_to_ring(&main_ring_, node);
}

// Removes node and all vnodes. Keys must be migrated by caller before removal.
bool RingTree::remove_node(const NodeID& node_id) {
    std::unique_lock<std::shared_mutex> lock(main_ring_.ring_mutex);
    
    auto it = main_ring_.members.find(node_id);
    if (it == main_ring_.members.end()) {
        return false; // Node not found
    }
    
    // Only remove if it's a Node, not a Ring
    if (!std::holds_alternative<std::shared_ptr<Node>>(it->second)) {
        return false;
    }
    
    auto node = std::get<std::shared_ptr<Node>>(it->second);
    
    // Remove all vnodes for this node
    auto new_end = std::remove_if(
        main_ring_.vnodes.begin(), main_ring_.vnodes.end(),
        [&node_id](const VirtualNodeEntry& entry) { return entry.node_id == node_id; }
    );
    main_ring_.vnodes.erase(new_end, main_ring_.vnodes.end());
    
    main_ring_.members.erase(it);
    main_ring_.size.fetch_sub(1, std::memory_order_release);
    num_nodes_.fetch_sub(1, std::memory_order_release);
    
    return true;
}

// Binary search for closest node (lower_bound). O(log N). Multiple readers allowed.
bool RingTree::find_closest(const std::string& key, NodeID& out_node_id) const {
    std::shared_lock<std::shared_mutex> lock(main_ring_.ring_mutex);
    
    Node* node = nullptr;
    Ring* node_ring = nullptr;
    VNodeHash vnode_hash = 0;
    
    if (!find_node_recursive(&main_ring_, key, node, node_ring, vnode_hash)) {
        return false;
    }
    
    out_node_id = node->id;
    return true;
}

// Binary search for successor node (upper_bound). O(log N). Useful for replication.
bool RingTree::find_next_closest(const std::string& key, NodeID& out_node_id) const {
    std::shared_lock<std::shared_mutex> lock(main_ring_.ring_mutex);
    
    if (main_ring_.vnodes.empty()) {
        return false;
    }
    
    VNodeHash key_hash = hash(key, 0);
    
    // Binary search for first vnode > key_hash
    auto it = std::upper_bound(
        main_ring_.vnodes.begin(), main_ring_.vnodes.end(),
        VirtualNodeEntry{key_hash, ""},
        [](const VirtualNodeEntry& a, const VirtualNodeEntry& b) {
            return a.hash < b.hash;
        }
    );
    
    if (it != main_ring_.vnodes.end()) {
        out_node_id = it->node_id;
        return true;
    }
    
    // Wrap around to first node
    if (!main_ring_.vnodes.empty()) {
        out_node_id = main_ring_.vnodes[0].node_id;
        return true;
    }
    
    return false;
}

// find_node: High-level API.
// Finds the node/ring/vnode responsible for a key, with recursive descent into subrings.
bool RingTree::find_node(const std::string& key, 
                         Node*& out_node, Ring*& out_ring, VNodeHash& out_vnode_hash) const {
    std::shared_lock<std::shared_mutex> lock(main_ring_.ring_mutex);
    return find_node_recursive(&main_ring_, key, out_node, out_ring, out_vnode_hash);
}

// lookup: Returns the node owner ID for a key.
bool RingTree::lookup(const std::string& key, NodeID& out_owner) const {
    Node* node = nullptr;
    Ring* node_ring = nullptr;
    VNodeHash vnode_hash = 0;
    
    if (find_node(key, node, node_ring, vnode_hash)) {
        out_owner = node->id;
        return true;
    }
    return false;
}

// members: Returns list of all member IDs in the ring.
std::vector<std::string> RingTree::members() const {
    std::shared_lock<std::shared_mutex> lock(main_ring_.ring_mutex);
    
    std::vector<std::string> result;
    for (const auto& [id, member] : main_ring_.members) {
        // Only add physical nodes, not subrings
        if (std::holds_alternative<std::shared_ptr<Node>>(member)) {
            result.push_back(id);
        }
    }
    return result;
}

// size: Returns count of members.
size_t RingTree::size() const {
    std::shared_lock<std::shared_mutex> lock(main_ring_.ring_mutex);
    return main_ring_.members.size();
}

// is_empty: Checks if ring has any keys.
bool RingTree::is_empty() const {
    std::shared_lock<std::shared_mutex> lock(main_ring_.ring_mutex);
    
    for (const auto& [id, member] : main_ring_.members) {
        if (std::holds_alternative<std::shared_ptr<Node>>(member)) {
            auto node = std::get<std::shared_ptr<Node>>(member);
            if (node->get_load_acquire() > 0) {
                return false;
            }
        }
    }
    return true;
}

// traversal: Recursively applies operation to all nodes.
void RingTree::traversal(std::function<void(Node*)> operation) const {
    std::shared_lock<std::shared_mutex> lock(main_ring_.ring_mutex);
    traversal_recursive(&main_ring_, operation);
}

// Auto-place key via recursive find_node. Handles overload by splitting nodes into subrings.
bool RingTree::insert_key(const std::string& key) {
    std::unique_lock<std::shared_mutex> lock(main_ring_.ring_mutex);
    return insert_key_recursive(&main_ring_, key, 0);
}

// RemoveKey: Removes key from ring and handles underflow.
bool RingTree::remove_key(const std::string& key) {
    std::unique_lock<std::shared_mutex> lock(main_ring_.ring_mutex);
    
    Node* node = nullptr;
    Ring* node_ring = nullptr;
    VNodeHash vnode_hash = 0;
    
    // Find which node owns this key
    if (!find_node_recursive(&main_ring_, key, node, node_ring, vnode_hash)) {
        return false;
    }
    
    // Check if key exists
    auto key_it = node->keys.find(key);
    if (key_it == node->keys.end()) {
        return false;
    }
    
    // Remove the key
    node->keys.erase(key_it);
    node->add_key_count(-1);
    node->add_load(-1);
    num_keys_.fetch_sub(1, std::memory_order_release);
    
    // TODO: Check underflow and potentially remove node (underflow handling at 20% threshold)
    
    return true;
}

// Auto-place key via recursive find_node. Handles overload by splitting nodes into subrings.

// Recursive key insertion with overload detection and node splitting.
bool RingTree::insert_key_recursive(Ring* ring, const std::string& key, int level) {
    if (!ring) return false;
    
    Node* node = nullptr;
    Ring* node_ring = nullptr;
    VNodeHash vnode_hash = 0;
    
    // Find the target node for this key (may be in a subring)
    if (!find_node_recursive(ring, key, node, node_ring, vnode_hash)) {
        return false;
    }
    
    // Key already exists
    if (node->keys.find(key) != node->keys.end()) {
        return false;
    }
    
    // Check if node is overloaded
    int64_t current_load = node->get_load_acquire();
    if (current_load < node->threshold) {
        // Normal insertion
        node->keys[key] = hash(key, level);
        node->add_key_count(1);
        node->add_load(1);
        num_keys_.fetch_add(1, std::memory_order_release);
        return true;
    }
    
    // Node is overloaded - try to add new node to parent ring
    if (static_cast<int>(node_ring->members.size()) < node_ring->max_count) {
        auto new_node = std::make_shared<Node>();
        new_node->id = "node_" + create_id();
        new_node->threshold = node->threshold;
        if (!insert_node_to_ring(node_ring, new_node)) {
            return false;
        }
        // Retry insert_key
        return insert_key_recursive(ring, key, level);
    }
    
    // Parent ring is full - split node into subring
    auto subring = split_node(node_ring, node);
    if (!subring) {
        return false;
    }
    
    // Recursively insert into subring
    return insert_key_recursive(subring.get(), key, level + 1);
}

// Recursively find node for key, descending into subrings if needed (internal implementation).
bool RingTree::find_node_recursive(const Ring* ring, const std::string& key, 
                                   Node*& out_node, Ring*& out_ring, 
                                   VNodeHash& out_vnode_hash) const {
    if (!ring || ring->vnodes.empty()) {
        return false;
    }
    
    VNodeHash key_hash = hash(key, 0);
    
    auto it = std::lower_bound(
        ring->vnodes.begin(), ring->vnodes.end(),
        VirtualNodeEntry{key_hash, ""},
        [](const VirtualNodeEntry& a, const VirtualNodeEntry& b) {
            return a.hash < b.hash;
        }
    );
    
    if (it == ring->vnodes.end()) {
        it = ring->vnodes.begin();
    }
    
    NodeID member_id = it->node_id;
    auto member_it = ring->members.find(member_id);
    if (member_it == ring->members.end()) {
        return false;
    }
    
    const Ring::Member& member = member_it->second;
    
    // Check if member is a Node or a Ring (subring)
    if (std::holds_alternative<std::shared_ptr<Node>>(member)) {
        // It's a physical node
        out_node = std::get<std::shared_ptr<Node>>(member).get();
        out_ring = const_cast<Ring*>(ring);
        out_vnode_hash = it->hash;
        return out_node != nullptr;
    } else {
        // It's a subring - recurse (member is shared_ptr<Ring>)
        auto subring = std::get<std::shared_ptr<Ring>>(member);
        return find_node_recursive(subring.get(), key, out_node, out_ring, out_vnode_hash);
    }
}

// traversal_recursive: Helper for traversal that recurses into subrings.
void RingTree::traversal_recursive(const Ring* ring, std::function<void(Node*)> operation) const {
    if (!ring) return;
    
    for (const auto& [id, member] : ring->members) {
        if (std::holds_alternative<std::shared_ptr<Node>>(member)) {
            auto node = std::get<std::shared_ptr<Node>>(member);
            operation(node.get());
        } else {
            // Member is shared_ptr<Ring> (subring)
            auto subring = std::get<std::shared_ptr<Ring>>(member);
            traversal_recursive(subring.get(), operation);
        }
    }
}

// should_collapse: Check if a ring should collapse back to a single node.
bool RingTree::should_collapse(Ring* ring) const {
    if (!ring || ring->members.size() > 2) {
        return false;
    }
    
    // Can only collapse if parent exists
    return ring->parent != nullptr;
}

// remap_subring_keys: Remap keys within subrings recursively.
bool RingTree::remap_subring_keys(Ring* subring, int level, Node* new_node, 
                                  VNodeHash new_vnode_hash, VNodeHash next_vnode_hash) {
    if (!subring) return false;
    
    for (const auto& [id, member] : subring->members) {
        if (std::holds_alternative<std::shared_ptr<Node>>(member)) {
            auto node = std::get<std::shared_ptr<Node>>(member);
            
            // Check keys at this level
            for (const auto& [key, key_hash] : node->keys) {
                if (should_move_key(key_hash, new_vnode_hash, next_vnode_hash)) {
                    move_key(key, key_hash, node.get(), new_node,
                            next_vnode_hash, new_vnode_hash);
                }
            }
        } else {
            // Recurse into deeper rings (member is shared_ptr<Ring>)
            auto deeper_ring = std::get<std::shared_ptr<Ring>>(member);
            if (!remap_subring_keys(deeper_ring.get(), level, new_node, new_vnode_hash, next_vnode_hash)) {
                return false;
            }
        }
    }
    
    return true;
}

// Split overloaded node into a subring with initial nodes.
std::shared_ptr<Ring> RingTree::split_node(Ring* parent_ring, Node* node) {
    if (!parent_ring || !node) {
        return nullptr;
    }
    
    // Create new subring with its own dedicated arena
    // This isolates vnode allocations from parent and siblings (key to hierarchical locality)
    auto subring = std::make_shared<Ring>();
    subring->id = "subring_" + node->id + "_" + create_id();
    subring->level = parent_ring->level + 1;
    subring->max_count = parent_ring->max_count * config_.branch_factor;
    subring->parent = parent_ring;
    // subring->arena is initialized by Ring constructor with default capacity
    
    // Backup node's keys
    auto old_keys = node->keys;
    node->keys.clear();
    node->load.store(0, std::memory_order_release);
    node->key_count.store(0, std::memory_order_release);
    
    // Add 2 initial nodes to subring for load distribution
    for (int i = 0; i < 2; ++i) {
        auto new_node = std::make_shared<Node>();
        new_node->id = "node_" + create_id();
        new_node->threshold = node->threshold;
        subring->members[new_node->id] = new_node;
        subring->size.fetch_add(1, std::memory_order_release);
        
        for (int j = 0; j < config_.num_replicas; ++j) {
            VNodeHash vnode_hash = hash(new_node->id, j);
            VirtualNodeEntry entry{vnode_hash, new_node->id};
            subring->vnodes.push_back(entry);
        }
    }
    std::sort(subring->vnodes.begin(), subring->vnodes.end());
    
    // Replace node with subring in parent's members (stores shared_ptr for lifecycle)
    parent_ring->members[node->id] = subring;
    
    // Re-insert all old keys into the subring
    for (auto& [key, key_hash] : old_keys) {
        if (!insert_key_recursive(subring.get(), key, parent_ring->level + 1)) {
            return nullptr;
        }
    }
    
    num_nodes_.fetch_sub(1, std::memory_order_release);  // Node became subring
    return subring;
}

// should_move_key: Check if key should move based on hash range.
bool RingTree::should_move_key(KeyHash key_hash, VNodeHash new_vnode_hash, VNodeHash next_vnode_hash) const {
    // Wraparound case: new_vnode_hash is larger than next_vnode_hash
    if (next_vnode_hash < new_vnode_hash) {
        // Move if key is in range (next_vnode_hash, new_vnode_hash]
        if (key_hash <= new_vnode_hash && key_hash > next_vnode_hash) {
            return true;
        }
    } else {
        // Regular case: new_vnode_hash is smallest
        if (key_hash <= new_vnode_hash) {
            return true;
        } else if (key_hash > new_vnode_hash && key_hash > next_vnode_hash) {
            return true;
        }
    }
    
    return false;
}

// move_key: Moves a key from one node to another, updating loads.
void RingTree::move_key(const std::string& key, KeyHash key_hash,
                        Node* old_node, Node* new_node,
                        VNodeHash old_vnode_hash, VNodeHash new_vnode_hash) {
    num_remaps_.fetch_add(1, std::memory_order_release);
    
    // Remove from old node's vnode
    old_node->keys.erase(key);
    old_node->add_key_count(-1);
    old_node->add_load(-1);
    
    // Add to new node's vnode
    new_node->keys[key] = key_hash;
    new_node->add_key_count(1);
    new_node->add_load(1);
}

// find_next_closest_internal: Finds the successor vnode.
bool RingTree::find_next_closest_internal(VNodeHash hash, VNodeHash& out_hash, NodeID& out_id) const {
    if (main_ring_.vnodes.empty()) {
        return false;
    }
    
    // Find first vnode strictly greater than hash
    auto it = std::upper_bound(
        main_ring_.vnodes.begin(), main_ring_.vnodes.end(),
        VirtualNodeEntry{hash, ""},
        [](const VirtualNodeEntry& a, const VirtualNodeEntry& b) {
            return a.hash < b.hash;
        }
    );
    
    if (it != main_ring_.vnodes.end()) {
        out_hash = it->hash;
        out_id = it->node_id;
        return true;
    }
    
    // Wrap to first vnode
    if (!main_ring_.vnodes.empty()) {
        out_hash = main_ring_.vnodes[0].hash;
        out_id = main_ring_.vnodes[0].node_id;
        return true;
    }
    
    return false;
}

// remap_keys_for_vnode: Remaps keys after adding a new vnode.
bool RingTree::remap_keys_for_vnode(Node* node, VNodeHash vnode_hash) {
    VNodeHash next_vnode_hash = 0;
    NodeID next_node_id;
    
    // Find the next vnode
    if (!find_next_closest_internal(vnode_hash, next_vnode_hash, next_node_id)) {
        return false;
    }
    
    // Find the next node member
    auto it = main_ring_.members.find(next_node_id);
    if (it == main_ring_.members.end()) {
        return false;
    }
    
    const Ring::Member& member = it->second;
    
    // Handle if next member is a Node or Ring
    if (std::holds_alternative<std::shared_ptr<Node>>(member)) {
        auto next_node = std::get<std::shared_ptr<Node>>(member);
        
        // Check keys in next node's vnode
        auto keys_to_check = next_node->keys;  // Copy for safe iteration
        for (const auto& [key, key_hash] : keys_to_check) {
            if (should_move_key(key_hash, vnode_hash, next_vnode_hash)) {
                move_key(key, key_hash, next_node.get(), node, next_vnode_hash, vnode_hash);
            }
        }
    } else {
        // Next member is a Ring (subring) stored as shared_ptr<Ring>
        auto subring = std::get<std::shared_ptr<Ring>>(member);
        if (!remap_subring_keys(subring.get(), main_ring_.level, node, vnode_hash, next_vnode_hash)) {
            return false;
        }
    }
    
    return true;
}

// collapse_ring: Convert a subring back to a single node.
bool RingTree::collapse_ring(std::shared_ptr<Ring> subring) {
    if (!subring || !subring->parent) {
        return false;  // Can't collapse root or orphaned ring
    }
    
    if (subring->members.size() > 2) {
        return false;  // Can only collapse if at most 2 members
    }
    
    Ring* parent = subring->parent;
    
    // Collect all keys from all nodes in the subring
    std::unordered_map<std::string, KeyHash> old_keys;
    for (const auto& [id, member] : subring->members) {
        if (std::holds_alternative<std::shared_ptr<Node>>(member)) {
            auto node = std::get<std::shared_ptr<Node>>(member);
            for (const auto& [key, key_hash] : node->keys) {
                old_keys[key] = key_hash;
            }
        }
    }
    
    // Replace subring with a single node in parent
    auto new_node = std::make_shared<Node>();
    new_node->id = subring->id;  // Preserve subring's ID
    new_node->threshold = 1000;  // Default threshold (should be per-node)
    
    // Add to parent
    parent->members[new_node->id] = new_node;
    
    // Add vnodes for the new node
    for (int i = 0; i < config_.num_replicas; ++i) {
        VNodeHash vnode_hash = hash(new_node->id, i);
        VirtualNodeEntry entry{vnode_hash, new_node->id};
        parent->vnodes.push_back(entry);
    }
    std::sort(parent->vnodes.begin(), parent->vnodes.end());
    
    // Reinsert all old keys into parent ring
    for (const auto& [key, key_hash] : old_keys) {
        // Re-insert through parent's insert_key_recursive
        if (!insert_key_recursive(parent, key, parent->level)) {
            return false;
        }
    }
    
    return true;
}

} // namespace ringtree
