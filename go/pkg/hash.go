package ringtree

import (
	"hash/fnv"

	"github.com/spaolacci/murmur3"
)

// Hierarchically distance-adaptive hash family for strings
// Each level uses more bits from the hash, decreasing collision probability for distant points
func hdahHash(key string, level int) uint64 {
	// Use FNV-1a 64-bit hash for the string
	h := fnv.New64a()
	h.Write([]byte(key))
	hashVal := h.Sum64()

	// At each level, use more bits: bitsToKeep = 4 + 4*level (min 4, max 64)
	bitsToKeep := 4 + 4*level
	if bitsToKeep > 64 {
		bitsToKeep = 64
	}
	mask := uint64((1<<bitsToKeep)-1) << (64 - bitsToKeep)
	return hashVal & mask
}

// hash returns a hash value based on the key and level, ensuring remap compatibility.
func hash(key string, level int) uint64 {
	// Hash the key independently
	keyHash := murmur3.Sum64([]byte(key))

	// Create a level-specific hash
	levelHash := uint64(level) * 0x9E3779B97F4A7C15 // 64-bit golden ratio constant

	// Combine the two hashes using XOR and shifts
	return keyHash ^ ((levelHash << 32) | (levelHash >> 32))
}

// inverseHash returns an alternative hash for Power of Two Choices
func inverseHash(key string, level int) uint64 {
	primaryHash := hash(key, level)
	return ^primaryHash + 1 // Equivalent to (2^64 - h) for uint64
}

// lshHash is a simple locality sensitive hash for strings, parameterized by level.
// At low levels, it produces coarse hashes (more collisions); at high levels, finer hashes.
func lshHash(key string, level int) uint64 {
	// Use FNV-1a 64-bit hash to get a bit vector from the string
	h := fnv.New64a()
	h.Write([]byte(key))
	hashVal := h.Sum64()

	// Simulate locality by masking only the top N bits, where N = 4 + level (min 4 bits)
	// This means low level = coarse, high level = fine
	bitsToKeep := 4 + level
	if bitsToKeep > 64 {
		bitsToKeep = 64
	}
	mask := uint64((1<<bitsToKeep)-1) << (64 - bitsToKeep)
	return hashVal & mask
}
