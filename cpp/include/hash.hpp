#pragma once

#include "types.hpp"
#include <cstdint>
#include <string>

namespace ringtree {

/**
 * Fast hash functions for ring tree operations
 * Designed for HFT-grade performance consistency
 */

/**
 * MurmurHash3: Fast, well-distributed, avalanche property
 * Returns consistent results across platforms (used in Go version)
 */
uint64_t murmur3_64(const std::string& key);

/**
 * FNV-1a: Fast, simple, good for general hashing
 * Used for vnode hash generation
 */
uint64_t fnv1a_64(const std::string& key);

/**
 * hash: Combined hash for vnode generation
 * Combines key hash with level-specific data
 * 
 * @param key String key to hash
 * @param level Ring hierarchy level
 * @return Combined hash value
 */
VNodeHash hash(const std::string& key, int level);

/**
 * inverse_hash: Complementary hash for power-of-two-choices
 * Used in load balancing decisions
 * 
 * @param key String key to hash
 * @param level Ring hierarchy level
 * @return Inverted hash value
 */
VNodeHash inverse_hash(const std::string& key, int level);

/**
 * lsh_hash: Locality-sensitive hash for multi-level trees
 * Coarser hashes at lower levels, finer at higher levels
 * 
 * @param key String key to hash
 * @param level Ring hierarchy level
 * @return Level-aware locality-sensitive hash
 */
VNodeHash lsh_hash(const std::string& key, int level);

/**
 * hdah_hash: Hierarchically distance-adaptive hash
 * Reduces collision probability as hierarchy deepens
 * 
 * @param key String key to hash
 * @param level Ring hierarchy level
 * @return Distance-adaptive hash value
 */
VNodeHash hdah_hash(const std::string& key, int level);

} // namespace ringtree
