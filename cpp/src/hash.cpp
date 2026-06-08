#include "hash.hpp"
#include <functional>

namespace ringtree {

// MurmurHash3 64-bit implementation
// Based on code.google.com/p/smhasher/wiki/MurmurHash3
static inline uint64_t fmix64(uint64_t k) {
    k ^= k >> 33;
    k *= 0xff51afd7ed558ccdUL;
    k ^= k >> 33;
    return k;
}

uint64_t murmur3_64(const std::string& key) {
    const uint8_t* data = reinterpret_cast<const uint8_t*>(key.data());
    const size_t nblocks = key.length() / 16;
    uint64_t h1 = 0, h2 = 0;
    
    const uint64_t c1 = 0x87c4dba53dc22d13UL;
    const uint64_t c2 = 0x4cf5ad432745937fUL;
    
    const uint64_t* blocks = reinterpret_cast<const uint64_t*>(data);
    for (size_t i = 0; i < nblocks; ++i) {
        uint64_t k1 = blocks[i * 2 + 0];
        uint64_t k2 = blocks[i * 2 + 1];
        
        k1 *= c1;
        k1 = ((k1 << 31) | (k1 >> 33));
        k1 *= c2;
        h1 ^= k1;
        
        h2 ^= k2;
        h1 = ((h1 << 27) | (h1 >> 37));
        h1 = (h1 * 5 + 0x52dce729);
    }
    
    const uint8_t* tail = data + nblocks * 16;
    uint64_t k1 = 0, k2 = 0;
    
    switch (key.length() & 15) {
        case 15: k2 ^= ((uint64_t)tail[14]) << 48;
        case 14: k2 ^= ((uint64_t)tail[13]) << 40;
        case 13: k2 ^= ((uint64_t)tail[12]) << 32;
        case 12: k2 ^= ((uint64_t)tail[11]) << 24;
        case 11: k2 ^= ((uint64_t)tail[10]) << 16;
        case 10: k2 ^= ((uint64_t)tail[9]) << 8;
        case 9:
            k2 ^= (uint64_t)tail[8];
            k2 *= c2;
            k2 = ((k2 << 33) | (k2 >> 31));
            k2 *= c1;
            h2 ^= k2;
            
        case 8: k1 ^= ((uint64_t)tail[7]) << 56;
        case 7: k1 ^= ((uint64_t)tail[6]) << 48;
        case 6: k1 ^= ((uint64_t)tail[5]) << 40;
        case 5: k1 ^= ((uint64_t)tail[4]) << 32;
        case 4: k1 ^= ((uint64_t)tail[3]) << 24;
        case 3: k1 ^= ((uint64_t)tail[2]) << 16;
        case 2: k1 ^= ((uint64_t)tail[1]) << 8;
        case 1:
            k1 ^= (uint64_t)tail[0];
            k1 *= c1;
            k1 = ((k1 << 31) | (k1 >> 33));
            k1 *= c2;
            h1 ^= k1;
    }
    
    h1 ^= key.length();
    h2 ^= key.length();
    
    h1 += h2;
    h2 += h1;
    
    h1 = fmix64(h1);
    h2 = fmix64(h2);
    
    h1 += h2;
    return h1;
}

// FNV-1a 64-bit hash
uint64_t fnv1a_64(const std::string& key) {
    const uint64_t FNV_prime = 1099511628211UL;
    const uint64_t FNV_offset = 14695981039346656037UL;
    
    uint64_t hash = FNV_offset;
    for (unsigned char c : key) {
        hash ^= c;
        hash *= FNV_prime;
    }
    return hash;
}

// Combined hash: key hash XOR level-adjusted hash
VNodeHash hash(const std::string& key, int level) {
    const uint64_t key_hash = murmur3_64(key);
    const uint64_t level_hash = static_cast<uint64_t>(level) * 0x9E3779B97F4A7C15UL;
    return key_hash ^ ((level_hash << 32) | (level_hash >> 32));
}

// Inverse hash for power-of-two-choices
VNodeHash inverse_hash(const std::string& key, int level) {
    const VNodeHash primary = hash(key, level);
    return ~primary + 1; // Two's complement
}

// Locality-sensitive hash: progressively finer at higher levels
VNodeHash lsh_hash(const std::string& key, int level) {
    const uint64_t hash_val = fnv1a_64(key);
    
    // Bit precision increases with level
    int bits_to_keep = 4 + level;
    if (bits_to_keep > 64) bits_to_keep = 64;
    
    const uint64_t mask = ((1UL << bits_to_keep) - 1) << (64 - bits_to_keep);
    return hash_val & mask;
}

// Hierarchically distance-adaptive hash
VNodeHash hdah_hash(const std::string& key, int level) {
    const uint64_t hash_val = fnv1a_64(key);
    
    // Reduce bits at lower levels (more collisions = coarser)
    int bits_to_keep = 4 + 4 * level;
    if (bits_to_keep > 64) bits_to_keep = 64;
    
    const uint64_t mask = ((1UL << bits_to_keep) - 1) << (64 - bits_to_keep);
    return hash_val & mask;
}

} // namespace ringtree
