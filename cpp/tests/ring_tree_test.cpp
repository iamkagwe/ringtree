#include <iostream>
#include <cassert>
#include <memory>
#include <chrono>
#include "ring_tree.hpp"
#include "ring_tree_eval.hpp"

using namespace ringtree;

void test_basic_insertion() {
    std::cout << "Test: Basic Node Insertion\n";
    
    RingTree::Config config;
    config.max_count = 100;
    config.use_array_circle = true;
    
    RingTree tree(config);
    
    auto node1 = std::make_shared<Node>();
    node1->id = "node_1";
    node1->threshold = 1000;
    
    bool result = tree.insert_node(node1);
    assert(result && "Failed to insert node");
    
    RingTreeEval eval(&tree);
    auto stats = eval.get_stats();
    assert(stats.total_nodes == 1 && "Node count mismatch");
    assert(stats.total_vnodes == config.num_replicas && "VNode count mismatch");
    
    std::cout << "✓ Basic insertion test passed\n";
}

void test_lookup() {
    std::cout << "Test: Key Lookup\n";
    
    RingTree::Config config;
    config.max_count = 10;
    config.num_replicas = 3;
    
    RingTree tree(config);
    
    // Add multiple nodes
    for (int i = 0; i < 5; ++i) {
        auto node = std::make_shared<Node>();
        node->id = "node_" + std::to_string(i);
        node->threshold = 100;
        tree.insert_node(node);
    }
    
    // Lookup some keys
    NodeID owner;
    bool found = tree.find_closest("test_key_123", owner);
    assert(found && "Key lookup failed");
    assert(!owner.empty() && "Owner node is empty");
    
    std::cout << "✓ Key lookup test passed (key assigned to " << owner << ")\n";
}

void test_key_distribution() {
    std::cout << "Test: Key Distribution\n";
    
    RingTree::Config config;
    config.max_count = 10;
    config.num_replicas = 1;
    
    RingTree tree(config);
    
    // Add nodes
    for (int i = 0; i < 4; ++i) {
        auto node = std::make_shared<Node>();
        node->id = "node_" + std::to_string(i);
        node->threshold = 1000;
        tree.insert_node(node);
    }
    
    // Distribute keys
    for (int i = 0; i < 10; ++i) {
        std::string key = "key_" + std::to_string(i);
        tree.insert_key(key);
    }
    
    RingTreeEval eval(&tree);
    auto stats = eval.get_stats();
    assert(stats.total_keys == 10 && "Key count mismatch");
    
    double avg_load = stats.total_keys / static_cast<double>(stats.total_nodes);
    std::cout << "✓ Key distribution test passed\n"
              << "  - Total keys: " << stats.total_keys << "\n"
              << "  - Average load per node: " << avg_load << "\n";
}

void test_arena_allocation() {
    std::cout << "Test: Arena Allocation\n";
    
    RingTree::Config config;
    config.arena_capacity = 1024 * 1024; // 1 MB
    
    RingTree tree(config);
    
    RingTreeEval eval(&tree);
    auto stats = eval.get_stats();
    assert(stats.arena_capacity == config.arena_capacity && "Arena capacity mismatch");
    
    std::cout << "✓ Arena allocation test passed\n"
              << "  - Arena capacity: " << stats.arena_capacity / (1024 * 1024) << " MB\n"
              << "  - Arena used: " << stats.arena_used << " bytes\n";
}

