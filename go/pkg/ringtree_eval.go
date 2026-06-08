package ringtree

import (
	"log"
	"math"
	"os"
	"runtime"
	"runtime/pprof"
	"sync"
	"time"
)

var numNodes int = 0 // tracks total number of keys
var numKeys int = 0  // tracks total number of nodes

var remaps []map[int]int // aggregates instantaneous remapping operations [actual:expected]
var remapped int = 0     // tracks the number of keys being remapped in the current operation

var timerStatus = sync.Map{}                          // Tracks active timers to avoid double logging
var operationTimes = make(map[string][]time.Duration) // Tracks elapsed times for each operation

// Helper function to compute the sum of a slice of integers.
func sum(loads []int) int {
	total := 0
	for _, load := range loads {
		total += load
	}
	return total
}

// RingInfo represents the structure for each ring's load information.
type RingInfo struct {
	ID       string
	Level    int
	Loads    []int
	Total    int
	Mean     float64
	Variance float64
	Stdev    float64
}

// LevelInfo stores the information for each level of the hierarchy.
type LevelInfo struct {
	Level     int // The level number in the hierarchy
	NodeCount int // The number of nodes at this level
	RingCount int // The number of subrings at this level
}

func timeTrack(start time.Time, operation string, message string) {
	elapsed := time.Since(start)
	//log.Printf("%s took %s %s.", operation, elapsed, message)

	// Track elapsed time for stats
	if operationTimes[operation] == nil {
		operationTimes[operation] = make([]time.Duration, 0)
	}
	operationTimes[operation] = append(operationTimes[operation], elapsed)
}

func memoryProfile(filename string) {
	f, err := os.Create(filename)
	if err != nil {
		log.Fatal("could not create memory profile: ", err)
	}
	defer f.Close()

	if err := pprof.WriteHeapProfile(f); err != nil {
		log.Fatal("could not write memory profile: ", err)
	}
}

func logMemoryUsage(operation string) {
	var m runtime.MemStats
	runtime.ReadMemStats(&m)
	log.Printf("%s: Alloc = %v KB, TotalAlloc = %v KB, Sys = %v KB, NumGC = %v\n",
		operation, m.Alloc/1024, m.TotalAlloc/1024, m.Sys/1024, m.NumGC)
}

// Recursively calculates the depth of the hierarchy.
func (r *Ring) GetDepth() int {
	var getDepth func(*Ring, int) int
	getDepth = func(ring *Ring, depth int) int {
		maxDepth := depth
		for _, member := range ring.members {
			if subring, ok := member.(*Ring); ok {
				subringDepth := getDepth(subring, depth+1)
				if subringDepth > maxDepth {
					maxDepth = subringDepth
				}
			}
		}
		return maxDepth
	}
	return getDepth(r, 0)
}

// GetLoads calculates the total load and individual node loads within a ring (excluding subrings).
func (r *Ring) GetLoads() (int, []int) {
	total := 0
	var loads []int

	for _, member := range r.members {
		if node, ok := member.(*Node); ok {
			loads = append(loads, node.load)
			total += node.load
		}
	}
	return total, loads
}

// Collects load statistics for all rings and subrings (run this on R0).
func (r *Ring) GetTotalLoads() []RingInfo {
	var result []RingInfo

	// Helper function to recursively gather loads.
	var gatherLoads func(*Ring) RingInfo
	gatherLoads = func(ring *Ring) RingInfo {
		var loads []int
		ringInfo := RingInfo{
			ID:    ring.id,
			Level: ring.level,
		}

		// Calculate loads and recurse through members.
		for _, member := range ring.members {
			switch member := member.(type) {
			case *Node:
				loads = append(loads, member.load)
			case *Ring:
				subringInfo := gatherLoads(member)
				loads = append(loads, subringInfo.Total)
				result = append(result, subringInfo)
			}
		}

		// Aggregate loads and compute stats.
		ringInfo.Total, ringInfo.Loads = sum(loads), loads
		ringInfo.Mean, ringInfo.Variance, ringInfo.Stdev = calculateStats(loads)

		return ringInfo
	}

	// Start with the top-level ring.
	mainRingInfo := gatherLoads(r)
	return append(result, mainRingInfo)
}

// Collects variance and standard deviation across the entire system.
func (r *Ring) GetSystemVariance() ([]int, float64, float64, float64) {
	var allLoads []int

	// Helper function to gather all node loads.
	var gatherAllLoads func(*Ring)
	gatherAllLoads = func(ring *Ring) {
		_, loads := ring.GetLoads() // Use GetLoads to extract node loads.
		allLoads = append(allLoads, loads...)

		for _, member := range ring.members {
			if subring, ok := member.(*Ring); ok {
				gatherAllLoads(subring)
			}
		}
	}

	gatherAllLoads(r) // Start from the top ring.

	// Calculate and return variance and standard deviation.
	mean, variance, stdDev := calculateStats(allLoads)
	return allLoads, mean, variance, stdDev
}

