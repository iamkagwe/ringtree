package ringtree

import (
	"crypto/rand"
	"encoding/base64"
	"fmt"
)

func GenerateRandomString(length int) (string, error) {
	// Generate random bytes
	randomBytes := make([]byte, length)
	if _, err := rand.Read(randomBytes); err != nil {
		return "", err
	}

	// Encode to base64 for a printable string
	return base64.URLEncoding.EncodeToString(randomBytes), nil
}

func createId() string {
	str, _ := GenerateRandomString(20)
	return "node" + str
}

// Print the load details from the GetTotalLoads output.
func PrintLoad(rt *Ring) {
	total, loads := rt.GetLoads()
	fmt.Printf("Total: %d\n", total)
	fmt.Println("Loads: ", loads)
	fmt.Println("----------------------------")
}

// Print the load details from the GetTotalLoads output, including variance and standard deviation.
func PrintLoadDetails(rt *Ring) {
	loadDetails := rt.GetTotalLoads()
	for _, loadInfo := range loadDetails {
		fmt.Printf("RingID: %s, Level: %d\n", loadInfo.ID, loadInfo.Level)
		fmt.Printf("Node Loads: %v\n", loadInfo.Loads)
		fmt.Printf("Total Load: %d\n", loadInfo.Total)
		fmt.Printf("Mean: %.2f\n", loadInfo.Mean)
		fmt.Printf("Variance: %.2f\n", loadInfo.Variance)
		fmt.Printf("Standard Deviation: %.2f\n", loadInfo.Stdev)
		fmt.Println("----------------------------")
	}
}

// PrintSystemVariance prints the system-wide variance and standard deviation for all nodes.
func PrintSystemVariance(rt *Ring) {
	loads, totalMean, totalVariance, totalStdDev := rt.GetSystemVariance()
	fmt.Printf("All Node Loads: %v\n", loads)
	fmt.Printf("Num nodes: %d\n", len(loads))
	fmt.Println("----------------------------")
	fmt.Printf("Total Mean: %.2f\n", totalMean)
	fmt.Printf("Total Variance: %.2f\n", totalVariance)
	fmt.Printf("Total Standard Deviation: %.2f\n", totalStdDev)
	fmt.Println("----------------------------")
}

// PrintHierarchyDetails prints the depth of the hierarchy, number of nodes, and number of rings at each level.
func PrintHierarchyDetails(rt *Ring) {
	// Get the hierarchy information
	maxDepth, levelInfo, numKeys, numNodes := rt.GetHierarchyInfo()
	// Loop through each level to print its details
	for i := 0; i <= maxDepth; i++ {
		if info, ok := levelInfo[i]; ok {
			fmt.Printf("Level: %d\n", info.Level)
			fmt.Printf("Number of Nodes: %d\n", info.NodeCount)
			fmt.Printf("Number of Rings: %d\n", info.RingCount)
			fmt.Println("----------------------------")
		}
	}
	fmt.Printf("Total Depth of Hierarchy: %d\n", maxDepth)
	fmt.Println("----------------------------")

	fmt.Printf("Total Number of Nodes: %d\n", numNodes)
	fmt.Printf("Total Number of Keys: %d\n", numKeys)
	fmt.Println("----------------------------")
}

func PrintRemapStats() {
	// Calculate stats from the remap results
	_, total, avgRemapped, avgRatio := GetRemapStats()

	fmt.Printf("Total Times Keys Remapped: %d\n", total)
	fmt.Printf("Average Remapped per Valid Entry: %.2f\n", avgRemapped)
	fmt.Printf("Average Ratio (Actual/Expected): %.2f\n", avgRatio)
	fmt.Println("----------------------------")

	/* Print each remap entry for detailed insights
	fmt.Println("Detailed Remap Information:")
	for i, remap := range remaps {
		for actual, expected := range remap {
			if actual == 0 {
				continue // Skip entries with 0 actual remaps
			}
			fmt.Printf("Entry %d - Actual: %d, Expected: %d\n", i+1, actual, expected)
		}
	}
	fmt.Println("----------------------------")*/
}

func PrintOperationTimeStats() {
	stats := GetTimeStats()

	fmt.Println("Operation Time Statistics:")
	fmt.Println("-----------------------------------------------------")
	fmt.Printf("%-20s %-15s %-15s %-15s\n", "Operation", "Mean (µs)", "Variance", "StdDev")

	for operation, stat := range stats {
		fmt.Printf("%-20s %-15.2f %-15.2f %-15.2f\n", operation, stat["Mean"], stat["Variance"], stat["Stdev"])
	}
	fmt.Println("-----------------------------------------------------")
}