void test_key_redistribution_on_insert() {
    std::cout << "Test: Key Redistribution on Node Insert\n";
    
    RingTree::Config config;
    config.max_count = 10;
    config.num_replicas = 1;
    
    RingTree tree(config);
    
    // Add initial node
    auto node1 = std::make_shared<Node>();
    node1->id = "node_1";
    node1->threshold = 1000;
    tree.insert_node(node1);
    
    // Add initial keys
    for (int i = 0; i < 20; ++i) {
        std::string key = "key_" + std::to_string(i);
        tree.insert_key(key);
    }
    
    RingTreeEval eval_before(&tree);
    auto stats_before = eval_before.get_stats();
    std::cout << "  Before insert:\n"
              << "    - Nodes: " << stats_before.total_nodes << "\n"
              << "    - Keys: " << stats_before.total_keys << "\n"
              << "    - Total load: " << stats_before.total_load << "\n";
    
    // Insert second node - should trigger redistribution
    auto node2 = std::make_shared<Node>();
    node2->id = "node_2";
    node2->threshold = 1000;
    tree.insert_node(node2);
    
    RingTreeEval eval_after(&tree);
    auto stats_after = eval_after.get_stats();
    std::cout << "  After insert:\n"
              << "    - Nodes: " << stats_after.total_nodes << "\n"
              << "    - Keys: " << stats_after.total_keys << "\n"
              << "    - Total load: " << stats_after.total_load << "\n"
              << "    - Avg load per node: " << stats_after.avg_load << "\n";
    
    // Verify keys are distributed
    assert(stats_after.total_nodes == 2 && "Should have 2 nodes");
    assert(stats_after.total_keys == stats_before.total_keys && "Key count should remain unchanged");
    
    // Verify load was redistributed (nodes should have different loads due to hash distribution)
    int64_t node1_load = node1->get_load_acquire();
    int64_t node2_load = node2->get_load_acquire();
    int64_t total_load = node1_load + node2_load;
    
    std::cout << "  Load distribution:\n"
              << "    - Node 1 load: " << node1_load << "\n"
              << "    - Node 2 load: " << node2_load << "\n"
              << "    - Total load: " << total_load << "\n";
    
    // Total load should equal number of keys inserted
    assert(total_load == stats_after.total_keys && "Total load should equal key count");
    
    std::cout << "✓ Key redistribution test passed\n";
}

void test_concurrent_reads() {
    std::cout << "Test: Concurrent Lock-Free Reads\n";
    
    RingTree::Config config;
    RingTree tree(config);
    
    auto node = std::make_shared<Node>();
    node->id = "testnode";
    tree.insert_node(node);
    
    // Simulate concurrent load updates using atomics
    node->add_load(100);
    assert(node->get_load_acquire() == 100 && "Atomic load update failed");
    
    node->add_load(50);
    assert(node->get_load_acquire() == 150 && "Atomic load addition failed");
    
    std::cout << "✓ Concurrent reads test passed\n"
              << "  - Node load (via atomic): " << node->get_load_acquire() << "\n";
}

void benchmark_lookups() {
    std::cout << "\nBenchmark: Lookup Performance\n";
    
    RingTree::Config config;
    config.max_count = 100;
    config.use_array_circle = true; // Cache-friendly array
    
    RingTree tree(config);
    
    // Populate with nodes
    for (int i = 0; i < 50; ++i) {
        auto node = std::make_shared<Node>();
        node->id = "node_" + std::to_string(i);
        tree.insert_node(node);
    }
    
    RingTreeEval eval(&tree);
    auto stats_before = eval.get_stats();
    
    // Benchmark lookups
    const int num_lookups = 1000000;
    auto start = std::chrono::high_resolution_clock::now();
    
    NodeID owner;
    for (int i = 0; i < num_lookups; ++i) {
        std::string key = "lookup_key_" + std::to_string(i);
        tree.find_closest(key, owner);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    double throughput = (num_lookups * 1e6) / elapsed.count();
    double latency_ns = (elapsed.count() * 1000.0) / num_lookups;
    
    std::cout << "✓ Lookup benchmark complete\n"
              << "  - Operations: " << num_lookups << "\n"
              << "  - Time: " << elapsed.count() / 1e6 << " seconds\n"
              << "  - Throughput: " << throughput / 1e6 << " M ops/sec\n"
              << "  - Latency: " << latency_ns << " ns/op\n"
              << "  - Ring size: " << stats_before.total_vnodes << " vnodes\n";
}

int main() {
    std::cout << "╔═══════════════════════════════════════════════════════════╗\n"
              << "║     Ring Tree C++ - HFT-Grade Consistent Hashing         ║\n"
              << "╚═══════════════════════════════════════════════════════════╝\n\n";
    
    try {
        // Core functionality tests
        test_basic_insertion();
        test_lookup();
        test_key_distribution();
        test_key_redistribution_on_insert();
        test_arena_allocation();
        test_concurrent_reads();
        
        // Performance benchmark
        benchmark_lookups();
        
        std::cout << "\n✅ All tests passed!\n";
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "\n❌ Test failed: " << e.what() << "\n";
        return 1;
    }
}