// GetHierarchyInfo calculates the depth of the hierarchy, the number of nodes, and the number of rings at each level.
func (r *Ring) GetHierarchyInfo() (int, map[int]LevelInfo, int, int) {
	levelInfo := make(map[int]LevelInfo)
	maxDepth := 0 // Track the maximum depth dynamically.

	// Helper function to gather level information and track depth.
	var gatherLevelInfo func(*Ring, int)
	gatherLevelInfo = func(ring *Ring, currentDepth int) {
		// Update maxDepth if the current depth is greater.
		if currentDepth > maxDepth {
			maxDepth = currentDepth
		}

		// Initialize level info if not present.
		if _, exists := levelInfo[currentDepth]; !exists {
			levelInfo[currentDepth] = LevelInfo{
				Level:     currentDepth,
				NodeCount: 0,
				RingCount: 0,
			}
		}

		// Increment the ring count at this level.
		info := levelInfo[currentDepth]
		info.RingCount++

		// Traverse members to count nodes and recurse into subrings.
		for _, member := range ring.members {
			switch member := member.(type) {
			case *Node:
				info.NodeCount++
			case *Ring:
				// Recurse into the subring, increasing the depth.
				gatherLevelInfo(member, currentDepth+1)
			}
		}
		levelInfo[currentDepth] = info
	}

	// Start gathering from the root ring at depth 0.
	gatherLevelInfo(r, 0)

	// Calculate total nodes and keys.
	return maxDepth, levelInfo, numKeys, numNodes
}

// Extracts remap statistics.
func GetRemapStats() ([]map[int]int, int, float64, float64) {
	totalRemapped, totalExpected, validEntries := 0, 0, 0

	for _, remap := range remaps {
		for actual, expected := range remap {
			if actual == 0 {
				continue
			}
			totalRemapped += actual
			totalExpected += expected
			validEntries++
		}
	}

	averageRemapped := float64(totalRemapped) / float64(validEntries)
	averageRatio := float64(totalRemapped) / float64(totalExpected)

	return remaps, totalRemapped, averageRemapped, averageRatio
}

// Time Complexity
func GetTimeStats() map[string]map[string]float64 {
	stats := make(map[string]map[string]float64)

	for operation, times := range operationTimes {
		if len(times) == 0 {
			continue // Skip empty operations
		}

		// Convert time.Duration to float64 (nanoseconds for higher precision)
		var durations []float64
		for _, t := range times {
			durations = append(durations, float64(t.Nanoseconds())/1000.0) // Convert to microseconds as float
		}

		// Calculate mean, variance, and standard deviation
		mean, variance, stdDev := calculateStatsFloat64(durations)

		// Store stats
		stats[operation] = map[string]float64{
			"Mean":     mean,
			"Variance": variance,
			"Stdev":    stdDev,
		}
	}
	return stats
}

// Appends remap complexity data to the remaps slice.
func calculateRemapComplexity() {
	if numNodes == 0 {
		numNodes = 1
	}
	expectedRemaps := numKeys / numNodes
	remaps = append(remaps, map[int]int{remapped: expectedRemaps})
	remapped = 0
}

// Utility function to calculate mean, variance, and standard deviation.
func calculateStats(values []int) (float64, float64, float64) {
	n := len(values)
	if n == 0 {
		return 0, 0, 0
	}

	sum := 0
	for _, v := range values {
		sum += v
	}
	mean := float64(sum) / float64(n)

	var varianceSum float64
	for _, v := range values {
		varianceSum += (float64(v) - mean) * (float64(v) - mean)
	}
	variance := varianceSum / float64(n)
	stdDev := math.Sqrt(variance)

	return mean, variance, stdDev
}

// calculateStatsFloat64 works for []float64 input
func calculateStatsFloat64(values []float64) (float64, float64, float64) {
	//fmt.Println(values)
	n := len(values)
	if n == 0 {
		return 0, 0, 0
	}

	sum := 0.0
	for _, v := range values {
		sum += v
	}
	mean := sum / float64(n)

	var varianceSum float64
	for _, v := range values {
		varianceSum += (v - mean) * (v - mean)
	}
	variance := varianceSum / float64(n)
	stdDev := math.Sqrt(variance)

	return mean, variance, stdDev
}
